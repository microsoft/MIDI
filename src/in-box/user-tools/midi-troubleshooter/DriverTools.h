// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace miditroubleshooter
{
    enum class DeviceDriverKind
    {
        Unknown = 0,

        // usbmidi2.inf, the combined MIDI 1.0 and MIDI 2.0 class driver
        UniversalMidiPacket,

        // wdma_usb.inf, the older USB Audio class driver
        ClassicUsbAudio,

        // usbaudio2.inf, the in-box USB Audio 2.0 driver. Recognized so that an audio only
        // device does not read as broken, but never offered as a choice: it carries no MIDI.
        UsbAudio2,

        // anything a manufacturer supplied
        Vendor
    };

    struct DeviceDriverChoice
    {
        // full path of the INF in the driver store
        std::wstring InfPath{};
        std::wstring InfFileName{};
        std::wstring Description{};
        std::wstring Manufacturer{};
        std::wstring Version{};

        DeviceDriverKind Kind{ DeviceDriverKind::Unknown };
    };

    struct HardwareDeviceInfo
    {
        std::wstring InstanceId{};
        std::wstring Name{};
        std::wstring Manufacturer{};
        std::wstring DriverInfName{};
        std::wstring DriverVersion{};
        std::wstring DriverProvider{};
        std::wstring ServiceName{};

        DeviceDriverKind CurrentDriver{ DeviceDriverKind::Unknown };

        bool HasProblem{ false };
        uint32_t ProblemCode{ 0 };

        // what this device could be switched to, resolved lazily by the page
        bool CanUseUniversalMidiPacketDriver{ false };
        bool CanUseClassicDriver{ false };
    };

    enum class KorgDriverKind
    {
        // korgum64.drv and friends, superseded by the in-box USB class driver
        UsbMidi = 0,

        // korgbm64.drv, superseded by in-box Bluetooth LE MIDI
        BleMidi
    };

    struct DriverPackageInfo
    {
        // oemNN.inf as published in %windir%\INF, which is what pnputil takes
        std::wstring PublishedName{};
        std::wstring OriginalName{};
        std::wstring Provider{};
        std::wstring ClassName{};
        std::wstring DisplayName{};

        KorgDriverKind Kind{ KorgDriverKind::UsbMidi };
    };

    struct DriverOperationResult
    {
        bool Succeeded{ false };
        bool RebootRequired{ false };
        std::wstring Message{};
        std::vector<std::wstring> Details{};
    };

    // Blocking. All of these run from a background thread.

    // USB devices in the audio and MIDI classes, which is the set the two class drivers
    // compete for, plus anything currently bound to a vendor MIDI driver.
    std::vector<HardwareDeviceInfo> EnumerateMidiHardwareDevices() noexcept;

    std::vector<DeviceDriverChoice> GetDriverChoicesForDevice(std::wstring const& instanceId) noexcept;

    DriverOperationResult SetDeviceDriver(std::wstring const& instanceId, DeviceDriverKind kind) noexcept;

    // Third-party driver packages whose provider looks like KORG, split by which in-box feature
    // now replaces them. Listed before anything is removed, so the customer can see exactly what
    // would go. A KORG package that matches neither is left out rather than guessed at.
    std::vector<DriverPackageInfo> FindKorgDriverPackages(KorgDriverKind kind) noexcept;

    DriverOperationResult RemoveDriverPackages(std::vector<DriverPackageInfo> const& packages) noexcept;
}
