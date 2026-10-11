# Regression test for Feature_Servicing_MIDI2WinMMRemovalWithoutPortLock (issue #1267).
#
# CMidiPort::NotifyInterfaceRemoval took the port lock, and SendLongMessage holds that lock for a
# whole midiOutLongMsg. Every interface removal calls NotifyInterfaceRemoval on every open port, so
# one long SysEx send made each removal wait until the send finished. Removals queued up behind it
# and were handled after the device had come back with the same interface ids, where they marked a
# handle opened on the live port as removed. Every call on that handle then returned
# MMSYSERR_NODRIVER (6) until the app closed it.
#
# Three loopback pairs stand in for the device, so no hardware and no elevation are needed:
#   hold    a long SysEx goes out on its A side and keeps that port busy for a few seconds
#   decoy   removed first, so a removal is waiting behind the long send
#   target  removed and created again with the same unique identifier, so its ports come back
#           with the same interface ids. A new handle is opened on it while the send is running.
#
# PASS: the new handle still works after the long send ends, and a handle left open on the decoy,
# which really is gone, still returns MMSYSERR_NODRIVER. FAIL: either one is wrong.
# The loopback carries about 100 MB/s, so the default 512 MB send lasts about 5 seconds. If the
# send ends before the target is back, the result is INCONCLUSIVE; raise -HoldMB.
#
# Usage:  powershell -File midi-removal-during-long-send-test.ps1 [-HoldMB 512]
# 32-bit: %windir%\SysWOW64\WindowsPowerShell\v1.0\powershell.exe -File midi-removal-during-long-send-test.ps1

param(
    [double] $HoldMB  = 512,
    [string] $MidiExe = 'midi'
)

$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @'
using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;

public static class LongSendRemovalProbe
{
    [DllImport("winmm.dll")] public static extern uint midiOutGetNumDevs();
    [DllImport("winmm.dll", CharSet = CharSet.Unicode)]
    public static extern uint midiOutGetDevCapsW(UIntPtr id, ref MIDIOUTCAPS caps, uint cbSize);
    [DllImport("winmm.dll")]
    public static extern uint midiOutOpen(out IntPtr h, uint id, IntPtr cb, IntPtr inst, uint flags);
    [DllImport("winmm.dll")] public static extern uint midiOutShortMsg(IntPtr h, uint msg);
    [DllImport("winmm.dll")] public static extern uint midiOutClose(IntPtr h);
    [DllImport("winmm.dll")] public static extern uint midiOutPrepareHeader(IntPtr h, IntPtr hdr, uint cbHdr);
    [DllImport("winmm.dll")] public static extern uint midiOutUnprepareHeader(IntPtr h, IntPtr hdr, uint cbHdr);
    [DllImport("winmm.dll")] public static extern uint midiOutLongMsg(IntPtr h, IntPtr hdr, uint cbHdr);

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

    const uint MIDIERR_STILLPLAYING = 65;

    public class SendResult
    {
        public uint Prepare = 999, Send = 999, Unprepare = 999;
        public double Ms;
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

    public static IntPtr MakeSysEx(int length)
    {
        IntPtr data = Marshal.AllocHGlobal(length);
        byte[] chunk = new byte[1 << 20];
        for (int i = 0; i < chunk.Length; i++) chunk[i] = (byte)(i & 0x7F);
        for (long offset = 0; offset < length; offset += chunk.Length)
        {
            Marshal.Copy(chunk, 0, new IntPtr(data.ToInt64() + offset), (int)Math.Min(chunk.Length, length - offset));
        }
        Marshal.WriteByte(data, 0, 0xF0);
        Marshal.WriteByte(data, length - 1, 0xF7);
        return data;
    }

    // Like rawloop from the issue: prepare, send, then unprepare while the buffer is still playing.
    public static SendResult SendSysEx(IntPtr h, IntPtr data, int length)
    {
        SendResult r = new SendResult();
        int size = Marshal.SizeOf(typeof(MIDIHDR));
        IntPtr hdr = Marshal.AllocHGlobal(size);
        MIDIHDR m = new MIDIHDR();
        m.lpData = data;
        m.dwBufferLength = (uint)length;
        Marshal.StructureToPtr(m, hdr, false);

        Stopwatch sw = Stopwatch.StartNew();
        r.Prepare = midiOutPrepareHeader(h, hdr, (uint)size);
        if (r.Prepare == 0)
        {
            r.Send = midiOutLongMsg(h, hdr, (uint)size);
            for (int i = 0; i < 400; i++)
            {
                r.Unprepare = midiOutUnprepareHeader(h, hdr, (uint)size);
                if (r.Unprepare != MIDIERR_STILLPLAYING) break;
                Thread.Sleep(5);
            }
        }
        r.Ms = sw.Elapsed.TotalMilliseconds;

        // A header winmm still holds is leaked rather than freed under it.
        if (r.Prepare != 0 || r.Unprepare == 0) Marshal.FreeHGlobal(hdr);
        return r;
    }

