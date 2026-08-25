// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

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

    bool IsDeviceConnected() const;
    bool IsShutdown() const noexcept { return m_shutdown; }

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

    gatt::GattDeviceService m_service{ nullptr };
    gatt::GattCharacteristic m_characteristic{ nullptr };
    gatt::GattSession m_session{ nullptr };
    winrt::event_token m_valueChangedToken{ };

    bt::BluetoothLEDevice m_device{ nullptr };
    winrt::event_token m_connectionStatusChangedToken{ };
    std::atomic<bool> m_deviceConnected{ true };

    mutable std::mutex m_identityLock;
    std::wstring m_endpointDeviceInterfaceId{ };
    std::wstring m_endpointDeviceInstanceId{ };

    mutable std::mutex m_callbackLock;
    wil::com_ptr_nothrow<IMidiCallback> m_callback{ nullptr };
    LONGLONG m_callbackContext{ 0 };

    // inbound and outbound translation state is per-direction and never shared
    MidiBleMidi1::PacketDecoder m_incomingPacketDecoder{ };
    bytestreamToUMP m_bytestreamToUmp{ };

    MidiBleMidi1::PacketBuilder m_outgoingPacketBuilder{ };
    umpToBytestream m_umpToBytestream{ };
    std::mutex m_outgoingTranslationLock;

    std::mutex m_outgoingQueueLock;
    std::deque<std::vector<uint8_t>> m_outgoingPackets{ };
    wil::slim_event_manual_reset m_outgoingPacketsAvailable;

    std::atomic<bool> m_shutdown{ false };
    std::atomic<bool> m_started{ false };

    // jthread members are declared last so they are destroyed first
    std::jthread m_writerThread;
};
