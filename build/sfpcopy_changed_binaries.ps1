# Copies locally built binaries over their System32 counterparts using sfpcopy.
#
# Only files that actually differ are copied, so this is safe to re-run and tells you exactly
# what changed. midisrv holds the transport DLLs open, so the service is stopped first.
#
# Must be run ELEVATED.

[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [string] $SourceDir = "g:\Github\microsoft\midi\src\in-box\VSFiles\x64\Release",
    [string] $SfpCopy   = "g:\sfpcopy.exe",
    [string] $DestDir   = (Join-Path $env:WINDIR "System32"),

    # Skip stopping/starting midisrv. The copies will fail for anything currently loaded.
    [switch] $NoServiceRestart,

    # Copy even when the file is already identical.
    [switch] $Force
)

$ErrorActionPreference = 'Stop'

if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator))
{
    Write-Error "Run this from an elevated prompt. Replacing files in System32 requires it."
    return
}

foreach ($p in @($SourceDir, $SfpCopy))
{
    if (-not (Test-Path $p)) { Write-Error "Not found: $p"; return }
}

# The System32 copy is what decides the candidate list: anything without one there is either a
# test binary or ships somewhere else (the Network transport goes to Program Files, not System32).
# wdmaud2.drv is loaded into every winmm client, not into midisrv, so stopping the service does
# not release it and those clients must be restarted before they pick up a new copy.
$candidates = Get-ChildItem -Path (Join-Path $SourceDir "*.dll"), (Join-Path $SourceDir "*.exe"), (Join-Path $SourceDir "*.drv") -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -notmatch 'unittests' } |
    Where-Object { Test-Path (Join-Path $DestDir $_.Name) }

if (-not $candidates)
{
    Write-Warning "Nothing in $SourceDir has a counterpart in $DestDir."
    return
}

$toCopy = foreach ($file in $candidates)
{
    $dest = Join-Path $DestDir $file.Name

    if ($Force)
    {
        [pscustomobject]@{ Source = $file.FullName; Dest = $dest; Name = $file.Name; Reason = "forced" }
        continue
    }

    $srcHash = (Get-FileHash -Algorithm SHA256 $file.FullName).Hash
    $dstHash = (Get-FileHash -Algorithm SHA256 $dest).Hash

    if ($srcHash -ne $dstHash)
    {
        [pscustomobject]@{ Source = $file.FullName; Dest = $dest; Name = $file.Name; Reason = "differs" }
    }
}

if (-not $toCopy)
{
    Write-Host "System32 already matches the build output. Nothing to do." -ForegroundColor Green
    return
}

Write-Host "`nWill copy $($toCopy.Count) file(s):" -ForegroundColor Cyan
$toCopy | ForEach-Object { "  {0,-45} ({1})" -f $_.Name, $_.Reason }

$serviceWasRunning = $false

if (-not $NoServiceRestart)
{
    $svc = Get-Service midisrv -ErrorAction SilentlyContinue

    if ($svc -and $svc.Status -eq 'Running')
    {
        $serviceWasRunning = $true

        if ($PSCmdlet.ShouldProcess("midisrv", "Stop service"))
        {
            Write-Host "`nStopping midisrv..." -ForegroundColor Yellow
            Stop-Service midisrv -Force
            (Get-Service midisrv).WaitForStatus('Stopped', '00:00:30')
        }
    }
}

$failed = @()

foreach ($item in $toCopy)
{
    if (-not $PSCmdlet.ShouldProcess($item.Dest, "sfpcopy from $($item.Source)"))
    {
        continue
    }

    Write-Host "`n$($item.Name)" -ForegroundColor White
    & $SfpCopy $item.Source $item.Dest

    if ($LASTEXITCODE -ne 0)
    {
        Write-Host "  FAILED (exit $LASTEXITCODE)" -ForegroundColor Red
        $failed += $item.Name
    }
    else
    {
        Write-Host "  ok" -ForegroundColor Green
    }
}

if ($serviceWasRunning)
{
    if ($PSCmdlet.ShouldProcess("midisrv", "Start service"))
    {
        Write-Host "`nStarting midisrv..." -ForegroundColor Yellow
        Start-Service midisrv
    }
}

if ($failed)
{
    Write-Host "`n$($failed.Count) file(s) failed:" -ForegroundColor Red
    $failed | ForEach-Object { "  $_" }
}
else
{
    Write-Host "`nDone." -ForegroundColor Green
}
