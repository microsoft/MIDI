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

#include "MidiEndpointCustomProperties.h"
#include "MidiEndpointMatchCriteria.h"

struct MidiEndpointCustomPropertiesCacheEntry
{
    std::shared_ptr<MidiEndpointMatchCriteria> Match{ nullptr };
    std::shared_ptr<MidiEndpointCustomProperties> Properties{ nullptr };
};

class MidiEndpointCustomPropertiesCache
{
public:
    MidiEndpointCustomPropertiesCache() = default;

    //std::shared_ptr<MidiEndpointCustomProperties> GetProperties(_In_ std::map<winrt::hstring, winrt::hstring> knownEndpointProperties);
    std::shared_ptr<MidiEndpointCustomProperties> GetProperties(_In_ MidiEndpointMatchCriteria& knownEndpointProperties);

    void Add(_In_ std::shared_ptr<MidiEndpointMatchCriteria> match, _In_ std::shared_ptr<MidiEndpointCustomProperties> properties);

    // TODO: Do we need a "remove" in here?

private:
    std::vector<MidiEndpointCustomPropertiesCacheEntry> m_entries;

};



#endif
