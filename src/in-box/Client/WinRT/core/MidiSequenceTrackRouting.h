// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Utilities.Sequencing.MidiSequenceTrackRouting.g.h"

namespace winrt::Windows::Devices::Midi2::Utilities::Sequencing::implementation
{
    struct MidiSequenceTrackRouting : MidiSequenceTrackRoutingT<MidiSequenceTrackRouting>
    {
        MidiSequenceTrackRouting() = default;

        midi2::MidiEndpointConnection Connection() const noexcept { return m_connection; }
        void Connection(_In_ midi2::MidiEndpointConnection const& value) noexcept { m_connection = value; }

        midi2::MidiGroup Group() const noexcept { return m_group; }
        void Group(_In_ midi2::MidiGroup const& value) noexcept { m_group = value; }

        midi2::MidiChannel ChannelOverride() const noexcept { return m_channelOverride; }
        void ChannelOverride(_In_ midi2::MidiChannel const& value) noexcept { m_channelOverride = value; }

        bool IsMuted() const noexcept { return m_isMuted; }
        void IsMuted(_In_ bool const value) noexcept { m_isMuted = value; }

    private:
        midi2::MidiEndpointConnection m_connection{ nullptr };
        midi2::MidiGroup m_group{ nullptr };
        midi2::MidiChannel m_channelOverride{ nullptr };
        bool m_isMuted{ false };
    };
}

namespace winrt::Windows::Devices::Midi2::Utilities::Sequencing::factory_implementation
{
    struct MidiSequenceTrackRouting : MidiSequenceTrackRoutingT<MidiSequenceTrackRouting, implementation::MidiSequenceTrackRouting>
    {
    };
}
