# Mutation fuzzer for the DLS parser. A user-supplied .dls is untrusted input, so the parser
# must reject a corrupt file rather than fault. Any exit code other than 0 (parsed) or
# 1 (rejected cleanly) is a defect.

[CmdletBinding()]
param(
    [string] $Exe = (Join-Path $PSScriptRoot '..\..\out\x64\Release\synthspike-dls.exe'),
    [string] $SeedFile = "$env:WINDIR\System32\drivers\gm.dls",
    [int] $Iterations = 400,
    [int] $Seed = 20260913
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $Exe))     { throw "executable not found: $Exe" }
if (-not (Test-Path $SeedFile)) { throw "seed file not found: $SeedFile" }

$Exe = (Resolve-Path $Exe).Path
$original = [System.IO.File]::ReadAllBytes($SeedFile)
$random = [System.Random]::new($Seed)

$workDir = Join-Path $env:TEMP 'dlsfuzz'
New-Item -ItemType Directory -Force -Path $workDir | Out-Null
$casePath = Join-Path $workDir 'case.dls'
$stdoutPath = Join-Path $workDir 'stdout.txt'
$stderrPath = Join-Path $workDir 'stderr.txt'

# Structure (RIFF headers, instrument list, pool table) lives in the first part of the file.
# Biasing mutations there hits far more parser paths than corrupting sample data would.
$structureBytes = [Math]::Min(400000, $original.Length)

$exitCodes = @{}
$crashes = [System.Collections.Generic.List[string]]::new()

for ($i = 0; $i -lt $Iterations; $i++) {
    $bytes = [byte[]]::new($original.Length)
    [Array]::Copy($original, $bytes, $original.Length)

    $mutations = $random.Next(1, 9)
    for ($m = 0; $m -lt $mutations; $m++) {
        $offset = if ($random.Next(100) -lt 85) { $random.Next(0, $structureBytes) } else { $random.Next(0, $bytes.Length) }
        $bytes[$offset] = [byte]$random.Next(0, 256)
    }

    # One case in four is also truncated, which is the classic way a length field ends up
    # describing more data than exists.
    $length = $bytes.Length
    if ($random.Next(4) -eq 0) {
        $length = $random.Next(8, $bytes.Length)
    }

    [System.IO.File]::WriteAllBytes($casePath, $bytes[0..($length - 1)])

    $process = Start-Process -FilePath $Exe -ArgumentList "`"$casePath`"", '--waves' `
        -NoNewWindow -Wait -PassThru -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
    $code = $process.ExitCode

    $exitCodes[$code] = 1 + ($exitCodes[$code] ?? 0)

    if ($code -ne 0 -and $code -ne 1) {
        $keepPath = Join-Path $workDir ("crash-{0}-{1:x8}.dls" -f $i, $code)
        [System.IO.File]::Copy($casePath, $keepPath, $true)
        $crashes.Add("iteration $i exit 0x{0:x8} saved to $keepPath" -f $code)
    }
}

Write-Output "cases run: $Iterations"
Write-Output 'exit code distribution:'
foreach ($entry in $exitCodes.GetEnumerator() | Sort-Object Name) {
    $label = switch ($entry.Key) {
        0 { 'parsed' }
        1 { 'rejected' }
        default { 'UNEXPECTED' }
    }
    Write-Output ("  {0,-12} {1,-10} {2}" -f $entry.Key, $label, $entry.Value)
}

if ($crashes.Count -gt 0) {
    Write-Output ''
    Write-Output "FAILURES: $($crashes.Count)"
    $crashes | ForEach-Object { Write-Output "  $_" }
    exit 1
}

Write-Output ''
Write-Output 'PASS - no faults, every case either parsed or was rejected cleanly'
exit 0
