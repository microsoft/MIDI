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
    // ATT overhead. A notification payload is always the negotiated MTU minus these three bytes,
    // and MaxNotificationSize already accounts for that.
    constexpr size_t MinimumNotificationByteCount = MidiBleMidi1::DefaultMaxPacketByteCount;

    std::vector<uint8_t> ReadRequestBytes(_In_ IBuffer const& buffer)
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
}


_Use_decl_annotations_
HRESULT
MidiBlePeripheral::Start(MidiBleProtocol::Protocol const protocol)
{
    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt8(static_cast<uint8_t>(protocol), "protocol")
    );

    RETURN_HR_IF(E_INVALIDARG, protocol == MidiBleProtocol::Protocol::Unknown);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED), m_running.load());

    auto cleanupOnFailure = wil::scope_exit([&]() { LOG_IF_FAILED(Stop()); });

    RETURN_IF_FAILED(CreateServiceProvider(protocol));

    m_protocol = protocol;

    RETURN_IF_FAILED(StartAdvertising());

    m_running = true;

    cleanupOnFailure.release();

    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"BLE MIDI peripheral published", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_advertisedName.c_str(), "advertised name"),
        TraceLoggingUInt8(static_cast<uint8_t>(protocol), "protocol")
    );

    return S_OK;
}


_Use_decl_annotations_
void
MidiBlePeripheral::SetClientChangedCallback(std::function<void()> callback)
{
    auto lock = std::scoped_lock{ m_clientChangedCallbackLock };

    m_clientChangedCallback = std::move(callback);
}


_Use_decl_annotations_
HRESULT
MidiBlePeripheral::AttachConnection(
    winrt::hstring const& remoteDeviceId,
    winrt::hstring const& remoteDeviceName,
    std::shared_ptr<MidiBleConnection>& connection
)
{
    connection = nullptr;

    RETURN_HR_IF(E_UNEXPECTED, !m_running.load());
    RETURN_HR_IF(E_INVALIDARG, remoteDeviceId.empty());

    auto newConnection = std::make_shared<MidiBleConnection>();
    RETURN_IF_NULL_ALLOC(newConnection);

    MidiBlePeripheralLink link{};

    link.WritePacket = [weakThis = weak_from_this()](std::vector<uint8_t> const& packet) -> HRESULT
        {
            if (auto self = weakThis.lock())
            {
                return self->NotifyPacket(packet);
            }

            return HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_CONNECTED);
        };

    link.MaxAttPayloadByteCount = [weakThis = weak_from_this()]() -> size_t
        {
            if (auto self = weakThis.lock())
            {
                return self->MaxNotificationByteCount();
            }

            return MinimumNotificationByteCount;
        };

    link.IsConnected = [weakThis = weak_from_this()]() -> bool
        {
            if (auto self = weakThis.lock())
            {
                return self->IsClientSubscribed();
            }

            return false;
        };

    RETURN_IF_FAILED(newConnection->InitializeAsPeripheral(
        MIDI_BLE_PERIPHERAL_DEVICE_ID,
        remoteDeviceName,
        m_protocol,
        link));

    RETURN_IF_FAILED(newConnection->Start());

    {
        auto lock = std::scoped_lock{ m_connectionLock };
        m_connection = newConnection;
    }

    connection = newConnection;

    return S_OK;
}


HRESULT
MidiBlePeripheral::DetachConnection()
{
    std::shared_ptr<MidiBleConnection> connection{ nullptr };

    {
        auto lock = std::scoped_lock{ m_connectionLock };

        connection = std::move(m_connection);
        m_connection = nullptr;
        m_remoteClientInfo = {};
        m_remoteDevice = nullptr;
    }

    if (connection != nullptr)
    {
        LOG_IF_FAILED(connection->Shutdown());
    }

    return S_OK;
}


