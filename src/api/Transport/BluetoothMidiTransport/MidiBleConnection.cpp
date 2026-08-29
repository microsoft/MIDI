// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

using namespace winrt::Windows::Storage::Streams;

namespace
{
    // A device which stops draining its queue must never be allowed to exhaust service memory.
    constexpr size_t MaxOutgoingPacketQueueDepth = 512;

    // ATT overhead. The usable payload is always the negotiated MTU minus these three bytes.
    constexpr size_t AttHeaderByteCount = 3;

    std::vector<uint8_t> ReadBufferBytes(_In_ IBuffer const& buffer)
    {
        std::vector<uint8_t> bytes;

        if (buffer == nullptr || buffer.Length() == 0)
        {
            return bytes;
        }

        bytes.resize(buffer.Length());

        auto reader = DataReader::FromBuffer(buffer);
        reader.ReadBytes(winrt::array_view<uint8_t>(bytes));

        return bytes;
    }

    void AppendWordBigEndian(_Inout_ std::vector<uint8_t>& bytes, _In_ uint32_t const word)
    {
        bytes.push_back(static_cast<uint8_t>((word >> 24) & 0xFF));
        bytes.push_back(static_cast<uint8_t>((word >> 16) & 0xFF));
        bytes.push_back(static_cast<uint8_t>((word >> 8) & 0xFF));
        bytes.push_back(static_cast<uint8_t>(word & 0xFF));
    }

    uint64_t MidiTimestampTicksPerMillisecond()
    {
        static uint64_t const ticks = std::max<uint64_t>(internal::GetMidiTimestampFrequency() / MILLISECONDS_PER_SECOND, 1);

        return ticks;
    }
}


_Use_decl_annotations_
HRESULT
MidiBleConnection::Initialize(
    winrt::hstring const& deviceId,
    winrt::hstring const& deviceName,
    MidiBleProtocol::Protocol const protocol,
    bt::BluetoothLEDevice const& device,
    gatt::GattDeviceService const& service,
    gatt::GattCharacteristic const& characteristic,
    gatt::GattSession const& session
)
{
    TraceLoggingWrite(
        MidiBluetoothMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(deviceId.c_str(), "device id"),
        TraceLoggingUInt8(static_cast<uint8_t>(protocol), "protocol")
    );

    RETURN_HR_IF(E_INVALIDARG, deviceId.empty());
    RETURN_HR_IF(E_INVALIDARG, protocol == MidiBleProtocol::Protocol::Unknown);
    RETURN_HR_IF_NULL(E_INVALIDARG, device);
    RETURN_HR_IF_NULL(E_INVALIDARG, service);
    RETURN_HR_IF_NULL(E_INVALIDARG, characteristic);
    RETURN_HR_IF_NULL(E_INVALIDARG, session);

    m_deviceId = deviceId;
    m_deviceName = deviceName;
    m_protocol = protocol;
    m_device = device;
    m_service = service;
    m_characteristic = characteristic;
    m_session = session;

    // BLE MIDI 1.0 endpoints are always a single group of MIDI 1.0 byte stream data
    m_bytestreamToUmp.defaultGroup = 0;
    m_bytestreamToUmp.enableRunningStatus = false;
    m_umpToBytestream.enableRunningStatus = false;

    m_outgoingPacketBuilder.SetMaxPacketByteCount(MaxAttPayloadByteCount());

    return S_OK;
}


