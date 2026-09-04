# Times the SetupDi walk that CMidiPorts::RefreshPortsForFlow performs while holding m_Lock.
# Pure client-side PnP cost - no service involvement - so this IS the lock hold duration.
#
# Usage:  powershell -File midi-walk-cost.ps1 [-Iterations 20]

param(
    [int] $Iterations = 20,
    [int] $MonitorSeconds = 0
)

$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class WalkCost
{
    const uint DIGCF_PRESENT = 0x02, DIGCF_DEVICEINTERFACE = 0x10;

    [StructLayout(LayoutKind.Sequential)]
    struct SP_DEVINFO_DATA { public uint cbSize; public Guid ClassGuid; public uint DevInst; public IntPtr Reserved; }

    [StructLayout(LayoutKind.Sequential)]
    struct SP_DEVICE_INTERFACE_DATA { public uint cbSize; public Guid InterfaceClassGuid; public uint Flags; public IntPtr Reserved; }

    [StructLayout(LayoutKind.Sequential)]
    struct DEVPROPKEY { public Guid fmtid; public uint pid; }

    [DllImport("setupapi.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern IntPtr SetupDiGetClassDevsW(ref Guid cls, IntPtr enumerator, IntPtr hwnd, uint flags);
    [DllImport("setupapi.dll", SetLastError = true)]
    static extern bool SetupDiEnumDeviceInfo(IntPtr h, uint i, ref SP_DEVINFO_DATA d);
    [DllImport("setupapi.dll", SetLastError = true)]
    static extern bool SetupDiEnumDeviceInterfaces(IntPtr h, ref SP_DEVINFO_DATA d, ref Guid cls, uint i, ref SP_DEVICE_INTERFACE_DATA id);
    [DllImport("setupapi.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern bool SetupDiGetDeviceInterfaceDetailW(IntPtr h, ref SP_DEVICE_INTERFACE_DATA id, IntPtr detail, uint size, out uint req, IntPtr info);
    [DllImport("setupapi.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern bool SetupDiGetDeviceInterfacePropertyW(IntPtr h, ref SP_DEVICE_INTERFACE_DATA id, ref DEVPROPKEY key, out uint type, byte[] buf, uint size, out uint req, uint flags);
    [DllImport("setupapi.dll")]
    static extern bool SetupDiDestroyDeviceInfoList(IntPtr h);

    [DllImport("kernel32.dll")] static extern bool QueryPerformanceCounter(out long v);
    [DllImport("kernel32.dll")] static extern bool QueryPerformanceFrequency(out long v);

    static Guid MidiOut = new Guid("6dc23320-ab33-4ce4-80d4-bbb3ebbf2814");
    static Guid MidiIn  = new Guid("504be32c-ccf6-4d2c-b73f-6f8b3747e22b");
    static Guid MidiPkey = new Guid("3f114a6a-11fa-4bd0-9d2c-6b7780cd80ad");
    static Guid IfacePkey = new Guid("026e516e-b814-414b-83cd-856d6fef4822");

    public static int LastInterfaceCount;

    // Mirrors RefreshPortsForFlow: class enumeration, interface detail, then the same four
    // per-interface property reads.
    public static double WalkMs(bool output)
    {
        Guid cls = output ? MidiOut : MidiIn;
        long freq; QueryPerformanceFrequency(out freq);
        long a; QueryPerformanceCounter(out a);

        int interfaces = 0;
        IntPtr h = SetupDiGetClassDevsW(ref cls, IntPtr.Zero, IntPtr.Zero, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);

        if (h != IntPtr.Zero && h != (IntPtr)(-1))
        {
            var dev = new SP_DEVINFO_DATA(); dev.cbSize = (uint)Marshal.SizeOf(typeof(SP_DEVINFO_DATA));
            var buf = new byte[2048];

            for (uint di = 0; SetupDiEnumDeviceInfo(h, di, ref dev); di++)
            {
                var iface = new SP_DEVICE_INTERFACE_DATA();
                iface.cbSize = (uint)Marshal.SizeOf(typeof(SP_DEVICE_INTERFACE_DATA));

                for (uint ii = 0; SetupDiEnumDeviceInterfaces(h, ref dev, ref cls, ii, ref iface); ii++)
                {
                    interfaces++;

                    uint req;
                    SetupDiGetDeviceInterfaceDetailW(h, ref iface, IntPtr.Zero, 0, out req, IntPtr.Zero);
                    IntPtr detail = Marshal.AllocHGlobal((int)Math.Max(req, 8));
                    Marshal.WriteInt32(detail, IntPtr.Size == 8 ? 8 : 6);
                    SetupDiGetDeviceInterfaceDetailW(h, ref iface, detail, req, out req, IntPtr.Zero);
                    Marshal.FreeHGlobal(detail);

                    uint t, r;
                    var k = new DEVPROPKEY();

                    k.fmtid = MidiPkey;  k.pid = 17;   // PKEY_MIDI_ServiceAssignedPortNumber
                    SetupDiGetDeviceInterfacePropertyW(h, ref iface, ref k, out t, buf, 4, out r, 0);

                    k.fmtid = IfacePkey; k.pid = 2;    // DEVPKEY_DeviceInterface_FriendlyName
                    SetupDiGetDeviceInterfacePropertyW(h, ref iface, ref k, out t, buf, 64, out r, 0);

                    k.fmtid = MidiPkey;  k.pid = 19;   // PKEY_MIDI_DriverDeviceInterface
                    SetupDiGetDeviceInterfacePropertyW(h, ref iface, ref k, out t, buf, 520, out r, 0);

                    k.fmtid = MidiPkey;  k.pid = 56;   // PKEY_MIDI_KsComponentId
                    SetupDiGetDeviceInterfacePropertyW(h, ref iface, ref k, out t, buf, 64, out r, 0);
                }
            }

            SetupDiDestroyDeviceInfoList(h);
        }

        long b; QueryPerformanceCounter(out b);
        LastInterfaceCount = interfaces;
        return (b - a) * 1000.0 / freq;
    }
}
'@

if ($MonitorSeconds -gt 0) {

    [WalkCost]::WalkMs($true) | Out-Null   # warm

    Write-Host "Walking DEVINTERFACE_MIDI_OUTPUT every ~20 ms for $MonitorSeconds seconds."
    Write-Host "PLUG AND UNPLUG a MIDI device a few times now.`n"

    $samples = New-Object System.Collections.ArrayList
    $deadline = (Get-Date).AddSeconds($MonitorSeconds)
    $nextTick = $MonitorSeconds

    while ((Get-Date) -lt $deadline) {
        $when = [DateTime]::Now
        $ms   = [WalkCost]::WalkMs($true)
        [void]$samples.Add([pscustomobject]@{ When = $when; Ms = $ms; Count = [WalkCost]::LastInterfaceCount })

        $left = [int]($deadline - (Get-Date)).TotalSeconds
        if ($left -le ($nextTick - 10)) { $nextTick = $left; Write-Host "  $left s remaining..." }

        Start-Sleep -Milliseconds 20
    }

    $all = $samples.Ms | Sort-Object
    $med = $all[[int]($all.Count/2)]

    # "Storm" = any walk within 1.5 s of an interface-count change.
    $changeTimes = @()
    for ($i = 1; $i -lt $samples.Count; $i++) {
        if ($samples[$i].Count -ne $samples[$i-1].Count) { $changeTimes += $samples[$i].When }
    }

    $storm = @($samples | Where-Object { $t = $_.When; ($changeTimes | Where-Object { [Math]::Abs(($t - $_).TotalSeconds) -le 1.5 }).Count -gt 0 })
    $quiet = @($samples | Where-Object { $t = $_.When; ($changeTimes | Where-Object { [Math]::Abs(($t - $_).TotalSeconds) -le 1.5 }).Count -eq 0 })

    function Stat($set, $label) {
        if ($set.Count -eq 0) { Write-Host ("  {0,-22} (none)" -f $label); return }
        $s = $set.Ms | Sort-Object
        Write-Host ("  {0,-22} n={1,-6} median={2,7:F3} ms   p95={3,7:F3} ms   max={4,8:F3} ms" -f `
            $label, $set.Count, $s[[int]($s.Count/2)], $s[[int]($s.Count*0.95)], $s[$s.Count-1])
    }

    Write-Host "`n================ RefreshPortsForFlow-equivalent walk cost ================"
    Stat $samples "all walks"
    Stat $quiet   "quiet"
    Stat $storm   "within 1.5s of change"

    Write-Host "`n  interface count changes:"
    if ($changeTimes.Count -eq 0) { Write-Host "    (none seen - was a device actually plugged?)" }
    for ($i = 1; $i -lt $samples.Count; $i++) {
        if ($samples[$i].Count -ne $samples[$i-1].Count) {
            Write-Host ("    {0}   {1} -> {2} interfaces   (this walk took {3:F3} ms)" -f `
                $samples[$i].When.ToString('HH:mm:ss.fff'), $samples[$i-1].Count, $samples[$i].Count, $samples[$i].Ms)
        }
    }

    Write-Host "`n  slowest 20 walks:"
    Write-Host "      wall clock       duration    interfaces   vs quiet median"
    $samples | Sort-Object Ms -Descending | Select-Object -First 20 | ForEach-Object {
        Write-Host ("    {0}   {1,8:F3} ms   {2,6}       {3,6:F1}x" -f `
            $_.When.ToString('HH:mm:ss.fff'), $_.Ms, $_.Count, ($_.Ms / $med))
    }
    Write-Host ""
    return
}

foreach ($flow in @($true, $false)) {
    $name = if ($flow) { 'MIDI OUT (DEVINTERFACE_MIDI_OUTPUT)' } else { 'MIDI IN  (DEVINTERFACE_MIDI_INPUT)' }

    [WalkCost]::WalkMs($flow) | Out-Null   # warm the caches, as a running driver would be
    $times = 1..$Iterations | ForEach-Object { [WalkCost]::WalkMs($flow) }
    $n = [WalkCost]::LastInterfaceCount
    $sorted = [double[]]$times; [Array]::Sort($sorted)

    Write-Host "`n=== $name ==="
    Write-Host ("  interfaces walked : {0}" -f $n)
    Write-Host ("  min               : {0:F3} ms" -f $sorted[0])
    Write-Host ("  median            : {0:F3} ms" -f $sorted[[int]($sorted.Length/2)])
    Write-Host ("  max               : {0:F3} ms" -f $sorted[$sorted.Length-1])
    if ($n -gt 0) {
        Write-Host ("  per interface     : {0:F3} ms  (median)" -f ($sorted[[int]($sorted.Length/2)] / $n))
    }
}
Write-Host ""
