// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <pch.h>

namespace midi2 = ::winrt::Windows::Devices::Midi2;

// This whole thing could be, in its current form, a winrt observable map. But
// The intent here is to provide a stub for what will eventuall be backed by 
// a cross-process cache service endpoint on MidiSrv, sharing data between
// applications, and with a lifetime not tied to the app instance.


namespace Windows::Devices::Midi2::Internal
{
    struct MidiCacheEntry
    {
        // if you change this from system_clock to something else, you need to change the other code in the base cache class
        std::chrono::time_point<std::chrono::system_clock> expiration;
        std::string data;                   // we only use string data. It's all UTF8 json
        size_t dataHash;
    };


    template<typename TKey>
    class InternalMidiCache

    {
    public:
        ~InternalMidiCache()
        {
            m_cache.clear();
        }

    protected:
        size_t HashData(_In_ std::string data)
        {
            std::hash<std::string> hasher;

            return hasher(data);
        }

        MidiCacheEntry CreateCacheEntry(
            _In_ winrt::hstring const data, 
            _In_ foundation::DateTime const expiration)
        {
            MidiCacheEntry entry;

            entry.data = winrt::to_string(data);    // UTF8
            entry.dataHash = HashData(entry.data);  // avoids doing a full string comparison when we want to know if data is different

            // assumes that we're using system time and not file time here
            entry.expiration = winrt::clock::to_sys(expiration);

            return entry;
        }

        void ExpireEntryIfDue(_In_ TKey cacheKey)
        {
            if (auto result = m_cache.find(cacheKey); result != m_cache.end())
            {
                auto expiration = m_cache[cacheKey].expiration;

                if (expiration <= std::chrono::system_clock::now())
                {
                    // we don't send out notifications on expiration. 
                    // We may decide to do that once this is moved to a service
                    m_cache.erase(cacheKey);
                }
            }
        }

        bool InternalIsDataPresent(_In_ TKey cacheKey)
        {
            ExpireEntryIfDue(cacheKey); // in the service impl, the service will take care of this

            if (auto result = m_cache.find(cacheKey); result != m_cache.end())
            {
                return true;
            }
            else
            {
                return false;
            }
        }

        MidiCacheEntry InternalGetCacheItem(_In_ TKey cacheKey)
        {
            return m_cache[cacheKey];
        }

        void InternalRemoveItem(_In_ TKey cacheKey)
        {
            m_cache.erase(cacheKey);
        }

        void InternalAddOrUpdateItem(_In_ TKey cacheKey, _In_ MidiCacheEntry entry)
        {
            m_cache[cacheKey] = entry;
        }

    private:
        // This is all local until we have the cache service in place
        std::map<TKey, MidiCacheEntry> m_cache{};


    };

}