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


wil::com_ptr<CMidi2NetworkMidiEndpointManager>
TransportState::GetEndpointManager()
{
    auto lock = m_stateLock.lock_shared();

    return m_endpointManager;
}

wil::com_ptr<CMidi2NetworkMidiConfigurationManager>
TransportState::GetConfigurationManager()
{
    auto lock = m_stateLock.lock_shared();

    return m_configurationManager;
}


HRESULT
TransportState::ShutdownHostsClientsAndConnections()
{
    std::vector<std::shared_ptr<MidiNetworkHost>> hosts{ };
    std::vector<std::shared_ptr<MidiNetworkClient>> clients{ };

    {
        auto lock = m_stateLock.lock_exclusive();

        hosts.swap(m_hosts);
        clients.swap(m_clients);

        m_pendingHostDefinitions.clear();
        m_pendingClientDefinitions.clear();
    }

    // Shutdown blocks on the network and re-enters this class, so it happens after the lock
    // is released and after the collections have been detached.
    for (auto const& host : hosts)
    {
        if (host != nullptr)
        {
            LOG_IF_FAILED(host->Shutdown());
        }
    }

    for (auto const& client : clients)
    {
        if (client != nullptr)
        {
            LOG_IF_FAILED(client->Shutdown());
        }
    }

    std::vector<std::shared_ptr<MidiNetworkConnection>> connections{ };

    {
        auto lock = m_stateLock.lock_exclusive();

        for (auto const& entry : m_networkConnections)
        {
            connections.push_back(entry.second);
        }

        m_networkConnections.clear();
        m_sessionConnections.clear();
    }

    for (auto const& connection : connections)
    {
        if (connection != nullptr)
        {
            LOG_IF_FAILED(connection->SendShutdownBye());
        }
    }

    for (auto const& connection : connections)
    {
        if (connection != nullptr)
        {
            LOG_IF_FAILED(connection->Shutdown());
        }
    }

    return S_OK;
}


HRESULT
TransportState::Shutdown()
{
    RETURN_IF_FAILED(ShutdownHostsClientsAndConnections());

    auto lock = m_stateLock.lock_exclusive();

    m_endpointManager.reset();
    m_configurationManager.reset();

    return S_OK;
}


std::wstring
TransportState::GetEffectiveProductInstanceId()
{
    if (!TransportSettings.ProductInstanceId.empty())
    {
        return TransportSettings.ProductInstanceId;
    }

    // reserve() does not change size(), so the name has to be sized before it is written into
    // and resized to the returned length afterward. Otherwise every read of it sees an empty
    // string and the generated identity degrades to "-midisrv".
    DWORD nameLength = MAX_COMPUTERNAME_LENGTH + 1;
    std::wstring machineName;
    machineName.resize(nameLength);

    if (GetComputerName(machineName.data(), &nameLength))
    {
        machineName.resize(nameLength);

        return internal::ToLowerTrimmedWStringCopy(machineName) + L"-midisrv";
    }

    return L"windows-midisrv";
}


HRESULT
TransportState::ConstructEndpointManager()
{
    wil::com_ptr<CMidi2NetworkMidiEndpointManager> endpointManager;
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2NetworkMidiEndpointManager>(&endpointManager));

    auto lock = m_stateLock.lock_exclusive();

    // another Activate call may have won the race. Keep the one already published.
    if (m_endpointManager == nullptr)
    {
        m_endpointManager = endpointManager;
    }

    return S_OK;
}

HRESULT
TransportState::ConstructConfigurationManager()
{
    wil::com_ptr<CMidi2NetworkMidiConfigurationManager> configurationManager;
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2NetworkMidiConfigurationManager>(&configurationManager));

    auto lock = m_stateLock.lock_exclusive();

    // another Activate call may have won the race. Keep the one already published.
    if (m_configurationManager == nullptr)
    {
        m_configurationManager = configurationManager;
    }

    return S_OK;
}




_Use_decl_annotations_
HRESULT 
TransportState::AddHost(
    std::shared_ptr<MidiNetworkHost> host)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, host);

    auto lock = m_stateLock.lock_exclusive();

    m_hosts.push_back(host);

    return S_OK;
}

std::vector<std::shared_ptr<MidiNetworkHost>>
TransportState::GetHosts()
{
    auto lock = m_stateLock.lock_shared();

    return m_hosts;
}

