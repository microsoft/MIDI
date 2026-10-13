# Regression test for Feature_Servicing_MIDI2WinMMUnprepareAfterDeviceRemoval.
#
# Once a device was removed, wdmaud2 answered midiInUnprepareHeader and midiOutUnprepareHeader with
# MMSYSERR_NODRIVER (6). The header stayed prepared, and apps that check the result, like RtMidi
# closing a port, took their error path. The fix passes the unprepare to the port as it does while
# the device is present, so winmm unprepares the header.
#
# One loopback pair stands in for the device, so no hardware and no elevation are needed. A control
# pass runs while the loopback is there. Then buffers are queued on an input and prepared on an
# output, the loopback is removed, and both handles are closed in the order RtMidi uses: reset,
# stop, unprepare, close.
#
# PASS: every unprepare after the removal returns 0 and clears MHDR_PREPARED, a buffer still queued
# is refused with MIDIERR_STILLPLAYING (65) both before and after the removal, and both closes
# return 0. FAIL: anything else. INCONCLUSIVE: the loopback could not be set up or removed.
#
# Usage:  powershell -File midi-unprepare-after-removal-test.ps1
# 32-bit: %windir%\SysWOW64\WindowsPowerShell\v1.0\powershell.exe -File midi-unprepare-after-removal-test.ps1

param(
    [string] $MidiExe = 'midi'
)

$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class UnprepareProbe
{
    [DllImport("winmm.dll")] public static extern uint midiInGetNumDevs();
    [DllImport("winmm.dll", CharSet = CharSet.Unicode)]
    public static extern uint midiInGetDevCapsW(UIntPtr id, ref MIDIINCAPS caps, uint cbSize);
    [DllImport("winmm.dll")] public static extern uint midiOutGetNumDevs();
    [DllImport("winmm.dll", CharSet = CharSet.Unicode)]
    public static extern uint midiOutGetDevCapsW(UIntPtr id, ref MIDIOUTCAPS caps, uint cbSize);

    [DllImport("winmm.dll")] public static extern uint midiInOpen(out IntPtr h, uint id, IntPtr cb, IntPtr inst, uint flags);
    [DllImport("winmm.dll")] static extern uint midiInPrepareHeader(IntPtr h, IntPtr hdr, uint cbHdr);
    [DllImport("winmm.dll")] static extern uint midiInUnprepareHeader(IntPtr h, IntPtr hdr, uint cbHdr);
    [DllImport("winmm.dll")] static extern uint midiInAddBuffer(IntPtr h, IntPtr hdr, uint cbHdr);
    [DllImport("winmm.dll")] public static extern uint midiInStart(IntPtr h);
    [DllImport("winmm.dll")] public static extern uint midiInStop(IntPtr h);
    [DllImport("winmm.dll")] public static extern uint midiInReset(IntPtr h);
    [DllImport("winmm.dll")] public static extern uint midiInClose(IntPtr h);

    [DllImport("winmm.dll")] public static extern uint midiOutOpen(out IntPtr h, uint id, IntPtr cb, IntPtr inst, uint flags);
    [DllImport("winmm.dll")] static extern uint midiOutPrepareHeader(IntPtr h, IntPtr hdr, uint cbHdr);
    [DllImport("winmm.dll")] static extern uint midiOutUnprepareHeader(IntPtr h, IntPtr hdr, uint cbHdr);
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

    [StructLayout(LayoutKind.Sequential)]
    public struct MIDIHDR
    {
        public IntPtr lpData;
        public uint dwBufferLength;
        public uint dwBytesRecorded;
        public IntPtr dwUser;
        public uint dwFlags;
        public IntPtr lpNext;
        public IntPtr reserved;
        public uint dwOffset;
        public IntPtr dwReserved0, dwReserved1, dwReserved2, dwReserved3, dwReserved4, dwReserved5, dwReserved6, dwReserved7;
    }

    public const uint MHDR_PREPARED = 2;
    const int BufferBytes = 256;
    static readonly uint HeaderSize = (uint)Marshal.SizeOf(typeof(MIDIHDR));

    public static int FindIn(string name)
    {
        uint n = midiInGetNumDevs();
        for (uint i = 0; i < n; i++)
        {
            MIDIINCAPS c = new MIDIINCAPS();
            if (midiInGetDevCapsW((UIntPtr)i, ref c, (uint)Marshal.SizeOf(typeof(MIDIINCAPS))) == 0 && c.szPname == name)
                return (int)i;
        }
        return -1;
    }

    public static int FindOut(string name)
    {
        uint n = midiOutGetNumDevs();
        for (uint i = 0; i < n; i++)
        {
            MIDIOUTCAPS c = new MIDIOUTCAPS();
            if (midiOutGetDevCapsW((UIntPtr)i, ref c, (uint)Marshal.SizeOf(typeof(MIDIOUTCAPS))) == 0 && c.szPname == name)
                return (int)i;
        }
        return -1;
    }

    public static IntPtr NewHeader()
    {
        IntPtr hdr = Marshal.AllocHGlobal((int)HeaderSize);
        MIDIHDR m = new MIDIHDR();
        m.lpData = Marshal.AllocHGlobal(BufferBytes);
        m.dwBufferLength = BufferBytes;
        Marshal.StructureToPtr(m, hdr, false);
        return hdr;
    }

    public static uint Flags(IntPtr hdr)
    {
        return ((MIDIHDR)Marshal.PtrToStructure(hdr, typeof(MIDIHDR))).dwFlags;
    }

    // Only once winmm has unprepared it. A header winmm may still hold is leaked instead.
    public static void FreeHeader(IntPtr hdr)
    {
        if (hdr == IntPtr.Zero || (Flags(hdr) & MHDR_PREPARED) != 0) return;
        Marshal.FreeHGlobal(((MIDIHDR)Marshal.PtrToStructure(hdr, typeof(MIDIHDR))).lpData);
        Marshal.FreeHGlobal(hdr);
    }

    public static uint InPrepare(IntPtr h, IntPtr hdr) { return midiInPrepareHeader(h, hdr, HeaderSize); }
    public static uint InUnprepare(IntPtr h, IntPtr hdr) { return midiInUnprepareHeader(h, hdr, HeaderSize); }
    public static uint InAdd(IntPtr h, IntPtr hdr) { return midiInAddBuffer(h, hdr, HeaderSize); }
    public static uint OutPrepare(IntPtr h, IntPtr hdr) { return midiOutPrepareHeader(h, hdr, HeaderSize); }
    public static uint OutUnprepare(IntPtr h, IntPtr hdr) { return midiOutUnprepareHeader(h, hdr, HeaderSize); }
}
'@

