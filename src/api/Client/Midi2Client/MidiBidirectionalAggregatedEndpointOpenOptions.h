// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiBidirectionalAggregatedEndpointOpenOptions.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiBidirectionalAggregatedEndpointOpenOptions : MidiBidirectionalAggregatedEndpointOpenOptionsT<MidiBidirectionalAggregatedEndpointOpenOptions>
    {
        MidiBidirectionalAggregatedEndpointOpenOptions() = default;

        bool DisableAutomaticEndpointDiscoveryMessages() { return m_disableAutomaticEndpointDiscoveryMessages; }
        void DisableAutomaticEndpointDiscoveryMessages(_In_ bool const value) { m_disableAutomaticEndpointDiscoveryMessages = value; }

        bool DisableAutomaticFunctionBlockInfoMessages() { return m_disableAutomaticFunctionBlockInfoMessages; }
        void DisableAutomaticFunctionBlockInfoMessages(_In_ bool const value) { m_disableAutomaticFunctionBlockInfoMessages = value; }


        winrt::Windows::Devices::Midi2::MidiEndpointConnectionSharingPreference ConnectionSharingPreference() { return m_connectionSharingPreference; }
        void ConnectionSharingPreference(_In_ winrt::Windows::Devices::Midi2::MidiEndpointConnectionSharingPreference const& value) { m_connectionSharingPreference = value; }

    private:
        bool m_disableAutomaticEndpointDiscoveryMessages = false;
        bool m_disableAutomaticFunctionBlockInfoMessages = false;

        winrt::Windows::Devices::Midi2::MidiEndpointConnectionSharingPreference m_connectionSharingPreference{ winrt::Windows::Devices::Midi2::MidiEndpointConnectionSharingPreference::Default };
    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiBidirectionalAggregatedEndpointOpenOptions : MidiBidirectionalAggregatedEndpointOpenOptionsT<MidiBidirectionalAggregatedEndpointOpenOptions, implementation::MidiBidirectionalAggregatedEndpointOpenOptions>
    {
    };
}