std::vector<std::shared_ptr<MidiNetworkHostDefinition>>
TransportState::GetPendingHostDefinitions()
{
    auto lock = m_stateLock.lock_shared();

    return m_pendingHostDefinitions;
}

std::vector<std::shared_ptr<MidiNetworkClient>>
TransportState::GetClients()
{
    auto lock = m_stateLock.lock_shared();

    return m_clients;
}

std::vector<std::shared_ptr<MidiNetworkClientDefinition>>
TransportState::GetPendingClientDefinitions()
{
    auto lock = m_stateLock.lock_shared();

    return m_pendingClientDefinitions;
}


_Use_decl_annotations_
std::shared_ptr<MidiNetworkHost>
TransportState::GetHost(winrt::guid const& hostEntryIdentifier)
{
    auto lock = m_stateLock.lock_shared();

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
std::shared_ptr<MidiNetworkHost>
TransportState::RemoveHost(winrt::guid const& hostEntryIdentifier, bool& removedPendingDefinition)
{
    std::shared_ptr<MidiNetworkHost> removed{ nullptr };

    auto lock = m_stateLock.lock_exclusive();

    for (auto it = m_hosts.begin(); it != m_hosts.end(); it++)
    {
        if (*it != nullptr && (*it)->GetDefinition().EntryIdentifier == hostEntryIdentifier)
        {
            removed = *it;
            m_hosts.erase(it);
            break;
        }
    }

    // A host which was created but never instantiated exists only as a pending definition, and
    // it holds the service instance name just as a live host does. Erasing it here is also what
    // stops the endpoint creator thread from registering a host for a definition which has been
    // taken away while that host was being built.
    auto const before = m_pendingHostDefinitions.size();

    m_pendingHostDefinitions.erase(
        std::remove_if(
            m_pendingHostDefinitions.begin(),
            m_pendingHostDefinitions.end(),
            [&hostEntryIdentifier](auto const& definition)
            {
                return definition != nullptr && definition->EntryIdentifier == hostEntryIdentifier;
            }),
        m_pendingHostDefinitions.end());

    removedPendingDefinition = m_pendingHostDefinitions.size() != before;

    return removed;
}

// Registers a host only if its definition is still pending. Both this and RemoveHost take the
// state lock exclusively, so a host can no longer be added after the entry it belongs to has
// been removed. Returns false when that happened, and the caller must then shut the host down
// rather than leaving its socket bound and its service instance name claimed.
_Use_decl_annotations_
bool
TransportState::AddHostIfStillPending(std::shared_ptr<MidiNetworkHost> host, winrt::guid const& hostEntryIdentifier)
{
    if (host == nullptr)
    {
        return false;
    }

    auto lock = m_stateLock.lock_exclusive();

    auto const stillPending = std::any_of(
        m_pendingHostDefinitions.begin(),
        m_pendingHostDefinitions.end(),
        [&hostEntryIdentifier](auto const& definition)
        {
            return definition != nullptr && definition->EntryIdentifier == hostEntryIdentifier;
        });

    if (!stillPending)
    {
        return false;
    }

    m_hosts.push_back(host);

    return true;
}

_Use_decl_annotations_
bool
TransportState::IsHostServiceInstanceNameInUse(
    std::wstring const& serviceInstanceName,
    winrt::guid const& excludingEntryIdentifier)
{
    if (serviceInstanceName.empty())
    {
        return false;
    }

    auto wanted = internal::ToLowerTrimmedWStringCopy(serviceInstanceName);

    // Snapshotted first: the accessors take the state lock, and comparing definitions calls into
    // the hosts, which must never happen while that lock is held.
    for (auto const& host : GetHosts())
    {
        if (host == nullptr)
        {
            continue;
        }

        auto definition = host->GetDefinition();

        if (definition.EntryIdentifier == excludingEntryIdentifier)
        {
            continue;
        }

        if (internal::ToLowerTrimmedWStringCopy(std::wstring{ definition.ServiceInstanceName }) == wanted)
        {
            return true;
        }
    }

    // Definitions which have been accepted but not yet started count too, otherwise two entries
    // in the same configuration update would both be admitted.
    for (auto const& definition : GetPendingHostDefinitions())
    {
        if (definition == nullptr)
        {
            continue;
        }

        if (definition->EntryIdentifier == excludingEntryIdentifier)
        {
            continue;
        }

        if (internal::ToLowerTrimmedWStringCopy(std::wstring{ definition->ServiceInstanceName }) == wanted)
        {
            return true;
        }
    }

    return false;
}

_Use_decl_annotations_
bool
TransportState::IsHostPortInUse(
    std::wstring const& port,
    winrt::guid const& excludingEntryIdentifier)
{
    // Compared as numbers, not as text: "05000" and "5000" are the same socket.
    auto const parsePort = [](std::wstring const& value) -> uint32_t
        {
            auto const trimmed = internal::TrimmedWStringCopy(value);

            if (trimmed.empty() ||
                !std::all_of(trimmed.begin(), trimmed.end(), [](wchar_t const ch) { return iswdigit(ch) != 0; }))
            {
                return 0;
            }

            auto const parsed = wcstoul(trimmed.c_str(), nullptr, 10);

            return (parsed >= 1 && parsed <= 65535) ? static_cast<uint32_t>(parsed) : 0;
        };

    auto const wanted = parsePort(port);

    if (wanted == 0)
    {
        return false;
    }

    // Snapshotted first: the accessors take the state lock, and reading a host's socket calls
    // into it, which must never happen while that lock is held.
    for (auto const& host : GetHosts())
    {
        if (host == nullptr)
        {
            continue;
        }

        if (host->GetDefinition().EntryIdentifier == excludingEntryIdentifier)
        {
            continue;
        }

        // The bound port, not the configured one. A host which asked for an automatic port owns
        // whatever the system gave it, and a manual entry must not collide with that either.
        if (parsePort(std::wstring{ host->ActualPort() }) == wanted)
        {
            return true;
        }
    }

    // Entries which have been accepted but not started yet. Only a manual port can be compared;
    // one waiting for an automatic port has no number to conflict with.
    for (auto const& definition : GetPendingHostDefinitions())
    {
        if (definition == nullptr || definition->UseAutomaticPortAllocation)
        {
            continue;
        }

        if (definition->EntryIdentifier == excludingEntryIdentifier)
        {
            continue;
        }

        if (parsePort(std::wstring{ definition->Port }) == wanted)
        {
            return true;
        }
    }

    return false;
}

_Use_decl_annotations_
std::shared_ptr<MidiNetworkClient>
TransportState::GetClient(winrt::guid const& clientEntryIdentifier)
{
    auto lock = m_stateLock.lock_shared();

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

    auto lock = m_stateLock.lock_exclusive();

    m_pendingHostDefinitions.push_back(hostDefinition);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
TransportState::AddClient(
    std::shared_ptr<MidiNetworkClient> client)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, client);

    auto lock = m_stateLock.lock_exclusive();

    m_clients.push_back(client);

    return S_OK;
}