_Use_decl_annotations_
void
MidiBlePeripheral::SetRemoteClientInfo(MidiBleRemoteClientInfo const& info)
{
    auto lock = std::scoped_lock{ m_connectionLock };

    m_remoteClientInfo = info;
}


MidiBleRemoteClientInfo
MidiBlePeripheral::RemoteClientInfo() const
{
    auto lock = std::scoped_lock{ m_connectionLock };

    return m_remoteClientInfo;
}


_Use_decl_annotations_
void
MidiBlePeripheral::SetRemoteDevice(bt::BluetoothLEDevice const& device)
{
    auto lock = std::scoped_lock{ m_connectionLock };

    m_remoteDevice = device;
}


uint16_t
MidiBlePeripheral::ConnectionIntervalUnits() const
{
    bt::BluetoothLEDevice device{ nullptr };

    {
        auto lock = std::scoped_lock{ m_connectionLock };
        device = m_remoteDevice;
    }

    if (device == nullptr)
    {
        return 0;
    }

    try
    {
        if (auto const parameters = device.GetConnectionParameters())
        {
            return parameters.ConnectionInterval();
        }
    }
    CATCH_LOG();

    return 0;
}


_Use_decl_annotations_
HRESULT
MidiBlePeripheral::CreateServiceProvider(MidiBleProtocol::Protocol const protocol)
{
    try
    {
        auto providerResult = MidiBleUtilities::AwaitWithTimeout(
            gatt::GattServiceProvider::CreateAsync(winrt::guid{ MidiBleProtocol::MidiServiceUuid }),
            MidiBleUtilities::BleOperationTimeoutMilliseconds,
            gatt::GattServiceProviderResult{ nullptr });

        RETURN_HR_IF_NULL(HRESULT_FROM_WIN32(ERROR_SERVICE_NOT_ACTIVE), providerResult);

        if (providerResult.Error() != bt::BluetoothError::Success)
        {
            TraceLoggingWrite(
                MidiBle2MidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Unable to create the BLE MIDI GATT service", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingUInt32(static_cast<uint32_t>(providerResult.Error()), "bluetooth error")
            );

            RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_SERVICE_NOT_ACTIVE));
        }

        m_serviceProvider = providerResult.ServiceProvider();
        RETURN_HR_IF_NULL(E_UNEXPECTED, m_serviceProvider);

        gatt::GattLocalCharacteristicParameters parameters{};

        // Notify carries data to the Central, WriteWithoutResponse carries data from it, and Read
        // exists only for the connection handshake, which answers with an empty payload.
        parameters.CharacteristicProperties(
            gatt::GattCharacteristicProperties::Notify |
            gatt::GattCharacteristicProperties::WriteWithoutResponse |
            gatt::GattCharacteristicProperties::Read);

        parameters.ReadProtectionLevel(gatt::GattProtectionLevel::Plain);
        parameters.WriteProtectionLevel(gatt::GattProtectionLevel::Plain);

        // Only one MIDI characteristic is published. The specification allows a Peripheral to
        // implement a single protocol, and WinRT provides no way to reject the subscription a
        // Central makes to the characteristic it prefers, so offering both would let a Central
        // subscribe to the one we are not serving.
        auto const characteristicUuid = protocol == MidiBleProtocol::Protocol::Midi2Ump ?
            winrt::guid{ MidiBleProtocol::Midi2UmpCharacteristicUuid } :
            winrt::guid{ MidiBleProtocol::Midi1DataIoCharacteristicUuid };

        auto characteristicResult = MidiBleUtilities::AwaitWithTimeout(
            m_serviceProvider.Service().CreateCharacteristicAsync(characteristicUuid, parameters),
            MidiBleUtilities::BleOperationTimeoutMilliseconds,
            gatt::GattLocalCharacteristicResult{ nullptr });

        RETURN_HR_IF_NULL(HRESULT_FROM_WIN32(ERROR_SERVICE_NOT_ACTIVE), characteristicResult);

        if (characteristicResult.Error() != bt::BluetoothError::Success)
        {
            TraceLoggingWrite(
                MidiBle2MidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Unable to create the BLE MIDI data I/O characteristic", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingUInt32(static_cast<uint32_t>(characteristicResult.Error()), "bluetooth error")
            );

            RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_SERVICE_NOT_ACTIVE));
        }

        m_characteristic = characteristicResult.Characteristic();
        RETURN_HR_IF_NULL(E_UNEXPECTED, m_characteristic);

        m_readRequestedToken = m_characteristic.ReadRequested(
            [weakThis = weak_from_this()](gatt::GattLocalCharacteristic const& sender, gatt::GattReadRequestedEventArgs const& args)
            {
                if (auto self = weakThis.lock())
                {
                    self->OnReadRequested(sender, args);
                }
            });

        m_writeRequestedToken = m_characteristic.WriteRequested(
            [weakThis = weak_from_this()](gatt::GattLocalCharacteristic const& sender, gatt::GattWriteRequestedEventArgs const& args)
            {
                if (auto self = weakThis.lock())
                {
                    self->OnWriteRequested(sender, args);
                }
            });

        m_subscribedClientsChangedToken = m_characteristic.SubscribedClientsChanged(
            [weakThis = weak_from_this()](gatt::GattLocalCharacteristic const& sender, foundation::IInspectable const& args)
            {
                if (auto self = weakThis.lock())
                {
                    self->OnSubscribedClientsChanged(sender, args);
                }
            });

        m_advertisementStatusChangedToken = m_serviceProvider.AdvertisementStatusChanged(
            [weakThis = weak_from_this()](gatt::GattServiceProvider const& sender, gatt::GattServiceProviderAdvertisementStatusChangedEventArgs const& args)
            {
                if (auto self = weakThis.lock())
                {
                    self->OnAdvertisementStatusChanged(sender, args);
                }
            });

        m_advertisedName = MidiBleUtilities::GetLocalBluetoothName();
    }
    CATCH_RETURN();

    return S_OK;
}


