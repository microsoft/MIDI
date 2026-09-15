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

namespace WindowsMidiServicesPluginConfigurationLib
{

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
    auto lock = m_entriesMutex.lock_shared();

    for (auto const& entry : m_entries)
    {
        if (entry->Match->Matches(knownEndpointProperties))
        {
            return entry->Properties;
        }
    }

    return nullptr;
}




_Use_decl_annotations_
bool MidiEndpointCustomPropertiesCache::Add(
    std::shared_ptr<MidiEndpointMatchCriteria> match, std::shared_ptr<MidiEndpointCustomProperties> properties)
{
    auto lock = m_entriesMutex.lock_exclusive();

    // Need to make sure we don't already have custom properties for this endpoint. If so, remove them.
    // we only remove one, because if we do this correctly each time, there will never be more than one
    // other match in here.

    std::vector<std::shared_ptr<MidiEndpointCustomPropertiesCacheEntry>>::iterator it;

    for (it = m_entries.begin(); it != m_entries.end(); it++)
    {
        if ((*it)->Match->Matches(*match))
        {
            m_entries.erase(it);
            break;
        }
    }

    auto entry = std::make_shared<MidiEndpointCustomPropertiesCacheEntry>();

    if (entry == nullptr)
    {
        return false;
    }

    entry->Match = match;
    entry->Properties = properties;

    m_entries.push_back(entry);

    return true;
}


_Use_decl_annotations_
bool MidiEndpointCustomPropertiesCache::AddWithProvenance(
    std::shared_ptr<MidiEndpointMatchCriteria> match,
    std::shared_ptr<MidiEndpointCustomProperties> properties,
    std::shared_ptr<MidiEndpointCustomizationProvenance> provenance,
    winrt::hstring const& resolvedEndpointDeviceId)
{
    auto lock = m_entriesMutex.lock_exclusive();

    std::shared_ptr<MidiEndpointCustomizationProvenance> retainedProvenance{ provenance };

    std::vector<std::shared_ptr<MidiEndpointCustomPropertiesCacheEntry>>::iterator it;

    for (it = m_entries.begin(); it != m_entries.end(); it++)
    {
        if ((*it)->Match->Matches(*match))
        {
            // An update which carries no provenance must not erase what an earlier one recorded.
            if (retainedProvenance == nullptr)
            {
                retainedProvenance = (*it)->Provenance;
            }

            m_entries.erase(it);
            break;
        }
    }

    auto entry = std::make_shared<MidiEndpointCustomPropertiesCacheEntry>();

    if (entry == nullptr)
    {
        return false;
    }

    entry->Match = match;
    entry->Properties = properties;
    entry->Provenance = retainedProvenance;
    entry->ResolvedEndpointDeviceId = resolvedEndpointDeviceId;

    m_entries.push_back(entry);

    return true;
}


std::vector<std::shared_ptr<MidiEndpointCustomPropertiesCacheEntry>> MidiEndpointCustomPropertiesCache::GetAllEntries()
{
    auto lock = m_entriesMutex.lock_shared();

    return m_entries;
}


_Use_decl_annotations_
bool MidiEndpointCustomPropertiesCache::Remove(MidiEndpointMatchCriteria& match)
{
    auto lock = m_entriesMutex.lock_exclusive();

    std::vector<std::shared_ptr<MidiEndpointCustomPropertiesCacheEntry>>::iterator it;

    for (it = m_entries.begin(); it != m_entries.end(); it++)
    {
        if ((*it)->Match->Matches(match))
        {
            m_entries.erase(it);
            return true;
        }
    }

    return false;
}


_Use_decl_annotations_
bool MidiEndpointCustomPropertiesCache::UpdateResolvedEndpointDeviceId(
    MidiEndpointMatchCriteria& match,
    winrt::hstring const& resolvedEndpointDeviceId)
{
    auto lock = m_entriesMutex.lock_exclusive();

    for (auto const& entry : m_entries)
    {
        if (entry->Match->Matches(match))
        {
            entry->ResolvedEndpointDeviceId = resolvedEndpointDeviceId;
            return true;
        }
    }

    return false;
}



}