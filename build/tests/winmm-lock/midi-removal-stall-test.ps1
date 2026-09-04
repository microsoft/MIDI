# Regression test for Feature_Servicing_MIDI2WinMMInterfaceRemovalPerf.
#
# CMidiPorts::MidiInterfaceChange used to call RefreshPortsForFlow (a full SetupDi walk) once per
# departing interface, holding m_Lock across the burst. Every reader - including midiOutShortMsg
# via GetOpenedPort, and midiInGetNumDevs which does nothing but read a cached int - blocked
# behind it. Measured 14-25 ms on every removal before the fix.
#
# Creates and removes a loopback pair to fire the same interface-removal path as a hot unplug,
# so no hardware is needed.
#
# Usage:  powershell -File midi-removal-stall-test.ps1 [-Cycles 5] [-FailAboveMs 5]

param(
    [int]    $Cycles      = 5,
    [double] $FailAboveMs = 1.0,
    [string] $MidiExe     = 'midi'
)

$ErrorActionPreference = 'Stop'
$namePrefix = 'KIRRemovalTest'

Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading;

public static class StallProbe
{
    [DllImport("winmm.dll")] public static extern uint midiOutGetNumDevs();
    [DllImport("winmm.dll")] public static extern uint midiInGetNumDevs();
    [DllImport("winmm.dll", CharSet = CharSet.Unicode)]
    public static extern uint midiOutGetDevCapsW(UIntPtr id, ref MIDIOUTCAPS caps, uint cbSize);
    [DllImport("winmm.dll")]
    public static extern uint midiOutOpen(out IntPtr h, uint id, IntPtr cb, IntPtr inst, uint flags);
    [DllImport("winmm.dll")] public static extern uint midiOutShortMsg(IntPtr h, uint msg);
    [DllImport("winmm.dll")] public static extern uint midiOutClose(IntPtr h);
    [DllImport("winmm.dll")] public static extern uint timeBeginPeriod(uint p);
    [DllImport("winmm.dll")] public static extern uint timeEndPeriod(uint p);
    [DllImport("kernel32.dll")] public static extern bool QueryPerformanceCounter(out long v);
    [DllImport("kernel32.dll")] public static extern bool QueryPerformanceFrequency(out long v);

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct MIDIOUTCAPS
    {
        public ushort wMid, wPid;
        public uint vDriverVersion;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string szPname;
        public ushort wTechnology, wVoices, wNotes, wChannelMask;
        public uint dwSupport;
    }

    public static long Freq, Start;
    public static DateTime StartWall;
    public static double Ms(long t) { return t * 1000.0 / Freq; }
    public static long Now() { long v; QueryPerformanceCounter(out v); return v; }
    public static DateTime Wall(long off) { return StartWall.AddMilliseconds(Ms(off)); }

    public class Sample { public long At; public double Dur; public uint In, Out; }

    public static List<Sample> Polls = new List<Sample>();   // getNumDevs pair timings
    public static List<Sample> Sends = new List<Sample>();   // midiOutShortMsg timings
    public static int SendErrors = 0;
    public static volatile bool Stop = false;

    static Thread _sender, _watcher;

    public static int FindOutDeviceByName(string needle)
    {
        uint n = midiOutGetNumDevs();
        for (uint i = 0; i < n; i++)
        {
            var c = new MIDIOUTCAPS();
            if (midiOutGetDevCapsW((UIntPtr)i, ref c, (uint)Marshal.SizeOf(typeof(MIDIOUTCAPS))) == 0)
                if (c.szPname != null && c.szPname.IndexOf(needle, StringComparison.OrdinalIgnoreCase) >= 0)
                    return (int)i;
        }
        return -1;
    }

    public static void StartThreads(int outDevice)
    {
        QueryPerformanceFrequency(out Freq);
        Stop = false;
        StartWall = DateTime.Now;
        Start = Now();
        _sender = new Thread(Sender) { IsBackground = true };
        _watcher = new Thread(Watcher) { IsBackground = true };
        _sender.Start(outDevice);
        _watcher.Start();
    }

    public static void StopThreads()
    {
        Stop = true;
        if (_sender != null) _sender.Join(3000);
        if (_watcher != null) _watcher.Join(3000);
    }

    static void Sender(object arg)
    {
        Thread.CurrentThread.Priority = ThreadPriority.Highest;
        IntPtr h;
        if (midiOutOpen(out h, (uint)(int)arg, IntPtr.Zero, IntPtr.Zero, 0) != 0) { SendErrors = -1; return; }
        try
        {
            long next = Now(), interval = Freq / 500;
            while (!Stop)
            {
                while (Now() < next && !Stop) { }
                next += interval;
                long a = Now();
                uint r = midiOutShortMsg(h, 0xFE);   // Active Sensing: single byte, silent
                long b = Now();
                Sends.Add(new Sample { At = a - Start, Dur = Ms(b - a) });
                if (r != 0) SendErrors++;
            }
        }
        finally { midiOutClose(h); }
    }

    static void Watcher()
    {
        while (!Stop)
        {
            long a = Now();
            uint i = midiInGetNumDevs();
            uint o = midiOutGetNumDevs();
            long b = Now();
            Polls.Add(new Sample { At = a - Start, Dur = Ms(b - a), In = i, Out = o });
            Thread.Sleep(5);
        }
    }
}
'@

