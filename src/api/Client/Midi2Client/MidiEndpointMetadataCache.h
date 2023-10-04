// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiEndpointMetadataCache.g.h"

#include "InternalMidiCache.h"


namespace winrt::Windows::Devices::Midi2::implementation
{

    struct MidiEndpointMetadataCache : MidiEndpointMetadataCacheT<MidiEndpointMetadataCache>,
        public internal::InternalMidiCache<std::tuple<std::string, std::string>>
    {
        MidiEndpointMetadataCache() = default;

        static void AddOrUpdateData(
            _In_ winrt::hstring const& endpointDeviceId,
            _In_ winrt::hstring const& propertyKey,
            _In_ winrt::hstring const& data,
            _In_ foundation::DateTime const& expirationTime) ;

        static void AddOrUpdateData(
            _In_ winrt::hstring const& endpointDeviceId,
            _In_ winrt::hstring const& propertyKey,
            _In_ winrt::hstring const& data);


        static void AddOrUpdateData(
            _In_ winrt::hstring const& endpointDeviceId,
            _In_ winrt::hstring const& propertyKey,
            _In_ midi2::IMidiCacheableMetadata const& data,
            _In_ foundation::DateTime const& expirationTime)
        {
            if (data != nullptr)
                AddOrUpdateData(endpointDeviceId, propertyKey, data.GetJsonString(), expirationTime);
        }

        static void AddOrUpdateData(
            _In_ winrt::hstring const& endpointDeviceId,
            _In_ winrt::hstring const& propertyKey,
            _In_ midi2::IMidiCacheableMetadata const& data)
        {
            if (data != nullptr)
                AddOrUpdateData(endpointDeviceId, propertyKey, data.GetJsonString());
        }

        static void RemoveData(
            _In_ winrt::hstring const& deviceId, 
            _In_ winrt::hstring const& propertyKey) ;
        
        static hstring GetData(
            _In_ winrt::hstring const& deviceId, 
            _In_ winrt::hstring const& propertyKey) ;
        
        static bool IsDataPresent(
            _In_ winrt::hstring const& deviceId, 
            _In_ winrt::hstring const& propertyKey) ;

        static winrt::event_token DeviceInformationUpdated(
            _In_ foundation::TypedEventHandler<foundation::IInspectable, midi2::MidiEndpointMetadataCacheUpdatedEventArgs> const& handler)
        {
            return m_dataUpdateEvent.add(handler);
        }
        static void DeviceInformationUpdated(_In_ winrt::event_token const& token) noexcept
        {
            if (m_dataUpdateEvent) m_dataUpdateEvent.remove(token);
        }

        static std::tuple<std::string, std::string> BuildCacheKey(
            _In_ winrt::hstring const& deviceId, 
            _In_ winrt::hstring const& key)
        {
            return std::tuple<std::string, std::string>(winrt::to_string(deviceId), winrt::to_string(key));
        }

    private:
        static winrt::event<foundation::TypedEventHandler<foundation::IInspectable, midi2::MidiEndpointMetadataCacheUpdatedEventArgs>> m_dataUpdateEvent;
    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiEndpointMetadataCache : MidiEndpointMetadataCacheT<MidiEndpointMetadataCache, implementation::MidiEndpointMetadataCache, winrt::static_lifetime>
    {
    };
}
