// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once
#include "MidiOutputEndpointOpenOptions.g.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiOutputEndpointOpenOptions : MidiOutputEndpointOpenOptionsT<MidiOutputEndpointOpenOptions>
    {
    public:
        MidiOutputEndpointOpenOptions() = default;

        winrt::Windows::Devices::Midi2::MidiEndpointConnectionSharingPreference ConnectionSharingPreference() { return m_connectionSharingPreference; }
        void ConnectionSharingPreference(_In_ winrt::Windows::Devices::Midi2::MidiEndpointConnectionSharingPreference const& value) { m_connectionSharingPreference = value; }

    private:
        winrt::Windows::Devices::Midi2::MidiEndpointConnectionSharingPreference m_connectionSharingPreference{ winrt::Windows::Devices::Midi2::MidiEndpointConnectionSharingPreference::Default };
    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiOutputEndpointOpenOptions : MidiOutputEndpointOpenOptionsT<MidiOutputEndpointOpenOptions, implementation::MidiOutputEndpointOpenOptions>
    {
    };
}