_Use_decl_annotations_
HRESULT
MidiBleConnection::InitializeAsPeripheral(
    winrt::hstring const& deviceId,
    winrt::hstring const& deviceName,
    MidiBleProtocol::Protocol const protocol,
    MidiBlePeripheralLink const& link
)
{
    TraceLoggingWrite(
        MidiBluetoothMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(deviceId.c_str(), "device id"),
        TraceLoggingUInt8(static_cast<uint8_t>(protocol), "protocol")
    );

    RETURN_HR_IF(E_INVALIDARG, deviceId.empty());
    RETURN_HR_IF(E_INVALIDARG, protocol == MidiBleProtocol::Protocol::Unknown);
    RETURN_HR_IF_NULL(E_INVALIDARG, link.WritePacket);
    RETURN_HR_IF_NULL(E_INVALIDARG, link.MaxAttPayloadByteCount);
    RETURN_HR_IF_NULL(E_INVALIDARG, link.IsConnected);

    m_deviceId = deviceId;
    m_deviceName = deviceName;
    m_protocol = protocol;
    m_isPeripheral = true;
    m_peripheralLink = link;

    m_bytestreamToUmp.defaultGroup = 0;
    m_bytestreamToUmp.enableRunningStatus = false;
    m_umpToBytestream.enableRunningStatus = false;

    m_outgoingPacketBuilder.SetMaxPacketByteCount(MaxAttPayloadByteCount());

    return S_OK;
}


HRESULT
MidiBleConnection::Start()
{
    TraceLoggingWrite(
        MidiBluetoothMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_deviceId.c_str(), "device id")
    );

    RETURN_HR_IF(E_UNEXPECTED, m_started.load());
    RETURN_HR_IF(E_UNEXPECTED, !m_isPeripheral && m_characteristic == nullptr);

    m_writerThread = std::jthread([weakThis = weak_from_this()](std::stop_token stopToken)
        {
            // LOG_IF_FAILED handles an HRESULT, not an exception, and an exception leaving a
            // jthread body terminates the whole service.
            try
            {
                if (auto self = weakThis.lock())
                {
                    LOG_IF_FAILED(self->WriterWorker(stopToken));
                }
            }
            CATCH_LOG();
        });

    // A peripheral has no Characteristic to subscribe to and no BluetoothLEDevice to watch. Its
    // link state is reported by the GATT service provider instead.
    if (!m_isPeripheral)
    {
        try
        {
            // revoking a token does not drain in-flight handlers, so the handler holds a weak reference
            m_valueChangedToken = m_characteristic.ValueChanged(
                [weakThis = weak_from_this()](gatt::GattCharacteristic const& sender, gatt::GattValueChangedEventArgs const& args)
                {
                    if (auto self = weakThis.lock())
                    {
                        self->OnCharacteristicValueChanged(sender, args);
                    }
                });

            m_connectionStatusChangedToken = m_device.ConnectionStatusChanged(
                [weakThis = weak_from_this()](bt::BluetoothLEDevice const& sender, foundation::IInspectable const& args)
                {
                    if (auto self = weakThis.lock())
                    {
                        self->OnDeviceConnectionStatusChanged(sender, args);
                    }
                });

            // The negotiated interval can be renegotiated at any time by either end, so it is
            // tracked rather than read once.
            m_connectionParametersChangedToken = m_device.ConnectionParametersChanged(
                [weakThis = weak_from_this()](bt::BluetoothLEDevice const&, foundation::IInspectable const&)
                {
                    if (auto self = weakThis.lock())
                    {
                        self->RefreshConnectionParameters();
                    }
                });

            RETURN_IF_FAILED(SubscribeToNotifications());

            RefreshConnectionParameters();
        }
        CATCH_RETURN();
    }

    m_started = true;

    return S_OK;
}


HRESULT
MidiBleConnection::SubscribeToNotifications()
{
    RETURN_HR_IF_NULL(HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_CONNECTED), m_characteristic);

    try
    {
        auto descriptorStatus = MidiBleUtilities::AwaitWithTimeout(
            m_characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(
                gatt::GattClientCharacteristicConfigurationDescriptorValue::Notify),
            MidiBleUtilities::BleOperationTimeoutMilliseconds,
            gatt::GattCommunicationStatus::Unreachable);

        if (descriptorStatus != gatt::GattCommunicationStatus::Success)
        {
            TraceLoggingWrite(
                MidiBluetoothMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Unable to subscribe to BLE MIDI characteristic notifications", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_deviceId.c_str(), "device id"),
                TraceLoggingUInt32(static_cast<uint32_t>(descriptorStatus), "gatt communication status")
            );

            RETURN_IF_FAILED(E_FAIL);
        }

        // the specified handshake: the Central reads the Characteristic after connecting and the
        // Peripheral answers with an empty payload. Some devices will not start notifying without it.
        MidiBleUtilities::AwaitWithTimeout(
            m_characteristic.ReadValueAsync(bt::BluetoothCacheMode::Uncached),
            MidiBleUtilities::BleOperationTimeoutMilliseconds,
            gatt::GattReadResult{ nullptr });
    }
    CATCH_RETURN();

    return S_OK;
}