HRESULT
MidiBlePeripheral::StartAdvertising()
{
    try
    {
        gatt::GattServiceProviderAdvertisingParameters parameters{};

        // Both are required: a Central has to be able to find us and then connect to us. The MIDI
        // service UUID is placed in the advertisement by the service provider itself, which is
        // what a BLE MIDI Central scans for.
        parameters.IsConnectable(true);
        parameters.IsDiscoverable(true);

        m_serviceProvider.StartAdvertising(parameters);
    }
    CATCH_RETURN();

    return S_OK;
}


HRESULT
MidiBlePeripheral::Stop()
{
    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    m_running = false;

    LOG_IF_FAILED(DetachConnection());

    try
    {
        if (m_characteristic != nullptr)
        {
            if (m_readRequestedToken)
            {
                m_characteristic.ReadRequested(m_readRequestedToken);
                m_readRequestedToken = {};
            }

            if (m_writeRequestedToken)
            {
                m_characteristic.WriteRequested(m_writeRequestedToken);
                m_writeRequestedToken = {};
            }

            if (m_subscribedClientsChangedToken)
            {
                m_characteristic.SubscribedClientsChanged(m_subscribedClientsChangedToken);
                m_subscribedClientsChangedToken = {};
            }
        }

        if (m_serviceProvider != nullptr)
        {
            if (m_advertisementStatusChangedToken)
            {
                m_serviceProvider.AdvertisementStatusChanged(m_advertisementStatusChangedToken);
                m_advertisementStatusChangedToken = {};
            }

            if (m_serviceProvider.AdvertisementStatus() == gatt::GattServiceProviderAdvertisementStatus::Started)
            {
                m_serviceProvider.StopAdvertising();
            }
        }
    }
    CATCH_LOG();

    m_characteristic = nullptr;
    m_serviceProvider = nullptr;

    {
        auto lock = std::scoped_lock{ m_clientLock };
        m_activeClient = nullptr;
        m_activeClientDeviceId = L"";
        m_subscribedClientCount = 0;
    }

    {
        auto lock = std::scoped_lock{ m_clientChangedCallbackLock };
        m_clientChangedCallback = nullptr;
    }

    m_protocol = MidiBleProtocol::Protocol::Unknown;
    m_advertisedName = L"";

    return S_OK;
}