# The console prints a boxed table and the guid is unbraced, so anchor on the label. The endpoint
# ids printed above it contain the interface class guid, which a naive guid match would grab.
$reAssoc = [regex]'Association\s*Id[^0-9a-fA-F]*([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})'

$MMSYSERR_NODRIVER = 6
$MIDIERR_STILLPLAYING = 65

$suffix = '{0:X4}' -f (Get-Random -Maximum 65536)
$portName = "KIRUnprepare $suffix A"
$bits = 8 * [IntPtr]::Size

$assoc = $null
$hIn = [IntPtr]::Zero
$hOut = [IntPtr]::Zero
$inClosed = $false
$outClosed = $false
$failures = 0
$verdict = 'INCONCLUSIVE'
$reason = ''

function Test-Result([string] $what, [uint32] $actual, [uint32] $expected) {
    if ($actual -eq $expected) {
        Write-Host ("    {0,-58} {1,3}" -f $what, $actual) -ForegroundColor Green
    }
    else {
        Write-Host ("    {0,-58} {1,3}  expected {2}" -f $what, $actual, $expected) -ForegroundColor Red
        $script:failures++
    }
}

function Test-Unprepared([string] $what, [IntPtr] $hdr) {
    Test-Result "$what, MHDR_PREPARED" ([UnprepareProbe]::Flags($hdr) -band [UnprepareProbe]::MHDR_PREPARED) 0
}

function Wait-Until([scriptblock] $condition, [int] $timeoutMs) {
    $clock = [Diagnostics.Stopwatch]::StartNew()
    while ($clock.ElapsedMilliseconds -lt $timeoutMs) {
        if (& $condition) { return $true }
        Start-Sleep -Milliseconds 100
    }
    return $false
}

