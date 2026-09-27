// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// PROTOTYPE. Runs the configured hosts and clients, and gives every connection an endpoint.
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

    std::shared_ptr<RtpMidiConnection> FindConnectionByEndpointDeviceInterfaceId(_In_ std::wstring const& endpointDeviceInterfaceId);

    // Lets a customization reach an endpoint which already exists
    winrt::hstring FindMatchingInstantiatedEndpoint(_In_ WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria& criteria);

    json::JsonArray BuildHostsStatusJson();
    json::JsonArray BuildClientsStatusJson();
    json::JsonArray BuildAdvertisedPeersJson();

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
    };

    struct ClientRuntime
    {
        RtpMidiClientDefinition Definition{};
        std::shared_ptr<RtpMidiNode> Node;
        ClientEntryState State{ ClientEntryState::Pending };
        HRESULT LastError{ S_OK };
        uint64_t NextAttemptTick{ 0 };
        bool InvitationOutstanding{ false };
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
    void ReconcileHosts();
    void ReconcileClients();
    void RefreshCalculatedLatency();

    HRESULT CreateEndpoint(_In_ std::shared_ptr<RtpMidiConnection> const& connection);
    HRESULT RemoveEndpoint(_In_ std::shared_ptr<RtpMidiConnection> const& connection);
    bool IsInstanceIdInUse(_In_ std::wstring const& instanceId);

    bool TryResolveClientTarget(_In_ RtpMidiClientDefinition const& definition, _Out_ RtpMidi::PeerAddress& target);

    std::vector<std::shared_ptr<RtpMidiNode>> RunningNodes();
    json::JsonArray BuildConnectionsJson(_In_ std::shared_ptr<RtpMidiNode> const& node);

    wil::com_ptr_nothrow<IMidiDeviceManager> m_midiDeviceManager;
    wil::com_ptr_nothrow<IMidiEndpointProtocolManager> m_midiProtocolManager;

    GUID m_containerId{};
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

    // worker thread only
    std::map<std::wstring, uint64_t> m_lastWrittenLatencyTicks;
    uint64_t m_nextLatencyRefreshTick{ 0 };

    // Repeats this PC's announcements, which the DNS client gets wrong. A host is withdrawn from
    // it before its registration is.
    WindowsMidiServicesInternal::MidiDnssdFollowUpAnnouncer m_announcer;

    std::jthread m_worker;
};