winrt::hstring
MidiBlePeripheral::ActiveClientDeviceId() const
{
    auto lock = std::scoped_lock{ m_clientLock };

    return m_activeClientDeviceId;
}


std::shared_ptr<MidiBleConnection>
MidiBlePeripheral::Connection() const
{
    auto lock = std::scoped_lock{ m_connectionLock };

    return m_connection;
}


bool
MidiBlePeripheral::IsClientSubscribed() const
{
    auto lock = std::scoped_lock{ m_clientLock };

    return m_activeClient != nullptr;
}


uint32_t
MidiBlePeripheral::SubscribedClientCount() const
{
    auto lock = std::scoped_lock{ m_clientLock };

    return m_subscribedClientCount;
}


gatt::GattSubscribedClient
MidiBlePeripheral::ActiveClient() const
{
    auto lock = std::scoped_lock{ m_clientLock };

    return m_activeClient;
}


_Use_decl_annotations_
void
MidiBlePeripheral::OnReadRequested(
    gatt::GattLocalCharacteristic const&,
    gatt::GattReadRequestedEventArgs const& args
)
{
    // The deferral has to be taken before the first suspension point, or the request is
    // considered unanswered as soon as this handler returns.
    auto deferral = args.GetDeferral();

    try
    {
        auto request = MidiBleUtilities::AwaitWithTimeout(
            args.GetRequestAsync(),
            MidiBleUtilities::BleDataOperationTimeoutMilliseconds,
            gatt::GattReadRequest{ nullptr });

        if (request != nullptr)
        {
            // The specified handshake. A read of the data I/O characteristic returns no data, and
            // some Centrals will not start sending until they have seen this answer.
            DataWriter writer;
            request.RespondWithValue(writer.DetachBuffer());
        }
    }
    CATCH_LOG();

    deferral.Complete();
}


_Use_decl_annotations_
void
MidiBlePeripheral::OnWriteRequested(
    gatt::GattLocalCharacteristic const&,
    gatt::GattWriteRequestedEventArgs const& args
)
{
    auto deferral = args.GetDeferral();

    try
    {
        auto request = MidiBleUtilities::AwaitWithTimeout(
            args.GetRequestAsync(),
            MidiBleUtilities::BleDataOperationTimeoutMilliseconds,
            gatt::GattWriteRequest{ nullptr });

        if (request != nullptr)
        {
            auto bytes = ReadRequestBytes(request.Value());

            if (!bytes.empty())
            {
                if (auto connection = Connection())
                {
                    connection->ProcessIncomingPacket(bytes.data(), bytes.size());
                }
            }

            // MIDI data is written without response, but a Central which asks for one has to be
            // answered or it will stop writing.
            if (request.Option() == gatt::GattWriteOption::WriteWithResponse)
            {
                request.Respond();
            }
        }
    }
    CATCH_LOG();

    deferral.Complete();
}


