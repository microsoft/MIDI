// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// What WinRT reports about the Central which is connected. Recorded in full because which of
// these survives a resolvable private address rotation is what should key the endpoint identity.
struct MidiBleRemoteClientInfo
{
    winrt::hstring Name{ };
    winrt::hstring Address{ };
    winrt::hstring AddressType{ };
    winrt::hstring BluetoothDeviceId{ };
    bool IsPaired{ false };
    bool HasGenericName{ false };
};
// Publishes this PC as a BLE MIDI Peripheral so a remote Central, such as a phone or tablet, can
// connect to it. Like the Network MIDI 2.0 host, the endpoint represents the remote device which
// connected, not this PC, so it is created when a Central subscribes and removed when it leaves.
class MidiBlePeripheral : public std::enable_shared_from_this<MidiBlePeripheral>
{
public:
    HRESULT Start(_In_ MidiBleProtocol::Protocol const protocol);
    HRESULT Stop();

    // Raised on a Bluetooth callback thread, so the handler must only queue work.
    void SetClientChangedCallback(_In_ std::function<void()> callback);

    bool IsRunning() const noexcept { return m_running.load(); }
    bool IsClientSubscribed() const;

    MidiBleProtocol::Protocol Protocol() const noexcept { return m_protocol; }

    // The name this PC advertises. Windows takes it from the computer name and the GATT service
    // provider gives an application no way to override it, so it is reported, not configured.
    winrt::hstring AdvertisedName() const noexcept { return m_advertisedName; }

    // The WinRT device id of the Central which is currently subscribed, or empty. This is what
    // tells the endpoint manager whether the remote device has changed.
    winrt::hstring ActiveClientDeviceId() const;

    HRESULT AttachConnection(
        _In_ winrt::hstring const& remoteDeviceId,
        _In_ winrt::hstring const& remoteDeviceName,
        _Out_ std::shared_ptr<MidiBleConnection>& connection);

    HRESULT DetachConnection();

    void SetRemoteClientInfo(_In_ MidiBleRemoteClientInfo const& info);
    MidiBleRemoteClientInfo RemoteClientInfo() const;

    // The remote Central chooses the interval in this direction, so this is the only place to see
    // what a phone or tablet actually asks for. Units of 1.25 ms, zero when unknown.
    void SetRemoteDevice(_In_ bt::BluetoothLEDevice const& device);
    uint16_t ConnectionIntervalUnits() const;

    std::shared_ptr<MidiBleConnection> Connection() const;

    uint32_t SubscribedClientCount() const;

private:
    HRESULT CreateServiceProvider(_In_ MidiBleProtocol::Protocol const protocol);
    HRESULT StartAdvertising();

    void OnReadRequested(_In_ gatt::GattLocalCharacteristic const& sender, _In_ gatt::GattReadRequestedEventArgs const& args);
    void OnWriteRequested(_In_ gatt::GattLocalCharacteristic const& sender, _In_ gatt::GattWriteRequestedEventArgs const& args);
    void OnSubscribedClientsChanged(_In_ gatt::GattLocalCharacteristic const& sender, _In_ foundation::IInspectable const& args);
    void OnAdvertisementStatusChanged(_In_ gatt::GattServiceProvider const& sender, _In_ gatt::GattServiceProviderAdvertisementStatusChangedEventArgs const& args);

    HRESULT NotifyPacket(_In_ std::vector<uint8_t> const& packet);
    size_t MaxNotificationByteCount() const;

    gatt::GattSubscribedClient ActiveClient() const;

    winrt::hstring m_advertisedName{ };
    MidiBleProtocol::Protocol m_protocol{ MidiBleProtocol::Protocol::Unknown };

    gatt::GattServiceProvider m_serviceProvider{ nullptr };
    gatt::GattLocalCharacteristic m_characteristic{ nullptr };

    winrt::event_token m_readRequestedToken{ };
    winrt::event_token m_writeRequestedToken{ };
    winrt::event_token m_subscribedClientsChangedToken{ };
    winrt::event_token m_advertisementStatusChangedToken{ };

    // BLE MIDI permits one active connection at a time. WinRT cannot refuse a subscription, so
    // the first client to subscribe is the one served and any others are ignored.
    mutable std::mutex m_clientLock;
    gatt::GattSubscribedClient m_activeClient{ nullptr };
    winrt::hstring m_activeClientDeviceId{ };
    uint32_t m_subscribedClientCount{ 0 };

    mutable std::mutex m_connectionLock;
    std::shared_ptr<MidiBleConnection> m_connection{ nullptr };
    MidiBleRemoteClientInfo m_remoteClientInfo{ };
    bt::BluetoothLEDevice m_remoteDevice{ nullptr };

    std::mutex m_clientChangedCallbackLock;
    std::function<void()> m_clientChangedCallback{ nullptr };

    std::atomic<bool> m_running{ false };
};
