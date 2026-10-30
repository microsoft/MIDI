# PROTOTYPE. Runs WerCrashTest.exe once per crash kind and builds the results table from the logs.
# See README.md. Run NOT elevated, as a customer would run a MIDI app, after setup-registry.ps1.
#
# Every run crashes WerCrashTest.exe on purpose. WER then writes a report and, if LocalDumps is set
# up, a dump to %LOCALAPPDATA%\Microsoft\MIDI\CrashDumps\WerCrashTest.

param(
    [string[]] $Kinds = @('av', 'cpp', 'terminate', 'fastfail', 'wil', 'wilplain', 'stowed'),
    [switch] $ShowUi,          # leave WER's UI alone (by default the app calls WerSetFlags(NO_UI))
    [switch] $NoRegister,      # don't call WerRegisterRuntimeExceptionModule
    [switch] $NoFilter,        # don't call SetUnhandledExceptionFilter
    [string] $Label = '',
    [ValidateSet('x64', 'ARM64')] [string] $Platform = 'x64',
    [ValidateSet('Release', 'Debug')] [string] $Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

$outDir = Join-Path $PSScriptRoot "out\$Platform\$Configuration"
$exe = Join-Path $outDir 'WerCrashTest.exe'
$logs = Join-Path $outDir 'logs'
$dumpFolder = Join-Path $env:LOCALAPPDATA 'Microsoft\MIDI\CrashDumps\WerCrashTest'
$resultsCsv = Join-Path $PSScriptRoot 'out\results.csv'

if (-not (Test-Path $exe)) { throw "Build first: $exe is missing." }

function Read-Events([string] $path) {
    if (-not (Test-Path $path)) { return @() }

    foreach ($line in Get-Content -LiteralPath $path) {
        $fields = [ordered]@{}
        foreach ($m in [regex]::Matches($line, '(\w+)=("([^"]*)"|\S+)')) {
            $fields[$m.Groups[1].Value] = if ($m.Groups[3].Success) { $m.Groups[3].Value } else { $m.Groups[2].Value }
        }

        [pscustomobject]$fields
    }
}

function Find-Event($events, [string] $name) { $events | Where-Object { $_.event -eq $name } | Select-Object -First 1 }

function Ms([object] $from, [object] $to) {
    if ($null -eq $from -or $null -eq $to) { return $null }
    [math]::Round(([double]$to.t - [double]$from.t) / 10000.0, 1)
}

$rows = foreach ($kind in $Kinds) {
    $appArgs = @($kind)
    if (-not $ShowUi) { $appArgs += '--no-ui' }
    if ($NoRegister) { $appArgs += '--no-register' }
    if ($NoFilter) { $appArgs += '--no-filter' }

    Write-Host "== $kind ($($appArgs -join ' '))"
    $before = @(Get-ChildItem -LiteralPath $dumpFolder -Filter '*.dmp' -ErrorAction SilentlyContinue | ForEach-Object Name)
    $process = Start-Process -FilePath $exe -ArgumentList $appArgs -NoNewWindow -PassThru
    $null = $process.Handle
    if (-not $process.WaitForExit(120000)) {
        Write-Warning "$kind did not exit in 120 s"
    }

    $crashedPid = $process.Id
    $exitCode = '0x{0:X8}' -f $process.ExitCode

    # Wait for the helper to finish (it logs helper_alive_5s_after), or give up if the callback
    # never fired.
    $moduleLog = Join-Path $logs "module-$crashedPid.log"
    $helperLog = Join-Path $logs "helper-$crashedPid.log"
    $waitUntil = (Get-Date).AddSeconds(90)
    $noCallbackUntil = (Get-Date).AddSeconds(20)
    while ((Get-Date) -lt $waitUntil) {
        if ((Test-Path $helperLog) -and (Select-String -LiteralPath $helperLog -Pattern 'event=(helper_alive_5s_after|bad_args)' -Quiet)) { break }
        if (-not (Test-Path $moduleLog) -and (Get-Date) -gt $noCallbackUntil) { break }
        Start-Sleep -Milliseconds 250
    }

    $app = Read-Events (Join-Path $logs "app-$crashedPid.log")
    $module = Read-Events $moduleLog
    $helper = Read-Events $helperLog

    $crashing = Find-Event $app 'crashing'
    $filter = Find-Event $app 'filter_called'
    $enter = Find-Event $module 'callback_enter'
    $exit = Find-Event $module 'callback_exit'
    $started = Find-Event $module 'helper_started'
    $hStart = Find-Event $helper 'helper_start'
    $seen = Find-Event $helper 'dump_seen'
    $complete = Find-Event $helper 'dump_complete'
    $crashedExit = Find-Event $helper 'crashed_exited'
    $werExit = Find-Event $helper 'werfault_exited'
    $alive = Find-Event $helper 'helper_alive_5s_after'

    $dump = Join-Path $dumpFolder "WerCrashTest.exe.$crashedPid.dmp"
    $dumpInfo = Get-Item -LiteralPath $dump -ErrorAction SilentlyContinue
    $after = @(Get-ChildItem -LiteralPath $dumpFolder -Filter '*.dmp' -ErrorAction SilentlyContinue | ForEach-Object Name)
    $removed = @($before | Where-Object { $after -notcontains $_ })

    [pscustomobject]@{
        Label                 = $Label
        Kind                  = $kind
        Pid                   = $crashedPid
        ExitCode              = $exitCode
        Filter                = if ($filter) { "yes $($filter.code)" } else { 'no' }
        Callback              = if ($enter) { "yes $($enter.code)" } else { 'no' }
        FastFailCode          = if ($enter) { $enter.info0 } else { '' }
        ContextRead           = if ($enter) { $enter.context_read } else { '' }
        CrashToCallbackMs     = Ms $crashing $enter
        CallbackUs            = if ($exit) { $exit.callback_us } else { '' }
        HelperStarted         = if ($started) { "yes breakaway=$($started.breakaway)" } elseif ($enter) { 'FAILED' } else { 'no' }
        WerFaultInJob         = if ($enter) { $enter.in_job } else { '' }
        HelperRunAfterMs      = if ($hStart) { $hStart.ms_after_callback_start } else { '' }
        DumpAtCallbackEnter   = if ($enter) { $enter.dump_exists_at_enter } else { '' }
        DumpSeenMs            = if ($seen) { $seen.ms_after_callback_start } else { '' }
        DumpLockedSeen        = if ($complete) { $complete.locked_seen } else { '' }
        CrashedExitMs         = if ($crashedExit) { $crashedExit.ms_after_callback_start } else { '' }
        DumpCompleteMs        = if ($complete) { $complete.ms_after_callback_start } else { '' }
        WerFaultExitMs        = if ($werExit) { $werExit.ms_after_callback_start } else { '' }
        HelperAliveAfterWer   = if ($alive) { 'yes' } elseif ($hStart) { 'NO' } else { '' }
        DumpWritten           = if ($dumpInfo) { 'yes' } else { 'no' }
        DumpMB                = if ($dumpInfo) { [math]::Round($dumpInfo.Length / 1MB, 1) } else { '' }
        Session               = if ($enter) { $enter.session } else { '' }
        DumpsBefore           = $before.Count
        DumpsAfter            = $after.Count
        DumpsRemoved          = $removed -join ' '
    }

    Start-Sleep -Seconds 2
}

$rows | Format-Table Kind, ExitCode, Filter, Callback, CrashToCallbackMs, CallbackUs, HelperStarted, HelperRunAfterMs, DumpAtCallbackEnter, DumpSeenMs, CrashedExitMs, DumpCompleteMs, WerFaultExitMs, HelperAliveAfterWer, DumpWritten, DumpMB, DumpsBefore, DumpsAfter, DumpsRemoved -AutoSize | Out-String -Width 400 | Write-Host

$rows | Export-Csv -LiteralPath $resultsCsv -Append -NoTypeInformation
Write-Host "Appended to $resultsCsv"
