// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"


MidiEndpointTable::MidiEndpointTable() = default;
MidiEndpointTable::~MidiEndpointTable() = default;

MidiEndpointTable& MidiEndpointTable::Current()
{
    // explanation: http://www.modernescpp.com/index.php/thread-safe-initialization-of-data/

    static MidiEndpointTable current;

    return current;
}


_Use_decl_annotations_
wil::com_ptr_nothrow<IMidiBiDi> MidiEndpointTable::GetDeviceEndpointInterfaceForDeviceEndpointId(std::wstring const endpointDeviceId) const noexcept
{
    try
    {
        auto result = m_endpoints.find(endpointDeviceId);

        if (result != m_endpoints.end())
            return result->second.MidiDeviceBiDi;
        else
            return nullptr;
    }
    catch (...)
    {
        return nullptr;
    }
}


_Use_decl_annotations_
void MidiEndpointTable::RemoveEndpointEntry(std::wstring endpointDeviceId) noexcept
{
    try
    {
        auto result = m_endpoints.find(endpointDeviceId);

        if (result != m_endpoints.end())
        {
            m_endpoints.erase(result);
        }
    }
    catch (...)
    {

    }
}
