# Copyright (c) Microsoft Corporation. All rights reserved.

<#
.SYNOPSIS
Removes unused Windows MIDI Services software device nodes (SWD\MIDISRV\* and SWD\MMDEVAPI\MIDI*).

.DESCRIPTION
Windows MIDI Services deactivates endpoints rather than deleting them: CMidiDeviceManager::RemoveEndpoint
only calls DeactivateEndpoint, deliberately, so that properties cached on the node - the user's custom
endpoint name, the device-supplied name, the product instance id - survive a disconnect and come back on
reconnect. Nothing in the product ever deletes one.

On a development machine that is fine until the test suites have run a few hundred times, at which point
thousands of nodes accumulate and enumeration gets slow enough to fail timing-sensitive tests
(MidiEndpointDeviceWatcherTests waits 10 seconds for EnumerationCompleted), to make 'midi enum endpoints'
take seconds, and to make Device Manager slow to open. This script is the cleanup.

Speed: everything is done in-process through cfgmgr32. This does NOT use Get-PnpDevice (a CIM provider
that is very slow at this scale), does NOT call Get-PnpDeviceProperty per device, and does NOT launch
pnputil.exe per device. Cataloguing ~9600 nodes, with friendly names and last arrival dates, takes
under a second.

Accuracy: presence is decided by CM_Locate_DevNodeW with CM_LOCATE_DEVNODE_NORMAL, which succeeds only
for a device that is currently configured in the device tree. Removal locates the node again with
CM_LOCATE_DEVNODE_PHANTOM and refuses anything that has become present since the scan, so a present
endpoint cannot be removed even in a race.

Nodes are classified by the transport code in the instance id:

  - MIDIU_<CODE>_...        service instance ids, for example MIDIU_NET2UDP_..., MIDIU_LOOP_A_...
  - MIDII_xxxxxxxx.<CODE>   legacy WinMM-facing ids under MMDEVAPI, for example MIDII_51C733B7.BLE10

Anything that does not parse is never a candidate. That is what protects the real audio endpoints under
SWD\MMDEVAPI ({0.0.0.00000000}.{guid}) and MicrosoftGSWavetableSynth.

A node that is not present is not necessarily an orphan. A USB or Bluetooth instrument that is switched
off right now looks exactly like test debris, and removing it discards the name the user gave it. The
defaults are therefore:

  - devices currently present are never touched, and cannot be
  - hardware-backed transports (KS, KSA, BLE10, BLE20) are excluded unless -IncludeHardware
  - the registry subtree is exported to a .reg file first unless -NoBackup

There is no age filter by default. "Not present" plus "software transport" already means the endpoint is
not in use, and those endpoints are recreated on demand from configuration. Use -NotSeenForDays if you
want one anyway.

Expect to need a second pass. The candidate list is a snapshot, and nodes that were present or in the
middle of being torn down during the first pass turn into removable phantoms behind it. Re-run until
-WhatIf reports nothing eligible.

Run with -WhatIf first. Removal requires an elevated prompt.

.PARAMETER Enumerator
Which SWD enumerator(s) to clean. Defaults to both MIDISRV and MMDEVAPI.

.PARAMETER Transport
Limit to one or more transport codes as they appear in the instance id, with or without the MIDIU_
prefix. For example NET2UDP, LOOP, BLOOP, APPDEV, APPPUB, DIAG, VPB.

.PARAMETER NamePattern
Limit to device nodes whose friendly name matches this wildcard pattern, for example '*Test*'.

.PARAMETER NotSeenForDays
Only remove nodes whose last arrival date is at least this many days ago. Defaults to 0, which disables
the age check. When greater than zero, a node whose last arrival date cannot be read is skipped rather
than guessed at.

.PARAMETER IncludeHardware
Also consider transports backed by real hardware (KS, KSA, BLE10, BLE20). Off by default: a switched-off
instrument is indistinguishable from debris, and removing its node loses its configuration.

.PARAMETER MaxCount
Stop after this many removals. Use a small number the first time to confirm the removal actually works
on this machine before committing to thousands.

.PARAMETER BackupPath
Folder to write the .reg backup into. Defaults to the temp folder.

.PARAMETER NoBackup
Skip the registry export.

.PARAMETER Force
Skip the confirmation prompt. Ignored when -WhatIf is used.

.EXAMPLE
.\remove_deactivated_midi_devices.ps1 -WhatIf
Reports what would be removed, changing nothing. Works from an ordinary prompt.

.EXAMPLE
.\remove_deactivated_midi_devices.ps1 -MaxCount 5 -Force
Removes five nodes, to confirm the mechanism works before doing the rest.

.EXAMPLE
.\remove_deactivated_midi_devices.ps1
Removes all non-present software MIDI nodes under both enumerators, after a backup and a prompt.

