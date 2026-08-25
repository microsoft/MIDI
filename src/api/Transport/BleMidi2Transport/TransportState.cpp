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
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2Ble2MidiEndpointManager>(&m_endpointManager));

    return S_OK;
}

HRESULT
TransportState::ConstructConfigurationManager()
{
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2Ble2MidiConfigurationManager>(&m_configurationManager));

    return S_OK;
}


_Use_decl_annotations_
HRESULT
TransportState::AddConnection(std::shared_ptr<MidiBleConnection> connection)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, connection);
    RETURN_HR_IF(E_INVALIDARG, connection->DeviceId().empty());

    auto lock = std::scoped_lock{ m_connectionsLock };

    m_connections.insert_or_assign(connection->DeviceId(), connection);

    return S_OK;
}


_Use_decl_annotations_
HRESULT
TransportState::RemoveConnection(winrt::hstring const& deviceId)
{
    std::shared_ptr<MidiBleConnection> connection{ nullptr };

    {
        auto lock = std::scoped_lock{ m_connectionsLock };

        if (auto entry = m_connections.find(deviceId); entry != m_connections.end())
        {
            connection = entry->second;
            m_connections.erase(entry);
        }
    }

    // shutdown blocks on the writer thread and on Bluetooth, so it never runs under the lock
    if (connection != nullptr)
    {
        LOG_IF_FAILED(connection->Shutdown());
    }

    return S_OK;
}


void
TransportState::ShutdownAllConnections()
{
    std::vector<std::shared_ptr<MidiBleConnection>> connections;

    {
        auto lock = std::scoped_lock{ m_connectionsLock };

        for (auto const& entry : m_connections)
        {
            connections.push_back(entry.second);
        }

        m_connections.clear();
    }

    for (auto const& connection : connections)
    {
        LOG_IF_FAILED(connection->Shutdown());
    }
}


_Use_decl_annotations_
std::shared_ptr<MidiBleConnection>
TransportState::GetConnectionByDeviceId(winrt::hstring const& deviceId)
{
    auto lock = std::scoped_lock{ m_connectionsLock };

    if (auto entry = m_connections.find(deviceId); entry != m_connections.end())
    {
        return entry->second;
    }

    return nullptr;
}


_Use_decl_annotations_
std::shared_ptr<MidiBleConnection>
TransportState::GetConnectionByEndpointDeviceInterfaceId(std::wstring const& endpointDeviceInterfaceId)
{
    auto const normalizedId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);

    auto lock = std::scoped_lock{ m_connectionsLock };

    for (auto const& entry : m_connections)
    {
        if (entry.second != nullptr && entry.second->EndpointDeviceInterfaceId() == normalizedId)
        {
            return entry.second;
        }
    }

    return nullptr;
}


std::vector<std::shared_ptr<MidiBleConnection>>
TransportState::GetConnections()
{
    auto lock = std::scoped_lock{ m_connectionsLock };

    std::vector<std::shared_ptr<MidiBleConnection>> connections;
    connections.reserve(m_connections.size());

    for (auto const& entry : m_connections)
    {
        connections.push_back(entry.second);
    }

    return connections;
}




//_Use_decl_annotations_
//HRESULT 
//TransportState::AddHost(
//    std::shared_ptr<MidiNetworkHost> host)
//{
//    RETURN_HR_IF_NULL(E_INVALIDARG, host);
//
//    m_hosts.push_back(host);
//
//    return S_OK;
//}

//_Use_decl_annotations_
//HRESULT
//TransportState::AddPendingHostDefinition(
//    std::shared_ptr<MidiNetworkHostDefinition> hostDefinition)
//{
//    RETURN_HR_IF_NULL(E_INVALIDARG, hostDefinition);
//
//    m_pendingHostDefinitions.push_back(hostDefinition);
//
//    return S_OK;
//}

//_Use_decl_annotations_
//HRESULT
//TransportState::AddClient(
//    std::shared_ptr<MidiNetworkClient> client)
//{
//    RETURN_HR_IF_NULL(E_INVALIDARG, client);
//
//    m_clients.push_back(client);
//
//    return S_OK;
//}