_Use_decl_annotations_
void
MidiBlePeripheral::OnSubscribedClientsChanged(
    gatt::GattLocalCharacteristic const& sender,
    foundation::IInspectable const&
)
{
    bool clientChanged{ false };
    winrt::hstring clientDeviceId{};

    try
    {
        auto clients = sender.SubscribedClients();

        auto lock = std::scoped_lock{ m_clientLock };

        m_subscribedClientCount = clients != nullptr ? clients.Size() : 0;

        if (m_subscribedClientCount == 0)
        {
            clientChanged = m_activeClient != nullptr;
            m_activeClient = nullptr;
            m_activeClientDeviceId = L"";
        }
        else
        {
            bool activeClientStillSubscribed{ false };

            for (auto const& client : clients)
            {
                if (!m_activeClientDeviceId.empty() && client.Session().DeviceId().Id() == m_activeClientDeviceId)
                {
                    activeClientStillSubscribed = true;
                    break;
                }
            }

            // BLE MIDI permits a single active connection, so the first Central to subscribe keeps
            // the link until it goes away.
            if (!activeClientStillSubscribed)
            {
                m_activeClient = clients.GetAt(0);
                m_activeClientDeviceId = m_activeClient.Session().DeviceId().Id();
                clientChanged = true;
            }
        }

        clientDeviceId = m_activeClientDeviceId;
    }
    CATCH_LOG();

    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"BLE MIDI peripheral subscribed clients changed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt32(SubscribedClientCount(), "subscribed client count"),
        TraceLoggingBool(clientChanged, "client changed"),
        TraceLoggingWideString(clientDeviceId.c_str(), "client device id")
    );

    if (!clientChanged)
    {
        return;
    }

    // Creating or removing the endpoint calls back into the service, which must never happen on a
    // Bluetooth callback thread, so this only queues the work.
    std::function<void()> callback{ nullptr };

    {
        auto lock = std::scoped_lock{ m_clientChangedCallbackLock };
        callback = m_clientChangedCallback;
    }

    if (callback != nullptr)
    {
        callback();
    }
}


_Use_decl_annotations_
void
MidiBlePeripheral::OnAdvertisementStatusChanged(
    gatt::GattServiceProvider const&,
    gatt::GattServiceProviderAdvertisementStatusChangedEventArgs const& args
)
{
    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"BLE MIDI peripheral advertisement status changed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt32(static_cast<uint32_t>(args.Status()), "advertisement status"),
        TraceLoggingUInt32(static_cast<uint32_t>(args.Error()), "bluetooth error")
    );
}


_Use_decl_annotations_
HRESULT
MidiBlePeripheral::NotifyPacket(std::vector<uint8_t> const& packet)
{
    RETURN_HR_IF(S_FALSE, packet.empty());

    auto characteristic = m_characteristic;
    RETURN_HR_IF_NULL(HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_CONNECTED), characteristic);

    auto client = ActiveClient();

    // Nothing is listening. This is not an error: an endpoint with no Central connected is the
    // normal state of a published peripheral.
    if (client == nullptr)
    {
        return S_FALSE;
    }

    try
    {
        DataWriter writer;
        writer.WriteBytes(winrt::array_view<uint8_t const>(packet));

        // The overload which takes a client notifies only that client, which is what keeps a
        // second Central from being fed data meant for the first.
        auto result = MidiBleUtilities::AwaitWithTimeout(
            characteristic.NotifyValueAsync(writer.DetachBuffer(), client),
            MidiBleUtilities::BleDataOperationTimeoutMilliseconds,
            gatt::GattClientNotificationResult{ nullptr });

        if (result == nullptr)
        {
            return S_FALSE;
        }

        auto const status = result.Status();

        if (status != gatt::GattCommunicationStatus::Success)
        {
            TraceLoggingWrite(
                MidiBle2MidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"BLE MIDI peripheral notification failed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingUInt32(static_cast<uint32_t>(status), "gatt communication status")
            );

            RETURN_IF_FAILED(E_FAIL);
        }
    }
    CATCH_RETURN();

    return S_OK;
}


size_t
MidiBlePeripheral::MaxNotificationByteCount() const
{
    try
    {
        if (auto client = ActiveClient())
        {
            auto const maxNotificationSize = client.MaxNotificationSize();

            if (maxNotificationSize >= MinimumNotificationByteCount)
            {
                return static_cast<size_t>(maxNotificationSize);
            }
        }
    }
    CATCH_LOG();

    return MinimumNotificationByteCount;
}
