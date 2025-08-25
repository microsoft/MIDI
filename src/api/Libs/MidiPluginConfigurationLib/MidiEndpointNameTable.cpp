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

#include "MidiDefs.h"
#include "swd_helpers.h"



#include "MidiEndpointNameTable.h"


_Use_decl_annotations_
std::shared_ptr<MidiEndpointNameTable> MidiEndpointNameTable::FromEndpointDeviceId(
    winrt::hstring const& endpointDeviceId)
{
    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
    additionalProperties.Append(STRING_PKEY_MIDI_Midi1PortNameTable);

    auto device = winrt::Windows::Devices::Enumeration::DeviceInformation::CreateFromIdAsync(endpointDeviceId).get();

    if (device != nullptr)
    {
        auto refArray = WindowsMidiServicesInternal::SafeGetSwdBinaryPropertyFromDeviceInformation(
            STRING_PKEY_MIDI_Midi1PortNameTable,
            device
        );

        if (refArray != nullptr)
        {
            auto refData = refArray.Value();

            return MidiEndpointNameTable::FromPropertyData(refData);
        }

        // even if no name table, we should still return the object in case the consumer wants to add new name table entries

        return std::make_shared<MidiEndpointNameTable>();
    }


    return nullptr;
}


_Use_decl_annotations_
std::shared_ptr<MidiEndpointNameTable> MidiEndpointNameTable::FromPropertyData(
    winrt::com_array<uint8_t> const& propertyData)
{
    std::shared_ptr<MidiEndpointNameTable> results = std::make_shared<MidiEndpointNameTable>();

    // ideally, this should get folded into this code and then the header removed. Need to refactor.
    auto nameEntries = WindowsMidiServicesInternal::Midi1PortNaming::ReadMidi1PortNameTableFromPropertyData(propertyData.data(), propertyData.size());

    for (auto const& entry : nameEntries)
    {
        auto ptr = std::make_shared<WindowsMidiServicesInternal::Midi1PortNaming::Midi1PortNameEntry>();

        if (ptr != nullptr)
        {
            memcpy(ptr.get(), &entry, sizeof(WindowsMidiServicesInternal::Midi1PortNaming::Midi1PortNameEntry));

            if (entry.DataFlowFromUserPerspective == MidiFlow::MidiFlowIn)
            {
                results->m_sourceEntries.emplace(ptr->GroupIndex, ptr);
            }
            else
            {
                results->m_destinationEntries.emplace(ptr->GroupIndex, ptr);
            }
        }
    }

    return results;
}


_Use_decl_annotations_
std::shared_ptr<WindowsMidiServicesInternal::Midi1PortNaming::Midi1PortNameEntry> MidiEndpointNameTable::GetEntry(
    uint8_t const groupIndex,
    MidiFlow const portFlow) const
{
    if (portFlow == MidiFlow::MidiFlowIn)
    {
        auto const entry = m_sourceEntries.find(groupIndex);

        if (entry != m_sourceEntries.end())
        {
            return entry->second;
        }
    }
    else if (portFlow == MidiFlow::MidiFlowOut)
    {
        auto const entry = m_destinationEntries.find(groupIndex);

        if (entry != m_destinationEntries.end())
        {
            return entry->second;
        }
    }

    return nullptr;
}

_Use_decl_annotations_
bool MidiEndpointNameTable::SetEntry(
    std::shared_ptr<WindowsMidiServicesInternal::Midi1PortNaming::Midi1PortNameEntry> const newEntry)
{
    if (newEntry == nullptr) return false;

    if (newEntry->DataFlowFromUserPerspective == MidiFlow::MidiFlowIn)
    {
        m_sourceEntries[newEntry->GroupIndex] = newEntry;

        return true;
    }
    else if (newEntry->DataFlowFromUserPerspective == MidiFlow::MidiFlowOut)
    {
        m_destinationEntries[newEntry->GroupIndex] = newEntry;

        return true;
    }

    return false;
}


_Use_decl_annotations_
bool MidiEndpointNameTable::UpdateCustomName(
    uint8_t const groupIndex, 
    MidiFlow const flow, 
    winrt::hstring const& name)
{
    auto entry = GetEntry(groupIndex, flow);

    if (entry != nullptr)
    {
        memset(entry->CustomName, MAXPNAMELEN, 0);
        wcsncpy_s(entry->CustomName, MAXPNAMELEN, name.c_str(), name.size());

        return true;
    }

    return false;
}


_Use_decl_annotations_
bool MidiEndpointNameTable::WriteProperties(
    std::vector<DEVPROPERTY>& destination)
{
    std::vector<WindowsMidiServicesInternal::Midi1PortNaming::Midi1PortNameEntry> entries;

    // copy all the map values over into this temp vector, so we can write to the data pointer
    std::transform(m_sourceEntries.begin(), m_sourceEntries.end(), std::back_inserter(entries), [](auto& p){ return *p.second; });
    std::transform(m_destinationEntries.begin(), m_destinationEntries.end(), std::back_inserter(entries), [](auto& p) { return *p.second; });


    m_nameTablePropertyData.clear();

    if (WindowsMidiServicesInternal::Midi1PortNaming::WriteMidi1PortNameTableToPropertyDataPointer(entries, m_nameTablePropertyData))
    {
        destination.push_back({ { PKEY_MIDI_Midi1PortNameTable, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BINARY, (ULONG)m_nameTablePropertyData.size(), (PVOID)m_nameTablePropertyData.data() });

        return true;
    }

    return false;
}