    static Thread _hold;
    public static SendResult HoldResult;

    public static bool HoldRunning { get { return _hold != null && _hold.IsAlive; } }

    public static void StartHold(IntPtr h, IntPtr data, int length)
    {
        HoldResult = null;
        _hold = new Thread(delegate () { HoldResult = SendSysEx(h, data, length); });
        _hold.IsBackground = true;
        _hold.Start();
    }

    public static void WaitHold()
    {
        if (_hold != null) _hold.Join();
    }
}
'@

# The console prints a boxed table and the guid is unbraced, so anchor on the label. The endpoint
# ids printed above it contain the interface class guid, which a naive guid match would grab.
$reAssoc = [regex]'Association\s*Id[^0-9a-fA-F]*([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})'

function New-TestLoopback([string] $nameA, [string] $nameB, [string] $uniqueId) {
    $out = & $MidiExe loopback create --name-a $nameA --name-b $nameB --unique-identifier $uniqueId 2>&1 | Out-String
    $m = $reAssoc.Match($out)
    if (-not $m.Success) { throw "could not create loopback '$nameA': $out" }
    return '{' + $m.Groups[1].Value + '}'
}

function Remove-TestLoopback([string] $assoc) {
    if ($assoc) { & $MidiExe loopback remove --association-id $assoc 2>&1 | Out-Null }
}

function Wait-ForPort([string] $name, [int] $timeoutMs) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt $timeoutMs) {
        $index = [LongSendRemovalProbe]::FindOut($name)
        if ($index -ge 0) { return $index }
        Start-Sleep -Milliseconds 100
    }
    return -1
}

$holdName   = 'KIRLongSend Hold A'
$decoyName  = 'KIRLongSend Decoy A'
$targetName = 'KIRLongSend Target A'
$holdBytes  = [int]($HoldMB * 1MB)
$bits       = 8 * [IntPtr]::Size

$hold = $null; $decoy = $null; $target = $null
$holdHandle = [IntPtr]::Zero; $decoyHandle = [IntPtr]::Zero; $targetHandle = [IntPtr]::Zero; $data = [IntPtr]::Zero
$verdict = 'INCONCLUSIVE'; $reason = ''

