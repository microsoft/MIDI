// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef MIDI_ENDPOINT_CUSTOM_PROPERTIES_CACHE_H
#define MIDI_ENDPOINT_CUSTOM_PROPERTIES_CACHE_H

#include <wil/resource.h>

#include "MidiEndpointCustomProperties.h"
#include "MidiEndpointCustomizationProvenance.h"
#include "MidiEndpointMatchCriteria.h"

namespace WindowsMidiServicesPluginConfigurationLib
{

    struct MidiEndpointCustomPropertiesCacheEntry
    {
        std::shared_ptr<MidiEndpointMatchCriteria> Match{ nullptr };
        std::shared_ptr<MidiEndpointCustomProperties> Properties{ nullptr };
        std::shared_ptr<MidiEndpointCustomizationProvenance> Provenance{ nullptr };

        // Empty when the entry did not match any endpoint the transport had instantiated. That is
        // what makes a customization orphaned, and this is the only place the answer is known.
        winrt::hstring ResolvedEndpointDeviceId{};
    };

    class MidiEndpointCustomPropertiesCache
    {
    public:
        MidiEndpointCustomPropertiesCache() = default;

        //std::shared_ptr<MidiEndpointCustomProperties> GetProperties(_In_ std::map<winrt::hstring, winrt::hstring> knownEndpointProperties);
        std::shared_ptr<MidiEndpointCustomProperties> GetProperties(_In_ MidiEndpointMatchCriteria& knownEndpointProperties);

        bool Add(_In_ std::shared_ptr<MidiEndpointMatchCriteria> match, _In_ std::shared_ptr<MidiEndpointCustomProperties> properties);

        bool AddWithProvenance(
            _In_ std::shared_ptr<MidiEndpointMatchCriteria> match,
            _In_ std::shared_ptr<MidiEndpointCustomProperties> properties,
            _In_opt_ std::shared_ptr<MidiEndpointCustomizationProvenance> provenance,
            _In_ winrt::hstring const& resolvedEndpointDeviceId);

        // A snapshot, so a caller can report on the whole set without holding the lock while it
        // builds a response.
        std::vector<std::shared_ptr<MidiEndpointCustomPropertiesCacheEntry>> GetAllEntries();

        bool Remove(_In_ MidiEndpointMatchCriteria& match);

        bool UpdateResolvedEndpointDeviceId(
            _In_ MidiEndpointMatchCriteria& match,
            _In_ winrt::hstring const& resolvedEndpointDeviceId);

    private:
        wil::srwlock m_entriesMutex;
        std::vector<std::shared_ptr<MidiEndpointCustomPropertiesCacheEntry>> m_entries;

    };

}

#endif
