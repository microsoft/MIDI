// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// The peripheral role needs the same translation, queuing, timestamping, and callback plumbing
// as the central role, and differs only in how bytes leave and arrive. Those points are supplied
// by the peripheral rather than duplicating the pipeline.
struct MidiBlePeripheralLink
{
    std::function<HRESULT(std::vector<uint8_t> const&)> WritePacket{ nullptr };
    std::function<size_t()> MaxAttPayloadByteCount{ nullptr };
    std::function<bool()> IsConnected{ nullptr };
};

// Owns the GATT session for one BLE MIDI device, translates between the wire format of the
// selected Characteristic and UMP, and carries messages to and from the service.
class MidiBleConnection : public std::enable_shared_from_this<MidiBleConnection>
{
public:
    HRESULT Initialize(
        _In_ winrt::hstring const& deviceId,
        _In_ winrt::hstring const& deviceName,
        _In_ MidiBleProtocol::Protocol const protocol,
        _In_ bt::BluetoothLEDevice const& device,
        _In_ gatt::GattDeviceService const& service,
        _In_ gatt::GattCharacteristic const& characteristic,
        _In_ gatt::GattSession const& session);

    HRESULT InitializeAsPeripheral(
        _In_ winrt::hstring const& deviceId,
        _In_ winrt::hstring const& deviceName,
        _In_ MidiBleProtocol::Protocol const protocol,
        _In_ MidiBlePeripheralLink const& link);

    // the bytes a remote Central wrote to our Characteristic
    void ProcessIncomingPacket(_In_reads_bytes_(byteCount) uint8_t const* const bytes, _In_ size_t const byteCount);

    // the translation state is per-connection, and a Central which drops and re-subscribes is a
    // new conversation
    void ResetForNewRemoteClient();

    HRESULT Start();
    HRESULT Shutdown();

    HRESULT ConnectMidiCallback(_In_ IMidiCallback* callback, _In_ LONGLONG context);
    HRESULT DisconnectMidiCallback();

    HRESULT QueueMidiMessagesToSendToDevice(
        _In_reads_bytes_(byteCount) void const* const data,
        _In_ uint32_t const byteCount);

    // the 12 hex digit Bluetooth address, which is how discovery, the config commands and this
    // table all refer to a device
    winrt::hstring DeviceId() const noexcept { return m_deviceId; }
    winrt::hstring DeviceName() const noexcept { return m_deviceName; }
    MidiBleProtocol::Protocol Protocol() const noexcept { return m_protocol; }

    MidiBleProtocol::NativeDataFormat NativeDataFormat() const noexcept
    {
        return m_protocol == MidiBleProtocol::Protocol::Midi2Ump ?
            MidiBleProtocol::NativeDataFormat::UniversalMidiPacket :
            MidiBleProtocol::NativeDataFormat::TimestampedMidi1ByteStream;
    }

    std::wstring EndpointDeviceInterfaceId() const;
    void SetEndpointDeviceInterfaceId(_In_ std::wstring const& endpointDeviceInterfaceId);

    std::wstring EndpointDeviceInstanceId() const;
    void SetEndpointDeviceInstanceId(_In_ std::wstring const& endpointDeviceInstanceId);

    // Nothing else can observe whether data is actually moving: an app sees only its own side,
    // and a BLE write without response is never acknowledged.
    uint64_t MessagesReceived() const noexcept { return m_messagesReceived.load(); }
    uint64_t MessagesSent() const noexcept { return m_messagesSent.load(); }
    uint64_t PacketsReceived() const noexcept { return m_packetsReceived.load(); }
    uint64_t PacketsSent() const noexcept { return m_packetsSent.load(); }

    // Set when the device refused an operation until the link is authenticated.
    bool RequiresPairing() const noexcept { return m_requiresPairing.load(); }
    int32_t LastSendErrorHresult() const noexcept { return m_lastSendErrorHresult.load(); }

    bool IsDeviceConnected() const;
    bool IsShutdown() const noexcept { return m_shutdown; }
    bool IsPeripheral() const noexcept { return m_isPeripheral; }

