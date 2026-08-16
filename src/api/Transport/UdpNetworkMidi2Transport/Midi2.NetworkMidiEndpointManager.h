// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once



class CMidi2NetworkMidiEndpointManager :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiEndpointManager>

{
public:
    STDMETHOD(Initialize(_In_ IMidiDeviceManager*, _In_ IMidiEndpointProtocolManager*));
    STDMETHOD(Shutdown)();

    STDMETHOD(InitiateDiscoveryAndNegotiation(_In_ std::wstring const& endpointDeviceInterfaceId));

    // Defers the above to the background worker. Discovery and negotiation call into the
    // service and take service-wide locks, and endpoint creation raises PnP notifications whose
    // callbacks take those same locks. Doing that on the socket receive callback deadlocked the
    // whole receive path, so nothing on that thread may block on the service.
    HRESULT QueueDiscoveryAndNegotiation(_In_ std::wstring const& endpointDeviceInterfaceId);

    // Shutting a connection down joins its two worker threads, so it cannot be done on the
    // socket receive callback without stalling every other datagram behind it.
    HRESULT QueueConnectionShutdown(_In_ std::shared_ptr<MidiNetworkConnection> connection);

    // endpoint for a remote client connected to this host
    STDMETHOD(CreateNewHostEndpointToRemoteClient(
        _In_ std::wstring const& configIdentifier,
        _In_ std::wstring const& parentHostDeviceInstanceId,
        _In_ std::wstring const& endpointName,
        _In_ std::wstring const& remoteEndpointProductInstanceId,
        _In_ winrt::Windows::Networking::HostName const& hostName,
        _In_ std::wstring const& networkPort,
        _In_ bool umpOnly,
        _Out_ std::wstring& createdNewDeviceInstanceId,
        _Out_ std::wstring& createdNewEndpointDeviceInterfaceId
    ));

    // endpoint for this client connected to a remote host
    STDMETHOD(CreateNewClientEndpointToRemoteHost(
        _In_ std::wstring const& configIdentifier,
        _In_ std::wstring const& endpointName,
        _In_ std::wstring const& remoteEndpointProductInstanceId,
        _In_ winrt::Windows::Networking::HostName const& hostName,
        _In_ std::wstring const& networkPort,
        _In_ bool umpOnly,
        _Out_ std::wstring& createdNewDeviceInstanceId,
        _Out_ std::wstring& createdNewEndpointDeviceInterfaceId
    ));

    STDMETHOD(DeleteEndpoint(_In_ std::wstring deviceInstanceId));

    STDMETHOD(StartRemoteHostWatcher)();
    STDMETHOD(StartBackgroundEndpointCreator)();
    STDMETHOD(StartBackgroundConnectionShutdown)();
    STDMETHOD(StartBackgroundNegotiation)();
    STDMETHOD(StartBackgroundHostEndpointCreation)();

    bool IsInitialized() { return m_initialized; }

    STDMETHOD(WakeupBackgroundEndpointCreatorThread)();
    STDMETHOD(WakeupBackgroundConnectionShutdownThread)();
    STDMETHOD(WakeupBackgroundNegotiationThread)();

    // Creating an endpoint takes over a second when several arrive at once, and it used to run
    // on the socket receive callback, so one burst of invitations stalled every datagram behind
    // it. The connection is answered with Invitation Reply: Pending and completed from here.
    HRESULT QueueHostEndpointCreation(
        _In_ std::shared_ptr<MidiNetworkHostConnection> connection,
        _In_ std::wstring const& clientUmpEndpointName,
        _In_ std::wstring const& clientProductInstanceId);

    HRESULT CreateParentDeviceForHost(
        _In_ winrt::hstring const& name,
        _In_ winrt::hstring const& id,
        _Inout_ std::wstring& createdNewDeviceInstanceId);

    HRESULT DeleteParentHostDevice(
        _In_ std::wstring const& deviceInstanceId);


    HRESULT StartNewClient(
        _In_ std::shared_ptr<MidiNetworkClientDefinition> clientDefinition,
        _In_ winrt::hstring const& hostNameOrIPAddress,
        _In_ uint16_t const hostPort);


private:
    STDMETHOD(CreateNewEndpoint(
        _In_ MidiNetworkConnectionRole thisServiceRole,
        _In_ std::wstring const& configIdentifier,
        _In_ std::wstring const& parentId,
        _In_ std::wstring const& endpointName,
        _In_ std::wstring const& remoteEndpointProductInstanceId,
        _In_ winrt::Windows::Networking::HostName const& hostName,
        _In_ std::wstring const& networkPort,
        _In_ bool umpOnly,
        _Out_ std::wstring& createdNewDeviceInstanceId,
        _Out_ std::wstring& createdNewEndpointDeviceInterfaceId
    ));

