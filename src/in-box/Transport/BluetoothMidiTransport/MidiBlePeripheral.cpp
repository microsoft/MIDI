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
        MidiBluetoothMidiTransportTelemetryProvider::Provider(),
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
        MidiBluetoothMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"BLE MIDI peripheral published", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(AdvertisedName().c_str(), "advertised name"),
        TraceLoggingUInt8(static_cast<uint8_t>(protocol), "protocol")
    );

    return S_OK;
}


_Use_decl_annotations_
void
MidiBlePeripheral::SetClientChangedCallback(std::function<void()> callback)
{
    auto lock = m_clientChangedCallbackLock.lock_exclusive();

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
    // Declared HRESULT, so it must not throw: callers use RETURN_IF_FAILED and an
    // escaping WinRT exception would unwind past them into a worker thread.
    try
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
            m_protocol.load(),
            link));

        RETURN_IF_FAILED(newConnection->Start());

        std::shared_ptr<MidiBleConnection> replacedConnection{ nullptr };
        bool attached{ false };

        {
            auto lock = m_connectionLock.lock_exclusive();

            // Checked again under the lock Stop takes to clear m_running, so a connection is never
            // left attached to a peripheral which has already detached everything.
            if (m_running.load())
            {
                replacedConnection = std::exchange(m_connection, newConnection);
                m_connectionClientDeviceId = remoteDeviceId;
                attached = true;
            }
        }

        // Shut down outside the lock, because it joins a thread. Replacing one without shutting it
        // down would leave its writer thread running for the life of the service.
        if (replacedConnection != nullptr && replacedConnection != newConnection)
        {
            LOG_IF_FAILED(replacedConnection->Shutdown());
        }

        if (!attached)
        {
            LOG_IF_FAILED(newConnection->Shutdown());

            RETURN_HR(E_UNEXPECTED);
        }

        connection = newConnection;

        return S_OK;
    }
    CATCH_RETURN()
}


HRESULT
MidiBlePeripheral::DetachConnection()
{
    std::shared_ptr<MidiBleConnection> connection{ nullptr };
    bt::BluetoothLEDevice device{ nullptr };

    {
        auto lock = m_connectionLock.lock_exclusive();

        connection = std::move(m_connection);
        m_connection = nullptr;
        m_connectionClientDeviceId = L"";
        m_remoteClientInfo = {};
        device = std::move(m_remoteDevice);
        m_remoteDevice = nullptr;
    }

    if (connection != nullptr)
    {
        LOG_IF_FAILED(connection->Shutdown());
    }

    MidiBleUtilities::CloseIfOpen(device);

    return S_OK;
}


_Use_decl_annotations_
void
MidiBlePeripheral::SetRemoteClientInfo(MidiBleRemoteClientInfo const& info)
{
    auto lock = m_connectionLock.lock_exclusive();

    m_remoteClientInfo = info;
}


MidiBleRemoteClientInfo
MidiBlePeripheral::RemoteClientInfo() const
{
    auto lock = m_connectionLock.lock_exclusive();

    return m_remoteClientInfo;
}


_Use_decl_annotations_
void
MidiBlePeripheral::SetRemoteDevice(bt::BluetoothLEDevice const& device)
{
    bt::BluetoothLEDevice previous{ nullptr };

    {
        auto lock = m_connectionLock.lock_exclusive();

        previous = std::move(m_remoteDevice);
        m_remoteDevice = device;
    }

    // Closing happens outside the lock: a replaced client leaves an object behind and Close can block
    MidiBleUtilities::CloseIfOpen(previous);
}