.EXAMPLE
.\remove_deactivated_midi_devices.ps1 -Transport NET2UDP -Force
Clears network transport debris only.
#>

[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory = $false)]
    [ValidateSet('MIDISRV', 'MMDEVAPI')]
    [string[]] $Enumerator = @('MIDISRV', 'MMDEVAPI'),

    [Parameter(Mandatory = $false)]
    [string[]] $Transport,

    [Parameter(Mandatory = $false)]
    [string] $NamePattern,

    [Parameter(Mandatory = $false)]
    [ValidateRange(0, 3650)]
    [int] $NotSeenForDays = 0,

    [Parameter(Mandatory = $false)]
    [switch] $IncludeHardware,

    [Parameter(Mandatory = $false)]
    [ValidateRange(1, [int]::MaxValue)]
    [int] $MaxCount = [int]::MaxValue,

    [Parameter(Mandatory = $false)]
    [string] $BackupPath = $env:TEMP,

    [Parameter(Mandatory = $false)]
    [switch] $NoBackup,

    [Parameter(Mandatory = $false)]
    [switch] $Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# These represent physical devices. Their nodes carry configuration for hardware that may just be
# switched off, so they are opt-in only.
$HardwareTransports = @('KS', 'KSA', 'BLE10', 'BLE20')

Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32;

public sealed class MidiSwdNode
{
    public string InstanceId;
    public string FriendlyName;
    public string Transport;
    public bool Present;
    public DateTime LastArrival;
    public bool HasLastArrival;
}

public static class MidiSwd
{
    [DllImport("cfgmgr32.dll", CharSet = CharSet.Unicode)]
    private static extern int CM_Locate_DevNodeW(out uint pdnDevInst, string pDeviceID, uint ulFlags);

    [DllImport("cfgmgr32.dll")]
    private static extern int CM_Uninstall_DevNode(uint dnDevInst, uint ulFlags);

    [StructLayout(LayoutKind.Sequential)]
    private struct DEVPROPKEY
    {
        public Guid fmtid;
        public uint pid;
    }

    [DllImport("cfgmgr32.dll", CharSet = CharSet.Unicode)]
    private static extern int CM_Get_DevNode_PropertyW(uint dnDevInst, ref DEVPROPKEY key,
        out uint propertyType, byte[] buffer, ref uint bufferSize, uint flags);

    private const uint CM_LOCATE_DEVNODE_NORMAL = 0;
    private const uint CM_LOCATE_DEVNODE_PHANTOM = 1;
    private const int CR_SUCCESS = 0;

    // DEVPKEY_Device_FriendlyName
    private static DEVPROPKEY NameKey = new DEVPROPKEY {
        fmtid = new Guid("a45c254e-df1c-4efd-8020-67d146a850e0"), pid = 14 };

    // DEVPKEY_Device_LastArrivalDate
    private static DEVPROPKEY ArrivalKey = new DEVPROPKEY {
        fmtid = new Guid("83da6326-97a6-4088-9453-a1923f573b29"), pid = 102 };

    private static byte[] GetProperty(uint devInst, ref DEVPROPKEY key)
    {
        uint type = 0;
        uint size = 0;

        CM_Get_DevNode_PropertyW(devInst, ref key, out type, null, ref size, 0);

        if (size == 0) { return null; }

        byte[] buffer = new byte[size];

        if (CM_Get_DevNode_PropertyW(devInst, ref key, out type, buffer, ref size, 0) != CR_SUCCESS)
        {
            return null;
        }

        return buffer;
    }

    // MIDIU_NET2UDP_CONTOSO_1234ABCD -> NET2UDP, MIDII_51C733B7.BLE10 -> BLE10. Anything else returns
    // null and is never treated as a candidate, which is what keeps the real audio endpoints under
    // SWD\MMDEVAPI out of range.
    private static string ParseTransport(string leaf)
    {
        if (leaf.StartsWith("MIDIU_", StringComparison.OrdinalIgnoreCase))
        {
            int start = 6;
            int end = leaf.IndexOf('_', start);

            if (end > start) { return leaf.Substring(start, end - start).ToUpperInvariant(); }

            return null;
        }

        if (leaf.StartsWith("MIDII_", StringComparison.OrdinalIgnoreCase))
        {
            int dot = leaf.LastIndexOf('.');

            if (dot > 0 && dot < leaf.Length - 1) { return leaf.Substring(dot + 1).ToUpperInvariant(); }

            return null;
        }

        return null;
    }

