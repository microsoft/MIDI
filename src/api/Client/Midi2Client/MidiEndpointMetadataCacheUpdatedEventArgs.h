// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiEndpointMetadataCacheUpdatedEventArgs.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiEndpointMetadataCacheUpdatedEventArgs : MidiEndpointMetadataCacheUpdatedEventArgsT<MidiEndpointMetadataCacheUpdatedEventArgs>
    {
        MidiEndpointMetadataCacheUpdatedEventArgs() = default;

        midi2::MidiCacheUpdateType UpdateType() { return m_updateType; }
        winrt::hstring EndpointDeviceId() { return m_endpointDeviceId; }
        winrt::hstring PropertyKey() { return m_propertyKey; }

        // internal initialize
        void InternalInitialize(
            _In_ midi2::MidiCacheUpdateType updateType, 
            _In_ winrt::hstring endpointDeviceId, 
            _In_ winrt::hstring propertyKey)
        {
            m_updateType = updateType;
            m_endpointDeviceId = endpointDeviceId;
            m_propertyKey = propertyKey;
        }

    private:
        midi2::MidiCacheUpdateType m_updateType{ 0 };
        winrt::hstring m_endpointDeviceId{};
        winrt::hstring m_propertyKey{};
    };
}