uint16_t
MidiBlePeripheral::ConnectionIntervalUnits() const
{
    bt::BluetoothLEDevice device{ nullptr };

    {
        auto lock = m_connectionLock.lock_exclusive();
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
    // Declared HRESULT, so it must not throw: callers use RETURN_IF_FAILED and an
    // escaping WinRT exception would unwind past them into a worker thread.
    try
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
                    MidiBluetoothMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Unable to create the BLE MIDI GATT service", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingUInt32(static_cast<uint32_t>(providerResult.Error()), "bluetooth error")
                );

                RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_SERVICE_NOT_ACTIVE));
            }

            auto serviceProvider = providerResult.ServiceProvider();
            RETURN_HR_IF_NULL(E_UNEXPECTED, serviceProvider);

            {
                auto lock = m_providerLock.lock_exclusive();
                m_serviceProvider = serviceProvider;
            }

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
                serviceProvider.Service().CreateCharacteristicAsync(characteristicUuid, parameters),
                MidiBleUtilities::BleOperationTimeoutMilliseconds,
                gatt::GattLocalCharacteristicResult{ nullptr });

            RETURN_HR_IF_NULL(HRESULT_FROM_WIN32(ERROR_SERVICE_NOT_ACTIVE), characteristicResult);

            if (characteristicResult.Error() != bt::BluetoothError::Success)
            {
                TraceLoggingWrite(
                    MidiBluetoothMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Unable to create the BLE MIDI data I/O characteristic", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingUInt32(static_cast<uint32_t>(characteristicResult.Error()), "bluetooth error")
                );

                RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_SERVICE_NOT_ACTIVE));
            }

            auto characteristic = characteristicResult.Characteristic();
            RETURN_HR_IF_NULL(E_UNEXPECTED, characteristic);

            {
                auto lock = m_providerLock.lock_exclusive();
                m_characteristic = characteristic;
            }

            m_readRequestedToken = characteristic.ReadRequested(
                [weakThis = weak_from_this()](gatt::GattLocalCharacteristic const& sender, gatt::GattReadRequestedEventArgs const& args)
                {
                    if (auto self = weakThis.lock())
                    {
                        self->OnReadRequested(sender, args);
                    }
                });

            m_writeRequestedToken = characteristic.WriteRequested(
                [weakThis = weak_from_this()](gatt::GattLocalCharacteristic const& sender, gatt::GattWriteRequestedEventArgs const& args)
                {
                    if (auto self = weakThis.lock())
                    {
                        self->OnWriteRequested(sender, args);
                    }
                });

            m_subscribedClientsChangedToken = characteristic.SubscribedClientsChanged(
                [weakThis = weak_from_this()](gatt::GattLocalCharacteristic const& sender, foundation::IInspectable const& args)
                {
                    if (auto self = weakThis.lock())
                    {
                        self->OnSubscribedClientsChanged(sender, args);
                    }
                });

            m_advertisementStatusChangedToken = serviceProvider.AdvertisementStatusChanged(
                [weakThis = weak_from_this()](gatt::GattServiceProvider const& sender, gatt::GattServiceProviderAdvertisementStatusChangedEventArgs const& args)
                {
                    if (auto self = weakThis.lock())
                    {
                        self->OnAdvertisementStatusChanged(sender, args);
                    }
                });

            auto advertisedName = MidiBleUtilities::GetLocalBluetoothName();

            {
                auto lock = m_providerLock.lock_exclusive();
                m_advertisedName = std::move(advertisedName);
            }
        }
        CATCH_RETURN();

        return S_OK;
    }
    CATCH_RETURN()
}


HRESULT
MidiBlePeripheral::StartAdvertising()
{
    try
    {
        auto const serviceProvider = ServiceProvider();
        RETURN_HR_IF_NULL(E_UNEXPECTED, serviceProvider);

        gatt::GattServiceProviderAdvertisingParameters parameters{};

        // Both are required: a Central has to be able to find us and then connect to us. The MIDI
        // service UUID is placed in the advertisement by the service provider itself, which is
        // what a BLE MIDI Central scans for.
        parameters.IsConnectable(true);
        parameters.IsDiscoverable(true);

        serviceProvider.StartAdvertising(parameters);
    }
    CATCH_RETURN();

    return S_OK;
}


