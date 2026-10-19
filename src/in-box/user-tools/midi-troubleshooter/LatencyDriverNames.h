// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Driver file names the DPC latency check knows, so a trace from another PC, where none of the
// drivers can be looked at, still gets sensible names and advice. A device class from this PC's
// own devices takes over from these when there is one.

#include "LatencyModel.h"

#include <cwctype>
#include <string_view>

namespace miditroubleshooter::latency::names
{
    inline bool EqualsNoCase(_In_ std::wstring_view const left, _In_ std::wstring_view const right) noexcept
    {
        if (left.size() != right.size())
        {
            return false;
        }

        for (size_t index = 0; index < left.size(); ++index)
        {
            if (std::towlower(left[index]) != std::towlower(right[index]))
            {
                return false;
            }
        }

        return true;
    }

    inline bool StartsWithNoCase(_In_ std::wstring_view const text, _In_ std::wstring_view const prefix) noexcept
    {
        return text.size() >= prefix.size() && EqualsNoCase(text.substr(0, prefix.size()), prefix);
    }

    // Windows parts that run other drivers' code from their own DPCs and interrupts. Time in
    // one of these belongs to the driver it ran for, when that can be found.
    inline constexpr std::wstring_view FrameworkDrivers[]
    {
        L"Wdf01000.sys", L"ndis.sys", L"storport.sys", L"ataport.sys", L"scsiport.sys", L"dxgkrnl.sys",
        L"portcls.sys", L"hidclass.sys", L"usbport.sys", L"ucx01000.sys", L"acx01000.sys", L"msgpioclx.sys",
        L"SerCx.sys", L"SerCx2.sys", L"SpbCx.sys", L"ntoskrnl.exe", L"hal.dll", L"ks.sys",
    };

    // Windows parts that do work on behalf of a device driver, such as the network and file
    // system stacks. They're never charged in place of the device driver they worked for.
    inline constexpr std::wstring_view InfrastructureDrivers[]
    {
        L"tcpip.sys", L"netio.sys", L"afd.sys", L"wfplwfs.sys", L"fwpkclnt.sys", L"Ndu.sys", L"mslldp.sys",
        L"winnat.sys", L"nwifi.sys", L"vwififlt.sys", L"Ntfs.sys", L"ReFS.sys", L"fltmgr.sys", L"CLASSPNP.SYS",
        L"disk.sys", L"partmgr.sys", L"volmgr.sys", L"volsnap.sys", L"Wof.sys", L"cng.sys", L"ksecdd.sys",
        L"symcryptk.dll", L"msrpc.sys", L"dxgmms2.sys", L"dxgmms1.sys", L"drmk.sys", L"PEAuth.sys",
    };

    inline bool IsFramework(_In_ std::wstring_view const fileName) noexcept
    {
        for (auto const name : FrameworkDrivers)
        {
            if (EqualsNoCase(fileName, name))
            {
                return true;
            }
        }

        return false;
    }

    inline bool IsInfrastructure(_In_ std::wstring_view const fileName) noexcept
    {
        if (IsFramework(fileName))
        {
            return true;
        }

        for (auto const name : InfrastructureDrivers)
        {
            if (EqualsNoCase(fileName, name))
            {
                return true;
            }
        }

        return false;
    }

    struct KnownDriver
    {
        std::wstring_view Name;
        DriverKind Kind;

        // true when Name is the start of the file name, for vendors that number their drivers
        bool Prefix;
    };

