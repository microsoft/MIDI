// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

struct MidiTransportSettings
{
    uint32_t OutboundPingInterval{ MIDI_NETWORK_OUTBOUND_PING_INTERVAL_DEFAULT };
    uint32_t InvitationPendingTimeout{ MIDI_NETWORK_INVITATION_PENDING_TIMEOUT_DEFAULT };
    uint16_t MaxHostConnections{ MIDI_NETWORK_HOST_MAX_CONNECTIONS_DEFAULT };
    uint16_t RetransmitBufferMaxCommandPacketCount{ MIDI_NETWORK_RETRANSMIT_BUFFER_PACKET_COUNT_DEFAULT };
    uint8_t ForwardErrorCorrectionMaxCommandPacketCount{ MIDI_NETWORK_FEC_PACKET_COUNT_DEFAULT };
    uint32_t DirectConnectionScanInterval{ MIDI_NETWORK_DIRECT_CONNECTION_SCAN_INTERVAL_DEFAULT };

    // Machine-wide identity shared by every host and by this PC's client identity. Empty means
    // the configuration did not supply one, so the machine-derived default is used instead.
    std::wstring ProductInstanceId{ };
};



// singleton
//
// Threading: every collection below is reachable from socket thread pool threads, the
// configuration manager, and the endpoint creator thread at the same time. m_stateLock guards
// all of them. No method may call into a host, client, or connection while holding it: those
// calls block on the network and re-enter this class (ending a session disassociates its
// endpoint), so holding the lock across them would both deadlock and let one wedged remote
// stall every other connection.
class TransportState
{

public:
    static TransportState& Current();

    // no copying
    TransportState(_In_ const TransportState&) = delete;
    TransportState& operator=(_In_ const TransportState&) = delete;

    MidiTransportSettings TransportSettings{ };

    // The configured machine-wide product instance id, or a machine-derived one if the
    // configuration did not supply it. Never empty.
    std::wstring GetEffectiveProductInstanceId();


    wil::com_ptr<CMidi2NetworkMidiEndpointManager> GetEndpointManager();

    wil::com_ptr<CMidi2NetworkMidiConfigurationManager> GetConfigurationManager();

    // Stops everything that owns a socket or a worker thread. Deliberately leaves the endpoint
    // and configuration managers alone: the endpoint manager calls this from its own Shutdown,
    // and releasing our reference to it there could destroy it mid-call.
    HRESULT ShutdownHostsClientsAndConnections();

    HRESULT Shutdown();

    HRESULT ConstructEndpointManager();
    HRESULT ConstructConfigurationManager();

    HRESULT AddHost(
        _In_ std::shared_ptr<MidiNetworkHost>);
    std::vector<std::shared_ptr<MidiNetworkHost>> GetHosts();

    HRESULT AddPendingHostDefinition(
        _In_ std::shared_ptr<MidiNetworkHostDefinition>);
    std::vector<std::shared_ptr<MidiNetworkHostDefinition>> GetPendingHostDefinitions();

    std::shared_ptr<MidiNetworkHost> GetHost(_In_ winrt::guid const& hostEntryIdentifier);

    // Detaches a host, and any pending definition sharing its entry identifier, from the state.
    // The host is returned rather than shut down here, because Shutdown blocks on the network
    // and re-enters this class, and the state lock must not be held across that. The caller owns
    // the returned host and must Shutdown() it. Returns nullptr when no live host matched;
    // removedPendingDefinition then says whether an entry was removed before it ever started.
    std::shared_ptr<MidiNetworkHost> RemoveHost(
        _In_ winrt::guid const& hostEntryIdentifier,
        _Out_ bool& removedPendingDefinition);

    // Used by the endpoint creator thread instead of AddHost, so a host cannot be registered
    // after its definition has been removed.
    bool AddHostIfStillPending(
        _In_ std::shared_ptr<MidiNetworkHost> host,
        _In_ winrt::guid const& hostEntryIdentifier);

    // The service instance name becomes both the DNS-SD instance and the virtual parent device
    // id, so two host entries cannot share one. Compared case-insensitively.
    bool IsHostServiceInstanceNameInUse(
        _In_ std::wstring const& serviceInstanceName,
        _In_ winrt::guid const& excludingEntryIdentifier);