HRESULT
MidiBlePeripheral::Stop()
{
    TraceLoggingWrite(
        MidiBluetoothMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    {
        // AttachConnection checks this under the same lock before it attaches anything
        auto lock = m_connectionLock.lock_exclusive();
        m_running = false;
    }

    LOG_IF_FAILED(DetachConnection());

    // Taken out under the lock, so the data path and configuration calls either hold their own
    // reference already or find nothing. The teardown below works on these copies.
    gatt::GattLocalCharacteristic characteristic{ nullptr };
    gatt::GattServiceProvider serviceProvider{ nullptr };

    {
        auto lock = m_providerLock.lock_exclusive();

        characteristic = std::exchange(m_characteristic, nullptr);
        serviceProvider = std::exchange(m_serviceProvider, nullptr);
        m_advertisedName = L"";
    }

    try
    {
        if (characteristic != nullptr)
        {
            if (m_readRequestedToken)
            {
                characteristic.ReadRequested(m_readRequestedToken);
                m_readRequestedToken = {};
            }

            if (m_writeRequestedToken)
            {
                characteristic.WriteRequested(m_writeRequestedToken);
                m_writeRequestedToken = {};
            }

            if (m_subscribedClientsChangedToken)
            {
                characteristic.SubscribedClientsChanged(m_subscribedClientsChangedToken);
                m_subscribedClientsChangedToken = {};
            }
        }

        if (serviceProvider != nullptr)
        {
            if (m_advertisementStatusChangedToken)
            {
                serviceProvider.AdvertisementStatusChanged(m_advertisementStatusChangedToken);
                m_advertisementStatusChangedToken = {};
            }

            if (serviceProvider.AdvertisementStatus() == gatt::GattServiceProviderAdvertisementStatus::Started)
            {
                serviceProvider.StopAdvertising();
            }
        }
    }
    CATCH_LOG();

    {
        auto lock = m_clientLock.lock_exclusive();
        m_activeClient = nullptr;
        m_activeClientDeviceId = L"";
        m_subscribedClientCount = 0;
    }

    {
        auto lock = m_clientChangedCallbackLock.lock_exclusive();
        m_clientChangedCallback = nullptr;
    }

    m_protocol = MidiBleProtocol::Protocol::Unknown;

    return S_OK;
}


winrt::hstring
MidiBlePeripheral::AdvertisedName() const
{
    auto lock = m_providerLock.lock_shared();

    return m_advertisedName;
}


gatt::GattLocalCharacteristic
MidiBlePeripheral::Characteristic() const
{
    auto lock = m_providerLock.lock_shared();

    return m_characteristic;
}


gatt::GattServiceProvider
MidiBlePeripheral::ServiceProvider() const
{
    auto lock = m_providerLock.lock_shared();

    return m_serviceProvider;
}


winrt::hstring
MidiBlePeripheral::ActiveClientDeviceId() const
{
    auto lock = m_clientLock.lock_exclusive();

    return m_activeClientDeviceId;
}


std::shared_ptr<MidiBleConnection>
MidiBlePeripheral::Connection() const
{
    auto lock = m_connectionLock.lock_exclusive();

    return m_connection;
}


bool
MidiBlePeripheral::IsClientSubscribed() const
{
    auto lock = m_clientLock.lock_exclusive();

    return m_activeClient != nullptr;
}


uint32_t
MidiBlePeripheral::SubscribedClientCount() const
{
    auto lock = m_clientLock.lock_exclusive();

    return m_subscribedClientCount;
}


gatt::GattSubscribedClient
MidiBlePeripheral::ActiveClient() const
{
    auto lock = m_clientLock.lock_exclusive();

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
            winrt::hstring writerDeviceId{ };

            if (auto const session = args.Session())
            {
                if (auto const bluetoothDeviceId = session.DeviceId())
                {
                    writerDeviceId = bluetoothDeviceId.Id();
                }
            }

            std::shared_ptr<MidiBleConnection> connection{ nullptr };
            winrt::hstring attachedClientDeviceId{ };

            {
                auto lock = m_connectionLock.lock_shared();

                connection = m_connection;
                attachedClientDeviceId = m_connectionClientDeviceId;
            }

            // Any Central in range can write to a published characteristic. Only the one this
            // connection was approved and attached for may feed its endpoint.
            bool const accepted =
                connection != nullptr &&
                MidiBleUtilities::IsWriteFromAttachedPeripheralClient(writerDeviceId, attachedClientDeviceId);

            if (accepted)
            {
                auto bytes = ReadRequestBytes(request.Value());

                if (!bytes.empty())
                {
                    connection->ProcessIncomingPacket(bytes.data(), bytes.size());
                }
            }
            else
            {
                TraceLoggingWrite(
                    MidiBluetoothMidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_VERBOSE,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Dropped a write from a Central which has no approved connection", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(writerDeviceId.c_str(), "writer device id"),
                    TraceLoggingWideString(attachedClientDeviceId.c_str(), "attached client device id")
                );
            }

            // MIDI data is written without response, but a Central which asks for one has to be
            // answered or it will stop writing. One which is not approved is told so.
            if (request.Option() == gatt::GattWriteOption::WriteWithResponse)
            {
                if (accepted)
                {
                    request.Respond();
                }
                else
                {
                    request.RespondWithProtocolError(gatt::GattProtocolError::InsufficientAuthorization());
                }
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

        auto lock = m_clientLock.lock_exclusive();

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
        MidiBluetoothMidiTransportTelemetryProvider::Provider(),
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
        auto lock = m_clientChangedCallbackLock.lock_exclusive();
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
        MidiBluetoothMidiTransportTelemetryProvider::Provider(),
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
    // Declared HRESULT, so it must not throw: callers use RETURN_IF_FAILED and an
    // escaping WinRT exception would unwind past them into a worker thread.
    try
    {
        RETURN_HR_IF(S_FALSE, packet.empty());

        auto characteristic = Characteristic();
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
            // second Central from being fed data meant for the first. Stop joins the writer thread
            // this runs on, so the wait ends as soon as the peripheral stops.
            auto result = MidiBleUtilities::AwaitWithTimeoutOrAbandon(
                characteristic.NotifyValueAsync(writer.DetachBuffer(), client),
                MidiBleUtilities::BleDataOperationTimeoutMilliseconds,
                gatt::GattClientNotificationResult{ nullptr },
                [this]() noexcept { return !m_running.load(); });

            if (result == nullptr)
            {
                return S_FALSE;
            }

            auto const status = result.Status();

            if (status != gatt::GattCommunicationStatus::Success)
            {
                TraceLoggingWrite(
                    MidiBluetoothMidiTransportTelemetryProvider::Provider(),
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
    CATCH_RETURN()
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
