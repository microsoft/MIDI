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
TransportState::AddHost(
    std::shared_ptr<MidiNetworkHost> host)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, host);

    m_hosts.push_back(host);

    return S_OK;
}


_Use_decl_annotations_
std::shared_ptr<MidiNetworkHost>
TransportState::GetHost(winrt::hstring hostEntryIdentifier)
{
    for (auto& host : m_hosts)
    {
        if (host->GetDefinition().EntryIdentifier == hostEntryIdentifier)
        {
            return host;
        }
    }

    return nullptr;
}

_Use_decl_annotations_
std::shared_ptr<MidiNetworkClient>
TransportState::GetClient(winrt::hstring clientEntryIdentifier)
{
    for (auto& client : m_clients)
    {
        if (client->GetDefinition().EntryIdentifier == clientEntryIdentifier)
        {
            return client;
        }
    }

    return nullptr;
}



_Use_decl_annotations_
HRESULT
TransportState::AddPendingHostDefinition(
    std::shared_ptr<MidiNetworkHostDefinition> hostDefinition)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, hostDefinition);

    m_pendingHostDefinitions.push_back(hostDefinition);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
TransportState::AddClient(
    std::shared_ptr<MidiNetworkClient> client)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, client);

    m_clients.push_back(client);

    return S_OK;
}

_Use_decl_annotations_
HRESULT 
TransportState::RemoveClient(winrt::hstring clientConfigEntryIdentifier)
{
    for (int i = 0; i < m_clients.size(); i++)
    {
        auto client = m_clients[i];

        if (client->GetDefinition().EntryIdentifier == clientConfigEntryIdentifier)
        {
            m_clients.erase(m_clients.begin() + i);
            return S_OK;
        }
    }

    return E_NOTFOUND;
}


_Use_decl_annotations_
HRESULT
TransportState::AddPendingClientDefinition(
    std::shared_ptr<MidiNetworkClientDefinition> clientDefinition)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, clientDefinition);

    m_pendingClientDefinitions.push_back(clientDefinition);

    return S_OK;
}



_Use_decl_annotations_
HRESULT
TransportState::AssociateMidiEndpointWithConnection(
    _In_ std::wstring endpointDeviceInterfaceId,
    _In_ winrt::Windows::Networking::HostName const& remoteHostName,
    _In_ winrt::hstring const& remotePort)
{
    auto cleanId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);

    // find the connection and then associate it here

    auto connection = GetNetworkConnection(remoteHostName, remotePort);

    if (connection != nullptr)
    {
        m_sessionConnections.insert_or_assign(cleanId, connection);

        return S_OK;
    }
    else
    {
        return E_FAIL;
    }

}

_Use_decl_annotations_
HRESULT
TransportState::DisassociateMidiEndpointFromConnection(
    std::wstring endpointDeviceInterfaceId)
{
    RETURN_HR_IF(E_INVALIDARG, endpointDeviceInterfaceId.empty());
    auto cleanId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);
    RETURN_HR_IF(E_INVALIDARG, cleanId.empty());

    if (auto session = m_sessionConnections.find(cleanId); session != m_sessionConnections.end())
    {
        //m_sessionConnections.find(cleanId)->second.reset();
        m_sessionConnections.erase(cleanId);
    }
    else
    {
        RETURN_IF_FAILED(E_NOTFOUND);
    }

    return S_OK;
}

_Use_decl_annotations_
std::shared_ptr<MidiNetworkConnection> 
TransportState::GetSessionConnection(_In_ std::wstring endpointDeviceInterfaceId)
{
    auto cleanId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);

    if (auto entry = m_sessionConnections.find(cleanId); entry != m_sessionConnections.end())
    {
        return entry->second;
    }

    return nullptr;
}





_Use_decl_annotations_
bool 
TransportState::NetworkConnectionExists(
    winrt::Windows::Networking::HostName const& remoteHostName, 
    winrt::hstring const& port)
{
    auto key = CreateNetworkConnectionMapKey(remoteHostName, port);

    return m_networkConnections.find(key) != m_networkConnections.end();
}


