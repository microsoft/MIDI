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
TransportState::AddPendingHostDefinition(std::shared_ptr<MidiNetworkHostDefinition> hostDefinition)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, hostDefinition);

    m_pendingHostDefinitions.push_back(hostDefinition);

    return S_OK;
}


_Use_decl_annotations_
HRESULT
TransportState::AddPendingClientDefinition(std::shared_ptr<MidiNetworkClientDefinition> clientDefinition)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, clientDefinition);

    m_pendingClientDefinitions.push_back(clientDefinition);

    return S_OK;
}


_Use_decl_annotations_
HRESULT
TransportState::AddSessionConnection(_In_ std::wstring endpointDeviceInterfaceId, std::shared_ptr<MidiNetworkConnection> connection)
{
    auto cleanId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);

    m_sessionConnections.insert_or_assign(cleanId, connection);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
TransportState::RemoveSessionConnection(_In_ std::wstring endpointDeviceInterfaceId)
{
    auto cleanId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);

    if (m_sessionConnections.find(cleanId) != m_sessionConnections.end())
    {
//        m_sessionConnections.find(cleanId)->second.reset();
        m_sessionConnections.erase(cleanId);
    }
    else
    {
        RETURN_IF_FAILED(E_NOTFOUND);
    }

    return S_OK;
}

_Use_decl_annotations_
std::shared_ptr<MidiNetworkConnection> TransportState::GetSessionConnection(_In_ std::wstring endpointDeviceInterfaceId)
{
    auto cleanId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);

    if (auto entry = m_sessionConnections.find(cleanId); entry != m_sessionConnections.end())
    {
        return entry->second;
    }

    return nullptr;
}