void
MidiBleConnection::ResetTranslationState()
{
    // A link drop can cut a SysEx transfer in half in either direction, and BLE MIDI 2.0
    // section 5.12 says UMP stream state is not preserved across disconnections either.
    {
        auto lock = std::scoped_lock{ m_outgoingTranslationLock };

        m_outgoingPacketBuilder.Reset();
        m_umpToBytestream.resetBuffer();
    }

    m_incomingPacketDecoder.Reset();
    m_bytestreamToUmp.resetBuffer();

    // The remote clock has no continuity across a link drop, so the mapping is rebuilt from the
    // first packet after reconnecting.
    m_incomingTimestampCorrelator.Reset();

    auto lock = std::scoped_lock{ m_outgoingQueueLock };
    m_outgoingPackets.clear();
}


_Use_decl_annotations_
void
MidiBleConnection::OnDeviceConnectionStatusChanged(
    bt::BluetoothLEDevice const& sender,
    foundation::IInspectable const&
)
{
    if (m_shutdown.load())
    {
        return;
    }

    try
    {
        bool const connected = sender != nullptr && sender.ConnectionStatus() == bt::BluetoothConnectionStatus::Connected;

        if (connected == m_deviceConnected.exchange(connected))
        {
            return;
        }

        TraceLoggingWrite(
            MidiBluetoothMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"BLE MIDI device connection status changed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_deviceId.c_str(), "device id"),
            TraceLoggingBool(connected, "connected")
        );

        ResetTranslationState();

        if (connected)
        {
            // Notifications do not always survive a link drop, and a device which came back
            // without them looks alive to apps while delivering nothing. That silent failure is
            // the main complaint about the older Windows BLE MIDI support.
            LOG_IF_FAILED(SubscribeToNotifications());

            auto const endpointDeviceInterfaceId = EndpointDeviceInterfaceId();

            if (m_protocol == MidiBleProtocol::Protocol::Midi2Ump && !endpointDeviceInterfaceId.empty())
            {
                // Section 5.12: the Central performs full discovery and protocol negotiation
                // again on reconnection. It is queued because negotiation calls back into the
                // service, which must never happen on a Bluetooth callback thread.
                if (auto endpointManager = TransportState::Current().GetEndpointManager())
                {
                    LOG_IF_FAILED(endpointManager->QueueDiscoveryAndNegotiation(endpointDeviceInterfaceId));
                }
            }
        }
        else if (!m_isPeripheral)
        {
            // The endpoint is deliberately left in place: the session is set to maintain the
            // connection, so Windows re-establishes the link on its own and apps keep their
            // routing across a device sleeping or briefly going out of range. This only records
            // the drop and lets the worker re-examine the device.
            if (auto endpointManager = TransportState::Current().GetEndpointManager())
            {
                endpointManager->OnConnectionDropped(m_deviceId);
            }
        }
    }
    CATCH_LOG();
}


