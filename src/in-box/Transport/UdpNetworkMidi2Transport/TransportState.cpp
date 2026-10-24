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
try
{
    std::vector<std::shared_ptr<MidiNetworkHost>> hosts{ };
    std::vector<std::shared_ptr<MidiNetworkClient>> clients{ };

    {
        auto lock = m_stateLock.lock_exclusive();

        hosts.swap(m_hosts);
        clients.swap(m_clients);

        m_hostDefinitions.clear();
        m_clientDefinitions.clear();
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
    std::map<std::wstring, std::shared_ptr<MidiNetworkConnection>> sessionConnections{ };

    {
        auto lock = m_stateLock.lock_exclusive();

        for (auto const& entry : m_networkConnections)
        {
            connections.push_back(entry.second);
        }

        m_networkConnections.clear();

        // Released after the lock, in case one of these is the last reference
        sessionConnections.swap(m_sessionConnections);
    }

    // A session can outlive its entry in the connection map, so these are shut down too
    for (auto const& [id, connection] : sessionConnections)
    {
        if (connection != nullptr &&
            std::find(connections.begin(), connections.end(), connection) == connections.end())
        {
            connections.push_back(connection);
        }
    }

    sessionConnections.clear();

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
CATCH_RETURN()


HRESULT
TransportState::Shutdown()
try
{
    RETURN_IF_FAILED(ShutdownHostsClientsAndConnections());

    // Both managers hold service objects. This class is a function-local static, so anything
    // still held here is released at DLL unload, after the service has destroyed those objects.
    wil::com_ptr<CMidi2NetworkMidiEndpointManager> endpointManager;
    wil::com_ptr<CMidi2NetworkMidiConfigurationManager> configurationManager;

    {
        auto lock = m_stateLock.lock_exclusive();

        endpointManager.swap(m_endpointManager);
        configurationManager.swap(m_configurationManager);
    }

    // Released outside the lock, in case this is the last reference and a destructor runs
    endpointManager.reset();
    configurationManager.reset();

    // Its threadpool work otherwise drains in the static destructor, at DLL unload. Every host
    // and connection which could signal has been shut down above.
    m_notificationSignal.Shutdown();

    return S_OK;
}
CATCH_RETURN()


// The machine-wide identity every host and this PC's client identity are built from. Derived
// rather than configurable: a remote recognizes this PC by it, so letting it be edited silently
// breaks every remote which has already stored it.
std::wstring
TransportState::GetEffectiveProductInstanceId()
{
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


// Every way a client starts has to arrive at this same name. A host knows this PC by the name
// and the product instance id, so a name that changes between the first connection and a
// restart makes this PC look like a new device to it. The computer name as Windows reports it,
// which is also what a new host on this PC is called, because the spec asks a device to use one
// name whether it is the host or the client.
std::wstring
TransportState::GetEffectiveLocalEndpointName()
{
    DWORD nameLength = MAX_COMPUTERNAME_LENGTH + 1;
    std::wstring machineName;
    machineName.resize(nameLength);

    if (GetComputerName(machineName.data(), &nameLength) && nameLength > 0)
    {
        machineName.resize(nameLength);

        return machineName;
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




std::vector<std::shared_ptr<MidiNetworkHost>>
TransportState::GetHosts()
{
    auto lock = m_stateLock.lock_shared();

    return m_hosts;
}

std::vector<MidiNetworkHostDefinition>
TransportState::GetHostDefinitions()
{
    auto lock = m_stateLock.lock_shared();

    return m_hostDefinitions;
}

_Use_decl_annotations_
std::optional<MidiNetworkHostDefinition>
TransportState::GetHostDefinition(winrt::guid const& hostEntryIdentifier)
{
    auto lock = m_stateLock.lock_shared();

    for (auto const& definition : m_hostDefinitions)
    {
        if (definition.EntryIdentifier == hostEntryIdentifier)
        {
            return definition;
        }
    }

    return std::nullopt;
}

_Use_decl_annotations_
HRESULT
TransportState::UpdateHostDefinition(
    winrt::guid const& hostEntryIdentifier,
    std::function<void(MidiNetworkHostDefinition&)> const& update)
{
    auto lock = m_stateLock.lock_exclusive();

    for (auto& definition : m_hostDefinitions)
    {
        if (definition.EntryIdentifier == hostEntryIdentifier)
        {
            update(definition);

            return S_OK;
        }
    }

    return S_FALSE;
}

std::vector<std::shared_ptr<MidiNetworkClient>>
TransportState::GetClients()
{
    auto lock = m_stateLock.lock_shared();

    return m_clients;
}

std::vector<MidiNetworkClientDefinition>
TransportState::GetClientDefinitions()
{
    auto lock = m_stateLock.lock_shared();

    return m_clientDefinitions;
}

_Use_decl_annotations_
std::optional<MidiNetworkClientDefinition>
TransportState::GetClientDefinition(winrt::guid const& clientEntryIdentifier)
{
    auto lock = m_stateLock.lock_shared();

    for (auto const& definition : m_clientDefinitions)
    {
        if (definition.EntryIdentifier == clientEntryIdentifier)
        {
            return definition;
        }
    }

    return std::nullopt;
}

_Use_decl_annotations_
HRESULT
TransportState::UpdateClientDefinition(
    winrt::guid const& clientEntryIdentifier,
    std::function<void(MidiNetworkClientDefinition&)> const& update)
{
    auto lock = m_stateLock.lock_exclusive();

    for (auto& definition : m_clientDefinitions)
    {
        if (definition.EntryIdentifier == clientEntryIdentifier)
        {
            update(definition);

            return S_OK;
        }
    }

    return S_FALSE;
}


_Use_decl_annotations_
std::shared_ptr<MidiNetworkHost>
TransportState::GetHost(winrt::guid const& hostEntryIdentifier)
{
    auto lock = m_stateLock.lock_shared();

    for (auto& host : m_hosts)
    {
        if (host->EntryIdentifier() == hostEntryIdentifier)
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
        if (*it != nullptr && (*it)->EntryIdentifier() == hostEntryIdentifier)
        {
            removed = *it;
            m_hosts.erase(it);
            break;
        }
    }

    // A host which was created but never instantiated exists only as a definition, and it holds
    // the service instance name just as a live host does. Erasing it here is also what stops the
    // endpoint creator thread from registering a host for a definition which has been taken away
    // while that host was being built.
    auto const before = m_hostDefinitions.size();

    std::erase_if(
        m_hostDefinitions,
        [&hostEntryIdentifier](auto const& definition)
        {
            return definition.EntryIdentifier == hostEntryIdentifier;
        });

    removedPendingDefinition = m_hostDefinitions.size() != before;

    return removed;
}

// Registers a host only if its definition is still there. Both this and RemoveHost take the state
// lock exclusively, so a host can no longer be added after the entry it belongs to has been
// removed. Returns false when that happened, and the caller must then shut the host down rather
// than leaving its socket bound and its service instance name claimed.
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
        m_hostDefinitions.begin(),
        m_hostDefinitions.end(),
        [&hostEntryIdentifier](auto const& definition)
        {
            return definition.EntryIdentifier == hostEntryIdentifier;
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
    for (auto const& definition : GetHostDefinitions())
    {
        if (definition.EntryIdentifier == excludingEntryIdentifier)
        {
            continue;
        }

        if (internal::ToLowerTrimmedWStringCopy(std::wstring{ definition.ServiceInstanceName }) == wanted)
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

        if (host->EntryIdentifier() == excludingEntryIdentifier)
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
    for (auto const& definition : GetHostDefinitions())
    {
        if (definition.UseAutomaticPortAllocation)
        {
            continue;
        }

        if (definition.EntryIdentifier == excludingEntryIdentifier)
        {
            continue;
        }

        if (parsePort(std::wstring{ definition.Port }) == wanted)
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
        if (client->EntryIdentifier() == clientEntryIdentifier)
        {
            return client;
        }
    }

    return nullptr;
}



_Use_decl_annotations_
HRESULT
TransportState::AddHostDefinition(
    MidiNetworkHostDefinition const& hostDefinition)
{
    auto lock = m_stateLock.lock_exclusive();

    m_hostDefinitions.push_back(hostDefinition);

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
        m_clientDefinitions.begin(),
        m_clientDefinitions.end(),
        [&clientEntryIdentifier](auto const& definition)
        {
            return definition.EntryIdentifier == clientEntryIdentifier;
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
TransportState::RemoveClient(
    winrt::guid const& clientConfigEntryIdentifier,
    bool& removedPendingDefinition,
    std::shared_ptr<MidiNetworkClient>& removedLiveClient)
{
    removedPendingDefinition = false;
    removedLiveClient = nullptr;

    auto lock = m_stateLock.lock_exclusive();

    bool removedLiveClientFlag{ false };

    for (auto it = m_clients.begin(); it != m_clients.end(); it++)
    {
        if ((*it)->EntryIdentifier() == clientConfigEntryIdentifier)
        {
            removedLiveClient = *it;
            m_clients.erase(it);

            removedLiveClientFlag = true;

            break;
        }
    }

    // The definition is what enumerateClients walks, and it is also what the endpoint creator
    // thread would use to build the client again on its next pass.
    auto const before = m_clientDefinitions.size();

    std::erase_if(
        m_clientDefinitions,
        [&clientConfigEntryIdentifier](auto const& definition)
        {
            return definition.EntryIdentifier == clientConfigEntryIdentifier;
        });

    removedPendingDefinition = m_clientDefinitions.size() != before;

    return (removedLiveClientFlag || removedPendingDefinition) ? S_OK : E_NOTFOUND;
}

_Use_decl_annotations_
HRESULT
TransportState::RemoveLiveClient(winrt::guid const& clientConfigEntryIdentifier){
    auto lock = m_stateLock.lock_exclusive();

    for (auto it = m_clients.begin(); it != m_clients.end(); it++)
    {
        if ((*it)->EntryIdentifier() == clientConfigEntryIdentifier)
        {
            m_clients.erase(it);

            return S_OK;
        }
    }

    return E_NOTFOUND;
}


_Use_decl_annotations_
HRESULT
TransportState::AddClientDefinition(
    MidiNetworkClientDefinition const& clientDefinition)
{
    auto lock = m_stateLock.lock_exclusive();

    m_clientDefinitions.push_back(clientDefinition);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
TransportState::MarkClientDefinitionForReconnect(winrt::guid const& clientConfigEntryIdentifier)
{
    auto lock = m_stateLock.lock_exclusive();

    for (auto& definition : m_clientDefinitions)
    {
        if (definition.EntryIdentifier == clientConfigEntryIdentifier)
        {
            if (!definition.Enabled)
            {
                return S_FALSE;
            }

            definition.State = MidiNetworkEntryState::Pending;

            definition.EndedByRemote = false;

            return S_OK;
        }
    }

    return S_FALSE;
}

_Use_decl_annotations_
HRESULT
TransportState::MarkClientDefinitionForRetry(
    winrt::guid const& clientConfigEntryIdentifier,
    uint32_t const errorCode)
{
    auto lock = m_stateLock.lock_exclusive();

    for (auto& definition : m_clientDefinitions)
    {
        if (definition.EntryIdentifier == clientConfigEntryIdentifier)
        {
            if (!definition.Enabled)
            {
                return S_FALSE;
            }

            definition.State = MidiNetworkEntryState::Pending;

            definition.EndedByRemote = false;
            definition.ClearWaitingForOtherSession();
            definition.LastErrorCode = errorCode;

            // An advertised host is only tried while it advertises. Nothing announces a direct one.
            if (definition.IsDirectConnection())
            {
                definition.RetryNotBeforeTickCount = GetTickCount64() + TransportSettings.DirectConnectionScanInterval;
            }

            return S_OK;
        }
    }

    return S_FALSE;
}

_Use_decl_annotations_
bool
TransportState::MarkClientDefinitionUnanswered(winrt::guid const& clientConfigEntryIdentifier)
{
    auto lock = m_stateLock.lock_exclusive();

    for (auto& definition : m_clientDefinitions)
    {
        if (definition.EntryIdentifier == clientConfigEntryIdentifier)
        {
            if (!definition.Enabled)
            {
                return false;
            }

            definition.State = MidiNetworkEntryState::Pending;

            definition.EndedByRemote = false;
            definition.ClearWaitingForOtherSession();
            definition.LastErrorCode = NETWORK_ERROR_CODE_NO_REPLY_TO_INVITATION;
            definition.UnansweredAttempts++;

            if (::WindowsMidiServicesInternal::IsNextMidiNetworkAddressUntried(definition.UnansweredAttempts, definition.AttemptAddressCount))
            {
                definition.RetryNotBeforeTickCount = 0;

                return true;
            }

            if (definition.IsDirectConnection())
            {
                definition.RetryNotBeforeTickCount = GetTickCount64() + TransportSettings.DirectConnectionScanInterval;
            }

            return false;
        }
    }

    return false;
}

_Use_decl_annotations_
HRESULT
TransportState::MarkClientDefinitionAddressUnreachable(winrt::guid const& clientConfigEntryIdentifier)
{
    auto lock = m_stateLock.lock_exclusive();

    for (auto& definition : m_clientDefinitions)
    {
        if (definition.EntryIdentifier == clientConfigEntryIdentifier)
        {
            definition.UnansweredAttempts++;

            return S_OK;
        }
    }

    return S_FALSE;
}

_Use_decl_annotations_
HRESULT
TransportState::MarkClientDefinitionForRetryAfter(
    winrt::guid const& clientConfigEntryIdentifier,
    uint32_t const delayMilliseconds,
    uint32_t const errorCode)
{
    auto lock = m_stateLock.lock_exclusive();

    for (auto& definition : m_clientDefinitions)
    {
        if (definition.EntryIdentifier == clientConfigEntryIdentifier)
        {
            if (!definition.Enabled)
            {
                return S_FALSE;
            }

            definition.State = MidiNetworkEntryState::Pending;

            definition.EndedByRemote = false;
            definition.RetryNotBeforeTickCount = GetTickCount64() + delayMilliseconds;
            definition.ClearWaitingForOtherSession();
            definition.LastErrorCode = errorCode;

            return S_OK;
        }
    }

    return S_FALSE;
}

_Use_decl_annotations_
HRESULT
TransportState::MarkClientDefinitionFailed(
    winrt::guid const& clientConfigEntryIdentifier,
    uint32_t const errorCode)
{
    auto lock = m_stateLock.lock_exclusive();

    for (auto& definition : m_clientDefinitions)
    {
        if (definition.EntryIdentifier == clientConfigEntryIdentifier)
        {
            if (!definition.Enabled)
            {
                return S_FALSE;
            }

            definition.State = MidiNetworkEntryState::Failed;

            definition.EndedByRemote = false;
            definition.ClearWaitingForOtherSession();
            definition.LastErrorCode = errorCode;

            return S_OK;
        }
    }

    return S_FALSE;
}

_Use_decl_annotations_
HRESULT
TransportState::MarkClientDefinitionSessionOpened(winrt::guid const& clientConfigEntryIdentifier)
{
    auto lock = m_stateLock.lock_exclusive();

    for (auto& definition : m_clientDefinitions)
    {
        if (definition.EntryIdentifier == clientConfigEntryIdentifier)
        {
            definition.ClearWaitingForOtherSession();
            definition.LastErrorCode = 0;
            definition.ConnectedAddress = definition.AttemptAddress;
            definition.UnansweredAttempts = 0;

            return S_OK;
        }
    }

    return S_FALSE;
}

_Use_decl_annotations_
HRESULT
TransportState::MarkClientDefinitionWaitingForSession(
    winrt::guid const& clientConfigEntryIdentifier,
    winrt::guid const& blockingEntryIdentifier,
    bool const blockingEntryIsHost,
    std::wstring const& remoteEndpointName,
    std::wstring const& remoteProductInstanceId)
{
    auto lock = m_stateLock.lock_exclusive();

    for (auto& definition : m_clientDefinitions)
    {
        if (definition.EntryIdentifier == clientConfigEntryIdentifier)
        {
            if (!definition.Enabled)
            {
                return S_FALSE;
            }

            // Pending, so the worker looks at it again, but it is only tried once the other
            // session has ended. That is checked on every pass, and a session ending wakes it.
            definition.State = MidiNetworkEntryState::Pending;
            definition.EndedByRemote = false;
            definition.RetryNotBeforeTickCount = 0;
            definition.LastErrorCode = 0;

            definition.WaitingForEntryIdentifier = blockingEntryIdentifier;
            definition.WaitingForHostSession = blockingEntryIsHost;
            definition.WaitingRemoteEndpointName = remoteEndpointName;
            definition.WaitingRemoteProductInstanceId = remoteProductInstanceId;

            return S_OK;
        }
    }

    return S_FALSE;
}

_Use_decl_annotations_
HRESULT
TransportState::ClearClientDefinitionWaitingForSession(winrt::guid const& clientConfigEntryIdentifier)
{
    auto lock = m_stateLock.lock_exclusive();

    for (auto& definition : m_clientDefinitions)
    {
        if (definition.EntryIdentifier == clientConfigEntryIdentifier)
        {
            definition.ClearWaitingForOtherSession();

            return S_OK;
        }
    }

    return S_FALSE;
}

bool
TransportState::AnyClientDefinitionWaitingForSession()
{
    auto lock = m_stateLock.lock_shared();

    return std::any_of(m_clientDefinitions.begin(), m_clientDefinitions.end(),
        [](MidiNetworkClientDefinition const& definition) { return definition.IsWaitingForOtherSession(); });
}
_Use_decl_annotations_
HRESULT
TransportState::MarkClientDefinitionEndedByRemote(
    winrt::guid const& clientConfigEntryIdentifier,
    uint32_t const errorCode)
{
    auto lock = m_stateLock.lock_exclusive();

    for (auto& definition : m_clientDefinitions)
    {
        if (definition.EntryIdentifier == clientConfigEntryIdentifier)
        {
            if (!definition.Enabled)
            {
                return S_FALSE;
            }

            definition.State = MidiNetworkEntryState::Failed;
            definition.EndedByRemote = true;
            definition.RetryNotBeforeTickCount = 0;
            definition.ClearWaitingForOtherSession();
            definition.LastErrorCode = errorCode;

            return S_OK;
        }
    }

    return S_FALSE;
}

_Use_decl_annotations_
uint32_t
TransportState::RearmClientDefinitionsEndedByRemote(std::function<bool(MidiNetworkClientDefinition const&)> const& isAdvertisedAgain)
{
    uint32_t count{ 0 };

    try
    {
        auto lock = m_stateLock.lock_exclusive();

        for (auto& definition : m_clientDefinitions)
        {
            if (definition.State != MidiNetworkEntryState::Failed ||
                !definition.EndedByRemote ||
                !definition.Enabled ||
                definition.IsDirectConnection() ||
                !isAdvertisedAgain(definition))
            {
                continue;
            }

            definition.State = MidiNetworkEntryState::Pending;
            definition.EndedByRemote = false;
            definition.RetryNotBeforeTickCount = 0;
            definition.LastErrorCode = 0;

            count++;
        }
    }
    CATCH_LOG();

    return count;
}

_Use_decl_annotations_
HRESULT
TransportState::RearmClientDefinition(winrt::guid const& clientConfigEntryIdentifier)
{
    auto lock = m_stateLock.lock_exclusive();

    for (auto& definition : m_clientDefinitions)
    {
        if (definition.EntryIdentifier == clientConfigEntryIdentifier)
        {
            definition.State = MidiNetworkEntryState::Pending;
            definition.EndedByRemote = false;
            definition.Enabled = true;
            definition.RetryNotBeforeTickCount = 0;
            definition.ClearWaitingForOtherSession();
            definition.LastErrorCode = 0;

            return S_OK;
        }
    }

    return S_FALSE;
}

_Use_decl_annotations_
HRESULT
TransportState::MarkClientDefinitionLive(
    winrt::guid const& clientConfigEntryIdentifier,
    winrt::hstring const& remoteAddress,
    uint32_t const remoteAddressCount)
{
    auto lock = m_stateLock.lock_exclusive();

    for (auto& definition : m_clientDefinitions)
    {
        if (definition.EntryIdentifier == clientConfigEntryIdentifier)
        {
            definition.State = MidiNetworkEntryState::Live;
            definition.EndedByRemote = false;
            definition.AttemptAddress = remoteAddress;
            definition.AttemptAddressCount = remoteAddressCount;

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

    for (auto& definition : m_hostDefinitions)
    {
        if (definition.EntryIdentifier == hostConfigEntryIdentifier)
        {
            definition.State = MidiNetworkEntryState::Live;

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

    for (auto& definition : m_hostDefinitions)
    {
        if (definition.EntryIdentifier == hostConfigEntryIdentifier)
        {
            definition.State = MidiNetworkEntryState::Failed;

            return S_OK;
        }
    }

    return S_FALSE;
}



_Use_decl_annotations_
HRESULT
TransportState::AssociateMidiEndpointWithConnection(
    std::wstring endpointDeviceInterfaceId,
    winrt::guid const& configEntryIdentifier,
    winrt::Windows::Networking::HostName const& remoteHostName,
    winrt::hstring const& remotePort,
    MidiNetworkConnection const* connection)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, connection);
    RETURN_HR_IF_NULL(E_INVALIDARG, remoteHostName);

    auto cleanId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);
    RETURN_HR_IF(E_INVALIDARG, cleanId.empty());

    auto key = CreateNetworkConnectionMapKey(configEntryIdentifier, remoteHostName, remotePort);

    auto lock = m_stateLock.lock_exclusive();

    // find the connection and then associate it here
    auto entry = m_networkConnections.find(key);

    RETURN_HR_IF(E_FAIL, entry == m_networkConnections.end());

    // A connection which has been detached, and replaced by a newer one for the same remote, must
    // not claim the newer one's endpoint
    RETURN_HR_IF(E_FAIL, entry->second.get() != connection);

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

    std::shared_ptr<MidiNetworkConnection> released{ nullptr };

    {
        auto lock = m_stateLock.lock_exclusive();

        auto entry = m_sessionConnections.find(cleanId);

        if (entry == m_sessionConnections.end())
        {
            RETURN_IF_FAILED(E_NOTFOUND);
        }

        // This can be the last reference. It is let go outside the lock, because destroying a
        // connection joins its threads, and they take this lock.
        released = std::move(entry->second);
        m_sessionConnections.erase(entry);
    }

    released.reset();

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

    // An interface id carries its instance id as one whole segment: \\?\swd#midisrv#<instance id>#{...}
    for (auto const& [instanceId, connection] : m_endpointsBeingCreated)
    {
        if (internal::EndpointInterfaceIdContainsString(cleanId, L"#" + instanceId + L"#"))
        {
            return connection;
        }
    }

    return nullptr;
}

_Use_decl_annotations_
HRESULT
TransportState::RegisterEndpointBeingCreated(
    std::wstring const& deviceInstanceId,
    std::shared_ptr<MidiNetworkConnection> const& connection)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, connection);

    auto cleanId = internal::NormalizeDeviceInstanceIdWStringCopy(deviceInstanceId);
    RETURN_HR_IF(E_INVALIDARG, cleanId.empty());

    auto lock = m_stateLock.lock_exclusive();

    m_endpointsBeingCreated.insert_or_assign(std::move(cleanId), connection);

    return S_OK;
}

_Use_decl_annotations_
void
TransportState::UnregisterEndpointBeingCreated(
    std::wstring const& deviceInstanceId,
    MidiNetworkConnection const* connection) noexcept
{
    try
    {
        auto cleanId = internal::NormalizeDeviceInstanceIdWStringCopy(deviceInstanceId);

        auto lock = m_stateLock.lock_exclusive();

        if (auto entry = m_endpointsBeingCreated.find(cleanId);
            entry != m_endpointsBeingCreated.end() && entry->second.get() == connection)
        {
            m_endpointsBeingCreated.erase(entry);
        }
    }
    CATCH_LOG();
}





_Use_decl_annotations_
std::shared_ptr<MidiNetworkConnection>
TransportState::DetachNetworkConnection(
    winrt::guid const& configEntryIdentifier,
    winrt::Windows::Networking::HostName const& remoteHostName,
    winrt::hstring const& remotePort,
    MidiNetworkConnection const* expected)
{
    if (remoteHostName == nullptr)
    {
        return nullptr;
    }

    auto key = CreateNetworkConnectionMapKey(configEntryIdentifier, remoteHostName, remotePort);

    auto lock = m_stateLock.lock_exclusive();

    if (auto entry = m_networkConnections.find(key); entry != m_networkConnections.end())
    {
        if (expected != nullptr && entry->second.get() != expected)
        {
            return nullptr;
        }

        auto connection = entry->second;

        m_networkConnections.erase(entry);

        return connection;
    }

    return nullptr;
}

_Use_decl_annotations_
std::shared_ptr<MidiNetworkConnection> 
TransportState::GetNetworkConnection(
    winrt::guid const& configEntryIdentifier,
    winrt::Windows::Networking::HostName const& remoteHostName, 
    winrt::hstring const& remotePort)
{
    if (remoteHostName == nullptr)
    {
        return nullptr;
    }

    auto key = CreateNetworkConnectionMapKey(configEntryIdentifier, remoteHostName, remotePort);

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
    std::vector<std::shared_ptr<MidiNetworkConnection>> removed;

    {
        auto lock = m_stateLock.lock_exclusive();

        removed = DetachNetworkConnectionsForConfigIdentifier(hostEntryConfigIdentifier);
    }

    // The caller has shut down the ones it knew about, and Shutdown does nothing the second time.
    // Anything admitted after the caller looked is shut down here, and outside the lock, because
    // Shutdown joins threads which take it.
    for (auto const& connection : removed)
    {
        LOG_IF_FAILED(connection->Shutdown());
    }

    return S_OK;
}

_Use_decl_annotations_
std::shared_ptr<MidiNetworkConnection>
TransportState::AddNetworkConnectionIfAbsent(
    winrt::Windows::Networking::HostName const& remoteHostName,
    winrt::hstring const& remotePort,
    std::shared_ptr<MidiNetworkConnection> connection,
    size_t const maxConnectionsForEntry
)
{
    if (connection == nullptr || remoteHostName == nullptr)
    {
        return nullptr;
    }

    auto const configEntryIdentifier = connection->ConfigIdentifier();
    auto key = CreateNetworkConnectionMapKey(configEntryIdentifier, remoteHostName, remotePort);

    auto lock = m_stateLock.lock_exclusive();

    if (auto existing = m_networkConnections.find(key); existing != m_networkConnections.end())
    {
        return existing->second;
    }

    if (maxConnectionsForEntry != SIZE_MAX)
    {
        size_t count{ 0 };

        for (auto const& entry : m_networkConnections)
        {
            if (entry.second->ConfigIdentifier() == configEntryIdentifier)
            {
                count++;
            }
        }

        if (count >= maxConnectionsForEntry)
        {
            return nullptr;
        }
    }

    auto result = m_networkConnections.emplace(std::move(key), connection);

    return result.first->second;
}

