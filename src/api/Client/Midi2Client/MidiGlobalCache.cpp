// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiGlobalCache.h"
#include "MidiGlobalCache.g.cpp"

#include "MidiGlobalInformationCacheUpdatedEventArgs.h"


namespace winrt::Windows::Devices::Midi2::implementation
{

    _Use_decl_annotations_
    void MidiGlobalCache::AddOrUpdateData(
        winrt::hstring const& propertyKey,
        winrt::hstring const& data,
        foundation::DateTime const& expirationTime
        )
    {
        auto cacheKey = BuildCacheKey(propertyKey);

        auto updateType = MidiCacheUpdateType::Updated;

        // check to see if this key already exists. Don't raise event if the data hasn't actually changed
        if (InternalIsDataPresent(cacheKey))
        {
            auto newHash = HashData(winrt::to_string(data));

            // hash here is unique enough for this use 1.0/size_t's maxvalue chance of collision
            if (InternalGetCacheItem(cacheKey).dataHash == newHash)
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
        InternalAddOrUpdateItem(cacheKey, CreateCacheEntry(data, expirationTime));

        // raise the updated event
        if (m_dataUpdateEvent)
        {
            auto args = winrt::make_self<MidiGlobalInformationCacheUpdatedEventArgs>();

            args->InternalInitialize(updateType, propertyKey);

            m_dataUpdateEvent((foundation::IInspectable)*this, *args);
        }
    }


    _Use_decl_annotations_
    void MidiGlobalCache::AddOrUpdateData(
        winrt::hstring const& propertyKey,
        winrt::hstring const& data
        )
    {
        //foundation::DateTime maxFuture = foundation::DateTime::max() ;

        // expiration max is just 400 hours into the future. When we have this in a service, we can set some
        // indefinite expiration value that we'll use
        foundation::DateTime maxFuture = winrt::clock::from_sys(std::chrono::system_clock::now() + std::chrono::hours(400));

        AddOrUpdateData(propertyKey, data, maxFuture);
    }



    _Use_decl_annotations_
    void MidiGlobalCache::RemoveData(
        winrt::hstring const& propertyKey
        )
    {
        auto cacheKey = BuildCacheKey(propertyKey);

        if (InternalIsDataPresent(cacheKey))
        {
            InternalRemoveItem(cacheKey);

            if (m_dataUpdateEvent)
            {
                auto args = winrt::make_self<MidiGlobalInformationCacheUpdatedEventArgs>();

                args->InternalInitialize(MidiCacheUpdateType::Removed, propertyKey);

                m_dataUpdateEvent((foundation::IInspectable)*this, *args);
            }
        }
        else
        {
            // the data isn't there. Do nothing
        }
    }

    _Use_decl_annotations_
    winrt::hstring MidiGlobalCache::GetData(
        winrt::hstring const& propertyKey
        )
    {
        auto cacheKey = BuildCacheKey(propertyKey);

        if (InternalIsDataPresent(cacheKey))
        {
            return winrt::to_hstring(InternalGetCacheItem(cacheKey).data);
        }
        else
        {
            // return empty hstring if the key isn't there
            return winrt::hstring{};
        }
    }

    _Use_decl_annotations_
    bool MidiGlobalCache::IsDataPresent(
        winrt::hstring const& key
        )
    {
        auto cacheKey = BuildCacheKey(key);

        return InternalIsDataPresent(cacheKey);
    }

}