    public static List<MidiSwdNode> Scan(string enumerator)
    {
        var results = new List<MidiSwdNode>();

        using (RegistryKey root = Registry.LocalMachine.OpenSubKey(
            @"SYSTEM\CurrentControlSet\Enum\SWD\" + enumerator))
        {
            if (root == null) { return results; }

            foreach (string leaf in root.GetSubKeyNames())
            {
                string transport = ParseTransport(leaf);

                if (transport == null) { continue; }

                var node = new MidiSwdNode {
                    InstanceId = @"SWD\" + enumerator + @"\" + leaf,
                    Transport = transport };

                uint devInst;

                node.Present = CM_Locate_DevNodeW(
                    out devInst, node.InstanceId, CM_LOCATE_DEVNODE_NORMAL) == CR_SUCCESS;

                if (!node.Present &&
                    CM_Locate_DevNodeW(
                        out devInst, node.InstanceId, CM_LOCATE_DEVNODE_PHANTOM) != CR_SUCCESS)
                {
                    devInst = 0;
                }

                if (devInst != 0)
                {
                    byte[] name = GetProperty(devInst, ref NameKey);

                    if (name != null)
                    {
                        node.FriendlyName = Encoding.Unicode.GetString(name).TrimEnd('\0');
                    }

                    byte[] arrival = GetProperty(devInst, ref ArrivalKey);

                    if (arrival != null && arrival.Length >= 8)
                    {
                        node.LastArrival = DateTime.FromFileTime(BitConverter.ToInt64(arrival, 0));
                        node.HasLastArrival = true;
                    }
                }

                results.Add(node);
            }
        }

        return results;
    }

    // Re-locates as a phantom, so a node that became present since the scan is refused here rather
    // than removed out from under a running endpoint.
    public static int Remove(string instanceId)
    {
        uint devInst;

        if (CM_Locate_DevNodeW(out devInst, instanceId, CM_LOCATE_DEVNODE_NORMAL) == CR_SUCCESS)
        {
            return -1;
        }

        int result = CM_Locate_DevNodeW(out devInst, instanceId, CM_LOCATE_DEVNODE_PHANTOM);

        if (result != CR_SUCCESS) { return result; }

        return CM_Uninstall_DevNode(devInst, 0);
    }
}
'@

$ConfigRetCodes = @{
    -1 = 'device became present, skipped'
     2 = 'CR_OUT_OF_MEMORY'
     5 = 'CR_INVALID_DEVNODE'
    13 = 'CR_NO_SUCH_DEVNODE'
    23 = 'CR_REMOVE_VETOED'
    51 = 'CR_ACCESS_DENIED (not elevated?)'
}

function Get-RetCodeText
{
    param([int] $Code)

    if ($ConfigRetCodes.ContainsKey($Code)) { return $ConfigRetCodes[$Code] }

    return ('CR 0x{0:X2}' -f $Code)
}

function Test-Elevated
{
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()

    return ([Security.Principal.WindowsPrincipal] $identity).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
}

# Only removal needs elevation, so -WhatIf stays usable from an ordinary prompt.
if (-not $WhatIfPreference -and -not (Test-Elevated))
{
    Write-Error ("Removing device nodes requires elevation. Re-run from an elevated prompt, " +
        "or add -WhatIf to see what would be removed.")
    return
}

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

$nodes = [System.Collections.Generic.List[object]]::new()

foreach ($enum in $Enumerator)
{
    $found = [MidiSwd]::Scan($enum)

    Write-Host ("SWD\{0,-9} {1,6} MIDI node(s)" -f $enum, $found.Count)

    $nodes.AddRange($found)
}

if ($nodes.Count -eq 0)
{
    Write-Host "No Windows MIDI Services device nodes found." -ForegroundColor Green
    return
}

$present = @($nodes | Where-Object { $_.Present })
$absent = @($nodes | Where-Object { -not $_.Present })

Write-Host ("Scanned {0} node(s) in {1} ms: {2} present, {3} not present." -f `
    $nodes.Count, $stopwatch.ElapsedMilliseconds, $present.Count, $absent.Count)

if ($absent.Count -eq 0)
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

foreach ($node in $absent)
{
    if (-not $IncludeHardware -and $HardwareTransports -contains $node.Transport)
    {
        $skippedHardware++
        continue
    }

    if ($normalizedTransports.Count -gt 0 -and $normalizedTransports -notcontains $node.Transport)
    {
        $skippedFiltered++
        continue
    }

    if ($PSBoundParameters.ContainsKey('NamePattern') -and $node.FriendlyName -notlike $NamePattern)
    {
        $skippedFiltered++
        continue
    }

    if ($NotSeenForDays -gt 0)
    {
        # An unreadable date is not evidence of an orphan, so it is left alone rather than assumed old.
        if (-not $node.HasLastArrival)
        {
            $skippedUnknownAge++
            continue
        }

        if ($node.LastArrival -gt $cutoff)
        {
            $skippedTooRecent++
            continue
        }
    }

    $candidates.Add($node)
}

Write-Host ""
Write-Host "Excluded from removal:" -ForegroundColor Yellow
Write-Host ("  {0,6}  currently present" -f $present.Count)
Write-Host ("  {0,6}  hardware-backed transport ({1})" -f `
    $skippedHardware, ($HardwareTransports -join ', '))

if ($NotSeenForDays -gt 0)
{
    Write-Host ("  {0,6}  seen within the last {1} day(s)" -f $skippedTooRecent, $NotSeenForDays)
    Write-Host ("  {0,6}  last-arrival date unreadable" -f $skippedUnknownAge)
}

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
        $dated = @($_.Group | Where-Object { $_.HasLastArrival })

        $oldestText = if ($dated.Count -gt 0)
        {
            ($dated | Sort-Object LastArrival | Select-Object -First 1).LastArrival.ToString('yyyy-MM-dd')
        }
        else
        {
            'unknown'
        }

        Write-Host ("  {0,6}  {1,-10}  oldest last seen {2}" -f $_.Count, $_.Name, $oldestText)
    }

Write-Host ""
Write-Host ("Total eligible: {0}" -f $candidates.Count) -ForegroundColor Cyan

if ($MaxCount -lt $candidates.Count)
{
    Write-Host ("Limited to the first {0} by -MaxCount." -f $MaxCount) -ForegroundColor Cyan

    $candidates = [System.Collections.Generic.List[object]]::new(
        [object[]] ($candidates | Select-Object -First $MaxCount))
}

Write-Host "Removing a node discards any custom endpoint name and cached properties held on it." `
    -ForegroundColor Yellow

if ($WhatIfPreference)
{
    foreach ($candidate in $candidates)
    {
        $null = $PSCmdlet.ShouldProcess($candidate.InstanceId, "Remove device node")
    }

    return
}

if (-not $NoBackup)
{
    if (-not (Test-Path -LiteralPath $BackupPath))
    {
        New-Item -ItemType Directory -Path $BackupPath -Force | Out-Null
    }

    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'

    foreach ($enum in $Enumerator)
    {
        $file = Join-Path $BackupPath ("midi-swd-{0}-{1}.reg" -f $enum.ToLowerInvariant(), $timestamp)

        & reg.exe export "HKLM\SYSTEM\CurrentControlSet\Enum\SWD\$enum" $file /y | Out-Null

        if ($LASTEXITCODE -ne 0)
        {
            Write-Error ("Backup of SWD\$enum failed (reg.exe exit $LASTEXITCODE). " +
                "Re-run with -NoBackup to proceed anyway.")
            return
        }

        Write-Host ("Backed up SWD\{0} to {1}" -f $enum, $file) -ForegroundColor DarkGray
    }
}

if (-not $Force)
{
    Write-Host ""

    if (-not $PSCmdlet.ShouldContinue(
            "Remove $($candidates.Count) unused MIDI device node(s)?", "Confirm removal"))
    {
        Write-Host "Canceled. Nothing was removed." -ForegroundColor Yellow
        return
    }
}

$stopwatch.Restart()

$removed = 0
$failures = @{}
$firstFailures = [System.Collections.Generic.List[object]]::new()
$index = 0

foreach ($candidate in $candidates)
{
    $index++

    if (-not $PSCmdlet.ShouldProcess($candidate.InstanceId, "Remove device node"))
    {
        continue
    }

    if (($index % 100) -eq 0 -or $index -eq $candidates.Count)
    {
        Write-Progress -Activity "Removing unused MIDI device nodes" `
            -Status "$index of $($candidates.Count), $removed removed" `
            -PercentComplete (($index / $candidates.Count) * 100)
    }

    $result = [MidiSwd]::Remove($candidate.InstanceId)

    if ($result -eq 0)
    {
        $removed++
    }
    else
    {
        if (-not $failures.ContainsKey($result)) { $failures[$result] = 0 }

        $failures[$result]++

        if ($firstFailures.Count -lt 10) { $firstFailures.Add($candidate) }
    }
}

Write-Progress -Activity "Removing unused MIDI device nodes" -Completed

Write-Host ""
Write-Host ("Removed {0} node(s) in {1:N1} s." -f $removed, $stopwatch.Elapsed.TotalSeconds) `
    -ForegroundColor Green

if ($failures.Count -gt 0)
{
    Write-Host ""
    Write-Host "Failures:" -ForegroundColor Red

    foreach ($code in ($failures.Keys | Sort-Object))
    {
        Write-Host ("  {0,6}  {1}" -f $failures[$code], (Get-RetCodeText -Code $code))
    }

    Write-Host ""
    Write-Host "First few:" -ForegroundColor Red

    $firstFailures | ForEach-Object { Write-Host ("  {0}" -f $_.InstanceId) }
}
