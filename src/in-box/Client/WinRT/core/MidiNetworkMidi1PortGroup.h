// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkMidi1PortGroup.g.h"

#include "..\..\..\Transport\UdpNetworkMidi2Transport\network_midi1_port_groups.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkMidi1PortGroup : MidiNetworkMidi1PortGroupT<MidiNetworkMidi1PortGroup>
    {
        MidiNetworkMidi1PortGroup() = default;

        MidiNetworkMidi1PortGroup(
            _In_ midi2::MidiGroup const& group,
            _In_ winrt::hstring const& name) noexcept;

        MidiNetworkMidi1PortGroup(
            _In_ midi2::MidiGroup const& group,
            _In_ winrt::hstring const& name,
            _In_ bool const hasSourcePort,
            _In_ bool const hasDestinationPort) noexcept;

        midi2::MidiGroup Group() const noexcept { return midi2::MidiGroup(m_groupIndex); }
        void Group(_In_ midi2::MidiGroup const& value) noexcept;

        winrt::hstring Name() const noexcept { return m_name; }
        void Name(_In_ winrt::hstring const& value) noexcept;

        bool HasSourcePort() const noexcept { return m_hasSourcePort; }
        void HasSourcePort(_In_ bool const value) noexcept { m_hasSourcePort = value; }

        bool HasDestinationPort() const noexcept { return m_hasDestinationPort; }
        void HasDestinationPort(_In_ bool const value) noexcept { m_hasDestinationPort = value; }

        // Not part of the API. For the SDK types which carry a list of these. A list from the caller
        // is read only through the interface, never by reaching into the implementation, because
        // the objects in it can come from a different copy of this library than the list's owner.
        static internal::MidiNetworkMidi1PortGroupList ToDefinitions(
            _In_ collections::IIterable<network::MidiNetworkMidi1PortGroup> const& groups) noexcept;

        static collections::IVector<network::MidiNetworkMidi1PortGroup> FromDefinitions(
            _In_ internal::MidiNetworkMidi1PortGroupList const& definitions) noexcept;

        // For an update config, which sends the list only once the caller has changed it, even to
        // empty. The flag is shared with the handler rather than reached through the owner,
        // because the caller can keep the list after the owner is gone.
        static collections::IObservableVector<network::MidiNetworkMidi1PortGroup> CreateChangeTrackedList(
            _In_ std::shared_ptr<std::atomic<bool>> const& changed);

    private:
        uint8_t m_groupIndex{ 0 };
        winrt::hstring m_name{};
        bool m_hasSourcePort{ true };
        bool m_hasDestinationPort{ true };
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Network::factory_implementation
{
    struct MidiNetworkMidi1PortGroup : MidiNetworkMidi1PortGroupT<MidiNetworkMidi1PortGroup, implementation::MidiNetworkMidi1PortGroup>
    {
    };
}