    // A manually assigned port has to be free. Checked against the port each started host is
    // actually bound to, which is what catches a manual port colliding with one the system
    // handed out automatically, as well as against other entries which have asked for the same
    // number but have not started yet.
    bool IsHostPortInUse(
        _In_ std::wstring const& port,
        _In_ winrt::guid const& excludingEntryIdentifier);






    HRESULT AddClient(_In_ std::shared_ptr<MidiNetworkClient>);

    // Registers a client only if its definition is still pending, the same way
    // AddHostIfStillPending does for hosts. A client takes seconds to build - invitation,
    // reply, then endpoint creation - and the entry can be removed in that window. Losing
    // that race means the client is unreachable: enumerateClients is definition-driven, so
    // nothing would ever list it, and nothing would ever disconnect it, while it keeps a
    // socket and a live MIDI endpoint. Returns false in that case, and the caller must shut
    // the client down instead.
    bool AddClientIfStillPending(
        _In_ std::shared_ptr<MidiNetworkClient> client,
        _In_ winrt::guid const& clientEntryIdentifier);

    // Removes the live client AND the configured definition it was built from. enumerateClients
    // is definition-driven, so leaving the definition behind keeps reporting an entry the user
    // has already disconnected. Returns S_OK if either was found, E_NOTFOUND if neither was.
    // removedPendingDefinition says whether an entry which never connected was taken away.
    HRESULT RemoveClient(
        _In_ winrt::guid const& clientConfigEntryIdentifier,
        _Out_ bool& removedPendingDefinition);

    // Drops only the live client, keeping the definition. Used when reconnecting an entry: the
    // dead client object has to go, but the entry it was built from is still wanted.
    HRESULT RemoveLiveClient(_In_ winrt::guid const& clientConfigEntryIdentifier);

    std::vector<std::shared_ptr<MidiNetworkClient>> GetClients();

    std::shared_ptr<MidiNetworkClient> GetClient(_In_ winrt::guid const& clientEntryIdentifier);


    HRESULT AddPendingClientDefinition(
        _In_ std::shared_ptr<MidiNetworkClientDefinition>);
    std::vector<std::shared_ptr<MidiNetworkClientDefinition>> GetPendingClientDefinitions();

    // Puts the entry back in front of the endpoint creator worker. Returns S_FALSE when no
    // definition remains, which is how a user-requested disconnect avoids being reconnected.
    HRESULT MarkClientDefinitionForReconnect(_In_ winrt::guid const& clientConfigEntryIdentifier);

    // The remote never answered an invitation. An advertised host is retried when it advertises
    // again, which costs nothing while it is absent. A direct address has no such signal, so it
    // is parked as unavailable rather than retried on a timer. Returns S_OK when it will be
    // retried, S_FALSE when it was parked.
    HRESULT MarkClientDefinitionUnavailableOrRetry(_In_ winrt::guid const& clientConfigEntryIdentifier);

    // The app asking to connect an entry which is already configured: "it is available now, try
    // again". Returns S_FALSE when there is no such definition.
    HRESULT RearmClientDefinition(_In_ winrt::guid const& clientConfigEntryIdentifier);

    // Set by the endpoint creator worker once an entry has been built, or once its definition has
    // been rejected. Every legal transition of MidiNetworkEntryState is one of these calls.
    HRESULT MarkClientDefinitionLive(_In_ winrt::guid const& clientConfigEntryIdentifier);
    HRESULT MarkHostDefinitionLive(_In_ winrt::guid const& hostConfigEntryIdentifier);
    HRESULT MarkHostDefinitionFailed(_In_ winrt::guid const& hostConfigEntryIdentifier);



    // these two sets of functions, and their related maps, work with the same
    // connection objects, just in different states

    // these are for when the connection is associated with a UMP endpoint
    HRESULT AssociateMidiEndpointWithConnection(
        _In_ std::wstring endpointDeviceInterfaceId, 
        _In_ winrt::Windows::Networking::HostName const& remoteHostName,
        _In_ winrt::hstring const& remotePort);

    HRESULT DisassociateMidiEndpointFromConnection(
        _In_ std::wstring endpointDeviceInterfaceId);

    std::shared_ptr<MidiNetworkConnection> GetSessionConnection(
        _In_ std::wstring endpointDeviceInterfaceId);

    // these are for when the connection is first created. They also live through when they become UMP endpoints
    bool NetworkConnectionExists(
        _In_ winrt::Windows::Networking::HostName const& remoteHostName,
        _In_ winrt::hstring const& remotePort);

