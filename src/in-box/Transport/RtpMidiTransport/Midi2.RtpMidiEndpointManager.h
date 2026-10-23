// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Runs the configured hosts and clients, and gives every connection an endpoint.
//
// All device manager calls happen on one worker thread. Network threads only queue work for it,
// because they are the data path for every other connection on the same port pair.
// ============================================================================

#pragma once

class CMidi2RtpMidiEndpointManager :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiEndpointManager>,
    public RtpMidiNode::IListener
{
public:
    STDMETHOD(Initialize)(_In_ IMidiDeviceManager* midiDeviceManager, _In_ IMidiEndpointProtocolManager* midiEndpointProtocolManager);
    STDMETHOD(Shutdown)();

    bool IsInitialized() const noexcept { return m_initialized.load(); }

    // Hosts and clients are brought in line with the definitions on the worker thread
    void WakeWorker() noexcept;

    // Invites again now, rather than after the retry wait
    HRESULT ReconnectClient(_In_ GUID const& entryId);

    // Ends one connection. The connection id is the one the enumerate commands report.
    HRESULT DisconnectConnection(_In_ GUID const& entryId, _In_ uint32_t const connectionId);

    // Ends every connection a remote of this name made to the host. Returns how many.
    size_t EndConnectionsFromRemote(_In_ GUID const& hostId, _In_ std::wstring const& remoteName);

    // Gives each remote connected to the host its own speed, or the host's when it has none
    void ApplyRemoteClientSettings(_In_ GUID const& hostId);

    std::shared_ptr<RtpMidiEndpoint> FindEndpointByInterfaceId(_In_ std::wstring const& endpointDeviceInterfaceId);

    // Lets a customization reach an endpoint which already exists
    winrt::hstring FindMatchingInstantiatedEndpoint(_In_ WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria& criteria);

    json::JsonArray BuildHostsStatusJson();
    json::JsonArray BuildClientsStatusJson();
    json::JsonArray BuildAdvertisedHostsJson();

    // RtpMidiNode::IListener
    void OnConnectionUp(_In_ std::shared_ptr<RtpMidiConnection> const& connection) override;
    void OnConnectionDown(_In_ std::shared_ptr<RtpMidiConnection> const& connection, _In_ RtpMidi::EndReason const reason) override;
    void OnInvitationEnded(_In_ RtpMidiNode const* node, _In_ RtpMidi::EndReason const reason) override;

private:
    enum class ClientEntryState { Pending, Live, Failed, Unavailable };

    struct HostRuntime
    {
        RtpMidiHostDefinition Definition{};
        std::shared_ptr<RtpMidiNode> Node;
        HRESULT LastError{ S_OK };
        uint64_t NextAttemptTick{ 0 };

        // Where the running node is: one adapter, or GUID_NULL for every adapter, and whether
        // that is because its own adapter is missing
        GUID NetworkAdapterId{};
        bool NetworkAdapterFallbackUsed{ false };

        // Not running because its adapter is missing and it may not fall back
        bool WaitingForNetworkAdapter{ false };
    };

    // Where a host should run, given the adapters there are now
    struct HostPlacement
    {
        GUID NetworkAdapterId{};
        std::vector<uint32_t> Interfaces;
        uint32_t InterfaceIndex{ 0 };
        bool FallbackUsed{ false };
        bool Wait{ false };
    };

    struct HostStart
    {
        RtpMidiHostDefinition Definition{};
        HostPlacement Placement{};
    };

    static HostPlacement PlaceHost(
        _In_ RtpMidiHostDefinition const& definition,
        _In_ std::vector<WindowsMidiServicesInternal::MidiNetworkAdapterInfo> const& adapters);

    struct ClientRuntime
    {
        RtpMidiClientDefinition Definition{};
        std::shared_ptr<RtpMidiNode> Node;
        ClientEntryState State{ ClientEntryState::Pending };
        HRESULT LastError{ S_OK };
        uint64_t NextAttemptTick{ 0 };
        bool InvitationOutstanding{ false };

        // A remote can have several addresses. They are tried in turn: the one the last connection
        // was made on first, then the others in the order Windows prefers. See midi_network_addresses.h.
        std::wstring AttemptAddress;            // where the last invitation went
        uint32_t AttemptAddressCount{ 0 };      // how many addresses the remote had then
        std::wstring ConnectedAddress;          // where the last connection was made
        uint32_t UnansweredAttempts{ 0 };       // invitations nobody answered since then
    };

    struct EndpointWork
    {
        bool Create{ true };
        std::shared_ptr<RtpMidiConnection> Connection;
    };

    static std::wstring BuildInstanceId(_In_ std::wstring const& readableName, _In_ std::wstring const& identity);

    HRESULT CreateParentDevice();

    // Stops whatever is running and lets go of the service's objects. Safe whether or not
    // Initialize finished, and safe to call twice.
    void StopRunning() noexcept;

    void WorkerLoop(_In_ std::stop_token stopToken);
    void ProcessEndpointWork();
    void ReconcileHosts(_In_ std::stop_token const& stopToken);
    void ReconcileClients(_In_ std::stop_token const& stopToken);
    void RefreshCalculatedLatency();

    // Discovery sends mDNS queries and takes in whatever any device on the network answers, so it
    // runs only while an entry needs it or an app has asked for the list of devices recently.
    // Entries need it to find a client's remote by name, and to name the remote of a connection.
    void UpdateDiscovery();
    void EnsureDiscoveryForRequest();
    HRESULT StartDiscovery();
    void OnDiscoveryAnswer() noexcept;

    HRESULT CreateEndpoint(_In_ std::shared_ptr<RtpMidiConnection> const& connection);
    HRESULT RemoveEndpoint(_In_ std::shared_ptr<RtpMidiConnection> const& connection);
    bool IsInstanceIdInUse(_In_ std::wstring const& instanceId);
    bool IsInstanceIdListedElsewhere(_In_ std::shared_ptr<RtpMidiEndpoint> const& endpoint);

    // Listed before the service is asked for it, so an app opening it during activation finds it.
    // In Midi2.RtpMidiEndpointManagerKept.cpp, with the rest of the endpoint work below.
    HRESULT ActivateRtpEndpoint(_In_ std::shared_ptr<RtpMidiEndpoint> const& endpoint, _In_ std::wstring const& uniqueIdentifier, _In_ std::wstring const& endpointDescription);
    HRESULT RemoveRtpEndpoint(_In_ std::shared_ptr<RtpMidiEndpoint> const& endpoint);

    // An endpoint the service would not remove is no longer listed, but still in the service, where
    // it would stop the next one with its id from being made. Tried again on every worker pass.
    void RetryFailedRemovals();
    std::shared_ptr<RtpMidiEndpoint> FindEndpointForConnection(_In_ RtpMidiConnection const* connection);
    std::shared_ptr<RtpMidiEndpoint> FindEndpointForEntry(_In_ GUID const& entryId);

    // A client entry which keeps its endpoint while the remote is away. Made when the transport
    // starts for "always", handed each connection, renamed, and removed when it runs out.
    void ReconcileKeptEndpoints();
    HRESULT CreateKeptEndpoint(_In_ RtpMidiClientDefinition const& definition, _In_ std::shared_ptr<RtpMidiConnection> const& connection);
    bool AttachToKeptEndpoint(_In_ std::shared_ptr<RtpMidiConnection> const& connection);
    bool DetachFromKeptEndpoint(_In_ std::shared_ptr<RtpMidiConnection> const& connection);
    static std::wstring KeptEndpointInstanceId(_In_ RtpMidiClientDefinition const& definition);
    static std::wstring KeptEndpointName(_In_ RtpMidiClientDefinition const& definition);

    // A remote which connects to a host and has its own settings there can keep its endpoint while
    // it is away. Read when its connection ends, so a change reaches a remote already connected.
    // In Midi2.RtpMidiEndpointManagerHostKept.cpp.
    static std::wstring HostRemoteIdentity(_In_ GUID const& hostId, _In_ std::wstring const& remoteName);
    static bool HostKeepsEndpointFor(_In_ GUID const& hostId, _In_ std::wstring const& remoteName);
    static bool ShouldKeepHostEndpoint(_In_ std::shared_ptr<RtpMidiEndpoint> const& endpoint);
    std::shared_ptr<RtpMidiEndpoint> FindHostRemoteEndpoint(_In_ GUID const& hostId, _In_ std::wstring const& remoteName);
    void ReconcileHostKeptEndpoints(_In_ uint64_t const now);
    HRESULT CreateHostKeptEndpoint(_In_ GUID const& hostId, _In_ std::wstring const& remoteName);
    bool AttachToKeptHostEndpoint(_In_ std::shared_ptr<RtpMidiConnection> const& connection);
    bool DetachFromKeptHostEndpoint(_In_ std::shared_ptr<RtpMidiConnection> const& connection);
    void MarkHostRemoteEndpointAway(_In_ std::shared_ptr<RtpMidiEndpoint> const& endpoint);
    void RefreshHostEndpointsKept(_In_ GUID const& hostId);
    void ReleaseHostEndpointWhenConnectionEnds(_In_ GUID const& hostId, _In_ uint32_t const connectionId);
    bool TryRemoveDetachedEndpoint(_In_ std::shared_ptr<RtpMidiEndpoint> const& endpoint);
    json::JsonObject BuildRemoteClientSettingsJson(_In_ GUID const& hostId, _In_ RtpMidiRemoteClientSettings const& settings);

    // The addresses a client should invite, in the order to try them, and the port they share.
    // False when there is nowhere to connect yet.
    bool TryResolveClientTarget(
        _In_ RtpMidiClientDefinition const& definition,
        _In_ std::stop_token const& stopToken,
        _Out_ std::vector<std::wstring>& addresses,
        _Out_ uint16_t& port);

    std::vector<std::shared_ptr<RtpMidiNode>> RunningNodes();
    json::JsonArray BuildConnectionsJson(_In_ std::shared_ptr<RtpMidiNode> const& node, _In_ uint32_t const entrySendSpeedLimit);

    wil::com_ptr_nothrow<IMidiDeviceManager> m_midiDeviceManager;
    wil::com_ptr_nothrow<IMidiEndpointProtocolManager> m_midiProtocolManager;

    std::wstring m_parentDeviceId;
    std::wstring m_localDnsHostName;
    std::atomic<bool> m_initialized{ false };
    bool m_winsockStarted{ false };

    WindowsMidiServicesInternal::MidiDnssdBrowser m_browser;

    // Orders starting and stopping discovery with shutdown. Its callbacks never take this lock.
    wil::srwlock m_discoveryLock;
    bool m_discoveryAllowed{ false };
    uint64_t m_discoveryStartedTick{ 0 };
    uint64_t m_nextDiscoveryStartTick{ 0 };
    std::atomic<uint64_t> m_lastAdvertisedRequestTick{ 0 };
    std::atomic<uint64_t> m_lastDiscoveryAnswerTick{ 0 };

    // Guards the runtime maps. Never held across a call into a node or the device manager.
    wil::srwlock m_runtimeLock;
    std::map<GUID, HostRuntime, GuidLess> m_hosts;
    std::map<GUID, ClientRuntime, GuidLess> m_clients;

    // A std::mutex, because the worker waits on it with a condition variable
    std::mutex m_workLock;
    std::condition_variable m_workChanged;
    bool m_wakeRequested{ false };
    std::deque<EndpointWork> m_endpointWork;

    wil::srwlock m_createdEndpointsLock;
    std::vector<std::shared_ptr<RtpMidiEndpoint>> m_createdEndpoints;

    struct FailedRemoval
    {
        std::wstring NormalizedInstanceId;
        uint32_t Attempts{ 0 };
    };

    // also under m_createdEndpointsLock
    std::vector<FailedRemoval> m_endpointsToRemove;

    // worker thread only
    std::map<std::wstring, uint64_t> m_lastWrittenLatencyTicks;
    uint64_t m_nextLatencyRefreshTick{ 0 };

    // A kept endpoint the service would not create waits before the next try. Worker thread only.
    std::map<GUID, uint64_t, GuidLess> m_keptEndpointRetryTicks;

    // The same for a host's remote, keyed by host id and the remote's name in lowercase
    std::map<std::wstring, uint64_t> m_keptHostEndpointRetryTicks;

    // Repeats this PC's announcements, which the DNS client gets wrong. A host is withdrawn from
    // it before its registration is.
    WindowsMidiServicesInternal::MidiDnssdFollowUpAnnouncer m_announcer;

    // Wakes the worker when an adapter gains or loses an address, so a host limited to one
    // follows it. Read and cleared by the worker.
    WindowsMidiServicesInternal::MidiNetworkChangeMonitor m_networkChangeMonitor;
    std::atomic<bool> m_networkAdaptersChanged{ false };
    uint64_t m_nextNetworkAdapterCheckTick{ 0 };

    // Tells the notifications app when a host starts or stops waiting for its adapter
    RtpMidiNotificationSignal m_notificationSignal;

    std::jthread m_worker;
};
