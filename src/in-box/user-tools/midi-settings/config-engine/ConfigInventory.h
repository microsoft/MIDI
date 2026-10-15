// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// UNSUPPORTED - DO NOT COPY THIS APPROACH. This code reads and writes the Windows MIDI Services
// configuration file directly. Microsoft does not support direct manipulation of that file by
// anything other than the in-box MIDI tools. Its format, its name, its location and the registry
// value that selects it are implementation details and can change in any release without notice,
// so this code can corrupt a configuration or stop working at any time. It exists for development
// and diagnostics inside the Windows MIDI Services repository only. Applications and third-party
// tools must use the Windows MIDI Services API (Windows.Devices.Midi2.ServiceConfig) instead.
// ============================================================================

#pragma once

#include "ConfigEngine.h"

namespace midisettings::configengine
{
    // The transport sections this engine understands, keyed the way the file keys them.
    namespace transportids
    {
        inline constexpr std::wstring_view KernelStreaming{ L"{26FA740D-469C-4D33-BEB1-3885DE7D6DF1}" };
        inline constexpr std::wstring_view KernelStreamingAggregate{ L"{0F273B18-E372-4D95-87AC-C31C3D22E937}" };
        inline constexpr std::wstring_view Loopback{ L"{942BF02D-93C0-4EA8-B03E-D51156CA75E1}" };
        inline constexpr std::wstring_view BasicLoopback{ L"{10088473-9478-4E62-850B-3D2315E135B8}" };
        inline constexpr std::wstring_view NetworkMidi2{ L"{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}" };
        inline constexpr std::wstring_view RtpMidi{ L"{54C9B2F6-C235-4000-A675-9F6958A1A4FA}" };
        inline constexpr std::wstring_view Bluetooth{ L"{5DC87270-F318-4838-A4F9-6AADC63E925F}" };
        inline constexpr std::wstring_view Synthesizer{ L"{7605713E-FEA9-409D-A90F-A81233200D0A}" };

        bool IsKnown(_In_ std::wstring_view sectionKey) noexcept;
    }

    // The parts a configuration is shown, shared and imported in.
    enum class ConfigPart : uint32_t
    {
        None = 0,
        DeviceNames = 0x01,
        Loopbacks = 0x02,
        NetworkMidi2 = 0x04,
        RtpMidi = 0x08,
        Bluetooth = 0x10,
        OtherSettings = 0x20,
        All = 0x3F,
    };

    inline constexpr uint32_t PartBit(_In_ ConfigPart const part) noexcept { return static_cast<uint32_t>(part); }

    enum class NetworkKind
    {
        NetworkMidi2,
        RtpMidi,
    };

    // A device's saved name, picture and other settings: one entry in a section's update list.
    struct DeviceNameEntry
    {
        std::wstring SectionKey{};

        // the same text for the same device, because the same match means the same device
        std::wstring MatchIdentity{};

        std::wstring Name{};
        std::wstring Image{};

        // from the match, for saying whether the device is plugged in here
        std::wstring EndpointDeviceId{};
        std::wstring DeviceInstanceId{};
        std::wstring TransportSuppliedName{};
    };

    struct LoopbackEntry
    {
        // a basic MIDI 1.0 loopback has one endpoint, a MIDI 2.0 loopback has two
        bool Basic{ false };
        std::wstring AssociationId{};
        std::vector<std::wstring> EndpointNames{};
        std::vector<std::wstring> UniqueIds{};
        std::vector<std::wstring> Images{};

        // "Max" for "Max (A)" and "Max (B)", or the name of a basic loopback. Empty when the two
        // ends of a MIDI 2.0 loopback are named some other way.
        std::wstring BaseName{};
    };

    struct HostEntry
    {
        NetworkKind Kind{ NetworkKind::NetworkMidi2 };
        std::wstring EntryId{};
        std::wstring Name{};
        std::wstring ServiceInstanceName{};
        std::wstring ProductInstanceId{};

        // set to one network adapter, which is the other PC's when it came in a package
        bool UsesOneAdapter{ false };
    };

    struct ClientEntry
    {
        NetworkKind Kind{ NetworkKind::NetworkMidi2 };
        std::wstring EntryId{};

        // the same text for a connection to the same remote
        std::wstring Identity{};

        std::wstring DisplayName{};
    };

    struct BluetoothEntry
    {
        std::wstring DeviceId{};
        std::wstring Name{};
    };

    // What a configuration has in it, in the terms the window shows.
    struct ConfigInventory
    {
        bool IsConfiguration{ false };
        std::wstring ConfigName{};

        std::vector<DeviceNameEntry> DeviceNames{};

        // every picture named anywhere in it, once each
        std::vector<std::wstring> Pictures{};

        std::vector<LoopbackEntry> Loopbacks{};
        std::vector<HostEntry> Hosts{};
        std::vector<ClientEntry> Clients{};
        std::vector<BluetoothEntry> BluetoothDevices{};

        bool HasSynthesizer{ false };

        // as written: "compatible" or "modern"
        std::wstring SynthMode{};

        // sections for transports this engine doesn't know
        std::vector<std::wstring> OtherSections{};

        size_t HostCount(_In_ NetworkKind kind) const noexcept;
        size_t ClientCount(_In_ NetworkKind kind) const noexcept;

        // nothing set up yet
        bool IsEmpty() const noexcept;

        // the parts that have something in them
        uint32_t PartsPresent() const noexcept;
    };

    ConfigInventory TakeInventory(_In_ json::JsonObject const& document) noexcept;
    ConfigInventory TakeInventory(_In_ std::string_view bytes) noexcept;

    // "configName" from the header, empty when there isn't one.
    std::wstring ConfigNameOf(_In_ json::JsonObject const& document) noexcept;

    // A copy that leaves out the parts that aren't wanted, for a package that doesn't carry
    // everything. The header stays.
    json::JsonObject WithOnlyParts(_In_ json::JsonObject const& document, _In_ uint32_t parts) noexcept;

    // The pictures the given parts use, once each.
    std::vector<std::wstring> PicturesUsedBy(_In_ ConfigInventory const& inventory, _In_ uint32_t parts);

    // A copy with the header's configName changed, and the header added when it's missing.
    json::JsonObject WithConfigName(_In_ json::JsonObject const& document, _In_ std::wstring const& configName) noexcept;

    // A configuration with nothing in it yet.
    std::string EmptyConfiguration(_In_ std::wstring const& configName) noexcept;
}
