# Measures whether CMidiPorts::m_Lock contention is observable from a WinMM client.
#
#   - a "sender" thread holds an open MIDI OUT port and calls midiOutShortMsg in a loop,
#     timing every individual call (this path takes m_Lock via GetOpenedPort)
#   - a "watcher" thread polls midiInGetNumDevs / midiOutGetNumDevs and timestamps every change
#
# Plug / unplug a MIDI device while it runs. Any device-change stall on the send path shows up
# as a latency spike lining up with a device count change.
#
# Usage:  powershell -File midi-lock-probe.ps1 [-Seconds 60] [-OutDevice <index>] [-ListOnly]

param(
    [int]    $Seconds   = 60,
    [int]    $OutDevice = -1,
    [switch] $ListOnly
)

$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading;

public static class MidiProbe
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

    public static long Freq;
    public static long Start;
    public static DateTime StartWall;

    public static double Ms(long ticks) { return ticks * 1000.0 / Freq; }
    public static long Now() { long v; QueryPerformanceCounter(out v); return v; }

    // ETL timestamps are absolute, so every reported offset also needs a wall-clock equivalent.
    public static string Wall(long offsetTicks)
    {
        return StartWall.AddMilliseconds(Ms(offsetTicks)).ToString("HH:mm:ss.fff");
    }

    // sender results
    public static List<long>   SendTicks  = new List<long>();   // duration of each midiOutShortMsg
    public static List<long>   SendAt     = new List<long>();   // when each call started
    public static int          SendErrors = 0;

    // watcher results
    public static List<string> Changes = new List<string>();

    public static volatile bool Stop = false;

    static Thread _sender, _watcher;

    public static void StartThreads(int outDevice)
    {
        Stop = false;
        StartWall = DateTime.Now;
        Start = Now();

        _sender  = new Thread(Sender);
        _watcher = new Thread(Watcher);
        _sender.IsBackground = true;
        _watcher.IsBackground = true;
        _sender.Start(outDevice);
        _watcher.Start();
    }

    public static void StopThreads()
    {
        Stop = true;
        if (_sender  != null) _sender.Join(3000);
        if (_watcher != null) _watcher.Join(3000);
    }

    public static string ListDevices()
    {
        var sb = new System.Text.StringBuilder();
        uint n = midiOutGetNumDevs();
        sb.AppendLine("MIDI OUT devices: " + n);
        for (uint i = 0; i < n; i++)
        {
            var c = new MIDIOUTCAPS();
            if (midiOutGetDevCapsW((UIntPtr)i, ref c, (uint)Marshal.SizeOf(typeof(MIDIOUTCAPS))) == 0)
                sb.AppendLine("  [" + i + "] " + c.szPname);
            else
                sb.AppendLine("  [" + i + "] <caps failed>");
        }
        sb.AppendLine("MIDI IN devices: " + midiInGetNumDevs());
        return sb.ToString();
    }

    public static void Sender(object arg)
    {
        Thread.CurrentThread.Priority = ThreadPriority.Highest;

        IntPtr h;
        uint id = (uint)(int)arg;

        if (midiOutOpen(out h, id, IntPtr.Zero, IntPtr.Zero, 0) != 0)
        {
            SendErrors = -1;
            return;
        }

        try
        {
            long nextDue = Now();
            long interval = Freq / 500;   // aim for a send every 2 ms

            while (!Stop)
            {
                while (Now() < nextDue && !Stop) { }
                nextDue += interval;

                long a = Now();
                uint r = midiOutShortMsg(h, 0xFE);   // Active Sensing: single byte, makes no sound
                long b = Now();

                SendTicks.Add(b - a);
                SendAt.Add(a - Start);

                if (r != 0) SendErrors++;
            }
        }
        finally { midiOutClose(h); }
    }

    public static void Watcher()
    {
        uint lastIn = midiInGetNumDevs();
        uint lastOut = midiOutGetNumDevs();
        Changes.Add(string.Format("{0,10:F1}  baseline  in={1} out={2}", 0.0, lastIn, lastOut));

        while (!Stop)
        {
            long a = Now();
            uint i = midiInGetNumDevs();
            uint o = midiOutGetNumDevs();
            long b = Now();

            if (i != lastIn || o != lastOut)
            {
                Changes.Add(string.Format("{0,10:F1} ms  {1}  CHANGE  in={2} out={3}   (getNumDevs pair took {4:F2} ms)",
                                          Ms(a - Start), Wall(a - Start), i, o, Ms(b - a)));
                lastIn = i; lastOut = o;
            }

            // Kept slow on purpose: this thread takes the same lock as the sender.
            Thread.Sleep(20);
        }
    }
}
'@

[MidiProbe]::QueryPerformanceFrequency([ref]$null) | Out-Null
$freq = 0L
[MidiProbe]::QueryPerformanceFrequency([ref]$freq) | Out-Null
[MidiProbe]::Freq = $freq

Write-Host ([MidiProbe]::ListDevices())

if ($ListOnly) { return }

if ($OutDevice -lt 0) {
    if ([MidiProbe]::midiOutGetNumDevs() -eq 0) { throw "No MIDI output devices. Create a loopback first." }
    $OutDevice = 0
}

Write-Host "Sending to OUT device [$OutDevice] for $Seconds seconds."
Write-Host "PLUG AND UNPLUG a DIFFERENT MIDI device a few times now.`n"

[MidiProbe]::timeBeginPeriod(1) | Out-Null
[MidiProbe]::StartThreads($OutDevice)

for ($s = $Seconds; $s -gt 0; $s--) {
    Start-Sleep -Seconds 1
    if ($s % 10 -eq 0) { Write-Host "  $s s remaining..." }
}

[MidiProbe]::StopThreads()
[MidiProbe]::timeEndPeriod(1) | Out-Null

$ticks = [MidiProbe]::SendTicks
$at    = [MidiProbe]::SendAt

if ($ticks.Count -eq 0) { throw "No sends recorded (midiOutOpen failed?). SendErrors=$([MidiProbe]::SendErrors)" }

$ms = New-Object 'double[]' $ticks.Count
for ($i = 0; $i -lt $ticks.Count; $i++) { $ms[$i] = [MidiProbe]::Ms($ticks[$i]) }
$sorted = [double[]]$ms.Clone(); [Array]::Sort($sorted)

function Pct($a, $p) { $a[[Math]::Min($a.Length - 1, [int][Math]::Floor($a.Length * $p))] }

Write-Host "`n================ midiOutShortMsg call latency ================"
Write-Host ("  calls        : {0}" -f $ms.Count)
Write-Host ("  send errors  : {0}" -f [MidiProbe]::SendErrors)
Write-Host ("  median       : {0:F4} ms" -f (Pct $sorted 0.50))
Write-Host ("  p99          : {0:F4} ms" -f (Pct $sorted 0.99))
Write-Host ("  p99.9        : {0:F4} ms" -f (Pct $sorted 0.999))
Write-Host ("  max          : {0:F4} ms" -f $sorted[$sorted.Length - 1])

Write-Host "`n  slowest 15 calls:"
Write-Host "      offset        wall clock        duration"
$idx = 0..($ms.Count - 1) | Sort-Object { - $ms[$_] } | Select-Object -First 15
foreach ($i in $idx) {
    Write-Host ("    {0,10:F1} ms   {1}   {2,9:F3} ms" -f [MidiProbe]::Ms($at[$i]), [MidiProbe]::Wall($at[$i]), $ms[$i])
}

Write-Host "`n================ device count changes ================"
[MidiProbe]::Changes | ForEach-Object { Write-Host "  $_" }
Write-Host ""