//_Use_decl_annotations_
//HRESULT
//TransportState::AddPendingClientDefinition(
//    std::shared_ptr<MidiNetworkClientDefinition> clientDefinition)
//{
//    RETURN_HR_IF_NULL(E_INVALIDARG, clientDefinition);
//
//    m_pendingClientDefinitions.push_back(clientDefinition);
//
//    return S_OK;
//}



//_Use_decl_annotations_
//HRESULT
//TransportState::AssociateMidiEndpointWithConnection(
//    _In_ std::wstring endpointDeviceInterfaceId,
//    _In_ winrt::Windows::Networking::HostName const& remoteHostName,
//    _In_ winrt::hstring const& remotePort)
//{
//    auto cleanId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);
//
//    // find the connection and then associate it here
//
//    auto connection = GetNetworkConnection(remoteHostName, remotePort);
//
//    if (connection != nullptr)
//    {
//        m_sessionConnections.insert_or_assign(cleanId, connection);
//
//        return S_OK;
//    }
//    else
//    {
//        return E_FAIL;
//    }
//
//}

//_Use_decl_annotations_
//HRESULT
//TransportState::DisassociateMidiEndpointFromConnection(
//    std::wstring endpointDeviceInterfaceId)
//{
//    RETURN_HR_IF(E_INVALIDARG, endpointDeviceInterfaceId.empty());
//    auto cleanId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);
//    RETURN_HR_IF(E_INVALIDARG, cleanId.empty());
//
//    if (auto session = m_sessionConnections.find(cleanId); session != m_sessionConnections.end())
//    {
//        //m_sessionConnections.find(cleanId)->second.reset();
//        m_sessionConnections.erase(cleanId);
//    }
//    else
//    {
//        RETURN_IF_FAILED(E_NOTFOUND);
//    }
//
//    return S_OK;
//}

//_Use_decl_annotations_
//std::shared_ptr<MidiNetworkConnection> 
//TransportState::GetSessionConnection(_In_ std::wstring endpointDeviceInterfaceId)
//{
//    auto cleanId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);
//
//    if (auto entry = m_sessionConnections.find(cleanId); entry != m_sessionConnections.end())
//    {
//        return entry->second;
//    }
//
//    return nullptr;
//}





//_Use_decl_annotations_
//bool 
//TransportState::NetworkConnectionExists(
//    winrt::Windows::Networking::HostName const& remoteHostName, 
//    winrt::hstring const& port)
//{
//    auto key = CreateNetworkConnectionMapKey(remoteHostName, port);
//
//    return m_networkConnections.find(key) != m_networkConnections.end();
//}


//_Use_decl_annotations_
//HRESULT 
//TransportState::RemoveNetworkConnection(
//    winrt::Windows::Networking::HostName const& remoteHostName, 
//    winrt::hstring const& remotePort)
//{
//    if (NetworkConnectionExists(remoteHostName, remotePort))
//    {
//        auto entry = m_networkConnections.find(CreateNetworkConnectionMapKey(remoteHostName, remotePort));
//
//        LOG_IF_FAILED(entry->second->Shutdown());
//
//        m_networkConnections.erase(CreateNetworkConnectionMapKey(remoteHostName, remotePort));
//    }
//
//    return S_OK;
//}

//_Use_decl_annotations_
//std::shared_ptr<MidiNetworkConnection> 
//TransportState::GetNetworkConnection(
//    winrt::Windows::Networking::HostName const& remoteHostName, 
//    winrt::hstring const& remotePort)
//{
//    auto key = CreateNetworkConnectionMapKey(remoteHostName, remotePort);
//
//    if (NetworkConnectionExists(remoteHostName, remotePort))
//    {
//        return m_networkConnections.find(key)->second;
//    }
//
//    return nullptr;
//}


//_Use_decl_annotations_
//HRESULT
//TransportState::AddNetworkConnection(
//    winrt::Windows::Networking::HostName const& remoteHostName,
//    winrt::hstring const& remotePort, 
//    std::shared_ptr<MidiNetworkConnection> connection
//)
//{
//    auto key = CreateNetworkConnectionMapKey(remoteHostName, remotePort);
//
//    m_networkConnections.insert_or_assign(key, connection);
//
//    return S_OK;
//}