HRESULT
MidiBleConnection::Shutdown()
{
    TraceLoggingWrite(
        MidiBluetoothMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_deviceId.c_str(), "device id")
    );

    if (m_shutdown.exchange(true))
    {
        return S_OK;
    }

    DisconnectMidiCallback();

    m_writerThread.request_stop();
    m_outgoingPacketsAvailable.SetEvent();

    if (m_writerThread.joinable() && m_writerThread.get_id() != std::this_thread::get_id())
    {
        m_writerThread.join();
    }

    try
    {
        if (m_device != nullptr && m_connectionStatusChangedToken)
        {
            m_device.ConnectionStatusChanged(m_connectionStatusChangedToken);
            m_connectionStatusChangedToken = {};
        }

        if (m_device != nullptr && m_connectionParametersChangedToken)
        {
            m_device.ConnectionParametersChanged(m_connectionParametersChangedToken);
            m_connectionParametersChangedToken = {};
        }

        if (m_characteristic != nullptr)
        {
            if (m_valueChangedToken)
            {
                m_characteristic.ValueChanged(m_valueChangedToken);
                m_valueChangedToken = {};
            }

            // Courtesy only, and the link is going away regardless, so it gets the shortest wait
            // of anything here.
            LOG_IF_FAILED(MidiBleUtilities::AwaitWithTimeout(
                m_characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(
                    gatt::GattClientCharacteristicConfigurationDescriptorValue::None),
                MidiBleUtilities::BleTeardownOperationTimeoutMilliseconds,
                gatt::GattCommunicationStatus::Unreachable) == gatt::GattCommunicationStatus::Success ? S_OK : S_FALSE);
        }
    }
    CATCH_LOG();

    try
    {
        if (m_session != nullptr)
        {
            m_session.MaintainConnection(false);
            m_session.Close();
        }
    }
    CATCH_LOG();

    try
    {
        if (m_service != nullptr)
        {
            m_service.Close();
        }
    }
    CATCH_LOG();

    m_characteristic = nullptr;
    m_session = nullptr;
    m_service = nullptr;
    m_device = nullptr;

    {
        auto lock = std::scoped_lock{ m_outgoingQueueLock };
        m_outgoingPackets.clear();
    }

    m_started = false;

    return S_OK;
}


_Use_decl_annotations_
HRESULT
MidiBleConnection::ConnectMidiCallback(
    IMidiCallback* callback,
    LONGLONG context
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, callback);

    auto lock = std::scoped_lock{ m_callbackLock };

    m_callback = callback;
    m_callbackContext = context;

    return S_OK;
}


HRESULT
MidiBleConnection::DisconnectMidiCallback()
{
    auto lock = std::scoped_lock{ m_callbackLock };

    m_callback = nullptr;
    m_callbackContext = 0;

    return S_OK;
}


_Use_decl_annotations_
std::wstring
MidiBleConnection::EndpointDeviceInterfaceId() const
{
    auto lock = std::scoped_lock{ m_identityLock };

    return m_endpointDeviceInterfaceId;
}

_Use_decl_annotations_
void
MidiBleConnection::SetEndpointDeviceInterfaceId(std::wstring const& endpointDeviceInterfaceId)
{
    auto lock = std::scoped_lock{ m_identityLock };

    m_endpointDeviceInterfaceId = endpointDeviceInterfaceId;
}

_Use_decl_annotations_
std::wstring
MidiBleConnection::EndpointDeviceInstanceId() const
{
    auto lock = std::scoped_lock{ m_identityLock };

    return m_endpointDeviceInstanceId;
}

_Use_decl_annotations_
void
MidiBleConnection::SetEndpointDeviceInstanceId(std::wstring const& endpointDeviceInstanceId)
{
    auto lock = std::scoped_lock{ m_identityLock };

    m_endpointDeviceInstanceId = endpointDeviceInstanceId;
}


void
MidiBleConnection::RefreshConnectionParameters()
{
    if (m_shutdown.load() || m_device == nullptr)
    {
        return;
    }

    try
    {
        auto const parameters = m_device.GetConnectionParameters();

        if (parameters == nullptr)
        {
            return;
        }

        auto const intervalUnits = parameters.ConnectionInterval();

        if (m_connectionIntervalUnits.exchange(intervalUnits) == intervalUnits)
        {
            return;
        }

        // The only place the real interval is visible. Both specifications require 15 ms or less
        // and prefer lower, and what was requested is not necessarily what the link settled on.
        TraceLoggingWrite(
            MidiBluetoothMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Negotiated BLE connection parameters", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_deviceId.c_str(), "device id"),
            TraceLoggingUInt16(intervalUnits, "connection interval units"),
            TraceLoggingFloat32(intervalUnits * 1.25f, "connection interval ms"),
            TraceLoggingUInt16(parameters.ConnectionLatency(), "connection latency"),
            TraceLoggingUInt16(parameters.LinkTimeout(), "link timeout")
        );
    }
    CATCH_LOG();
}


