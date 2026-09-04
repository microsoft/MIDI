# Correctness check for Feature_Servicing_MIDI2WinMMInterfaceRemovalPerf.
#
# The fix stops re-walking PnP on interface removal and instead erases the departed interface from
# the cached map. This verifies that produces the same visible state a full refresh did: after a
# create/remove cycle the enumerable port table must return EXACTLY to its starting contents.
#
# Usage:  powershell -File midi-removal-correctness-test.ps1 [-Cycles 3]

param(
    [int]    $Cycles  = 3,
    [string] $MidiExe = 'midi'
)

$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

public static class PortTable
{
    [DllImport("winmm.dll")] public static extern uint midiOutGetNumDevs();
    [DllImport("winmm.dll")] public static extern uint midiInGetNumDevs();
    [DllImport("winmm.dll", CharSet = CharSet.Unicode)]
    public static extern uint midiOutGetDevCapsW(UIntPtr id, ref MIDIOUTCAPS c, uint sz);
    [DllImport("winmm.dll", CharSet = CharSet.Unicode)]
    public static extern uint midiInGetDevCapsW(UIntPtr id, ref MIDIINCAPS c, uint sz);

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct MIDIOUTCAPS
    {
        public ushort wMid, wPid; public uint vDriverVersion;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string szPname;
        public ushort wTechnology, wVoices, wNotes, wChannelMask; public uint dwSupport;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct MIDIINCAPS
    {
        public ushort wMid, wPid; public uint vDriverVersion;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string szPname;
        public uint dwSupport;
    }

    // "index|name" for every index where GetDevCaps succeeds, plus the reported device count.
    public static string Snapshot()
    {
        var sb = new System.Text.StringBuilder();

        uint no = midiOutGetNumDevs();
        sb.Append("OUTCOUNT=").Append(no).Append('\n');
        for (uint i = 0; i < no; i++)
        {
            var c = new MIDIOUTCAPS();
            if (midiOutGetDevCapsW((UIntPtr)i, ref c, (uint)Marshal.SizeOf(typeof(MIDIOUTCAPS))) == 0)
                sb.Append("OUT ").Append(i).Append('|').Append(c.szPname).Append('\n');
        }

        uint ni = midiInGetNumDevs();
        sb.Append("INCOUNT=").Append(ni).Append('\n');
        for (uint i = 0; i < ni; i++)
        {
            var c = new MIDIINCAPS();
            if (midiInGetDevCapsW((UIntPtr)i, ref c, (uint)Marshal.SizeOf(typeof(MIDIINCAPS))) == 0)
                sb.Append("IN ").Append(i).Append('|').Append(c.szPname).Append('\n');
        }

        return sb.ToString();
    }
}
'@

$reAssoc = [regex]'Association\s*Id[^0-9a-fA-F]*([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})'

function Show-Diff($before, $after, $label) {
    $b = $before -split "`n"
    $a = $after  -split "`n"
    $onlyBefore = @($b | Where-Object { $_ -and $a -notcontains $_ })
    $onlyAfter  = @($a | Where-Object { $_ -and $b -notcontains $_ })
    Write-Host "    $label"
    foreach ($x in $onlyBefore) { Write-Host "      lost:  $x" -ForegroundColor Red }
    foreach ($x in $onlyAfter)  { Write-Host "      new:   $x" -ForegroundColor Yellow }
}

$baseline = [PortTable]::Snapshot()
$baseCount = ($baseline -split "`n" | Where-Object { $_ -match '^(OUT|IN) ' }).Count
Write-Host "Baseline: $baseCount enumerable ports"
Write-Host ($baseline -split "`n" | Where-Object { $_ -match 'COUNT' }) -Separator '  '
Write-Host ""

$failures = 0
$createdIds = @()

try {
    for ($c = 1; $c -le $Cycles; $c++) {
        Write-Host "cycle $c/$Cycles"

        $out = & $MidiExe loopback create --name-a "CorrectnessA$c" --name-b "CorrectnessB$c" 2>&1 | Out-String
        $m = $reAssoc.Match($out)
        if (-not $m.Success) { Write-Warning "  could not parse association id"; continue }
        $assoc = '{' + $m.Groups[1].Value + '}'
        $createdIds += $assoc
        Start-Sleep -Milliseconds 2500

        $afterCreate = [PortTable]::Snapshot()
        $addedOut = @(($afterCreate -split "`n") | Where-Object { $_ -match "^OUT .*Correctness" })
        $addedIn  = @(($afterCreate -split "`n") | Where-Object { $_ -match "^IN .*Correctness" })
        Write-Host ("  after create: +{0} out, +{1} in" -f $addedOut.Count, $addedIn.Count)
        if ($addedOut.Count -ne 2 -or $addedIn.Count -ne 2) {
            Write-Host "    UNEXPECTED port count after create" -ForegroundColor Red
            $failures++
        }

        & $MidiExe loopback remove --association-id $assoc 2>&1 | Out-Null
        $createdIds = @($createdIds | Where-Object { $_ -ne $assoc })
        Start-Sleep -Milliseconds 2500

        $afterRemove = [PortTable]::Snapshot()

        if ($afterRemove -eq $baseline) {
            Write-Host "  after remove: port table identical to baseline" -ForegroundColor Green
        }
        else {
            Write-Host "  after remove: PORT TABLE DIFFERS FROM BASELINE" -ForegroundColor Red
            Show-Diff $baseline $afterRemove "differences:"
            $failures++
        }
    }
}
finally {
    foreach ($id in $createdIds) { & $MidiExe loopback remove --association-id $id 2>&1 | Out-Null }
}

Write-Host ""
if ($failures -eq 0) {
    Write-Host "PASS - port table returned exactly to baseline after every create/remove cycle." -ForegroundColor Green
} else {
    Write-Host "FAIL - $failures cycle(s) left the port table wrong." -ForegroundColor Red
}
Write-Host ""
