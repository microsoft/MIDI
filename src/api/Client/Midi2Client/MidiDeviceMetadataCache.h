// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiDeviceMetadataCache.g.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiDeviceMetadataCache : MidiDeviceMetadataCacheT<MidiDeviceMetadataCache>
    {
        MidiDeviceMetadataCache() = default;

        void AddOrUpdateData(
            _In_ winrt::hstring const& deviceId,
            _In_ winrt::hstring const& key,
            _In_ winrt::hstring const& data,
            _In_ winrt::Windows::Foundation::DateTime const& expirationTime) ;
        
        void RemoveData(
            _In_ winrt::hstring const& deviceId, 
            _In_ winrt::hstring const& key) ;
        
        hstring GetData(
            _In_ winrt::hstring const& deviceId, 
            _In_ winrt::hstring const& key) ;
        
        bool IsDataPresent(
            _In_ winrt::hstring const& deviceId, 
            _In_ winrt::hstring const& key) ;

        winrt::event_token DeviceInformationUpdated(_In_ foundation::TypedEventHandler<foundation::IInspectable, midi2::MidiDeviceInformationCacheUpdatedEventArgs> const& handler)
        {
            return m_dataUpdateEvent.add(handler);
        }
        void DeviceInformationUpdated(_In_ winrt::event_token const& token) noexcept
        {
            if (m_dataUpdateEvent) m_dataUpdateEvent.remove(token);
        }


    private:
        winrt::event<foundation::TypedEventHandler<foundation::IInspectable, midi2::MidiDeviceInformationCacheUpdatedEventArgs>> m_dataUpdateEvent;


    };
}