bool
MidiBleConnection::IsDeviceConnected() const
{    try
    {
        if (m_isPeripheral)
        {
            return m_peripheralLink.IsConnected != nullptr && m_peripheralLink.IsConnected();
        }

        if (m_device != nullptr)
        {
            return m_device.ConnectionStatus() == bt::BluetoothConnectionStatus::Connected;
        }
    }
    CATCH_LOG();

    return false;
}


size_t
MidiBleConnection::MaxAttPayloadByteCount() const
{
    try
    {
        if (m_isPeripheral)
        {
            if (m_peripheralLink.MaxAttPayloadByteCount != nullptr)
            {
                auto const payloadByteCount = m_peripheralLink.MaxAttPayloadByteCount();

                if (payloadByteCount >= MidiBleMidi1::DefaultMaxPacketByteCount)
                {
                    return payloadByteCount;
                }
            }

            return MidiBleMidi1::DefaultMaxPacketByteCount;
        }

        if (m_session != nullptr)
        {
            auto const maxPduSize = m_session.MaxPduSize();

            if (maxPduSize > AttHeaderByteCount)
            {
                return static_cast<size_t>(maxPduSize) - AttHeaderByteCount;
            }
        }
    }
    CATCH_LOG();

    return MidiBleMidi1::DefaultMaxPacketByteCount;
}


_Use_decl_annotations_
void
MidiBleConnection::OnCharacteristicValueChanged(
    gatt::GattCharacteristic const&,
    gatt::GattValueChangedEventArgs const& args
)
{
    if (m_shutdown.load())
    {
        return;
    }

    try
    {
        auto bytes = ReadBufferBytes(args.CharacteristicValue());

        if (bytes.empty())
        {
            return;
        }

        ProcessIncomingPacket(bytes.data(), bytes.size());
    }
    CATCH_LOG();
}


_Use_decl_annotations_
void
MidiBleConnection::ProcessIncomingPacket(
    uint8_t const* const bytes,
    size_t const byteCount
)
{
    if (m_shutdown.load() || bytes == nullptr || byteCount == 0)
    {
        return;
    }

    m_packetsReceived++;

    if (m_protocol == MidiBleProtocol::Protocol::Midi2Ump)
    {
        ProcessIncomingUmpPayload(bytes, byteCount);
    }
    else
    {
        ProcessIncomingMidi1Packet(bytes, byteCount);
    }
}


void
MidiBleConnection::ResetForNewRemoteClient()
{
    {
        auto lock = std::scoped_lock{ m_outgoingQueueLock };
        m_outgoingPackets.clear();
    }

    ResetTranslationState();
}


