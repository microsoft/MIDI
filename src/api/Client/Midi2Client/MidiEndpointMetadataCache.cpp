// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiEndpointMetadataCache.h"
#include "MidiEndpointMetadataCache.g.cpp"

#include "MidiEndpointMetadataCacheUpdatedEventArgs.h"


namespace winrt::Windows::Devices::Midi2::implementation
{


    _Use_decl_annotations_
    void MidiEndpointMetadataCache::AddOrUpdateData(
        winrt::hstring const& endpointDeviceId,
        winrt::hstring const& propertyKey,
        winrt::hstring const& data,
        foundation::DateTime const& expirationTime
        ) 
    {
        auto cacheKey = BuildCacheKey(endpointDeviceId, propertyKey);

        auto updateType = MidiCacheUpdateType::Updated;

        // check to see if this key already exists. Don't raise event if the data hasn't actually changed
        if (InternalIsDataPresent(cacheKey))
        {

            auto newHash = HashData(winrt::to_string(data));

            // hash here is unique enough for this use 1.0/size_t's maxvalue chance of collision
            if (m_cache[cacheKey].dataHash == newHash)
            {
                // data has not changed. Do nothing
                return;
            }

            updateType = MidiCacheUpdateType::Updated;
        }
        else
        {
            updateType = MidiCacheUpdateType::Added;
        }

        // update the cache
        m_cache[cacheKey] = CreateCacheEntry(data, expirationTime);

        // raise the updated event
        if (m_dataUpdateEvent)
        {
            auto args = winrt::make_self<MidiEndpointMetadataCacheUpdatedEventArgs>();

            args->InternalInitialize(updateType, endpointDeviceId, propertyKey);

            m_dataUpdateEvent((foundation::IInspectable)*this, *args);
        }
    }


    _Use_decl_annotations_
    void MidiEndpointMetadataCache::AddOrUpdateData(
            winrt::hstring const& endpointDeviceId,
            winrt::hstring const& propertyKey,
            winrt::hstring const& data
        )
    {
        //foundation::DateTime maxFuture = foundation::DateTime::max() ;
        
        // expiration max is just 400 hours into the future. When we have this in a service, we can set some
        // indefinite expiration value that we'll use
        foundation::DateTime maxFuture = winrt::clock::from_sys(std::chrono::system_clock::now() + std::chrono::hours(400));

        AddOrUpdateData(endpointDeviceId, propertyKey, data, maxFuture);
    }



    _Use_decl_annotations_
    void MidiEndpointMetadataCache::RemoveData(
        winrt::hstring const& deviceId,
        winrt::hstring const& key
        ) 
    {
        auto cacheKey = BuildCacheKey(deviceId, key);

        if (InternalIsDataPresent(cacheKey))
        {
            m_cache.erase(cacheKey);

            if (m_dataUpdateEvent)
            {
                auto args = winrt::make_self<MidiEndpointMetadataCacheUpdatedEventArgs>();

                args->InternalInitialize(MidiCacheUpdateType::Removed, deviceId, key);

                m_dataUpdateEvent((foundation::IInspectable)*this, *args);
            }
        }
        else
        {
            // the data isn't there. Do nothing
        }

    }

    _Use_decl_annotations_
    winrt::hstring MidiEndpointMetadataCache::GetData(
        winrt::hstring const& deviceId,
        winrt::hstring const& key
        ) 
    {
        auto cacheKey = BuildCacheKey(deviceId, key);

        if (InternalIsDataPresent(cacheKey))
        {
            return winrt::to_hstring(m_cache[cacheKey].data);
        }
        else
        {
            // return empty hstring if the key isn't there
            return winrt::hstring{};
        }
    }

    _Use_decl_annotations_
    bool MidiEndpointMetadataCache::IsDataPresent(
        winrt::hstring const& deviceId,
        winrt::hstring const& key
        ) 
    {
        auto cacheKey = BuildCacheKey(deviceId, key);

        return InternalIsDataPresent(cacheKey);
    }

}
