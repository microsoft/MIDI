// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Utilities.Sequencing.MidiClockDestination.g.h"


namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::Sequencing::implementation
{
    struct MidiClockDestination : MidiClockDestinationT<MidiClockDestination>
    {
        MidiClockDestination(midi2::MidiEndpointConnection const& connection, collections::IVector<midi2::MidiGroup> const& groups) noexcept;

        midi2::MidiEndpointConnection Connection() const noexcept { return m_connection; }
        collections::IVectorView<midi2::MidiGroup> Groups() const noexcept { return m_groups.GetView(); }




    private:
        midi2::MidiEndpointConnection m_connection{ nullptr };

        collections::IVector<midi2::MidiGroup> m_groups = winrt::single_threaded_vector< midi2::MidiGroup>();

    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::Sequencing::factory_implementation
{
    struct MidiClockDestination : MidiClockDestinationT<MidiClockDestination, implementation::MidiClockDestination>
    {
    };
}