// The mirror of AddHostIfStillPending. Both the add and RemoveClient take the state lock
// exclusively, so a client can no longer be registered after the entry it belongs to has gone.
_Use_decl_annotations_
bool
TransportState::AddClientIfStillPending(
    std::shared_ptr<MidiNetworkClient> client,
    winrt::guid const& clientEntryIdentifier)
{
    if (client == nullptr)
    {
        return false;
    }

    auto lock = m_stateLock.lock_exclusive();

    auto const stillPending = std::any_of(
        m_pendingClientDefinitions.begin(),
        m_pendingClientDefinitions.end(),
        [&clientEntryIdentifier](auto const& definition)
        {
            return definition != nullptr && definition->EntryIdentifier == clientEntryIdentifier;
        });

    if (!stillPending)
    {
        return false;
    }

    m_clients.push_back(client);

    return true;
}

_Use_decl_annotations_
HRESULT 
TransportState::RemoveClient(winrt::guid const& clientConfigEntryIdentifier, bool& removedPendingDefinition)
{
    removedPendingDefinition = false;

    auto lock = m_stateLock.lock_exclusive();

    bool removedLiveClient{ false };

    for (auto it = m_clients.begin(); it != m_clients.end(); it++)
    {
        if ((*it)->GetDefinition().EntryIdentifier == clientConfigEntryIdentifier)
        {
            m_clients.erase(it);

            removedLiveClient = true;

            break;
        }
    }

    // The definition is what enumerateClients walks, and it is also what the endpoint creator
    // thread would use to build the client again on its next pass.
    auto const before = m_pendingClientDefinitions.size();

    m_pendingClientDefinitions.erase(
        std::remove_if(
            m_pendingClientDefinitions.begin(),
            m_pendingClientDefinitions.end(),
            [&clientConfigEntryIdentifier](auto const& definition)
            {
                return definition != nullptr && definition->EntryIdentifier == clientConfigEntryIdentifier;
            }),
        m_pendingClientDefinitions.end());

    removedPendingDefinition = m_pendingClientDefinitions.size() != before;

    return (removedLiveClient || removedPendingDefinition) ? S_OK : E_NOTFOUND;
}

