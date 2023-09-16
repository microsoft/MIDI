// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once
#include "MidiGlobalCache.g.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiGlobalCache : MidiGlobalCacheT<MidiGlobalCache>
    {
        MidiGlobalCache() = default;

        void AddOrUpdateData(
            _In_ winrt::hstring const& key, 
            _In_ winrt::hstring const& data,
            _In_ foundation::DateTime const& expirationTime);

        void RemoveData(
            _In_ winrt::hstring const& key);

        hstring GetData(
            _In_ winrt::hstring const& key);

        bool IsDataPresent(
            _In_ winrt::hstring const& key);
        

        winrt::event_token GlobalInformationUpdated(
            _In_ foundation::TypedEventHandler<foundation::IInspectable, midi2::MidiGlobalInformationCacheUpdatedEventArgs> const& handler)
        {
            return m_dataUpdateEvent.add(handler);
        }
        void GlobalInformationUpdated(_In_ winrt::event_token const& token) noexcept
        {
            if (m_dataUpdateEvent) m_dataUpdateEvent.remove(token);
        }


        std::string BuildCacheKey(winrt::hstring const& key)
        {
            return winrt::to_string(key);
        }

        bool InternalIsDataPresent(std::string cacheKey)
        {
            if (auto result = m_cache.find(cacheKey); result != m_cache.end())
            {
                return true;
            }
            else
            {
                return false;
            }
        }

    private:
        winrt::event<foundation::TypedEventHandler<foundation::IInspectable, midi2::MidiGlobalInformationCacheUpdatedEventArgs>> m_dataUpdateEvent;

        // This is all local until we have the cache service in place
        std::map<std::string, std::string> m_cache{};


    };
}