# The console prints a boxed table and the guid is unbraced, so anchor on the label. The endpoint
# ids printed above it contain the interface class guid, which a naive guid match would grab.
$reAssoc = [regex]'Association\s*Id[^0-9a-fA-F]*([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})'

$createdIds = @()

$outIndex = [StallProbe]::FindOutDeviceByName('Basic App Loopback')
if ($outIndex -lt 0) { $outIndex = [StallProbe]::FindOutDeviceByName('App Loopback') }
if ($outIndex -lt 0) { throw "No loopback MIDI output found to send to. Create one, or edit this script." }

Write-Host "Sending to MIDI OUT index $outIndex"
Write-Host "Running $Cycles create/remove cycles.`n"

[StallProbe]::timeBeginPeriod(1) | Out-Null
[StallProbe]::StartThreads($outIndex)

$removals = @()
$creates  = @()
try {
    for ($c = 1; $c -le $Cycles; $c++) {
        $a = "$namePrefix A $c"
        $b = "$namePrefix B $c"

        $tCreate = [StallProbe]::Now() - [StallProbe]::Start
        $out = & $MidiExe loopback create --name-a $a --name-b $b 2>&1 | Out-String
        $creates += $tCreate
        $m = $reAssoc.Match($out)
        if (-not $m.Success) {
            Write-Warning "cycle ${c}: could not parse association id"
            continue
        }
        $assoc = '{' + $m.Groups[1].Value + '}'
        $createdIds += $assoc
        Start-Sleep -Milliseconds 2500

        $tRemove = [StallProbe]::Now() - [StallProbe]::Start
        & $MidiExe loopback remove --association-id $assoc 2>&1 | Out-Null
        $removals += $tRemove
        $createdIds = @($createdIds | Where-Object { $_ -ne $assoc })
        Write-Host ("  cycle {0}/{1}  created and removed {2}" -f $c, $Cycles, $assoc)

        Start-Sleep -Milliseconds 2500
    }
}
finally {
    [StallProbe]::StopThreads()
    [StallProbe]::timeEndPeriod(1) | Out-Null
    foreach ($id in $createdIds) { & $MidiExe loopback remove --association-id $id 2>&1 | Out-Null }
}

$polls = [StallProbe]::Polls
$sends = [StallProbe]::Sends
if ($polls.Count -eq 0 -or $sends.Count -eq 0) { throw "no samples collected" }

# The fix targets removal only, so bucket create and removal separately - arrival still refreshes.
function InWindow($at, $marks) {
    foreach ($m in $marks) {
        $d = [StallProbe]::Ms($at - $m)
        if ($d -ge -50 -and $d -le 1500) { return $true }
    }
    return $false
}

$pollRemoval = @($polls | Where-Object { InWindow $_.At $removals })
$pollCreate  = @($polls | Where-Object { (InWindow $_.At $creates) -and -not (InWindow $_.At $removals) })
$pollIdle    = @($polls | Where-Object { -not (InWindow $_.At $removals) -and -not (InWindow $_.At $creates) })
$sendRemoval = @($sends | Where-Object { InWindow $_.At $removals })

function Report($set, $label) {
    if ($set.Count -eq 0) { Write-Host ("  {0,-36} (no samples)" -f $label); return 0 }
    $s = ($set | ForEach-Object { $_.Dur }) | Sort-Object
    $max = $s[$s.Count - 1]
    Write-Host ("  {0,-36} n={1,-6} median={2,7:F3} ms   max={3,9:F3} ms" -f $label, $set.Count, $s[[int]($s.Count/2)], $max)
    return $max
}

Write-Host "`n================ results ================"
$null           = Report $pollIdle    "getNumDevs pair, idle"
$null           = Report $pollCreate  "getNumDevs pair, during create"
$maxPollRemoval = Report $pollRemoval "getNumDevs pair, during REMOVAL"
$null           = Report $sendRemoval "midiOutShortMsg, during REMOVAL"
Write-Host ("  send errors: {0}" -f [StallProbe]::SendErrors)

Write-Host "`n  slowest 10 getNumDevs pairs in removal windows:"
$pollRemoval | Sort-Object Dur -Descending | Select-Object -First 10 | ForEach-Object {
    Write-Host ("    {0}   {1,8:F3} ms   in={2} out={3}" -f [StallProbe]::Wall($_.At).ToString('HH:mm:ss.fff'), $_.Dur, $_.In, $_.Out)
}

Write-Host ""
if ($removals.Count -eq 0) {
    Write-Host "INCONCLUSIVE - no removals were performed." -ForegroundColor Yellow
}
elseif ($maxPollRemoval -gt $FailAboveMs) {
    Write-Host ("FAIL  - getNumDevs stalled up to {0:F3} ms during removal (limit {1:F1} ms)." -f $maxPollRemoval, $FailAboveMs) -ForegroundColor Red
    Write-Host "        midiInGetNumDevs only takes the lock and reads a cached int, so this is lock wait."
}
else {
    Write-Host ("PASS  - getNumDevs max {0:F3} ms during removal, under the {1:F1} ms limit." -f $maxPollRemoval, $FailAboveMs) -ForegroundColor Green
}
Write-Host ""
