// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once
#include "MidiEndpointConnectionOptions.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiEndpointConnectionOptions : MidiEndpointConnectionOptionsT<MidiEndpointConnectionOptions>
    {
    public:
        MidiEndpointConnectionOptions() = default;

        midi2::MidiEndpointConnectionSharingPreference ConnectionSharingPreference() { return m_connectionSharingPreference; }
        void ConnectionSharingPreference(_In_ midi2::MidiEndpointConnectionSharingPreference const& value) { m_connectionSharingPreference = value; }


        midi2::MidiStreamConfigurationRequestedSettings RequestedStreamConfiguration() { return m_streamRequestedConfigurationSettings; }
        void RequestedStreamConfiguration(_In_ midi2::MidiStreamConfigurationRequestedSettings const& value) { m_streamRequestedConfigurationSettings = value; }

    private:
        bool m_disableAutomaticEndpointMetadataHandling = false;
        bool m_disableAutomaticFunctionBlockMetadataHandling = false;
        bool m_disableAutomaticStreamConfiguration = false;

        midi2::MidiStreamConfigurationRequestedSettings m_streamRequestedConfigurationSettings;

        midi2::MidiEndpointConnectionSharingPreference m_connectionSharingPreference{ midi2::MidiEndpointConnectionSharingPreference::Default };
    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiEndpointConnectionOptions : MidiEndpointConnectionOptionsT<MidiEndpointConnectionOptions, implementation::MidiEndpointConnectionOptions>
    {
    };
}
