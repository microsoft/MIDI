// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once
#include "MidiBidirectionalEndpointOpenOptions.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiBidirectionalEndpointOpenOptions : MidiBidirectionalEndpointOpenOptionsT<MidiBidirectionalEndpointOpenOptions>
    {
    public:
        MidiBidirectionalEndpointOpenOptions() = default;

        bool DisableAutomaticEndpointMetadataHandling() { return m_disableAutomaticEndpointMetadataHandling; }
        void DisableAutomaticEndpointMetadataHandling(_In_ bool const value) { m_disableAutomaticEndpointMetadataHandling = value; }
        
        bool DisableAutomaticFunctionBlockMetadataHandling() { return m_disableAutomaticFunctionBlockMetadataHandling; }
        void DisableAutomaticFunctionBlockMetadataHandling(_In_ bool const value) { m_disableAutomaticFunctionBlockMetadataHandling = value; }

        bool DisableAutomaticStreamConfiguration() { return m_disableAutomaticStreamConfiguration; }
        void DisableAutomaticStreamConfiguration(_In_ bool const value) { m_disableAutomaticStreamConfiguration = value; }


        midi2::MidiEndpointConnectionSharingPreference ConnectionSharingPreference() { return m_connectionSharingPreference; }
        void ConnectionSharingPreference(_In_ midi2::MidiEndpointConnectionSharingPreference const& value) { m_connectionSharingPreference = value; }


        midi2::MidiStreamConfigurationRequestedSettings RequestedStreamConfiguration() { return m_streamRequestedConfigurationSettings; }
        void RequestedStreamConfiguration(_In_ midi2::MidiStreamConfigurationRequestedSettings const& value) { m_streamRequestedConfigurationSettings = value; }

    private:
        bool m_disableAutomaticEndpointMetadataHandling = false;
        bool m_disableAutomaticFunctionBlockMetadataHandling = false;
        bool m_disableAutomaticStreamConfiguration = false;

        midi2::MidiStreamConfigurationRequestedSettings m_streamRequestedConfigurationSettings;


        winrt::Windows::Devices::Midi2::MidiEndpointConnectionSharingPreference m_connectionSharingPreference{ winrt::Windows::Devices::Midi2::MidiEndpointConnectionSharingPreference::Default };
    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiBidirectionalEndpointOpenOptions : MidiBidirectionalEndpointOpenOptionsT<MidiBidirectionalEndpointOpenOptions, implementation::MidiBidirectionalEndpointOpenOptions>
    {
    };
}