    inline constexpr KnownDriver KnownDrivers[]
    {
        { L"ntoskrnl.exe", DriverKind::WindowsKernel, false },
        { L"hal.dll", DriverKind::WindowsKernel, false },
        { L"Wdf01000.sys", DriverKind::WindowsKernel, false },
        { L"symcryptk.dll", DriverKind::WindowsKernel, false },
        { L"cng.sys", DriverKind::WindowsKernel, false },
        { L"msrpc.sys", DriverKind::WindowsKernel, false },

        { L"winhv", DriverKind::Hypervisor, true },
        { L"vmswitch.sys", DriverKind::Hypervisor, false },
        { L"vmbus", DriverKind::Hypervisor, true },
        { L"vmbkmcl", DriverKind::Hypervisor, true },
        { L"storvsp.sys", DriverKind::Hypervisor, false },
        { L"vid.sys", DriverKind::Hypervisor, false },
        { L"hvservice.sys", DriverKind::Hypervisor, false },
        { L"hvsocket.sys", DriverKind::Hypervisor, false },
        { L"vhdmp.sys", DriverKind::Hypervisor, false },
        { L"vpcivsp.sys", DriverKind::Hypervisor, false },

        { L"ndis.sys", DriverKind::NetworkStack, false },
        { L"tcpip.sys", DriverKind::NetworkStack, false },
        { L"netio.sys", DriverKind::NetworkStack, false },
        { L"afd.sys", DriverKind::NetworkStack, false },
        { L"wfplwfs.sys", DriverKind::NetworkStack, false },
        { L"fwpkclnt.sys", DriverKind::NetworkStack, false },
        { L"Ndu.sys", DriverKind::NetworkStack, false },
        { L"mslldp.sys", DriverKind::NetworkStack, false },
        { L"winnat.sys", DriverKind::NetworkStack, false },
        { L"http.sys", DriverKind::NetworkStack, false },

        { L"nwifi.sys", DriverKind::WiFi, false },
        { L"vwififlt.sys", DriverKind::WiFi, false },
        { L"Netw", DriverKind::WiFi, true },
        { L"rtwlan", DriverKind::WiFi, true },
        { L"mrvlpcie", DriverKind::WiFi, true },
        { L"athw", DriverKind::WiFi, true },
        { L"qcwlan", DriverKind::WiFi, true },
        { L"mtkwl", DriverKind::WiFi, true },
        { L"bcmwl", DriverKind::WiFi, true },

        { L"rt640x64.sys", DriverKind::Ethernet, false },
        { L"rt68cx", DriverKind::Ethernet, true },
        { L"rtcx", DriverKind::Ethernet, true },
        { L"e1d", DriverKind::Ethernet, true },
        { L"e1i", DriverKind::Ethernet, true },
        { L"e1r", DriverKind::Ethernet, true },
        { L"e2f", DriverKind::Ethernet, true },
        { L"b57nd", DriverKind::Ethernet, true },
        { L"aqnic", DriverKind::Ethernet, true },
        { L"rtux64w10.sys", DriverKind::Ethernet, false },

        { L"bthport.sys", DriverKind::Bluetooth, false },
        { L"BTHUSB.sys", DriverKind::Bluetooth, false },
        { L"bthenum.sys", DriverKind::Bluetooth, false },
        { L"BthLEEnum.sys", DriverKind::Bluetooth, false },
        { L"rfcomm.sys", DriverKind::Bluetooth, false },
        { L"ibtusb.sys", DriverKind::Bluetooth, false },
        { L"qcbtuart", DriverKind::Bluetooth, true },

        { L"dxgkrnl.sys", DriverKind::Graphics, false },
        { L"dxgmms2.sys", DriverKind::Graphics, false },
        { L"dxgmms1.sys", DriverKind::Graphics, false },
        { L"nvlddmkm.sys", DriverKind::Graphics, false },
        { L"amdkmdag.sys", DriverKind::Graphics, false },
        { L"igdkmd", DriverKind::Graphics, true },
        { L"qcdx", DriverKind::Graphics, true },
        { L"BasicDisplay.sys", DriverKind::Graphics, false },
        { L"BasicRender.sys", DriverKind::Graphics, false },

        { L"storport.sys", DriverKind::Storage, false },
        { L"stornvme.sys", DriverKind::Storage, false },
        { L"storahci.sys", DriverKind::Storage, false },
        { L"iaStor", DriverKind::Storage, true },
        { L"ataport.sys", DriverKind::Storage, false },
        { L"scsiport.sys", DriverKind::Storage, false },
        { L"CLASSPNP.SYS", DriverKind::Storage, false },
        { L"disk.sys", DriverKind::Storage, false },
        { L"Ntfs.sys", DriverKind::Storage, false },
        { L"ReFS.sys", DriverKind::Storage, false },
        { L"fltmgr.sys", DriverKind::Storage, false },
        { L"volsnap.sys", DriverKind::Storage, false },
        { L"USBSTOR.SYS", DriverKind::Storage, false },
        { L"UASPStor.sys", DriverKind::Storage, false },
        { L"sdstor.sys", DriverKind::Storage, false },
        { L"sdbus.sys", DriverKind::Storage, false },
        { L"secnvme.sys", DriverKind::Storage, false },

        { L"USBXHCI.SYS", DriverKind::UsbController, false },
        { L"USBPORT.SYS", DriverKind::UsbController, false },
        { L"usbehci.sys", DriverKind::UsbController, false },
        { L"usbohci.sys", DriverKind::UsbController, false },
        { L"usbuhci.sys", DriverKind::UsbController, false },
        { L"UCX01000.SYS", DriverKind::UsbController, false },
        { L"USBHUB3.SYS", DriverKind::UsbController, false },
        { L"usbhub.sys", DriverKind::UsbController, false },
        { L"usbccgp.sys", DriverKind::UsbController, false },
        { L"asmtxhci.sys", DriverKind::UsbController, false },
        { L"amdxhc", DriverKind::UsbController, true },
        { L"iusb3", DriverKind::UsbController, true },

        { L"HDAudBus.sys", DriverKind::Audio, false },
        { L"HdAudio.sys", DriverKind::Audio, false },
        { L"portcls.sys", DriverKind::Audio, false },
        { L"ks.sys", DriverKind::Audio, false },
        { L"ksthunk.sys", DriverKind::Audio, false },
        { L"usbaudio.sys", DriverKind::Audio, false },
        { L"usbaudio2.sys", DriverKind::Audio, false },
        { L"USBMidi2.sys", DriverKind::Audio, false },
        { L"acx01000.sys", DriverKind::Audio, false },
        { L"AcxHdAudio.sys", DriverKind::Audio, false },
        { L"RTKVHD64.sys", DriverKind::Audio, false },
        { L"IntcAudioBus.sys", DriverKind::Audio, false },
        { L"IntcDAud.sys", DriverKind::Audio, false },

        { L"ACPI.sys", DriverKind::Power, false },
        { L"intelppm.sys", DriverKind::Power, false },
        { L"amdppm.sys", DriverKind::Power, false },
        { L"processr.sys", DriverKind::Power, false },
        { L"intelpep.sys", DriverKind::Power, false },
        { L"pep", DriverKind::Power, true },

        { L"WdFilter.sys", DriverKind::Security, false },
        { L"WdNisDrv.sys", DriverKind::Security, false },
        { L"WdBoot.sys", DriverKind::Security, false },
        { L"PEAuth.sys", DriverKind::Security, false },
        { L"ksecdd.sys", DriverKind::Security, false },

        { L"hidclass.sys", DriverKind::Input, false },
        { L"hidusb.sys", DriverKind::Input, false },
        { L"hidi2c.sys", DriverKind::Input, false },
        { L"mouclass.sys", DriverKind::Input, false },
        { L"kbdclass.sys", DriverKind::Input, false },
        { L"mouhid.sys", DriverKind::Input, false },
        { L"kbdhid.sys", DriverKind::Input, false },
        { L"i8042prt.sys", DriverKind::Input, false },
        { L"SynTP.sys", DriverKind::Input, false },
        { L"iaLPSS2", DriverKind::Input, true },
        { L"SpbCx.sys", DriverKind::Input, false },
        { L"msgpioclx.sys", DriverKind::Input, false },
    };

