# Regression test for Feature_Servicing_MIDI2WinMMPortListLockScope (close).
#
# Closing a MIDI input that is still running waits for any callback in progress. CMidiPorts::Close
# used to hold m_Lock during that wait. A callback that forwards a message with midiOutShortMsg
# (MIDI thru) reaches GetOpenedPort, which also takes m_Lock, so the two threads waited on each
# other forever.
#
# Opens the "App Loopback (B)" input with a callback that, on the first short message, waits and
# then forwards one message to "App Loopback (A)". The main thread closes the input during that
# wait, without stopping it first. The probe runs in a child process so a hang can be ended.
#
# Usage:  powershell -File midi-close-callback-test.ps1 [-TimeoutSeconds 5]

param(
    [int]    $TimeoutSeconds = 5,
    [string] $InName         = 'App Loopback (B)',
    [string] $OutName        = 'App Loopback (A)',
    [switch] $Child
)

$ErrorActionPreference = 'Stop'

if (-not $Child) {
    $hostExe = (Get-Process -Id $PID).Path
    $outFile = [IO.Path]::GetTempFileName()
    $arguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$PSCommandPath`"", '-Child',
                   '-TimeoutSeconds', $TimeoutSeconds, '-InName', "`"$InName`"", '-OutName', "`"$OutName`"")

    $probe = Start-Process -FilePath $hostExe -ArgumentList $arguments -NoNewWindow -PassThru -RedirectStandardOutput $outFile
    # Without reading Handle now, ExitCode comes back empty after the process exits.
    $null = $probe.Handle
    if (-not $probe.WaitForExit(($TimeoutSeconds + 20) * 1000)) {
        $probe.Kill()
        Write-Host "FAIL  - the probe process did not finish, so it was ended." -ForegroundColor Red
        Remove-Item $outFile -ErrorAction SilentlyContinue
        exit 1
    }

    Get-Content $outFile | ForEach-Object { Write-Host "  $_" }
    Remove-Item $outFile -ErrorAction SilentlyContinue

    Write-Host ""
    switch ($probe.ExitCode) {
        0       { Write-Host "PASS  - midiInClose returned while the input's callback was calling midiOutShortMsg." -ForegroundColor Green; exit 0 }
        2       { Write-Host "INCONCLUSIVE - the loopback ports were not found, or no message arrived." -ForegroundColor Yellow; exit 2 }
        3       { Write-Host "FAIL  - midiInClose returned an error." -ForegroundColor Red; exit 1 }
        default { Write-Host "FAIL  - midiInClose did not return: the close and the callback are waiting on each other." -ForegroundColor Red; exit 1 }
    }
}

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Threading;

public static class CloseProbe
{
    public delegate void MidiInProc(IntPtr hMidiIn, uint wMsg, IntPtr dwInstance, IntPtr dwParam1, IntPtr dwParam2);

    [DllImport("winmm.dll")] public static extern uint midiInGetNumDevs();
    [DllImport("winmm.dll", CharSet = CharSet.Unicode)]
    public static extern uint midiInGetDevCapsW(UIntPtr id, ref MIDIINCAPS caps, uint cbSize);
    [DllImport("winmm.dll")] public static extern uint midiOutGetNumDevs();
    [DllImport("winmm.dll", CharSet = CharSet.Unicode)]
    public static extern uint midiOutGetDevCapsW(UIntPtr id, ref MIDIOUTCAPS caps, uint cbSize);
    [DllImport("winmm.dll")] public static extern uint midiInOpen(out IntPtr h, uint id, MidiInProc cb, IntPtr inst, uint flags);
    [DllImport("winmm.dll")] public static extern uint midiInStart(IntPtr h);
    [DllImport("winmm.dll")] public static extern uint midiInStop(IntPtr h);
    [DllImport("winmm.dll")] public static extern uint midiInClose(IntPtr h);
    [DllImport("winmm.dll")] public static extern uint midiOutOpen(out IntPtr h, uint id, IntPtr cb, IntPtr inst, uint flags);
    [DllImport("winmm.dll")] public static extern uint midiOutShortMsg(IntPtr h, uint msg);
    [DllImport("winmm.dll")] public static extern uint midiOutClose(IntPtr h);

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

    const uint MIM_DATA = 0x3C3;
    const uint CALLBACK_FUNCTION = 0x30000;
    const uint HarmlessCc = 0x000066BF;   // CC 102 = 0 on channel 16

    // winmm keeps the function pointer, so the delegate must outlive the open handle.
    static readonly MidiInProc Callback = OnMidiIn;
    static readonly ManualResetEvent InCallback = new ManualResetEvent(false);
    static IntPtr _out;
    static int _first;

    public static int HoldMs = 500;
    public static string Report = "";

    static void OnMidiIn(IntPtr h, uint msg, IntPtr inst, IntPtr p1, IntPtr p2)
    {
        if (msg != MIM_DATA || Interlocked.Exchange(ref _first, 1) != 0)
            return;

        InCallback.Set();
        Thread.Sleep(HoldMs);
        midiOutShortMsg(_out, HarmlessCc);
    }

    static int FindIn(string needle)
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

    static int FindOut(string needle)
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

    // 0 = midiInClose returned, 1 = it did not return in time, 2 = rig problem, 3 = it returned an error.
    public static int Run(string inName, string outName, int timeoutMs)
    {
        int inId = FindIn(inName), outId = FindOut(outName);
        if (inId < 0 || outId < 0) { Report = "ports not found"; return 2; }

        IntPtr hIn;
        if (midiInOpen(out hIn, (uint)inId, Callback, IntPtr.Zero, CALLBACK_FUNCTION) != 0) { Report = "midiInOpen failed"; return 2; }
        if (midiOutOpen(out _out, (uint)outId, IntPtr.Zero, IntPtr.Zero, 0) != 0) { midiInClose(hIn); Report = "midiOutOpen failed"; return 2; }

        midiInStart(hIn);
        Thread.Sleep(100);
        midiOutShortMsg(_out, HarmlessCc);

        if (!InCallback.WaitOne(3000))
        {
            midiInStop(hIn);
            midiInClose(hIn);
            midiOutClose(_out);
            Report = "no message arrived";
            return 2;
        }

        // Close while the callback is still running, without stopping the input first.
        uint closeResult = uint.MaxValue;
        long started = Environment.TickCount;
        var closer = new Thread(() => { closeResult = midiInClose(hIn); }) { IsBackground = true };
        closer.Start();

        if (!closer.Join(timeoutMs))
        {
            Report = String.Format("midiInClose had not returned after {0} ms", timeoutMs);
            return 1;
        }

        midiOutClose(_out);
        Report = String.Format("midiInClose returned {0} after {1} ms", closeResult, Environment.TickCount - started);
        return closeResult == 0 ? 0 : 3;
    }
}
'@

$code = [CloseProbe]::Run($InName, $OutName, $TimeoutSeconds * 1000)
Write-Output ([CloseProbe]::Report)

if ($code -eq 1) {
    # The hung threads would stop this process from exiting normally.
    [Console]::Out.Flush()
    [Diagnostics.Process]::GetCurrentProcess().Kill()
}

exit $code