_Use_decl_annotations_
void
MidiBleConnection::ProcessIncomingMidi1Packet(
    uint8_t const* const bytes,
    size_t const byteCount
)
{
    std::vector<MidiBleMidi1::DecodedSegment> segments;

    if (!m_incomingPacketDecoder.DecodePacket(bytes, byteCount, segments))
    {
        TraceLoggingWrite(
            MidiBluetoothMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Malformed BLE MIDI 1.0 packet discarded", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_deviceId.c_str(), "device id"),
            TraceLoggingUInt64(static_cast<uint64_t>(byteCount), "byte count")
        );

        return;
    }

    if (segments.empty())
    {
        return;
    }

    // The sender's clock is mapped onto ours through a single running offset, so spacing is kept
    // both within a packet and across packet boundaries.
    auto const receiveTimestamp = internal::GetCurrentMidiTimestamp();
    auto const ticksPerMillisecond = MidiTimestampTicksPerMillisecond();

    std::vector<uint64_t> segmentTimestamps;
    m_incomingTimestampCorrelator.MapPacket(segments, receiveTimestamp, ticksPerMillisecond, segmentTimestamps);

    for (size_t segmentIndex = 0; segmentIndex < segments.size(); segmentIndex++)
    {
        auto const& segment = segments[segmentIndex];
        auto const timestamp = segmentTimestamps[segmentIndex];

        std::vector<uint32_t> words;
        words.reserve(segment.Bytes.size());

        for (size_t i = 0; i < segment.Bytes.size(); i++)
        {
            m_bytestreamToUmp.bytestreamParse(segment.Bytes[i]);

            if (i == segment.Bytes.size() - 1)
            {
                // flush any partial SysEx into a UMP but stay in the SysEx state, because the
                // rest of the transfer arrives in a later segment or a later BLE packet
                m_bytestreamToUmp.dumpSysex7State(false);
            }

            while (m_bytestreamToUmp.availableUMP())
            {
                words.push_back(m_bytestreamToUmp.readUMP());
            }
        }

        if (!words.empty())
        {
            LOG_IF_FAILED(SendUmpWordsToCallback(words.data(), words.size(), timestamp));
        }
    }
}


_Use_decl_annotations_
void
MidiBleConnection::ProcessIncomingUmpPayload(
    uint8_t const* const bytes,
    size_t const byteCount
)
{
    // the ATT notification payload is UMP data with no header or other framing
    if (byteCount % sizeof(uint32_t) != 0)
    {
        TraceLoggingWrite(
            MidiBluetoothMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"BLE UMP payload is not a whole number of words", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_deviceId.c_str(), "device id"),
            TraceLoggingUInt64(static_cast<uint64_t>(byteCount), "byte count")
        );

        return;
    }

    size_t const wordCount = byteCount / sizeof(uint32_t);

    std::vector<uint32_t> words;
    words.reserve(wordCount);

    for (size_t i = 0; i < wordCount; i++)
    {
        size_t const offset = i * sizeof(uint32_t);

        // each 32-bit UMP word is transmitted big-endian
        words.push_back(
            (static_cast<uint32_t>(bytes[offset]) << 24) |
            (static_cast<uint32_t>(bytes[offset + 1]) << 16) |
            (static_cast<uint32_t>(bytes[offset + 2]) << 8) |
            static_cast<uint32_t>(bytes[offset + 3]));
    }

    // A malformed UMP invalidates everything behind it in the same payload, because without a
    // valid length there is no way to find the next message boundary.
    size_t validWordCount{ 0 };

    while (validWordCount < words.size())
    {
        auto const messageWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(words[validWordCount]);

        if (messageWordCount == 0 || validWordCount + messageWordCount > words.size())
        {
            TraceLoggingWrite(
                MidiBluetoothMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Incomplete UMP in BLE payload. Discarding it and the remainder of the payload.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_deviceId.c_str(), "device id")
            );

            break;
        }

        validWordCount += messageWordCount;
    }

    if (validWordCount > 0)
    {
        LOG_IF_FAILED(SendUmpWordsToCallback(words.data(), validWordCount, internal::GetCurrentMidiTimestamp()));
    }
}


_Use_decl_annotations_
HRESULT
MidiBleConnection::SendUmpWordsToCallback(
    uint32_t const* const words,
    size_t const wordCount,
    uint64_t const timestamp
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, words);
    RETURN_HR_IF(E_INVALIDARG, wordCount == 0);

    // counted here because it is the one place both the MIDI 1.0 and the UMP path pass through
    for (size_t i = 0; i < wordCount; )
    {
        auto const messageWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(words[i]);

        if (messageWordCount == 0 || i + messageWordCount > wordCount)
        {
            break;
        }

        m_messagesReceived++;
        i += messageWordCount;
    }

    auto lock = std::scoped_lock{ m_callbackLock };

    if (m_callback == nullptr)
    {
        return S_FALSE;
    }

    RETURN_IF_FAILED(m_callback->Callback(
        MessageOptionFlags_None,
        const_cast<uint32_t*>(words),
        static_cast<UINT>(wordCount * sizeof(uint32_t)),
        static_cast<LONGLONG>(timestamp),
        m_callbackContext));

    return S_OK;
}