    inline DriverKind KindFromFileName(_In_ std::wstring_view const fileName) noexcept
    {
        for (auto const& known : KnownDrivers)
        {
            if (known.Prefix ? StartsWithNoCase(fileName, known.Name) : EqualsNoCase(fileName, known.Name))
            {
                return known.Kind;
            }
        }

        return DriverKind::Unknown;
    }

    // A stable name for each kind, used in the technical report.
    inline std::wstring_view KindName(_In_ DriverKind const kind) noexcept
    {
        switch (kind)
        {
        case DriverKind::WindowsKernel: return L"windows_kernel";
        case DriverKind::Hypervisor: return L"virtualization";
        case DriverKind::NetworkStack: return L"network_stack";
        case DriverKind::WiFi: return L"wifi";
        case DriverKind::Ethernet: return L"ethernet";
        case DriverKind::Bluetooth: return L"bluetooth";
        case DriverKind::Graphics: return L"graphics";
        case DriverKind::Storage: return L"storage";
        case DriverKind::UsbController: return L"usb_controller";
        case DriverKind::Audio: return L"audio";
        case DriverKind::Power: return L"power";
        case DriverKind::Security: return L"security";
        case DriverKind::Input: return L"input";
        case DriverKind::Other: return L"other";
        case DriverKind::Unknown:
        default: return L"unknown";
        }
    }
}
