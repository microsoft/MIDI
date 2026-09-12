# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License
# ============================================================================
# Measures MIDI round trip time for a device whose MIDI OUT is wired back to
# its MIDI IN with a MIDI cable. See docs/kb/usb-midi-device-round-trip-measurements.md
# ============================================================================

<#
.SYNOPSIS
    Measures round trip time and jitter for a MIDI device wired OUT to IN, or discovers which
    input group an output group loops back to.

.DESCRIPTION
    Drives the MeasureDeviceRoundTripLatency and DiscoverLoopbackGroupMapping test methods in
    Midi2.WinRTClient.Session.integrationtests.dll, which must be built first.

    Connect a MIDI cable from the device's MIDI OUT to its MIDI IN before running. Nothing else
    should be sending to the device at the time.

.PARAMETER EndpointId
    The endpoint to measure. Get it from "midi enumerate endpoints --verbose".

.PARAMETER Scan
    Discover which input group each output group returns on, instead of measuring. Run this first
    on any device with more than one port.

.EXAMPLE
    .\measure-device-round-trip.ps1 -EndpointId '\\?\swd#midisrv#midiu_ks_123#{guid}' -Scan

.EXAMPLE
    .\measure-device-round-trip.ps1 -EndpointId '\\?\swd#midisrv#midiu_ks_123#{guid}' -OutGroup 1 -InGroup 9 -Runs 4
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $EndpointId,

    # Discover the loopback group mapping instead of measuring.
    [switch] $Scan,

    # 1 based, as shown by the midi console.
    [ValidateRange(1, 16)]
    [int] $OutGroup = 1,

    # Defaults to OutGroup. Some devices return on a different group: run -Scan to find out.
    [ValidateRange(0, 16)]
    [int] $InGroup = 0,

    # mixed alternates a 3 byte and a 2 byte message, which is what separates wire time from
    # everything else, and prevents a device that uses running status on its MIDI OUT from
    # measuring faster than it behaves on real traffic.
    [ValidateSet('cc', 'pc', 'mixed')]
    [string] $MessageKind = 'mixed',

    # Send this many messages as one block instead of spacing them out, to test whether the
    # device drops messages that arrive while it is transmitting. 0 uses the normal spaced run.
    [ValidateRange(0, 100)]
    [int] $Burst = 0,

    [ValidateRange(1, 50)]
    [int] $Runs = 3,

    # Raise to about 3000 for a device whose ports switch between input and output automatically,
    # which needs sustained traffic before a port will reverse.
    [ValidateRange(100, 30000)]
    [int] $ScanSettleMilliseconds = 400,

    [ValidateRange(1, 16)]
    [int] $ScanMaxGroup = 16,

    [string] $TestBinaryFolder = (Join-Path $PSScriptRoot '..\..\..\vsfiles-sdk\out\tests\x64\Release'),

    [string] $TaefPath = 'C:\Program Files (x86)\Windows Kits\10\Testing\Runtimes\TAEF\x64\TE.exe'
)

$ErrorActionPreference = 'Stop'

$testBinary = Join-Path $TestBinaryFolder 'Midi2.WinRTClient.Session.integrationtests.dll'

if (-not (Test-Path $testBinary))
{
    throw "Test binary not found at $testBinary. Build Midi2-AppSDK.sln for x64 Release first."
}

if (-not (Test-Path $TaefPath))
{
    throw "TAEF not found at $TaefPath. Install the Windows Driver Kit, or pass -TaefPath."
}

if ($InGroup -eq 0) { $InGroup = $OutGroup }

$methodName = if ($Scan) { 'DiscoverLoopbackGroupMapping' } else { 'MeasureDeviceRoundTripLatency' }

$env:MIDI_RTT_TEST_ENDPOINT_ID = $EndpointId
$env:MIDI_RTT_TEST_GROUP = "$OutGroup"
$env:MIDI_RTT_TEST_IN_GROUP = "$InGroup"
$env:MIDI_RTT_TEST_MESSAGE = $MessageKind
$env:MIDI_RTT_TEST_MAX_GROUP = "$ScanMaxGroup"
$env:MIDI_RTT_TEST_SCAN_SETTLE_MS = "$ScanSettleMilliseconds"

if ($Burst -gt 0) { $env:MIDI_RTT_TEST_BURST = "$Burst" } else { Remove-Item Env:\MIDI_RTT_TEST_BURST -ErrorAction SilentlyContinue }

# TAEF's /name: argument does not survive PowerShell argument parsing, so it goes through cmd.
$runner = Join-Path $env:TEMP 'midi-round-trip-run.cmd'

@"
@echo off
cd /d "$TestBinaryFolder"
"$TaefPath" Midi2.WinRTClient.Session.integrationtests.dll /name:MidiMessageSchedulerTests::$methodName /logOutput:Low
"@ | Set-Content -Path $runner -Encoding ASCII

try
{
    if ($Scan)
    {
        Write-Host "Scanning groups 1 to $ScanMaxGroup on $EndpointId" -ForegroundColor Cyan
    }
    else
    {
        Write-Host "Measuring out group $OutGroup to in group $InGroup on $EndpointId" -ForegroundColor Cyan
    }

    for ($run = 1; $run -le $Runs; $run++)
    {
        if (-not $Scan -and $Runs -gt 1) { Write-Host "--- run $run of $Runs ---" -ForegroundColor DarkGray }

        $output = & $runner 2>&1 | ForEach-Object { ($_ -replace '^\[[^\]]*\]\[[^\]]*\]', '').TrimEnd() }

        $output |
            Select-String -Pattern 'endpoint|group|message|sent / returned|round trip|service timestamp|client arrival|byte|outlier|position|no message exceeded|Nothing came back|out group|burst|first back|wire time|fixed overhead|Check the' |
            ForEach-Object { $_.Line }

        if ($Scan) { break }
    }
}
finally
{
    Remove-Item Env:\MIDI_RTT_TEST_ENDPOINT_ID, Env:\MIDI_RTT_TEST_GROUP, Env:\MIDI_RTT_TEST_IN_GROUP,
                Env:\MIDI_RTT_TEST_MESSAGE, Env:\MIDI_RTT_TEST_MAX_GROUP, Env:\MIDI_RTT_TEST_SCAN_SETTLE_MS,
                Env:\MIDI_RTT_TEST_BURST -ErrorAction SilentlyContinue

    Remove-Item $runner -ErrorAction SilentlyContinue
}
