// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once



class CMidi2Ble2MidiEndpointManager :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiEndpointManager>

{
public:
    STDMETHOD(Initialize(_In_ IMidiDeviceManager*, _In_ IMidiEndpointProtocolManager*));
    STDMETHOD(Shutdown)();

    STDMETHOD(InitiateDiscoveryAndNegotiation(_In_ std::wstring const& endpointDeviceInterfaceId));

    // Negotiation calls back into the service, which must never happen on a Bluetooth callback
    // thread, so reconnects queue it for the background worker instead.
    STDMETHOD(QueueDiscoveryAndNegotiation)(_In_ std::wstring const& endpointDeviceInterfaceId);

    STDMETHOD(DeleteEndpoint(_In_ std::wstring deviceInstanceId));

    STDMETHOD(StartAdvertisementWatcher)();
    STDMETHOD(StartGattServiceWatcher)();
    STDMETHOD(StartBackgroundEndpointCreator)();

    bool IsInitialized() const noexcept { return m_initialized; }

    std::vector<MidiBleProtocol::DiscoveredDevice> GetDiscoveredDevices();

    // Both take the 12 hex digit Bluetooth address this transport uses as a device id.
    STDMETHOD(ConnectDevice)(_In_ winrt::hstring const& deviceId);
    STDMETHOD(DisconnectDevice)(_In_ winrt::hstring const& deviceId);

    STDMETHOD(ConnectConfiguredDevices)();

    // Publishes this PC as a BLE MIDI Peripheral and gives it an endpoint, so an app can open it
    // and be ready before a remote Central ever connects.
    STDMETHOD(StartPeripheral)(_In_ MidiBleProtocol::Protocol const protocol);
    STDMETHOD(StopPeripheral)();

    // Lets a customization find an endpoint which already exists, so a rename does not require
    // disconnecting the device.
    winrt::hstring FindMatchingInstantiatedEndpoint(
        _In_ WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria& criteria);

    // A rename has to reach the MIDI 1.0 ports as well as the endpoint node, and those are named
    // from the group terminal blocks and the port name table rather than from the endpoint name.
    STDMETHOD(RefreshMidi1PortsForRenamedEndpoint)(
        _In_ winrt::hstring const& endpointDeviceInterfaceId,
        _In_opt_ std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties> const customProperties);

    STDMETHOD(WakeupBackgroundEndpointCreatorThread)();

private:
    HRESULT CreateParentDevice();

    void OnAdvertisementReceived(
        _In_ bt::Advertisement::BluetoothLEAdvertisementWatcher const& sender,
        _In_ bt::Advertisement::BluetoothLEAdvertisementReceivedEventArgs const& args);

    void OnAdvertisementWatcherStopped(
        _In_ bt::Advertisement::BluetoothLEAdvertisementWatcher const& sender,
        _In_ bt::Advertisement::BluetoothLEAdvertisementWatcherStoppedEventArgs const& args);

    HRESULT OnDeviceWatcherAdded(_In_ enumeration::DeviceWatcher const&, _In_ enumeration::DeviceInformation const& args);
    HRESULT OnDeviceWatcherUpdated(_In_ enumeration::DeviceWatcher const&, _In_ enumeration::DeviceInformationUpdate const& args);
    HRESULT OnDeviceWatcherRemoved(_In_ enumeration::DeviceWatcher const&, _In_ enumeration::DeviceInformationUpdate const& args);
    HRESULT OnDeviceWatcherStopped(_In_ enumeration::DeviceWatcher const&, _In_ foundation::IInspectable const&);

    HRESULT EndpointCreatorWorker(_In_ std::stop_token stopToken);

    HRESULT ConnectDeviceInternal(_In_ winrt::hstring const& deviceId);
    HRESULT ConnectDeviceCore(_In_ winrt::hstring const& deviceId, _Out_ winrt::hstring& failureDetail);

    // Connecting runs several GATT calls in sequence, each of which can take seconds against a
    // sleeping device. Shutdown joins this worker, so every stage checks whether to give up.
    bool IsStopping() noexcept { return m_backgroundEndpointCreatorThread.get_stop_token().stop_requested(); }
    HRESULT DisconnectDeviceInternal(_In_ winrt::hstring const& deviceId);
    HRESULT ResolveDeviceNameInternal(_In_ winrt::hstring const& deviceId);

    HRESULT CreateEndpointForConnection(_In_ std::shared_ptr<MidiBleConnection> connection);

    void OnPeripheralClientChanged();
    HRESULT ProcessPeripheralClientChange();
    HRESULT RemovePeripheralEndpoint();

    HRESULT CreateEndpoint(
        _In_ std::shared_ptr<MidiBleConnection> connection,
        _In_ std::wstring const& endpointName,
        _In_ std::wstring const& endpointDescription,
        _In_ std::wstring const& instanceId,
        _In_ std::wstring const& uniqueIdentifier);