try {
    try { $data = [LongSendRemovalProbe]::MakeSysEx($holdBytes) }
    catch { throw "could not allocate a $HoldMB MB SysEx buffer in this $bits-bit process. Try a smaller -HoldMB." }

    $hold   = New-TestLoopback 'KIRLongSend Hold A' 'KIRLongSend Hold B' 'KIRLongSendHold'
    $decoy  = New-TestLoopback $decoyName 'KIRLongSend Decoy B' 'KIRLongSendDecoy'
    $target = New-TestLoopback $targetName 'KIRLongSend Target B' 'KIRLongSendTarget'

    $holdIndex = Wait-ForPort $holdName 15000
    $decoyIndex = Wait-ForPort $decoyName 15000
    if ($holdIndex -lt 0 -or $decoyIndex -lt 0 -or (Wait-ForPort $targetName 15000) -lt 0) { throw 'the test loopback ports never showed up in WinMM' }

    $r = [LongSendRemovalProbe]::midiOutOpen([ref]$holdHandle, [uint32]$holdIndex, [IntPtr]::Zero, [IntPtr]::Zero, 0)
    if ($r -ne 0) { throw "midiOutOpen on '$holdName' returned $r" }
    $r = [LongSendRemovalProbe]::midiOutOpen([ref]$decoyHandle, [uint32]$decoyIndex, [IntPtr]::Zero, [IntPtr]::Zero, 0)
    if ($r -ne 0) { throw "midiOutOpen on '$decoyName' returned $r" }

    Write-Host "$bits-bit process. Sending $HoldMB MB of SysEx to '$holdName' on another thread."
    $clock = [Diagnostics.Stopwatch]::StartNew()
    [LongSendRemovalProbe]::StartHold($holdHandle, $data, $holdBytes)
    Start-Sleep -Milliseconds 200

    Remove-TestLoopback $decoy; $decoy = $null
    Write-Host ("  {0,6} ms  removed the decoy loopback" -f $clock.ElapsedMilliseconds)
    Remove-TestLoopback $target; $target = $null
    Write-Host ("  {0,6} ms  removed the target loopback" -f $clock.ElapsedMilliseconds)
    $target = New-TestLoopback $targetName 'KIRLongSend Target B' 'KIRLongSendTarget'
    Write-Host ("  {0,6} ms  created the target loopback again, same unique identifier" -f $clock.ElapsedMilliseconds)
    Start-Sleep -Milliseconds 500

    $targetIndex = Wait-ForPort $targetName 3000
    $holdWasRunning = [LongSendRemovalProbe]::HoldRunning
    $openResult = if ($targetIndex -ge 0) { [LongSendRemovalProbe]::midiOutOpen([ref]$targetHandle, [uint32]$targetIndex, [IntPtr]::Zero, [IntPtr]::Zero, 0) } else { 2 }
    $before = if ($openResult -eq 0) { [LongSendRemovalProbe]::midiOutShortMsg($targetHandle, 0xFE) } else { 999 }
    Write-Host ("  {0,6} ms  new handle on '{1}' (port {2}): midiOutOpen={3} midiOutShortMsg={4}, long send still running: {5}" -f $clock.ElapsedMilliseconds, $targetName, $targetIndex, $openResult, $before, $holdWasRunning)

    [LongSendRemovalProbe]::WaitHold()
    $holdResult = [LongSendRemovalProbe]::HoldResult
    Write-Host ("  {0,6} ms  long send finished: prepare={1} longmsg={2} unprepare={3} after {4:F0} ms" -f $clock.ElapsedMilliseconds, $holdResult.Prepare, $holdResult.Send, $holdResult.Unprepare, $holdResult.Ms)

    # Removals that queued behind the send are handled as soon as it ends.
    Start-Sleep -Milliseconds 1500

    $after = 999; $prepareAfter = 999
    if ($openResult -eq 0) {
        $after = [LongSendRemovalProbe]::midiOutShortMsg($targetHandle, 0xFE)
        $small = [LongSendRemovalProbe]::MakeSysEx(64)
        $prepareAfter = ([LongSendRemovalProbe]::SendSysEx($targetHandle, $small, 64)).Prepare
    }
    Write-Host ("  {0,6} ms  same handle again: midiOutShortMsg={1} midiOutPrepareHeader={2}" -f $clock.ElapsedMilliseconds, $after, $prepareAfter)

    $decoyAfter = [LongSendRemovalProbe]::midiOutShortMsg($decoyHandle, 0xFE)
    Write-Host ("  {0,6} ms  handle left open on the removed decoy: midiOutShortMsg={1}" -f $clock.ElapsedMilliseconds, $decoyAfter)

    if ($openResult -ne 0 -or $before -ne 0) {
        $reason = "the new handle did not work even before the long send ended (open $openResult, send $before)."
    }
    elseif (-not $holdWasRunning) {
        $reason = "the long send ended before the new handle was opened, so nothing was waiting behind it. Raise -HoldMB."
    }
    elseif ($decoyAfter -ne 6) {
        $verdict = 'FAIL'
        $reason = "the handle on the removed decoy port was not marked removed (send $decoyAfter, expected 6)."
    }
    elseif ($after -eq 0 -and $prepareAfter -eq 0) {
        $verdict = 'PASS'
        $reason = 'the handle opened on the re-created port still works after the long send ended.'
    }
    elseif ($after -eq 6 -or $prepareAfter -eq 6) {
        $verdict = 'FAIL'
        $reason = 'a removal handled after the port came back marked the new handle as removed (MMSYSERR_NODRIVER).'
    }
    else {
        $reason = "unexpected results (send $after, prepare $prepareAfter)."
    }
}
finally {
    if ($targetHandle -ne [IntPtr]::Zero) { [LongSendRemovalProbe]::midiOutClose($targetHandle) | Out-Null }
    if ($decoyHandle -ne [IntPtr]::Zero) { [LongSendRemovalProbe]::midiOutClose($decoyHandle) | Out-Null }
    [LongSendRemovalProbe]::WaitHold()
    if ($holdHandle -ne [IntPtr]::Zero) { [LongSendRemovalProbe]::midiOutClose($holdHandle) | Out-Null }
    if ($data -ne [IntPtr]::Zero) { [Runtime.InteropServices.Marshal]::FreeHGlobal($data) }
    Remove-TestLoopback $decoy
    Remove-TestLoopback $target
    Remove-TestLoopback $hold
}

Write-Host ""
$color = switch ($verdict) { 'PASS' { 'Green' } 'FAIL' { 'Red' } default { 'Yellow' } }
Write-Host ("{0,-12} - {1}" -f $verdict, $reason) -ForegroundColor $color
Write-Host ""
