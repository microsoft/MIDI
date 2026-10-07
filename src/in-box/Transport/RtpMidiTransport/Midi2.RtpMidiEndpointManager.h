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

    std::shared_ptr<RtpMidiConnection> FindConnectionByEndpointDeviceInterfaceId(_In_ std::wstring const& endpointDeviceInterfaceId);

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

    struct CreatedEndpoint
    {
        std::weak_ptr<RtpMidiConnection> Connection;
        winrt::hstring InstanceId;
        winrt::hstring InterfaceId;
        winrt::hstring EndpointName;
    };

    HRESULT CreateParentDevice();

    void WorkerLoop(_In_ std::stop_token stopToken);
    void ProcessEndpointWork();
    void ReconcileHosts(_In_ std::stop_token const& stopToken);
    void ReconcileClients(_In_ std::stop_token const& stopToken);
    void RefreshCalculatedLatency();

    HRESULT CreateEndpoint(_In_ std::shared_ptr<RtpMidiConnection> const& connection);
    HRESULT RemoveEndpoint(_In_ std::shared_ptr<RtpMidiConnection> const& connection);
    bool IsInstanceIdInUse(_In_ std::wstring const& instanceId);

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

    // Guards the runtime maps. Never held across a call into a node or the device manager.
    std::mutex m_runtimeLock;
    std::map<GUID, HostRuntime, GuidLess> m_hosts;
    std::map<GUID, ClientRuntime, GuidLess> m_clients;

    std::mutex m_workLock;
    std::condition_variable m_workChanged;
    bool m_wakeRequested{ false };
    std::deque<EndpointWork> m_endpointWork;

    std::mutex m_createdEndpointsLock;
    std::vector<CreatedEndpoint> m_createdEndpoints;

    // The endpoint the worker is activating. Apps can see it before activation returns, and so
    // before it has a record above. Guarded by m_createdEndpointsLock.
    std::shared_ptr<RtpMidiConnection> m_endpointBeingCreated;
    std::wstring m_endpointBeingCreatedInstanceId;

    // worker thread only
    std::map<std::wstring, uint64_t> m_lastWrittenLatencyTicks;
    uint64_t m_nextLatencyRefreshTick{ 0 };

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
