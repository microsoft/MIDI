// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiClockDestination.h"
#include "Utilities.Sequencing.MidiClockDestination.g.cpp"


namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::Sequencing::implementation
{
    MidiClockDestination::MidiClockDestination(
        midi2::MidiEndpointConnection const& connection, 
        collections::IVector<midi2::MidiGroup> const& groups) noexcept
    {
        m_connection = connection;

        if (groups != nullptr && groups.Size() > 0)
        {
            for (auto const& group : groups)
            {
                m_groups.Append(group);
            }
        }

    }

}