    // The interval the link actually negotiated, in units of 1.25 ms. Zero when unknown.
    uint16_t ConnectionIntervalUnits() const noexcept { return m_connectionIntervalUnits.load(); }
    void RefreshConnectionParameters();

private:
    void OnCharacteristicValueChanged(_In_ gatt::GattCharacteristic const& sender, _In_ gatt::GattValueChangedEventArgs const& args);
    void OnDeviceConnectionStatusChanged(_In_ bt::BluetoothLEDevice const& sender, _In_ foundation::IInspectable const& args);

    HRESULT SubscribeToNotifications();
    void ResetTranslationState();

    void ProcessIncomingMidi1Packet(_In_reads_bytes_(byteCount) uint8_t const* const bytes, _In_ size_t const byteCount);
    void ProcessIncomingUmpPayload(_In_reads_bytes_(byteCount) uint8_t const* const bytes, _In_ size_t const byteCount);

    HRESULT SendUmpWordsToCallback(
        _In_reads_(wordCount) uint32_t const* const words,
        _In_ size_t const wordCount,
        _In_ uint64_t const timestamp);

    void BuildOutgoingMidi1Packets(_In_reads_(wordCount) uint32_t const* const words, _In_ size_t const wordCount);
    void BuildOutgoingUmpPackets(_In_reads_(wordCount) uint32_t const* const words, _In_ size_t const wordCount);

    void QueueOutgoingPackets(_In_ std::vector<std::vector<uint8_t>>&& packets);

    HRESULT WriterWorker(_In_ std::stop_token stopToken);
    HRESULT WritePacketToDevice(_In_ std::vector<uint8_t> const& packet);

    size_t MaxAttPayloadByteCount() const;

    winrt::hstring m_deviceId{ };
    winrt::hstring m_deviceName{ };
    MidiBleProtocol::Protocol m_protocol{ MidiBleProtocol::Protocol::Unknown };

    bool m_isPeripheral{ false };
    MidiBlePeripheralLink m_peripheralLink{ };

    gatt::GattDeviceService m_service{ nullptr };
    gatt::GattCharacteristic m_characteristic{ nullptr };
    gatt::GattSession m_session{ nullptr };
    winrt::event_token m_valueChangedToken{ };

    bt::BluetoothLEDevice m_device{ nullptr };
    winrt::event_token m_connectionStatusChangedToken{ };
    winrt::event_token m_connectionParametersChangedToken{ };
    std::atomic<bool> m_deviceConnected{ true };
    std::atomic<uint16_t> m_connectionIntervalUnits{ 0 };

    mutable std::mutex m_identityLock;
    std::wstring m_endpointDeviceInterfaceId{ };
    std::wstring m_endpointDeviceInstanceId{ };

    mutable std::mutex m_callbackLock;
    wil::com_ptr_nothrow<IMidiCallback> m_callback{ nullptr };
    LONGLONG m_callbackContext{ 0 };

    // inbound and outbound translation state is per-direction and never shared
    MidiBleMidi1::PacketDecoder m_incomingPacketDecoder{ };
    MidiBleMidi1::TimestampCorrelator m_incomingTimestampCorrelator{ };
    bytestreamToUMP m_bytestreamToUmp{ };

    MidiBleMidi1::PacketBuilder m_outgoingPacketBuilder{ };
    umpToBytestream m_umpToBytestream{ };
    std::mutex m_outgoingTranslationLock;

    std::mutex m_outgoingQueueLock;
    std::deque<std::vector<uint8_t>> m_outgoingPackets{ };
    wil::slim_event_manual_reset m_outgoingPacketsAvailable;

    std::atomic<bool> m_shutdown{ false };
    std::atomic<bool> m_started{ false };

    std::atomic<uint64_t> m_messagesReceived{ 0 };
    std::atomic<uint64_t> m_messagesSent{ 0 };
    std::atomic<uint64_t> m_packetsReceived{ 0 };
    std::atomic<uint64_t> m_packetsSent{ 0 };
    std::atomic<bool> m_requiresPairing{ false };
    std::atomic<int32_t> m_lastSendErrorHresult{ 0 };

    // jthread members are declared last so they are destroyed first
    std::jthread m_writerThread;
};