try {
    $out = & $MidiExe loopback create --name-a $portName --name-b "KIRUnprepare $suffix B" --unique-identifier "KIRUnprep$suffix" 2>&1 | Out-String
    $m = $reAssoc.Match($out)
    if (-not $m.Success) { throw "could not create the loopback: $out" }
    $assoc = '{' + $m.Groups[1].Value + '}'

    if (-not (Wait-Until { [UnprepareProbe]::FindIn($portName) -ge 0 -and [UnprepareProbe]::FindOut($portName) -ge 0 } 15000)) {
        throw "the '$portName' ports never showed up in WinMM"
    }

    $r = [UnprepareProbe]::midiInOpen([ref]$hIn, [uint32][UnprepareProbe]::FindIn($portName), [IntPtr]::Zero, [IntPtr]::Zero, 0)
    if ($r -ne 0) { throw "midiInOpen returned $r" }
    $r = [UnprepareProbe]::midiOutOpen([ref]$hOut, [uint32][UnprepareProbe]::FindOut($portName), [IntPtr]::Zero, [IntPtr]::Zero, 0)
    if ($r -ne 0) { throw "midiOutOpen returned $r" }

    Write-Host "$bits-bit process. '$portName', with the loopback present:"

    $control = [UnprepareProbe]::NewHeader()
    Test-Result 'midiInPrepareHeader' ([UnprepareProbe]::InPrepare($hIn, $control)) 0
    Test-Result 'midiInAddBuffer' ([UnprepareProbe]::InAdd($hIn, $control)) 0
    Test-Result 'midiInUnprepareHeader, buffer still queued' ([UnprepareProbe]::InUnprepare($hIn, $control)) $MIDIERR_STILLPLAYING
    Test-Result 'midiInReset' ([UnprepareProbe]::midiInReset($hIn)) 0
    Test-Result 'midiInUnprepareHeader' ([UnprepareProbe]::InUnprepare($hIn, $control)) 0
    Test-Unprepared 'midiInUnprepareHeader' $control
    [UnprepareProbe]::FreeHeader($control)

    $control = [UnprepareProbe]::NewHeader()
    Test-Result 'midiOutPrepareHeader' ([UnprepareProbe]::OutPrepare($hOut, $control)) 0
    Test-Result 'midiOutUnprepareHeader' ([UnprepareProbe]::OutUnprepare($hOut, $control)) 0
    Test-Unprepared 'midiOutUnprepareHeader' $control
    [UnprepareProbe]::FreeHeader($control)

    $controlFailures = $failures

    # What an app has when the device goes: buffers queued on the input, and a header kept prepared on the output
    $in1 = [UnprepareProbe]::NewHeader()
    $in2 = [UnprepareProbe]::NewHeader()
    $out1 = [UnprepareProbe]::NewHeader()
    foreach ($hdr in @($in1, $in2)) {
        if ([UnprepareProbe]::InPrepare($hIn, $hdr) -ne 0 -or [UnprepareProbe]::InAdd($hIn, $hdr) -ne 0) { throw 'could not queue an input buffer' }
    }
    if ([UnprepareProbe]::midiInStart($hIn) -ne 0) { throw 'midiInStart failed' }
    if ([UnprepareProbe]::OutPrepare($hOut, $out1) -ne 0) { throw 'could not prepare an output header' }

    & $MidiExe loopback remove --association-id $assoc 2>&1 | Out-Null
    $assoc = $null

    # A removed port answers MMSYSERR_NODRIVER to everything else
    if (-not (Wait-Until { [UnprepareProbe]::midiOutShortMsg($hOut, 0xFE) -eq $MMSYSERR_NODRIVER -and [UnprepareProbe]::midiInStart($hIn) -eq $MMSYSERR_NODRIVER } 10000)) {
        throw 'the open handles were never marked removed'
    }

    Write-Host ""
    Write-Host "After the loopback was removed, closing the way RtMidi does:"

    Test-Result 'midiInUnprepareHeader, buffer still queued' ([UnprepareProbe]::InUnprepare($hIn, $in1)) $MIDIERR_STILLPLAYING
    Test-Result 'midiInReset' ([UnprepareProbe]::midiInReset($hIn)) 0
    Test-Result 'midiInStop' ([UnprepareProbe]::midiInStop($hIn)) 0
    Test-Result 'midiInUnprepareHeader, first buffer' ([UnprepareProbe]::InUnprepare($hIn, $in1)) 0
    Test-Unprepared 'midiInUnprepareHeader, first buffer' $in1
    Test-Result 'midiInUnprepareHeader, second buffer' ([UnprepareProbe]::InUnprepare($hIn, $in2)) 0
    Test-Unprepared 'midiInUnprepareHeader, second buffer' $in2
    Test-Result 'midiInClose' ([UnprepareProbe]::midiInClose($hIn)) 0
    $inClosed = $true

    Test-Result 'midiOutUnprepareHeader' ([UnprepareProbe]::OutUnprepare($hOut, $out1)) 0
    Test-Unprepared 'midiOutUnprepareHeader' $out1
    Test-Result 'midiOutClose' ([UnprepareProbe]::midiOutClose($hOut)) 0
    $outClosed = $true

    foreach ($hdr in @($in1, $in2, $out1)) { [UnprepareProbe]::FreeHeader($hdr) }

    if ($controlFailures -gt 0) {
        $reason = 'the control pass failed while the loopback was present, so the results after removal mean nothing.'
    }
    elseif ($failures -eq 0) {
        $verdict = 'PASS'
        $reason = 'buffers unprepare after the device is removed, and queued buffers are still refused.'
    }
    else {
        $verdict = 'FAIL'
        $reason = "$failures result(s) after the removal were wrong."
    }
}
catch {
    $reason = $_.Exception.Message
}
finally {
    if ($hIn -ne [IntPtr]::Zero -and -not $inClosed) { [void][UnprepareProbe]::midiInReset($hIn); [void][UnprepareProbe]::midiInClose($hIn) }
    if ($hOut -ne [IntPtr]::Zero -and -not $outClosed) { [void][UnprepareProbe]::midiOutClose($hOut) }
    if ($assoc) { & $MidiExe loopback remove --association-id $assoc 2>&1 | Out-Null }
}

Write-Host ""
switch ($verdict) {
    'PASS'  { Write-Host "PASS - $reason" -ForegroundColor Green; exit 0 }
    'FAIL'  { Write-Host "FAIL - $reason" -ForegroundColor Red; exit 1 }
    default { Write-Host "INCONCLUSIVE - $reason" -ForegroundColor Yellow; exit 2 }
}
