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

    STDMETHOD(WakeupBackgroundEndpointCreatorThread)();

private:
    HRESULT CreateParentDevice();

    void OnAdvertisementReceived(
        _In_ bt::Advertisement::BluetoothLEAdvertisementWatcher const& sender,
        _In_ bt::Advertisement::BluetoothLEAdvertisementReceivedEventArgs const& args);

    HRESULT OnDeviceWatcherAdded(_In_ enumeration::DeviceWatcher const&, _In_ enumeration::DeviceInformation const& args);
    HRESULT OnDeviceWatcherUpdated(_In_ enumeration::DeviceWatcher const&, _In_ enumeration::DeviceInformationUpdate const& args);
    HRESULT OnDeviceWatcherRemoved(_In_ enumeration::DeviceWatcher const&, _In_ enumeration::DeviceInformationUpdate const& args);
    HRESULT OnDeviceWatcherStopped(_In_ enumeration::DeviceWatcher const&, _In_ foundation::IInspectable const&);

    HRESULT EndpointCreatorWorker(_In_ std::stop_token stopToken);

    HRESULT ConnectDeviceInternal(_In_ winrt::hstring const& deviceId);
    HRESULT DisconnectDeviceInternal(_In_ winrt::hstring const& deviceId);

    HRESULT CreateEndpointForConnection(_In_ std::shared_ptr<MidiBleConnection> connection);

    bool TryGetDiscoveredDevice(_In_ winrt::hstring const& deviceId, _Out_ MidiBleProtocol::DiscoveredDevice& device);
    void UpsertDiscoveredDevice(_In_ MidiBleProtocol::DiscoveredDevice const& device);
    void UpdateDiscoveredDeviceConnectionState(
        _In_ winrt::hstring const& deviceId,
        _In_ bool const isConnected,
        _In_ MidiBleProtocol::Protocol const protocol,
        _In_ winrt::hstring const& endpointDeviceId);

    bt::Advertisement::BluetoothLEAdvertisementWatcher m_advertisementWatcher{ nullptr };
    winrt::event_token m_advertisementReceivedToken{ };

    enumeration::DeviceWatcher m_deviceWatcher{ nullptr };
    winrt::event_token m_deviceWatcherAddedToken;
    winrt::event_token m_deviceWatcherUpdatedToken;
    winrt::event_token m_deviceWatcherRemovedToken;
    winrt::event_token m_deviceWatcherStoppedToken;

    std::map<winrt::hstring, MidiBleProtocol::DiscoveredDevice> m_discoveredDevices;
    std::mutex m_discoveredDevicesLock;

    // Connecting opens a GATT session and creates a device node, so it never runs on a caller's
    // thread or on a watcher callback. The worker drains these instead.
    std::deque<winrt::hstring> m_pendingConnectRequests;
    std::deque<winrt::hstring> m_pendingDisconnectRequests;
    std::deque<std::wstring> m_pendingNegotiations;
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
