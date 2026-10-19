// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Pure preprocessor defines, for the default port count and the name length
#include "..\..\Transport\UdpNetworkMidi2Transport\network_json_defs.h"

// What the MIDI 1.0 ports dialog edits for a Network MIDI 2.0 connection or host, and how it
// travels to and from the SDK. Windows only uses these for a device which doesn't describe its own
// groups with function blocks.
namespace winrt::midinetworksetup::implementation
{
    // One group's MIDI 1.0 ports. An input brings what the device sends into apps, which the API
    // calls a source. An output sends from apps to the device, which the API calls a destination.
    struct Midi1PortGroupChoice
    {
        uint8_t GroupIndex{ 0 };
        std::wstring Name{};
        bool HasInput{ false };
        bool HasOutput{ false };

        bool operator==(Midi1PortGroupChoice const&) const = default;
    };

    // Either the count alone, which is groups 1 to Count with both ports and the names Windows
    // chooses, or a list of groups. A list which says the same thing as a count is kept as the
    // count, so an entry saved before groups could be chosen compares equal to the same choice
    // made now.
    struct Midi1PortLayout
    {
        // used when Groups is empty
        uint8_t Count{ MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_DEFAULT };

        // sorted by group, one entry per group, and every entry has at least one port
        std::vector<Midi1PortGroupChoice> Groups{};

        bool operator==(Midi1PortLayout const& other) const noexcept
        {
            return Groups.empty() ?
                (other.Groups.empty() && Count == other.Count) :
                Groups == other.Groups;
        }
    };

    // Sorted, one entry per group with the last one winning, no entry without a port, names
    // trimmed and cut to length, and the count form whenever it says the same thing.
    Midi1PortLayout NormalizedMidi1PortLayout(_In_ Midi1PortLayout layout) noexcept;

    // What an entry has: its list of groups, or its count when the list is empty
    Midi1PortLayout Midi1PortLayoutFromApi(
        _In_ uint8_t const count,
        _In_ winrt::Windows::Foundation::Collections::IIterable<winrt::Windows::Devices::Midi2::Transports::Network::MidiNetworkMidi1PortGroup> const& groups) noexcept;

    // The count to save with a layout. A transport from before groups could be chosen only reads
    // the count, so it gets enough ports to reach the highest group chosen.
    uint8_t Midi1PortCountFor(_In_ Midi1PortLayout const& layout) noexcept;

    // Replaces the contents of target, leaving it empty for the count form. On an update config,
    // only call this when the layout changed: changing the list is what tells the SDK to send it.
    void WriteMidi1PortGroups(
        _In_ Midi1PortLayout const& layout,
        _In_ winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi2::Transports::Network::MidiNetworkMidi1PortGroup> const& target);

    // The same, but the count form is written out as its groups. One device's settings on a host
    // have no count, and an empty list there means the host's ports.
    void WriteMidi1PortGroupsAsList(
        _In_ Midi1PortLayout const& layout,
        _In_ winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi2::Transports::Network::MidiNetworkMidi1PortGroup> const& target);

    // For example "2 sources (In) and 2 destinations (Out), named by you"
    winrt::hstring Midi1PortLayoutSummary(_In_ Midi1PortLayout const& layout) noexcept;

    // The summary as a line of its own, for example
    // "MIDI 1.0 ports: 1 source (In) and 1 destination (Out), named by Windows"
    winrt::hstring Midi1PortsLineText(_In_ Midi1PortLayout const& layout) noexcept;

    // A remote client's name or product instance id, compared the way the service compares
    // them: without case, and ignoring spaces at either end.
    bool IsSameRemoteClientText(_In_ winrt::hstring const& left, _In_ winrt::hstring const& right) noexcept;

    // A host's settings for one remote client, or nullptr when it has none. The service keys them
    // on the name and product instance id together.
    winrt::Windows::Devices::Midi2::Transports::Network::MidiNetworkRemoteClientSettings FindRemoteClientSettings(
        _In_ winrt::Windows::Foundation::Collections::IIterable<winrt::Windows::Devices::Midi2::Transports::Network::MidiNetworkRemoteClientSettings> const& settings,
        _In_ winrt::hstring const& remoteName,
        _In_ winrt::hstring const& remoteProductInstanceId) noexcept;
}