    std::shared_ptr<MidiNetworkConnection> GetNetworkConnection(
        _In_ winrt::Windows::Networking::HostName const& remoteHostName,
        _In_ winrt::hstring const& remotePort);

    std::vector<std::shared_ptr<MidiNetworkConnection>> GetAllNetworkConnectionsForHost(_In_ winrt::guid const& hostEntryIdentifier);

    // The same set, typed. A host's connections are all host-role, and the approval and
    // enumeration paths need the host-only surface.
    std::vector<std::shared_ptr<MidiNetworkHostConnection>> GetHostConnectionsForHost(_In_ winrt::guid const& hostEntryIdentifier);

    std::vector<std::shared_ptr<MidiNetworkConnection>> GetAllNetworkConnectionsForClient(_In_ winrt::guid const& clientEntryIdentifier);

    size_t CountNetworkConnectionsForConfigIdentifier(_In_ winrt::guid const& configEntryIdentifier);

    // Removes and shuts down connections which have no session and have gone quiet. Safe to call
    // from any thread except a worker belonging to one of the connections being reclaimed.
    HRESULT ReapIdleNetworkConnections(_In_ winrt::guid const& configEntryIdentifier);

    HRESULT RemoveAllNetworkConnectionsForHost(_In_ winrt::guid const& hostEntryIdentifier);

    HRESULT RemoveAllNetworkConnectionsForClient(_In_ winrt::guid const& clientEntryIdentifier);

    HRESULT AddNetworkConnection(
        _In_ winrt::Windows::Networking::HostName const& remoteHostName,
        _In_ winrt::hstring const& remotePort, 
        _In_ std::shared_ptr<MidiNetworkConnection> connection);

    // Inserts only if nothing is mapped to this remote yet, and returns whichever connection
    // ends up in the map. Datagrams from one remote can be dispatched on different thread pool
    // threads, so a separate exists-then-add would build duplicate connections for the same peer.
    std::shared_ptr<MidiNetworkConnection> AddNetworkConnectionIfAbsent(
        _In_ winrt::Windows::Networking::HostName const& remoteHostName,
        _In_ winrt::hstring const& remotePort,
        _In_ std::shared_ptr<MidiNetworkConnection> connection);

    HRESULT RemoveNetworkConnection(
        _In_ winrt::Windows::Networking::HostName const& remoteHostName,
        _In_ winrt::hstring const& remotePort);

    // Removes the entry and hands the connection back WITHOUT shutting it down. Shutdown joins
    // worker threads, which must never happen on the socket receive callback.
    std::shared_ptr<MidiNetworkConnection> DetachNetworkConnection(
        _In_ winrt::Windows::Networking::HostName const& remoteHostName,
        _In_ winrt::hstring const& remotePort);


private:
    TransportState();
    ~TransportState();

    wil::srwlock m_stateLock;

    wil::com_ptr<CMidi2NetworkMidiEndpointManager> m_endpointManager;
    wil::com_ptr<CMidi2NetworkMidiConfigurationManager> m_configurationManager;

    std::vector<std::shared_ptr<MidiNetworkHost>> m_hosts{ };
    std::vector<std::shared_ptr<MidiNetworkClient>> m_clients{ };

    std::vector<std::shared_ptr<MidiNetworkHostDefinition>> m_pendingHostDefinitions{ };
    std::vector<std::shared_ptr<MidiNetworkClientDefinition>> m_pendingClientDefinitions{ };

    std::map<std::wstring, std::shared_ptr<MidiNetworkConnection>> m_sessionConnections{ };

    // Map of MidiNetworkConnections and their related remote client addresses
    // the keys for these two maps are the values created with CreateConnectionMapKey
    std::map<std::string, std::shared_ptr<MidiNetworkConnection>> m_networkConnections{ };

    inline std::string CreateNetworkConnectionMapKey(_In_ winrt::Windows::Networking::HostName const& remoteHostName, _In_ winrt::hstring const& remotePort)
    {
        return winrt::to_string(remoteHostName.CanonicalName() + L":" + remotePort);
    }

    // caller already holds m_stateLock exclusively
    std::vector<std::shared_ptr<MidiNetworkConnection>> DetachNetworkConnectionsForConfigIdentifier(_In_ winrt::guid const& configEntryIdentifier);

};