_Use_decl_annotations_
HRESULT
MidiBleConnection::QueueMidiMessagesToSendToDevice(
    void const* const data,
    uint32_t const byteCount
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, data);
    RETURN_HR_IF(E_INVALIDARG, byteCount < sizeof(uint32_t));
    RETURN_HR_IF(E_INVALIDARG, byteCount % sizeof(uint32_t) != 0);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_CONNECTED), m_shutdown.load());

    auto const words = static_cast<uint32_t const*>(data);
    auto const wordCount = byteCount / sizeof(uint32_t);

    auto lock = std::scoped_lock{ m_outgoingTranslationLock };

    if (m_protocol == MidiBleProtocol::Protocol::Midi2Ump)
    {
        BuildOutgoingUmpPackets(words, wordCount);
    }
    else
    {
        BuildOutgoingMidi1Packets(words, wordCount);
    }

    return S_OK;
}


_Use_decl_annotations_
void
MidiBleConnection::BuildOutgoingMidi1Packets(
    uint32_t const* const words,
    size_t const wordCount
)
{
    auto const nowMilliseconds = internal::ConvertTimestampToWholeMilliseconds(
        internal::GetCurrentMidiTimestamp(),
        internal::GetMidiTimestampFrequency());

    auto const timestamp = static_cast<uint16_t>(nowMilliseconds & MidiBleMidi1::TimestampMask);

    m_outgoingPacketBuilder.SetMaxPacketByteCount(MaxAttPayloadByteCount());

    std::vector<uint8_t> messageBytes;

    size_t index{ 0 };

    while (index < wordCount)
    {
        auto const messageWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(words[index]);

        if (messageWordCount == 0 || index + messageWordCount > wordCount)
        {
            // an incomplete message leaves the translator with no way to find the next boundary
            m_umpToBytestream.resetBuffer();
            break;
        }

        for (uint8_t i = 0; i < messageWordCount; i++)
        {
            m_umpToBytestream.UMPStreamParse(words[index + i]);
        }

        messageBytes.clear();

        while (m_umpToBytestream.availableBS())
        {
            messageBytes.push_back(m_umpToBytestream.readBS());
        }

        if (!messageBytes.empty())
        {
            m_outgoingPacketBuilder.AppendMessage(messageBytes.data(), messageBytes.size(), timestamp);
            m_messagesSent++;
        }

        index += messageWordCount;
    }

    QueueOutgoingPackets(m_outgoingPacketBuilder.TakePackets());
}


_Use_decl_annotations_
void
MidiBleConnection::BuildOutgoingUmpPackets(
    uint32_t const* const words,
    size_t const wordCount
)
{
    auto const maxPayloadByteCount = MaxAttPayloadByteCount();

    std::vector<std::vector<uint8_t>> packets;
    std::vector<uint8_t> current;

    size_t index{ 0 };

    while (index < wordCount)
    {
        auto const messageWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(words[index]);

        if (messageWordCount == 0 || index + messageWordCount > wordCount)
        {
            break;
        }

        size_t const messageByteCount = messageWordCount * sizeof(uint32_t);

        // fragmentation is prohibited: a UMP which does not fit is deferred to the next packet
        if (!current.empty() && current.size() + messageByteCount > maxPayloadByteCount)
        {
            packets.push_back(std::move(current));
            current.clear();
        }

        for (uint8_t i = 0; i < messageWordCount; i++)
        {
            AppendWordBigEndian(current, words[index + i]);
        }

        m_messagesSent++;
        index += messageWordCount;
    }

    if (!current.empty())
    {
        packets.push_back(std::move(current));
    }

    QueueOutgoingPackets(std::move(packets));
}


