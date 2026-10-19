// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkMidi1PortGroup.h"
#include "Transports.Network.MidiNetworkMidi1PortGroup.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    _Use_decl_annotations_
    MidiNetworkMidi1PortGroup::MidiNetworkMidi1PortGroup(
        midi2::MidiGroup const& group,
        winrt::hstring const& name) noexcept
    {
        Group(group);
        Name(name);
    }

    _Use_decl_annotations_
    MidiNetworkMidi1PortGroup::MidiNetworkMidi1PortGroup(
        midi2::MidiGroup const& group,
        winrt::hstring const& name,
        bool const hasSourcePort,
        bool const hasDestinationPort) noexcept :
        m_hasSourcePort(hasSourcePort),
        m_hasDestinationPort(hasDestinationPort)
    {
        Group(group);
        Name(name);
    }

    // A null group, or one out of range, leaves the group as it was
    _Use_decl_annotations_
    void MidiNetworkMidi1PortGroup::Group(midi2::MidiGroup const& value) noexcept
    {
        try
        {
            if (value == nullptr) return;

            auto const index = value.Index();

            if (index < MIDI_NETWORK_MIDI1_PORT_GROUP_COUNT)
            {
                m_groupIndex = index;
            }
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception setting the group of a network MIDI 1.0 port group.");
        }
    }

    // Cleaned the way the service cleans it, so what reads back is what MIDI 1.0 apps will show
    _Use_decl_annotations_
    void MidiNetworkMidi1PortGroup::Name(winrt::hstring const& value) noexcept
    {
        m_name = winrt::hstring{ internal::CleanMidiNetworkMidi1PortGroupName(std::wstring_view{ value.c_str(), value.size() }) };
    }

    _Use_decl_annotations_
    internal::MidiNetworkMidi1PortGroupList MidiNetworkMidi1PortGroup::ToDefinitions(
        collections::IIterable<network::MidiNetworkMidi1PortGroup> const& groups) noexcept
    {
        internal::MidiNetworkMidi1PortGroupList definitions{};

        try
        {
            if (groups == nullptr) return definitions;

            for (auto const& group : groups)
            {
                if (group == nullptr) continue;

                auto const midiGroup = group.Group();

                if (midiGroup == nullptr) continue;

                internal::MidiNetworkMidi1PortGroupDefinition definition{};

                definition.GroupIndex = midiGroup.Index();
                definition.Name = std::wstring{ group.Name() };
                definition.HasSourcePort = group.HasSourcePort();
                definition.HasDestinationPort = group.HasDestinationPort();

                definitions.push_back(std::move(definition));
            }
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception reading network MIDI 1.0 port groups.");

            return internal::MidiNetworkMidi1PortGroupList{};
        }

        // Sorted, one per group, and cleaned, the way the service will read it
        return internal::NormalizeMidiNetworkMidi1PortGroups(definitions);
    }

    _Use_decl_annotations_
    collections::IVector<network::MidiNetworkMidi1PortGroup> MidiNetworkMidi1PortGroup::FromDefinitions(
        internal::MidiNetworkMidi1PortGroupList const& definitions) noexcept
    {
        auto groups = winrt::single_threaded_vector<network::MidiNetworkMidi1PortGroup>();

        try
        {
            for (auto const& definition : definitions)
            {
                groups.Append(winrt::make<MidiNetworkMidi1PortGroup>(
                    midi2::MidiGroup(definition.GroupIndex),
                    winrt::hstring{ definition.Name },
                    definition.HasSourcePort,
                    definition.HasDestinationPort));
            }
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception building network MIDI 1.0 port groups.");
        }

        return groups;
    }

    _Use_decl_annotations_
    collections::IObservableVector<network::MidiNetworkMidi1PortGroup> MidiNetworkMidi1PortGroup::CreateChangeTrackedList(
        std::shared_ptr<std::atomic<bool>> const& changed)
    {
        auto groups = winrt::single_threaded_observable_vector<network::MidiNetworkMidi1PortGroup>();

        // Clear raises this too, even on an empty list, which is how a caller asks to go back to
        // the count
        groups.VectorChanged([changed](auto&&, auto&&)
            {
                changed->store(true);
            });

        return groups;
    }
}