    bool TryGetDiscoveredDevice(_In_ winrt::hstring const& deviceId, _Out_ MidiBleProtocol::DiscoveredDevice& device);
    void UpsertDiscoveredDevice(_In_ MidiBleProtocol::DiscoveredDevice const& device);
    void RecordConnectResult(
        _In_ winrt::hstring const& deviceId,
        _In_ HRESULT const hr,
        _In_ winrt::hstring const& detail);

    void QueueConnectIfWanted(_In_ winrt::hstring const& deviceId);
    void QueueNameResolutionIfNeeded(_In_ winrt::hstring const& deviceId);

    // False while the name is still being resolved. An endpoint created in that window would be
    // named after the Bluetooth address, which is meaningless to the user.
    bool IsDeviceNameable(_In_ winrt::hstring const& deviceId);
    void UpdateDiscoveredDeviceConnectionState(
        _In_ winrt::hstring const& deviceId,
        _In_ bool const isConnected,
        _In_ MidiBleProtocol::Protocol const protocol,
        _In_ winrt::hstring const& endpointDeviceId);

    bt::Advertisement::BluetoothLEAdvertisementWatcher m_advertisementWatcher{ nullptr };
    winrt::event_token m_advertisementReceivedToken{ };
    winrt::event_token m_advertisementStoppedToken{ };

    enumeration::DeviceWatcher m_deviceWatcher{ nullptr };
    winrt::event_token m_deviceWatcherAddedToken;
    winrt::event_token m_deviceWatcherUpdatedToken;
    winrt::event_token m_deviceWatcherRemovedToken;
    winrt::event_token m_deviceWatcherStoppedToken;

    std::map<winrt::hstring, MidiBleProtocol::DiscoveredDevice> m_discoveredDevices;
    std::mutex m_discoveredDevicesLock;

    struct CreatedEndpointRecord
    {
        winrt::hstring DeviceId;
        winrt::hstring EndpointDeviceId;
        winrt::hstring DeviceInstanceId;
        winrt::hstring TransportSuppliedEndpointName;
    };

    std::vector<CreatedEndpointRecord> m_createdEndpoints;
    std::mutex m_createdEndpointsLock;

    // Releasing the request withdraws the connection interval preference, so one is held per
    // connected device for as long as that connection lasts.
    std::map<winrt::hstring, bt::BluetoothLEPreferredConnectionParametersRequest> m_connectionParameterRequests{ };
    std::mutex m_connectionParameterRequestsLock;

    // Only ever touched on the background worker, which is the one thread allowed to create or
    // remove endpoints.
    winrt::hstring m_peripheralClientDeviceId{ };
    std::wstring m_peripheralEndpointInstanceId{ };
    bool m_peripheralClientChangePending{ false };
    // Connecting opens a GATT session and creates a device node, so it never runs on a caller's
    // thread or on a watcher callback. The worker drains these instead.
    std::deque<winrt::hstring> m_pendingConnectRequests;
    std::deque<winrt::hstring> m_pendingDisconnectRequests;
    std::deque<std::wstring> m_pendingNegotiations;

    // Devices whose advertisement carried no name. Resolving one needs an async call, so it
    // cannot happen on the advertisement callback.
    std::deque<winrt::hstring> m_pendingNameResolutions;

    struct NameResolutionAttempts
    {
        uint32_t Count{ 0 };
        uint64_t LastAttemptTimestamp{ 0 };
        bool GaveUp{ false };
    };

    std::map<winrt::hstring, NameResolutionAttempts> m_nameResolutionAttempts;

    // A device is only offered to the user once it can be identified by name. Set of ids for
    // which that has been given up on, so they are shown by address as a last resort.
    std::set<winrt::hstring> GetDeviceIdsWithUnresolvableNames();

    // Devices the user has asked to be connected. BLE peripherals sleep aggressively, so a
    // single attempt is almost always made against a device that cannot answer. The intent is
    // kept and retried every time the device advertises.
    std::set<winrt::hstring> m_desiredConnections;
    std::map<winrt::hstring, uint64_t> m_lastConnectAttemptTimestamp;

    std::mutex m_pendingRequestsLock;

    bool m_initialized{ false };

    GUID m_containerId{ };
    GUID m_transportId{ };
    std::wstring m_parentDeviceId{ };

    wil::com_ptr_nothrow<IMidiDeviceManager> m_midiDeviceManager;
    wil::com_ptr_nothrow<IMidiEndpointProtocolManager> m_midiProtocolManager;

    wil::slim_event_manual_reset m_backgroundEndpointCreatorThreadWakeup;

    // jthread members are declared last so they are destroyed first
    std::jthread m_backgroundEndpointCreatorThread;
};