_Use_decl_annotations_
void
MidiBleConnection::QueueOutgoingPackets(std::vector<std::vector<uint8_t>>&& packets)
{
    if (packets.empty())
    {
        return;
    }

    size_t droppedPacketCount{ 0 };

    {
        auto lock = std::scoped_lock{ m_outgoingQueueLock };

        for (auto& packet : packets)
        {
            if (m_outgoingPackets.size() >= MaxOutgoingPacketQueueDepth)
            {
                m_outgoingPackets.pop_front();
                droppedPacketCount++;
            }

            m_outgoingPackets.push_back(std::move(packet));
        }
    }

    if (droppedPacketCount > 0)
    {
        TraceLoggingWrite(
            MidiBluetoothMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Outgoing BLE MIDI queue is full. Oldest packets dropped.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_deviceId.c_str(), "device id"),
            TraceLoggingUInt64(static_cast<uint64_t>(droppedPacketCount), "dropped packet count")
        );
    }

    m_outgoingPacketsAvailable.SetEvent();
}


_Use_decl_annotations_
HRESULT
MidiBleConnection::WriterWorker(std::stop_token stopToken)
{
    // COM calls made from this thread fail CO_E_NOTINITIALIZED without this
    winrt::init_apartment();

    // Without this the thread parks in wait() and never sees the stop request, so the jthread
    // destructor's join would hang forever.
    std::stop_callback wakeOnStop{ stopToken, [this]() { m_outgoingPacketsAvailable.SetEvent(); } };

    while (!stopToken.stop_requested() && !m_shutdown.load())
    {
        std::vector<uint8_t> packet;
        bool havePacket{ false };

        {
            auto lock = std::scoped_lock{ m_outgoingQueueLock };

            if (!m_outgoingPackets.empty())
            {
                packet = std::move(m_outgoingPackets.front());
                m_outgoingPackets.pop_front();
                havePacket = true;
            }
            else
            {
                m_outgoingPacketsAvailable.ResetEvent();
            }
        }

        if (havePacket)
        {
            LOG_IF_FAILED(WritePacketToDevice(packet));
        }
        else
        {
            m_outgoingPacketsAvailable.wait();
        }
    }

    return S_OK;
}


_Use_decl_annotations_
HRESULT
MidiBleConnection::WritePacketToDevice(std::vector<uint8_t> const& packet)
{
    RETURN_HR_IF(S_FALSE, packet.empty());
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_CONNECTED), m_shutdown.load());

    if (m_isPeripheral)
    {
        RETURN_HR_IF_NULL(HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_CONNECTED), m_peripheralLink.WritePacket);

        auto const hr = m_peripheralLink.WritePacket(packet);

        if (SUCCEEDED(hr))
        {
            if (hr == S_OK)
            {
                m_packetsSent++;
            }

            m_lastSendErrorHresult = 0;
        }
        else
        {
            m_lastSendErrorHresult = static_cast<int32_t>(hr);
        }

        return hr;
    }

    auto characteristic = m_characteristic;
    RETURN_HR_IF_NULL(HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_CONNECTED), characteristic);

    try
    {
        DataWriter writer;
        writer.WriteBytes(winrt::array_view<uint8_t const>(packet));

        // the Central always writes without response
        auto status = MidiBleUtilities::AwaitWithTimeout(
            characteristic.WriteValueAsync(writer.DetachBuffer(), gatt::GattWriteOption::WriteWithoutResponse),
            MidiBleUtilities::BleDataOperationTimeoutMilliseconds,
            gatt::GattCommunicationStatus::Unreachable);

        if (status != gatt::GattCommunicationStatus::Success)
        {
            m_lastSendErrorHresult = static_cast<int32_t>(E_FAIL);

            TraceLoggingWrite(
                MidiBluetoothMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"BLE MIDI characteristic write failed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_deviceId.c_str(), "device id"),
                TraceLoggingUInt32(static_cast<uint32_t>(status), "gatt communication status")
            );

            return S_FALSE;
        }

        m_packetsSent++;
        m_lastSendErrorHresult = 0;
    }
    catch (...)
    {
        m_lastSendErrorHresult = static_cast<int32_t>(winrt::to_hresult());
        RETURN_CAUGHT_EXCEPTION();
    }

    return S_OK;
}