_Use_decl_annotations_
HRESULT
TransportState::RemoveLiveClient(winrt::guid const& clientConfigEntryIdentifier){
    auto lock = m_stateLock.lock_exclusive();

    for (auto it = m_clients.begin(); it != m_clients.end(); it++)
    {
        if ((*it)->GetDefinition().EntryIdentifier == clientConfigEntryIdentifier)
        {
            m_clients.erase(it);

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

    auto lock = m_stateLock.lock_exclusive();

    m_pendingClientDefinitions.push_back(clientDefinition);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
TransportState::MarkClientDefinitionForReconnect(winrt::guid const& clientConfigEntryIdentifier)
{
    auto lock = m_stateLock.lock_exclusive();

    for (auto const& definition : m_pendingClientDefinitions)
    {
        if (definition != nullptr && definition->EntryIdentifier == clientConfigEntryIdentifier)
        {
            if (!definition->Enabled)
            {
                return S_FALSE;
            }

            definition->State = MidiNetworkEntryState::Pending;

            return S_OK;
        }
    }

    return S_FALSE;
}

_Use_decl_annotations_
HRESULT
TransportState::MarkClientDefinitionUnavailableOrRetry(winrt::guid const& clientConfigEntryIdentifier)
{
    auto lock = m_stateLock.lock_exclusive();

    for (auto const& definition : m_pendingClientDefinitions)
    {
        if (definition != nullptr && definition->EntryIdentifier == clientConfigEntryIdentifier)
        {
            if (!definition->Enabled)
            {
                return S_FALSE;
            }

            if (definition->IsDirectConnection())
            {
                definition->State = MidiNetworkEntryState::Unavailable;

                return S_FALSE;
            }

            definition->State = MidiNetworkEntryState::Pending;

            return S_OK;
        }
    }

    return S_FALSE;
}

_Use_decl_annotations_
HRESULT
TransportState::RearmClientDefinition(winrt::guid const& clientConfigEntryIdentifier)
{
    auto lock = m_stateLock.lock_exclusive();

    for (auto const& definition : m_pendingClientDefinitions)
    {
        if (definition != nullptr && definition->EntryIdentifier == clientConfigEntryIdentifier)
        {
            definition->State = MidiNetworkEntryState::Pending;
            definition->Enabled = true;

            return S_OK;
        }
    }

    return S_FALSE;
}

_Use_decl_annotations_
HRESULT
TransportState::MarkClientDefinitionLive(winrt::guid const& clientConfigEntryIdentifier)
{
    auto lock = m_stateLock.lock_exclusive();

    for (auto const& definition : m_pendingClientDefinitions)
    {
        if (definition != nullptr && definition->EntryIdentifier == clientConfigEntryIdentifier)
        {
            definition->State = MidiNetworkEntryState::Live;

            return S_OK;
        }
    }

    return S_FALSE;
}

_Use_decl_annotations_
HRESULT
TransportState::MarkHostDefinitionLive(winrt::guid const& hostConfigEntryIdentifier)
{
    auto lock = m_stateLock.lock_exclusive();

    for (auto const& definition : m_pendingHostDefinitions)
    {
        if (definition != nullptr && definition->EntryIdentifier == hostConfigEntryIdentifier)
        {
            definition->State = MidiNetworkEntryState::Live;

            return S_OK;
        }
    }

    return S_FALSE;
}

_Use_decl_annotations_
HRESULT
TransportState::MarkHostDefinitionFailed(winrt::guid const& hostConfigEntryIdentifier)
{
    auto lock = m_stateLock.lock_exclusive();

    for (auto const& definition : m_pendingHostDefinitions)
    {
        if (definition != nullptr && definition->EntryIdentifier == hostConfigEntryIdentifier)
        {
            definition->State = MidiNetworkEntryState::Failed;

            return S_OK;
        }
    }

    return S_FALSE;
}



_Use_decl_annotations_
HRESULT
TransportState::AssociateMidiEndpointWithConnection(
    _In_ std::wstring endpointDeviceInterfaceId,
    _In_ winrt::Windows::Networking::HostName const& remoteHostName,
    _In_ winrt::hstring const& remotePort)
{
    auto cleanId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);
    RETURN_HR_IF(E_INVALIDARG, cleanId.empty());

    auto key = CreateNetworkConnectionMapKey(remoteHostName, remotePort);

    auto lock = m_stateLock.lock_exclusive();

    // find the connection and then associate it here
    auto entry = m_networkConnections.find(key);

    RETURN_HR_IF(E_FAIL, entry == m_networkConnections.end());

    m_sessionConnections.insert_or_assign(cleanId, entry->second);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
TransportState::DisassociateMidiEndpointFromConnection(
    std::wstring endpointDeviceInterfaceId)
{
    RETURN_HR_IF(E_INVALIDARG, endpointDeviceInterfaceId.empty());
    auto cleanId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);
    RETURN_HR_IF(E_INVALIDARG, cleanId.empty());

    auto lock = m_stateLock.lock_exclusive();

    if (m_sessionConnections.erase(cleanId) == 0)
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

    auto lock = m_stateLock.lock_shared();

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

    auto lock = m_stateLock.lock_shared();

    return m_networkConnections.find(key) != m_networkConnections.end();
}


_Use_decl_annotations_
std::shared_ptr<MidiNetworkConnection>
TransportState::DetachNetworkConnection(
    winrt::Windows::Networking::HostName const& remoteHostName,
    winrt::hstring const& remotePort)
{
    auto key = CreateNetworkConnectionMapKey(remoteHostName, remotePort);

    auto lock = m_stateLock.lock_exclusive();

    if (auto entry = m_networkConnections.find(key); entry != m_networkConnections.end())
    {
        auto connection = entry->second;

        m_networkConnections.erase(entry);

        return connection;
    }

    return nullptr;
}

_Use_decl_annotations_
HRESULT 
TransportState::RemoveNetworkConnection(
    winrt::Windows::Networking::HostName const& remoteHostName, 
    winrt::hstring const& remotePort)
{
    auto key = CreateNetworkConnectionMapKey(remoteHostName, remotePort);

    std::shared_ptr<MidiNetworkConnection> connection{ nullptr };

    {
        auto lock = m_stateLock.lock_exclusive();

        if (auto entry = m_networkConnections.find(key); entry != m_networkConnections.end())
        {
            connection = entry->second;

            m_networkConnections.erase(entry);
        }
    }

    // shutdown blocks and re-enters this class, so it happens outside the lock
    if (connection != nullptr)
    {
        LOG_IF_FAILED(connection->Shutdown());
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

    auto lock = m_stateLock.lock_shared();

    if (auto entry = m_networkConnections.find(key); entry != m_networkConnections.end())
    {
        return entry->second;
    }

    return nullptr;
}

_Use_decl_annotations_
std::vector<std::shared_ptr<MidiNetworkConnection>>
TransportState::GetAllNetworkConnectionsForClient(winrt::guid const& clientEntryConfigIdentifier)
{
    std::vector<std::shared_ptr<MidiNetworkConnection>> results;

    auto lock = m_stateLock.lock_shared();

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
TransportState::GetAllNetworkConnectionsForHost(winrt::guid const& hostEntryConfigIdentifier)
{
    std::vector<std::shared_ptr<MidiNetworkConnection>> results;

    auto lock = m_stateLock.lock_shared();

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
std::vector<std::shared_ptr<MidiNetworkHostConnection>>
TransportState::GetHostConnectionsForHost(winrt::guid const& hostEntryConfigIdentifier)
{
    std::vector<std::shared_ptr<MidiNetworkHostConnection>> results;

    auto lock = m_stateLock.lock_shared();

    for (auto& conn : m_networkConnections)
    {
        if (conn.second->ConfigIdentifier() != hostEntryConfigIdentifier)
        {
            continue;
        }

        // A client entry identifier can never equal a host's, so this only skips something
        // which was mis-registered.
        if (conn.second->Role() == MidiNetworkConnectionRole::ConnectionWindowsIsHost)
        {
            results.push_back(std::static_pointer_cast<MidiNetworkHostConnection>(conn.second));
        }
    }

    return results;
}

_Use_decl_annotations_
size_t
TransportState::CountNetworkConnectionsForConfigIdentifier(winrt::guid const& configEntryIdentifier)
{
    size_t count{ 0 };

    auto lock = m_stateLock.lock_shared();

    for (auto& conn : m_networkConnections)
    {
        if (conn.second->ConfigIdentifier() == configEntryIdentifier)
        {
            count++;
        }
    }

    return count;
}

_Use_decl_annotations_
HRESULT
TransportState::ReapIdleNetworkConnections(winrt::guid const& configEntryIdentifier)
{
    std::vector<std::shared_ptr<MidiNetworkConnection>> reclaimed;

    {
        auto lock = m_stateLock.lock_exclusive();

        for (auto it = m_networkConnections.begin(); it != m_networkConnections.end(); )
        {
            if (it->second->ConfigIdentifier() == configEntryIdentifier && it->second->IsIdleAndReclaimable())
            {
                reclaimed.push_back(it->second);

                it = m_networkConnections.erase(it);
            }
            else
            {
                it++;
            }
        }
    }

    // Shutdown blocks on midisrv (DeleteEndpoint -> RemoveEndpoint), and this is called from the
    // socket receive callback, so it is queued rather than run here. Reaping ~40 aged-out
    // connections inline left the host unable to receive anything for the whole reap.
    auto endpointManager = GetEndpointManager();

    for (auto const& connection : reclaimed)
    {
        if (endpointManager != nullptr)
        {
            LOG_IF_FAILED(endpointManager->QueueConnectionShutdown(connection));
        }
        else
        {
            // no manager means we are tearing down anyway, so there is no callback left to stall
            LOG_IF_FAILED(connection->Shutdown());
        }
    }

    return S_OK;
}

_Use_decl_annotations_
std::vector<std::shared_ptr<MidiNetworkConnection>>
TransportState::DetachNetworkConnectionsForConfigIdentifier(winrt::guid const& configEntryIdentifier)
{
    std::vector<std::shared_ptr<MidiNetworkConnection>> removed;

    for (auto it = m_networkConnections.begin(); it != m_networkConnections.end(); )
    {
        if (it->second->ConfigIdentifier() == configEntryIdentifier)
        {
            removed.push_back(it->second);

            it = m_networkConnections.erase(it);
        }
        else
        {
            it++;
        }
    }

    return removed;
}

_Use_decl_annotations_
HRESULT 
TransportState::RemoveAllNetworkConnectionsForHost(winrt::guid const& hostEntryConfigIdentifier)
{
    auto lock = m_stateLock.lock_exclusive();

    // the caller has already shut these down. This is just housekeeping.
    DetachNetworkConnectionsForConfigIdentifier(hostEntryConfigIdentifier);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
TransportState::RemoveAllNetworkConnectionsForClient(winrt::guid const& clientEntryConfigIdentifier)
{
    auto lock = m_stateLock.lock_exclusive();

    // the caller has already shut these down. This is just housekeeping.
    DetachNetworkConnectionsForConfigIdentifier(clientEntryConfigIdentifier);

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
    RETURN_HR_IF_NULL(E_INVALIDARG, connection);

    auto key = CreateNetworkConnectionMapKey(remoteHostName, remotePort);

    auto lock = m_stateLock.lock_exclusive();

    m_networkConnections.insert_or_assign(key, connection);

    return S_OK;
}

_Use_decl_annotations_
std::shared_ptr<MidiNetworkConnection>
TransportState::AddNetworkConnectionIfAbsent(
    winrt::Windows::Networking::HostName const& remoteHostName,
    winrt::hstring const& remotePort,
    std::shared_ptr<MidiNetworkConnection> connection
)
{
    if (connection == nullptr)
    {
        return nullptr;
    }

    auto key = CreateNetworkConnectionMapKey(remoteHostName, remotePort);

    auto lock = m_stateLock.lock_exclusive();

    auto result = m_networkConnections.emplace(key, connection);

    return result.first->second;
}

