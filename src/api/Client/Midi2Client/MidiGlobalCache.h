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
    struct MidiGlobalCache : MidiGlobalCacheT<MidiGlobalCache>,
        public internal::InternalMidiCache<std::string>
    {
        MidiGlobalCache() = default;

        void AddOrUpdateData(
            _In_ winrt::hstring const& propertyKey, 
            _In_ winrt::hstring const& data,
            _In_ foundation::DateTime const& expirationTime);

        void AddOrUpdateData(
            _In_ winrt::hstring const& propertyKey,
            _In_ winrt::hstring const& data);

        void RemoveData(
            _In_ winrt::hstring const& propertyKey);

        hstring GetData(
            _In_ winrt::hstring const& propertyKey);

        bool IsDataPresent(
            _In_ winrt::hstring const& propertyKey);
        

        winrt::event_token GlobalInformationUpdated(
            _In_ foundation::TypedEventHandler<foundation::IInspectable, midi2::MidiGlobalInformationCacheUpdatedEventArgs> const& handler)
        {
            return m_dataUpdateEvent.add(handler);
        }
        void GlobalInformationUpdated(_In_ winrt::event_token const& token) noexcept
        {
            if (m_dataUpdateEvent) m_dataUpdateEvent.remove(token);
        }

        std::string BuildCacheKey(winrt::hstring const& propertyKey)
        {
            return winrt::to_string(propertyKey);
        }


    private:
        winrt::event<foundation::TypedEventHandler<foundation::IInspectable, midi2::MidiGlobalInformationCacheUpdatedEventArgs>> m_dataUpdateEvent;

    };
}