_Use_decl_annotations_
HRESULT 
TransportState::RemoveNetworkConnection(
    winrt::Windows::Networking::HostName const& remoteHostName, 
    winrt::hstring const& remotePort)
{
    if (NetworkConnectionExists(remoteHostName, remotePort))
    {
        auto entry = m_networkConnections.find(CreateNetworkConnectionMapKey(remoteHostName, remotePort));

        LOG_IF_FAILED(entry->second->Shutdown());

        m_networkConnections.erase(CreateNetworkConnectionMapKey(remoteHostName, remotePort));
    }

    return S_OK;
}

_Use_decl_annotations_
std::shared_ptr<MidiNetworkConnection> 
TransportState::GetNetworkConnection(
    winrt::Windows::Networking::HostName const& remoteHostName, 
    winrt::hstring const& remotePort)
{
    auto key = CreateNetworkConnectionMapKey(remoteHostName, remotePort);

    if (NetworkConnectionExists(remoteHostName, remotePort))
    {
        return m_networkConnections.find(key)->second;
    }

    return nullptr;
}

_Use_decl_annotations_
std::vector<std::shared_ptr<MidiNetworkConnection>>
TransportState::GetAllNetworkConnectionsForClient(winrt::hstring const& clientEntryConfigIdentifier)
{
    std::vector<std::shared_ptr<MidiNetworkConnection>> results;

    for (auto& conn : m_networkConnections)
    {
        if (conn.second->ConfigIdentifier() == clientEntryConfigIdentifier)
        {
            results.push_back(conn.second);
        }
    }

    return results;
}

_Use_decl_annotations_
std::vector<std::shared_ptr<MidiNetworkConnection>> 
TransportState::GetAllNetworkConnectionsForHost(winrt::hstring const& hostEntryConfigIdentifier)
{
    std::vector<std::shared_ptr<MidiNetworkConnection>> results;

    for (auto& conn : m_networkConnections)
    {
        if (conn.second->ConfigIdentifier() == hostEntryConfigIdentifier)
        {
            results.push_back(conn.second);
        }
    }

    return results;
}

_Use_decl_annotations_
HRESULT 
TransportState::RemoveAllNetworkConnectionsForHost(winrt::hstring const& hostEntryConfigIdentifier)
{
    do
    {
        auto item = std::find_if(m_networkConnections.begin(), m_networkConnections.end(), [&](const std::pair<std::string, std::shared_ptr<MidiNetworkConnection>>& connection)
        {
            if (connection.second->ConfigIdentifier() == hostEntryConfigIdentifier)
            {
                return true;
            }

            return false;
        });

        // exit if the no matches were found. We're done in that case.
        if (item == m_networkConnections.end())
        {
            break;
        }
        else
        {
            // this is assuming that shutdown has already been peformed on these connections. 
            // we're just doing housekeeping here

            m_networkConnections.erase(item);
        }
    } while (TRUE);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
TransportState::RemoveAllNetworkConnectionsForClient(winrt::hstring const& clientEntryConfigIdentifier)
{
    do
    {
        auto item = std::find_if(m_networkConnections.begin(), m_networkConnections.end(), [&](const std::pair<std::string, std::shared_ptr<MidiNetworkConnection>>& connection)
            {
                if (connection.second->ConfigIdentifier() == clientEntryConfigIdentifier)
                {
                    return true;
                }

                return false;
            });

        // exit if the no matches were found. We're done in that case.
        if (item == m_networkConnections.end())
        {
            break;
        }
        else
        {
            // this is assuming that shutdown has already been peformed on these connections. 
            // we're just doing housekeeping here

            m_networkConnections.erase(item);
        }
    } while (TRUE);

    return S_OK;
}


_Use_decl_annotations_
HRESULT
TransportState::AddNetworkConnection(
    winrt::Windows::Networking::HostName const& remoteHostName,
    winrt::hstring const& remotePort, 
    std::shared_ptr<MidiNetworkConnection> connection
)
{
    auto key = CreateNetworkConnectionMapKey(remoteHostName, remotePort);

    m_networkConnections.insert_or_assign(key, connection);

    return S_OK;
}
