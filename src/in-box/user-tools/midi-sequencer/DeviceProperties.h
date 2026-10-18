// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Everything Windows MIDI Services says about one endpoint, for the device details panel, and the
// rules the engine uses for a device's offset and automation interval.
//
// Every call here asks the service, so none of it may run on the UI thread. Values that MIDI
// Settings writes are read from the endpoint's property bag rather than from the newest SDK
// members, so an older copy of the SDK in Windows can't be asked for something it doesn't have.

namespace midisequencer
{
    // Which way a group's messages go, from the sequencer's side: a destination is something the
    // sequencer plays to, a source is something it records from.
    enum class BlockDirection : uint8_t
    {
        Destination = 0,
        Source = 1,
        Both = 2,
    };

    enum class BlockProtocol : uint8_t
    {
        Unknown = 0,
        Midi1 = 1,

        // A MIDI 1.0 cable, which carries about a thousand controller messages a second.
        Midi1CableSpeed = 2,

        Midi2 = 3,
    };

    // A function block, or a group terminal block when the device declares no function blocks.
    struct DeviceBlock
    {
        std::wstring Name{};
        uint8_t FirstGroup{ 0 };
        uint8_t GroupCount{ 0 };
        BlockDirection Direction{ BlockDirection::Both };
        BlockProtocol Protocol{ BlockProtocol::Unknown };
        bool Active{ true };
    };

    // How early to send to a device, and whether that's the customer's own choice in MIDI
    // Settings or what its transport worked out.
    struct DeviceOffset
    {
        uint32_t Microseconds{ 0 };
        bool ChosenInMidiSettings{ false };
    };

    struct DeviceProperties
    {
        // False when the service doesn't know the endpoint, for example after it was unplugged.
        bool Found{ false };

        std::wstring Id{};
        std::wstring Name{};
        std::wstring Description{};

        // The customer's picture, or the transport's default when there isn't one.
        std::wstring PicturePath{};
        bool CustomPicture{ false };

        std::wstring TransportCode{};
        std::wstring TransportName{};
        std::wstring Manufacturer{};
        std::wstring ParentName{};
        std::wstring SerialNumber{};
        std::wstring ProductInstanceId{};
        uint16_t VendorId{ 0 };
        uint16_t ProductId{ 0 };

        // A MIDI 1.0 byte stream device, such as most USB MIDI 1.0 devices.
        bool ByteStream{ false };

        // The protocol the endpoint settled on with Windows MIDI Services.
        bool SpeaksMidi2{ false };

        bool DeclaresMidi1{ false };
        bool DeclaresMidi2{ false };
        uint8_t UmpVersionMajor{ 0 };
        uint8_t UmpVersionMinor{ 0 };

        // Function blocks when it declares any, otherwise its group terminal blocks.
        std::vector<DeviceBlock> Blocks{};
        bool FunctionBlocks{ false };
        bool StaticFunctionBlocks{ false };

        // The identity it answered with, when it did.
        bool HasIdentity{ false };
        std::array<uint8_t, 3> SystemExclusiveId{};
        uint16_t Family{ 0 };
        uint16_t Model{ 0 };
        std::array<uint8_t, 4> SoftwareRevision{};

        // Set in MIDI Settings, or by the transport for the offset.
        DeviceOffset Offset{};
        uint32_t AutomationIntervalMilliseconds{ 0 };
        bool SupportsMpe{ false };
        bool RequiresNoteOffTranslation{ false };
    };

    DeviceProperties ReadDeviceProperties(_In_ std::wstring const& endpointDeviceId) noexcept;

    // By the same rule and limit as the service's scheduler: the offset from MIDI Settings when
    // one is chosen there, otherwise the transport's.
    DeviceOffset ReadDeviceOffset(_In_ midi2enum::MidiEndpointDeviceInformation const& device) noexcept;

    // How often a ramp may be sent to the device, from MIDI Settings. 0 when it has none.
    uint32_t ReadAutomationInterval(_In_ midi2enum::MidiEndpointDeviceInformation const& device) noexcept;

    // The name Windows MIDI Services gives a transport, such as "Bluetooth MIDI 1.0", or the
    // code itself when the service doesn't say.
    std::wstring TransportDisplayName(_In_ std::wstring const& transportCode) noexcept;
}
