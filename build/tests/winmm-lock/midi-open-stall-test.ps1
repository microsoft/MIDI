# Regression test for Feature_Servicing_MIDI2WinMMPortListLockScope (open).
#
# CMidiPorts::Open used to hold m_Lock while it connected the new port to the service. Every
# midiOutShortMsg goes through GetOpenedPort, which takes m_Lock, so sends on an unrelated port
# stalled for the whole open. Measured 7-12 ms per open of a software loopback input.
#
# A Highest-priority thread sends Active Sensing to "App Loopback (A)" every 2 ms and times each
# call, while the main thread opens and closes the "App Loopback (B)" input over and over.
#
# Usage:  powershell -File midi-open-stall-test.ps1 [-Cycles 20] [-FailAboveMs 2]

param(
    [int]    $Cycles      = 20,
    [double] $FailAboveMs = 2.0,
    [string] $InName      = 'App Loopback (B)',
    [string] $OutName     = 'App Loopback (A)'
)

$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading;

public static class OpenStallProbe
{
    [DllImport("winmm.dll")] public static extern uint midiInGetNumDevs();
    [DllImport("winmm.dll", CharSet = CharSet.Unicode)]
    public static extern uint midiInGetDevCapsW(UIntPtr id, ref MIDIINCAPS caps, uint cbSize);
    [DllImport("winmm.dll")] public static extern uint midiOutGetNumDevs();
    [DllImport("winmm.dll", CharSet = CharSet.Unicode)]
    public static extern uint midiOutGetDevCapsW(UIntPtr id, ref MIDIOUTCAPS caps, uint cbSize);
    [DllImport("winmm.dll")] public static extern uint midiInOpen(out IntPtr h, uint id, IntPtr cb, IntPtr inst, uint flags);
    [DllImport("winmm.dll")] public static extern uint midiInClose(IntPtr h);
    [DllImport("winmm.dll")] public static extern uint midiOutOpen(out IntPtr h, uint id, IntPtr cb, IntPtr inst, uint flags);
    [DllImport("winmm.dll")] public static extern uint midiOutShortMsg(IntPtr h, uint msg);
    [DllImport("winmm.dll")] public static extern uint midiOutClose(IntPtr h);
    [DllImport("winmm.dll")] public static extern uint timeBeginPeriod(uint p);
    [DllImport("winmm.dll")] public static extern uint timeEndPeriod(uint p);
    [DllImport("kernel32.dll")] public static extern bool QueryPerformanceCounter(out long v);
    [DllImport("kernel32.dll")] public static extern bool QueryPerformanceFrequency(out long v);

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct MIDIINCAPS
    {
        public ushort wMid, wPid;
        public uint vDriverVersion;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string szPname;
        public uint dwSupport;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct MIDIOUTCAPS
    {
        public ushort wMid, wPid;
        public uint vDriverVersion;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string szPname;
        public ushort wTechnology, wVoices, wNotes, wChannelMask;
        public uint dwSupport;
    }

    public class Sample { public long At; public double Dur; }
    public class Window { public long Start, End; }

    public static long Freq, Start;
    public static List<Sample> Sends = new List<Sample>();
    public static List<Window> Opens = new List<Window>();
    public static List<double> CloseMs = new List<double>();
    public static int SendErrors = 0;
    static volatile bool _stop;
    static Thread _sender;

    public static double Ms(long t) { return t * 1000.0 / Freq; }
    public static long Now() { long v; QueryPerformanceCounter(out v); return v; }

    public static int FindIn(string needle)
    {
        uint n = midiInGetNumDevs();
        for (uint i = 0; i < n; i++)
        {
            var c = new MIDIINCAPS();
            if (midiInGetDevCapsW((UIntPtr)i, ref c, (uint)Marshal.SizeOf(typeof(MIDIINCAPS))) == 0)
                if (c.szPname != null && c.szPname.IndexOf(needle, StringComparison.OrdinalIgnoreCase) >= 0)
                    return (int)i;
        }
        return -1;
    }

    public static int FindOut(string needle)
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

    public static void StartSender(int outDevice)
    {
        QueryPerformanceFrequency(out Freq);
        Start = Now();
        _stop = false;
        _sender = new Thread(Sender) { IsBackground = true };
        _sender.Start(outDevice);
    }

    public static void StopSender()
    {
        _stop = true;
        if (_sender != null) _sender.Join(3000);
    }

    static void Sender(object arg)
    {
        Thread.CurrentThread.Priority = ThreadPriority.Highest;
        IntPtr h;
        if (midiOutOpen(out h, (uint)(int)arg, IntPtr.Zero, IntPtr.Zero, 0) != 0) { SendErrors = -1; return; }
        try
        {
            long next = Now(), interval = Freq / 500;
            while (!_stop)
            {
                while (Now() < next && !_stop) { }
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

    public static string Churn(int inDevice, int cycles)
    {
        for (int c = 0; c < cycles; c++)
        {
            IntPtr h;
            long a = Now();
            uint r = midiInOpen(out h, (uint)inDevice, IntPtr.Zero, IntPtr.Zero, 0);
            long b = Now();
            if (r != 0) return String.Format("midiInOpen failed with {0} on cycle {1}", r, c + 1);
            Opens.Add(new Window { Start = a - Start, End = b - Start });
            Thread.Sleep(50);

            a = Now();
            midiInClose(h);
            CloseMs.Add(Ms(Now() - a));
            Thread.Sleep(50);
        }
        return null;
    }
}
'@

$inIndex = [OpenStallProbe]::FindIn($InName)
$outIndex = [OpenStallProbe]::FindOut($OutName)
if ($inIndex -lt 0 -or $outIndex -lt 0) { throw "Loopback ports '$InName' / '$OutName' not found." }

Write-Host "Sending to MIDI OUT index $outIndex, opening and closing MIDI IN index $inIndex $Cycles times.`n"

[OpenStallProbe]::timeBeginPeriod(1) | Out-Null
[OpenStallProbe]::StartSender($outIndex)
try {
    Start-Sleep -Milliseconds 500
    $failure = [OpenStallProbe]::Churn($inIndex, $Cycles)
    Start-Sleep -Milliseconds 200
}
finally {
    [OpenStallProbe]::StopSender()
    [OpenStallProbe]::timeEndPeriod(1) | Out-Null
}

if ($failure) { throw $failure }

$sends = [OpenStallProbe]::Sends
$opens = [OpenStallProbe]::Opens
if ($sends.Count -eq 0 -or $opens.Count -eq 0) { throw "no samples collected" }

# A send that started before an open finished and ended after the open started overlapped it.
$duringOpen = @($sends | Where-Object {
    $s = $_
    $end = $s.At + [long]($s.Dur * [OpenStallProbe]::Freq / 1000)
    @($opens | Where-Object { $s.At -le $_.End -and $end -ge $_.Start }).Count -gt 0
})
$idle = @($sends | Where-Object { $duringOpen -notcontains $_ })

function Report($values, $label) {
    if ($values.Count -eq 0) { Write-Host ("  {0,-34} (no samples)" -f $label); return 0 }
    $s = $values | Sort-Object
    $max = $s[$s.Count - 1]
    Write-Host ("  {0,-34} n={1,-6} median={2,7:F3} ms   max={3,9:F3} ms" -f $label, $s.Count, $s[[int]($s.Count/2)], $max)
    return $max
}

Write-Host "================ results ================"
$null          = Report @($opens | ForEach-Object { [OpenStallProbe]::Ms($_.End - $_.Start) }) "midiInOpen"
$null          = Report @([OpenStallProbe]::CloseMs) "midiInClose"
$null          = Report @($idle | ForEach-Object { $_.Dur }) "midiOutShortMsg, no open running"
$maxDuringOpen = Report @($duringOpen | ForEach-Object { $_.Dur }) "midiOutShortMsg, during an open"
Write-Host ("  send errors: {0}" -f [OpenStallProbe]::SendErrors)

Write-Host ""
if ($duringOpen.Count -eq 0) {
    Write-Host "INCONCLUSIVE - no send overlapped an open." -ForegroundColor Yellow
    exit 2
}
elseif ($maxDuringOpen -gt $FailAboveMs) {
    Write-Host ("FAIL  - midiOutShortMsg stalled up to {0:F3} ms while another port was opening (limit {1:F1} ms)." -f $maxDuringOpen, $FailAboveMs) -ForegroundColor Red
    exit 1
}
else {
    Write-Host ("PASS  - midiOutShortMsg max {0:F3} ms while another port was opening, under the {1:F1} ms limit." -f $maxDuringOpen, $FailAboveMs) -ForegroundColor Green
    exit 0
}
