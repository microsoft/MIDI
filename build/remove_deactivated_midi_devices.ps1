# Copyright (c) Microsoft Corporation. All rights reserved.

<#
.SYNOPSIS
Removes deactivated Windows MIDI Services software device nodes (SWD\MIDISRV\*).

.DESCRIPTION
Windows MIDI Services deactivates endpoints rather than deleting them: CMidiDeviceManager::RemoveEndpoint
only calls DeactivateEndpoint, deliberately, so that properties cached on the node - the user's custom
endpoint name, the device-supplied name, the product instance id - survive a disconnect and come back on
reconnect. Nothing in the product ever deletes one.

On a development machine that is fine until the test suites have run a few hundred times, at which point
tens of thousands of nodes accumulate and enumeration gets slow enough to fail timing-sensitive tests
(MidiEndpointDeviceWatcherTests waits 10 seconds for EnumerationCompleted). This script is the cleanup.

A deactivated node is NOT necessarily an orphan. A USB synth that is simply unplugged right now looks
exactly like test debris, and removing it discards the name the user gave it. The defaults are therefore
conservative:

  - devices currently present are never touched, and cannot be
  - hardware-backed transports (KS, KSA) are excluded unless -IncludeHardware
  - only nodes not seen for -NotSeenForDays (default 7) are eligible
  - a node whose last-arrival date cannot be read is skipped, not guessed at

Run with -WhatIf first. Requires an elevated prompt.

.PARAMETER Transport
Limit to one or more transport prefixes as they appear in the instance id, with or without the MIDIU_
prefix. For example NET2UDP, LOOP, BLOOP, APPDEV, APPPUB, DIAG, VIRT.

.PARAMETER NamePattern
Limit to device nodes whose friendly name matches this wildcard pattern, for example '*Test*'.

.PARAMETER NotSeenForDays
Only remove nodes whose last arrival date is at least this many days ago. Defaults to 7. Use 0 to
disable the age check entirely, which also makes nodes with no readable date eligible.

.PARAMETER IncludeHardware
Also consider transports backed by real hardware (KS, KSA). Off by default: an unplugged USB device is
indistinguishable from debris, and removing its node loses its configuration.

.PARAMETER Force
Skip the confirmation prompt. Ignored when -WhatIf is used.

.EXAMPLE
.\remove_deactivated_midi_devices.ps1 -WhatIf
Reports what would be removed, changing nothing.

.EXAMPLE
.\remove_deactivated_midi_devices.ps1 -Transport NET2UDP -NotSeenForDays 1
Clears network transport debris left by recent test runs.

.EXAMPLE
.\remove_deactivated_midi_devices.ps1 -NamePattern '*Test*' -NotSeenForDays 0 -Force
Clears everything obviously created by a test, with no age check and no prompt.
#>

[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory = $false)]
    [string[]] $Transport,

    [Parameter(Mandatory = $false)]
    [string] $NamePattern,

    [Parameter(Mandatory = $false)]
    [ValidateRange(0, 3650)]
    [int] $NotSeenForDays = 7,

    [Parameter(Mandatory = $false)]
    [switch] $IncludeHardware,

    [Parameter(Mandatory = $false)]
    [switch] $Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$InstanceIdPrefix = 'SWD\MIDISRV\'

# These represent physical devices. Their nodes carry configuration for hardware that may just be
# unplugged, so they are opt-in only.
$HardwareTransports = @('KS', 'KSA')

function Test-Elevated
{
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()

    return ([Security.Principal.WindowsPrincipal] $identity).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
}

# MIDIU_NET2UDP_CONTOSO_1234ABCD -> NET2UDP
function Get-TransportCode
{
    param([string] $InstanceId)

    $leaf = $InstanceId.Substring($InstanceIdPrefix.Length)

    if ($leaf -match '^MIDIU_([A-Za-z0-9]+)_')
    {
        return $Matches[1].ToUpperInvariant()
    }

    return 'UNKNOWN'
}