    enumeration::DeviceWatcher m_deviceWatcher{ nullptr };
    winrt::event_token m_deviceWatcherAddedToken;
    winrt::event_token m_deviceWatcherUpdatedToken;
    winrt::event_token m_deviceWatcherRemovedToken;
    winrt::event_token m_deviceWatcherStoppedToken;

    HRESULT OnDeviceWatcherAdded(_In_ enumeration::DeviceWatcher const&, _In_ enumeration::DeviceInformation const& args);
    HRESULT OnDeviceWatcherUpdated(_In_ enumeration::DeviceWatcher const&, _In_ enumeration::DeviceInformationUpdate const& args);
    HRESULT OnDeviceWatcherRemoved(_In_ enumeration::DeviceWatcher const&, _In_ enumeration::DeviceInformationUpdate const& args);
    HRESULT OnDeviceWatcherStopped(_In_ enumeration::DeviceWatcher const&, _In_ foundation::IInspectable const&);

    std::map<winrt::hstring, enumeration::DeviceInformation> m_foundAdvertisedHosts;

    bool m_initialized{ false };

    GUID m_containerId{};
    GUID m_transportId{ };
    std::wstring m_clientParentDeviceInstanceId{};

    HRESULT CreateParentDeviceForClients();

    wil::com_ptr_nothrow<IMidiDeviceManager> m_midiDeviceManager;
    wil::com_ptr_nothrow<IMidiEndpointProtocolManager> m_midiProtocolManager;


    wil::slim_event_manual_reset m_backgroundEndpointCreatorThreadWakeup;
    HRESULT EndpointCreatorWorker(_In_ std::stop_token stopToken);

    wil::critical_section m_pendingNegotiationsLock;
    std::vector<std::wstring> m_pendingNegotiations;

    // Negotiation calls into the service and can block there indefinitely, so it gets its own
    // thread. On the endpoint creator thread a wedged negotiation stopped every configured
    // client from connecting.
    wil::slim_event_manual_reset m_backgroundNegotiationThreadWakeup;

    // Set by the worker as it leaves. Shutdown waits on this rather than joining the thread,
    // because a negotiation blocked in the service would otherwise hold up service stop.
    wil::slim_event_manual_reset m_negotiationThreadExitedEvent;

    HRESULT NegotiationWorker(_In_ std::stop_token stopToken);

    // Teardown runs on its own thread. Deactivating an endpoint is slow, and it has no ordering
    // relationship with starting a new connection, so sharing one worker meant a disconnecting
    // device delayed an unrelated device connecting.
    wil::slim_event_manual_reset m_backgroundConnectionShutdownThreadWakeup;
    HRESULT ConnectionShutdownWorker(_In_ std::stop_token stopToken);

    wil::critical_section m_pendingConnectionShutdownsLock;
    std::vector<std::shared_ptr<MidiNetworkConnection>> m_pendingConnectionShutdowns;

    struct PendingHostEndpointCreation
    {
        std::shared_ptr<MidiNetworkHostConnection> Connection;
        std::wstring ClientUmpEndpointName;
        std::wstring ClientProductInstanceId;

        // Time to Invitation Reply: Accepted is queue wait plus activation, and only the
        // measurement tells us which of the two to attack.
        std::chrono::steady_clock::time_point QueuedAt{ std::chrono::steady_clock::now() };
    };

    wil::slim_event_manual_reset m_backgroundHostEndpointCreationThreadWakeup;
    HRESULT HostEndpointCreationWorker(_In_ std::stop_token stopToken);

    wil::critical_section m_pendingHostEndpointCreationsLock;
    std::vector<PendingHostEndpointCreation> m_pendingHostEndpointCreations;

    // Must remain the last members. Members are destroyed in reverse declaration order, so this
    // guarantees the workers are joined before the wakeup events they wait on are destroyed.
    std::jthread m_backgroundEndpointCreatorThread;
    std::jthread m_backgroundConnectionShutdownThread;
    std::jthread m_backgroundNegotiationThread;
    std::jthread m_backgroundHostEndpointCreationThread;

};
