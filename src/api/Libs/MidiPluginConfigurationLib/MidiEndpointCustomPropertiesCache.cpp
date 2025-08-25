// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include <windows.h>

#include <string>
#include <vector>
#include <algorithm>

#include <winrt/windows.foundation.h>
#include <winrt/windows.foundation.collections.h>
#include <winrt/windows.devices.enumeration.h>

#include <mmsystem.h>
#include <wtypes.h>
#include <combaseapi.h>

#include "MidiEndpointCustomPropertiesCache.h"

//_Use_decl_annotations_
//std::shared_ptr<MidiEndpointCustomProperties> MidiEndpointCustomPropertiesCache::GetProperties(
//    std::map<winrt::hstring, winrt::hstring> knownEndpointProperties)
//{
//    for (auto const& entry : m_entries)
//    {
//        if (entry.Match->IsMatch(knownEndpointProperties))
//        {
//            return entry.Properties;
//        }
//    }
//
//    return nullptr;
//}


_Use_decl_annotations_
std::shared_ptr<MidiEndpointCustomProperties> MidiEndpointCustomPropertiesCache::GetProperties(
    MidiEndpointMatchCriteria& knownEndpointProperties)
{
    for (auto const& entry : m_entries)
    {
        if (entry.Match->Matches(knownEndpointProperties))
        {
            return entry.Properties;
        }
    }

    return nullptr;
}




_Use_decl_annotations_
void MidiEndpointCustomPropertiesCache::Add(
    std::shared_ptr<MidiEndpointMatchCriteria> match, std::shared_ptr<MidiEndpointCustomProperties> properties)
{
    MidiEndpointCustomPropertiesCacheEntry entry{};
    entry.Match = match;
    entry.Properties = properties;

    m_entries.push_back(entry);
}