function Get-LastArrival
{
    param([string] $InstanceId)

    try
    {
        $property = Get-PnpDeviceProperty -InstanceId $InstanceId `
            -KeyName 'DEVPKEY_Device_LastArrivalDate' -ErrorAction Stop

        if ($null -ne $property -and $property.Data -is [datetime])
        {
            return $property.Data
        }
    }
    catch
    {
    }

    return $null
}

# Only removal needs elevation, so -WhatIf stays usable from an ordinary prompt.
if (-not $WhatIfPreference)
{
    if (-not (Test-Elevated))
    {
        Write-Error ("pnputil /remove-device requires elevation. Re-run from an elevated prompt, " +
            "or add -WhatIf to see what would be removed.")
        return
    }

    if (-not (Get-Command pnputil.exe -ErrorAction SilentlyContinue))
    {
        Write-Error "pnputil.exe was not found."
        return
    }
}

Write-Host "Enumerating $InstanceIdPrefix* device nodes. This takes a while when there are many." `
    -ForegroundColor Cyan

$allNodes = @(Get-PnpDevice -ErrorAction SilentlyContinue |
    Where-Object { $_.InstanceId -like "$InstanceIdPrefix*" })

if ($allNodes.Count -eq 0)
{
    Write-Host "No Windows MIDI Services device nodes found." -ForegroundColor Green
    return
}

# 'OK' means the endpoint is live right now. Everything else is deactivated or otherwise not present.
$present = @($allNodes | Where-Object { $_.Status -eq 'OK' })
$deactivated = @($allNodes | Where-Object { $_.Status -ne 'OK' })

Write-Host ("Found {0} node(s): {1} present, {2} deactivated." -f `
    $allNodes.Count, $present.Count, $deactivated.Count)

if ($deactivated.Count -eq 0)
{
    Write-Host "Nothing to clean up." -ForegroundColor Green
    return
}

$normalizedTransports = @()

if ($PSBoundParameters.ContainsKey('Transport'))
{
    $normalizedTransports = @($Transport | ForEach-Object {
        ($_ -replace '^MIDIU_', '').ToUpperInvariant()
    })
}

$cutoff = (Get-Date).AddDays(-$NotSeenForDays)

$candidates = [System.Collections.Generic.List[object]]::new()
$skippedHardware = 0
$skippedTooRecent = 0
$skippedUnknownAge = 0
$skippedFiltered = 0

$index = 0

foreach ($node in $deactivated)
{
    $index++

    if (($index % 250) -eq 0)
    {
        Write-Progress -Activity "Examining deactivated MIDI device nodes" `
            -Status "$index of $($deactivated.Count)" `
            -PercentComplete (($index / $deactivated.Count) * 100)
    }

    $transportCode = Get-TransportCode -InstanceId $node.InstanceId

    if (-not $IncludeHardware -and $HardwareTransports -contains $transportCode)
    {
        $skippedHardware++
        continue
    }

    if ($normalizedTransports.Count -gt 0 -and $normalizedTransports -notcontains $transportCode)
    {
        $skippedFiltered++
        continue
    }

    if ($PSBoundParameters.ContainsKey('NamePattern') -and $node.FriendlyName -notlike $NamePattern)
    {
        $skippedFiltered++
        continue
    }

    $lastArrival = $null

    if ($NotSeenForDays -gt 0)
    {
        $lastArrival = Get-LastArrival -InstanceId $node.InstanceId

        # An unreadable date is not evidence of an orphan, so it is left alone rather than assumed old.
        if ($null -eq $lastArrival)
        {
            $skippedUnknownAge++
            continue
        }

        if ($lastArrival -gt $cutoff)
        {
            $skippedTooRecent++
            continue
        }
    }

    $candidates.Add([PSCustomObject]@{
        InstanceId   = $node.InstanceId
        FriendlyName = $node.FriendlyName
        Transport    = $transportCode
        LastArrival  = $lastArrival
    })
}

Write-Progress -Activity "Examining deactivated MIDI device nodes" -Completed

Write-Host ""
Write-Host "Excluded from removal:" -ForegroundColor Yellow
Write-Host ("  {0,6}  currently present" -f $present.Count)
Write-Host ("  {0,6}  hardware-backed transport ({1})" -f $skippedHardware, ($HardwareTransports -join ', '))
Write-Host ("  {0,6}  seen within the last {1} day(s)" -f $skippedTooRecent, $NotSeenForDays)
Write-Host ("  {0,6}  last-arrival date unreadable" -f $skippedUnknownAge)
Write-Host ("  {0,6}  excluded by -Transport / -NamePattern" -f $skippedFiltered)

if ($candidates.Count -eq 0)
{
    Write-Host ""
    Write-Host "No nodes match. Nothing to do." -ForegroundColor Green
    return
}

Write-Host ""
Write-Host "Eligible for removal, by transport:" -ForegroundColor Cyan

$candidates |
    Group-Object Transport |
    Sort-Object Count -Descending |
    ForEach-Object {
        $oldest = ($_.Group | Where-Object { $null -ne $_.LastArrival } |
            Sort-Object LastArrival | Select-Object -First 1).LastArrival

        $oldestText = if ($null -ne $oldest) { $oldest.ToString('yyyy-MM-dd') } else { 'unknown' }

        Write-Host ("  {0,6}  {1,-10}  oldest last seen {2}" -f $_.Count, $_.Name, $oldestText)
    }

Write-Host ""
Write-Host ("Total eligible: {0}" -f $candidates.Count) -ForegroundColor Cyan
Write-Host "Removing a node discards any custom endpoint name and cached properties held on it." `
    -ForegroundColor Yellow

if (-not $WhatIfPreference -and -not $Force)
{
    Write-Host ""

    if (-not $PSCmdlet.ShouldContinue(
            "Remove $($candidates.Count) deactivated MIDI device node(s)?", "Confirm removal"))
    {
        Write-Host "Cancelled. Nothing was removed." -ForegroundColor Yellow
        return
    }
}

$removed = 0
$failed = [System.Collections.Generic.List[object]]::new()
$index = 0

foreach ($candidate in $candidates)
{
    $index++

    if (-not $PSCmdlet.ShouldProcess($candidate.InstanceId, "Remove device node"))
    {
        continue
    }

    Write-Progress -Activity "Removing deactivated MIDI device nodes" `
        -Status "$index of $($candidates.Count): $($candidate.FriendlyName)" `
        -PercentComplete (($index / $candidates.Count) * 100)

    $output = & pnputil.exe /remove-device $candidate.InstanceId 2>&1
    $exitCode = $LASTEXITCODE

    # 3010 is ERROR_SUCCESS_REBOOT_REQUIRED, which is still a successful removal.
    if ($exitCode -eq 0 -or $exitCode -eq 3010)
    {
        $removed++
    }
    else
    {
        $failed.Add([PSCustomObject]@{
            InstanceId = $candidate.InstanceId
            ExitCode   = $exitCode
            Output     = ($output | Out-String).Trim()
        })
    }
}

Write-Progress -Activity "Removing deactivated MIDI device nodes" -Completed

if ($WhatIfPreference)
{
    return
}

Write-Host ""
Write-Host ("Removed {0} of {1} node(s)." -f $removed, $candidates.Count) -ForegroundColor Green

if ($failed.Count -gt 0)
{
    Write-Host ("{0} removal(s) failed:" -f $failed.Count) -ForegroundColor Red

    $failed | Select-Object -First 10 | ForEach-Object {
        Write-Host ("  [{0}] {1}" -f $_.ExitCode, $_.InstanceId) -ForegroundColor Red
        Write-Host ("         {0}" -f $_.Output) -ForegroundColor DarkGray
    }

    if ($failed.Count -gt 10)
    {
        Write-Host ("  ... and {0} more." -f ($failed.Count - 10)) -ForegroundColor Red
    }

    Write-Host "A node the service re-activated mid-sweep will fail here; re-run to pick it up." `
        -ForegroundColor DarkGray
}
