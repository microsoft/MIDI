# Copyright (c) Microsoft Corporation and Contributors.
# Licensed under the MIT License
# ============================================================================
# This is part of Windows MIDI Services
# Further information: https://aka.ms/midi
# ============================================================================
# MIDI Glass phase 0 spike. Nothing here ships.
#
# Runs the rendering spike once per approach, each in its own process so the memory numbers are
# not contaminated by whichever approach ran first, and prints the three side by side.

[CmdletBinding()]
param(
    [int] $Controls = 200,
    [int] $Animate = 12,
    [int] $Seconds = 20,
    [string[]] $Modes = @('xaml', 'composition', 'hybrid'),
    [string] $Endpoint = '',
    [switch] $NoMidi,
    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Release',
    [ValidateSet('x64', 'ARM64')]
    [string] $Platform = 'x64',
    [string] $OutputPath = ''
)

$ErrorActionPreference = 'Stop'

$exe = Join-Path $PSScriptRoot "..\out\glassspike-surface\$Platform\$Configuration\glassspike.exe"

if (-not (Test-Path $exe))
{
    throw "Build the spike first: pwsh -File glassspike-surface\build.ps1"
}

if ([string]::IsNullOrWhiteSpace($OutputPath))
{
    $OutputPath = Join-Path $PSScriptRoot ("results-{0}.jsonl" -f (Get-Date -Format 'yyyyMMdd-HHmmss'))
}

Write-Host ("Spike: {0} controls, {1} animating, {2} s per mode" -f $Controls, $Animate, $Seconds) -ForegroundColor Cyan
Write-Host ("Results: {0}" -f $OutputPath) -ForegroundColor Cyan
Write-Host ''

foreach ($mode in $Modes)
{
    $spikeArgs = @(
        '--mode', $mode,
        '--controls', $Controls,
        '--animate', $Animate,
        '--seconds', $Seconds,
        '--autorun',
        '--out', $OutputPath
    )

    if ($NoMidi) { $spikeArgs += '--no-midi' }
    if (-not [string]::IsNullOrWhiteSpace($Endpoint)) { $spikeArgs += @('--endpoint', $Endpoint) }

    Write-Host ("Running {0}..." -f $mode) -ForegroundColor Yellow

    $process = Start-Process -FilePath $exe -ArgumentList $spikeArgs -PassThru

    if (-not $process.WaitForExit(($Seconds + 120) * 1000))
    {
        $process.Kill()
        Write-Host ("  {0} did not exit and was stopped." -f $mode) -ForegroundColor Red
        continue
    }

    Write-Host ("  {0} exited with {1}." -f $mode, $process.ExitCode)
}

Write-Host ''

if (-not (Test-Path $OutputPath))
{
    Write-Host 'No results were written.' -ForegroundColor Red
    return
}

$results = Get-Content $OutputPath | Where-Object { $_.Trim() } | ForEach-Object { $_ | ConvertFrom-Json }

$render = $results |
    Select-Object `
        @{ n = 'mode';       e = { $_.mode } },
        @{ n = 'n';          e = { '{0}/{1}' -f $_.controls, $_.animated } },
        @{ n = 'build ms';   e = { '{0:N1}' -f $_.buildMs } },
        @{ n = 'elements';   e = { $_.xamlElements } },
        @{ n = 'p50 ms';     e = { '{0:N2}' -f $_.frameP50Ms } },
        @{ n = 'p99 ms';     e = { '{0:N2}' -f $_.frameP99Ms } },
        @{ n = 'max ms';     e = { '{0:N2}' -f $_.frameMaxMs } },
        @{ n = 'late';       e = { '{0}/{1}' -f $_.framesOver20Ms, $_.frames } },
        @{ n = 'frame us';   e = { '{0:N0}' -f $_.animateP50Us } },
        @{ n = 'p99 us';     e = { '{0:N0}' -f $_.animateP99Us } }

$other = $results |
    Select-Object `
        @{ n = 'mode';       e = { $_.mode } },
        @{ n = 'value us';   e = { '{0:N2}' -f $_.setValueP50Us } },
        @{ n = 'private MB'; e = { '{0:N1}' -f (($_.privateBytesAfterBuild - $_.privateBytesBaseline) / 1MB) } },
        @{ n = 'theme ms';   e = { '{0:N1}' -f $_.themeSwapMs } },
        @{ n = 'theme all';  e = { if ($_.themeSwapReachedEveryControl) { 'yes' } else { 'NO' } } },
        @{ n = 'a11y';       e = { if ($_.automationPeers) { 'yes' } else { 'no' } } },
        @{ n = 'machine';    e = { '{0:N0}% busy' -f $_.systemBusyPercent } },
        @{ n = 'clean';      e = { if ($_.clean) { 'yes' } else { 'NO' } } },
        @{ n = 'input p50';  e = { if ($_.pointerSamples -gt 0) { '{0:N0} us' -f $_.pointerToSendP50Us } else { 'not measured' } } },
        @{ n = 'devices';    e = { $_.pointerDeviceTypes } }

($render | Format-Table -AutoSize | Out-String -Width 200) | Write-Host
($other | Format-Table -AutoSize | Out-String -Width 200) | Write-Host

$dirty = @($results | Where-Object { -not $_.clean })

if ($dirty.Count -gt 0)
{
    Write-Host ("{0} of {1} runs happened while the machine was in use. Their frame numbers are not evidence." -f $dirty.Count, $results.Count) -ForegroundColor Yellow
}


