// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"


TransportState::TransportState() = default;
TransportState::~TransportState() = default;

TransportState& TransportState::Current()
{
    // explanation: http://www.modernescpp.com/index.php/thread-safe-initialization-of-data/

    static TransportState current;

    return current;
}



HRESULT
TransportState::ConstructEndpointManager()
{
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2NetworkMidiEndpointManager>(&m_endpointManager));

    return S_OK;
}


HRESULT
TransportState::ConstructConfigurationManager()
{
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2NetworkMidiConfigurationManager>(&m_configurationManager));

    return S_OK;
}


_Use_decl_annotations_
HRESULT 
TransportState::AddHost(std::shared_ptr<MidiNetworkHost> host)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, host);

    m_hosts.push_back(host);

    return S_OK;
}



_Use_decl_annotations_
HRESULT
TransportState::AddSessionConnection(_In_ std::wstring endpointDeviceInterfaceId, std::shared_ptr<MidiNetworkConnection> connection)
{
    m_sessionConnections.insert_or_assign(endpointDeviceInterfaceId, connection);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
TransportState::RemoveSessionConnection(_In_ std::wstring endpointDeviceInterfaceId)
{
    if (m_sessionConnections.find(endpointDeviceInterfaceId) != m_sessionConnections.end())
    {
        m_sessionConnections.erase(endpointDeviceInterfaceId);
    }

    return S_OK;
}

_Use_decl_annotations_
std::shared_ptr<MidiNetworkConnection> TransportState::GetSessionConnection(_In_ std::wstring endpointDeviceInterfaceId)
{
    if (auto entry = m_sessionConnections.find(endpointDeviceInterfaceId); entry != m_sessionConnections.end())
    {
        return entry->second;
    }

    return nullptr;
}

