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

    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED), m_running.load());

    auto cleanupOnFailure = wil::scope_exit([&]() { LOG_IF_FAILED(Stop()); });

    // Both Characteristics are published whatever was asked for. The protocol is decided by
    // which one the Central subscribes to.
    UNREFERENCED_PARAMETER(protocol);

    RETURN_IF_FAILED(CreateServiceProvider());

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

        // Known once a Central has subscribed to one of the Characteristics
        auto const protocol = m_protocol.load();
        RETURN_HR_IF(E_UNEXPECTED, protocol == MidiBleProtocol::Protocol::Unknown);

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
            protocol,
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
    winrt::event_token parametersToken{ };

    {
        auto lock = m_connectionLock.lock_exclusive();

        connection = std::move(m_connection);
        m_connection = nullptr;
        m_connectionClientDeviceId = L"";
        m_remoteClientInfo = {};
        device = std::move(m_remoteDevice);
        m_remoteDevice = nullptr;
        parametersToken = std::exchange(m_remoteConnectionParametersChangedToken, {});
    }

    if (connection != nullptr)
    {
        LOG_IF_FAILED(connection->Shutdown());
    }

    if (device != nullptr && parametersToken)
    {
        try
        {
            device.ConnectionParametersChanged(parametersToken);
        }
        CATCH_LOG();
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
    winrt::event_token previousToken{ };

    {
        auto lock = m_connectionLock.lock_exclusive();

        previous = std::move(m_remoteDevice);
        previousToken = std::exchange(m_remoteConnectionParametersChangedToken, {});
        m_remoteDevice = device;
    }

    if (previous != nullptr && previousToken)
    {
        try
        {
            previous.ConnectionParametersChanged(previousToken);
        }
        CATCH_LOG();
    }

    // Closing happens outside the lock: a replaced client leaves an object behind and Close can block
    MidiBleUtilities::CloseIfOpen(previous);

    if (device == nullptr)
    {
        return;
    }

    // The remote Central chooses the interval, and can change it. The connection needs it for
    // the JR Clock period and the smoothing delay.
    try
    {
        auto token = device.ConnectionParametersChanged(
            [weakThis = weak_from_this()](bt::BluetoothLEDevice const&, foundation::IInspectable const&)
            {
                if (auto self = weakThis.lock())
                {
                    self->OnRemoteConnectionParametersChanged();
                }
            });

        bool kept{ false };

        {
            auto lock = m_connectionLock.lock_exclusive();

            if (m_remoteDevice == device)
            {
                m_remoteConnectionParametersChangedToken = token;
                kept = true;
            }
        }

        if (!kept)
        {
            device.ConnectionParametersChanged(token);
        }
    }
    CATCH_LOG();

    OnRemoteConnectionParametersChanged();
}


void
MidiBlePeripheral::OnRemoteConnectionParametersChanged()
{
    if (auto connection = Connection())
    {
        connection->SetConnectionIntervalUnits(ConnectionIntervalUnits());
    }
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


HRESULT
MidiBlePeripheral::CreateServiceProvider()
{
    // Declared HRESULT, so it must not throw: callers use RETURN_IF_FAILED and an
    // escaping WinRT exception would unwind past them into a worker thread.
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

        // Both Characteristics are published, so a Central which supports only one of the
        // protocols can still connect. WinRT can't refuse a subscription the way BLE MIDI 2.0
        // section 3.3 asks (ATT error 0x80), so the first subscription is served and any other is
        // ignored. A Central which follows the specification subscribes to only one, preferring
        // UMP, so it never notices.
        RETURN_IF_FAILED(CreateMidiCharacteristic(serviceProvider, MidiBleProtocol::Protocol::Midi1));
        RETURN_IF_FAILED(CreateMidiCharacteristic(serviceProvider, MidiBleProtocol::Protocol::Midi2Ump));

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

        return S_OK;
    }
    CATCH_RETURN()
}


_Use_decl_annotations_
HRESULT
MidiBlePeripheral::CreateMidiCharacteristic(
    gatt::GattServiceProvider const& serviceProvider,
    MidiBleProtocol::Protocol const protocol
)
{
    try
    {
        gatt::GattLocalCharacteristicParameters parameters{};

        // Notify carries data to the Central, WriteWithoutResponse carries data from it, and Read
        // exists only for the connection handshake, which answers with an empty payload.
        parameters.CharacteristicProperties(
            gatt::GattCharacteristicProperties::Notify |
            gatt::GattCharacteristicProperties::WriteWithoutResponse |
            gatt::GattCharacteristicProperties::Read);

        parameters.ReadProtectionLevel(gatt::GattProtectionLevel::Plain);
        parameters.WriteProtectionLevel(gatt::GattProtectionLevel::Plain);

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
                TraceLoggingWideString(L"Unable to create a BLE MIDI characteristic", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingUInt8(static_cast<uint8_t>(protocol), "protocol"),
                TraceLoggingUInt32(static_cast<uint32_t>(characteristicResult.Error()), "bluetooth error")
            );

            RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_SERVICE_NOT_ACTIVE));
        }

        auto characteristic = characteristicResult.Characteristic();
        RETURN_HR_IF_NULL(E_UNEXPECTED, characteristic);

        PublishedCharacteristic published{};
        published.Characteristic = characteristic;

        published.ReadRequestedToken = characteristic.ReadRequested(
            [weakThis = weak_from_this()](gatt::GattLocalCharacteristic const& sender, gatt::GattReadRequestedEventArgs const& args)
            {
                if (auto self = weakThis.lock())
                {
                    self->OnReadRequested(sender, args);
                }
            });

        published.WriteRequestedToken = characteristic.WriteRequested(
            [weakThis = weak_from_this(), protocol](gatt::GattLocalCharacteristic const&, gatt::GattWriteRequestedEventArgs const& args)
            {
                if (auto self = weakThis.lock())
                {
                    self->OnWriteRequested(protocol, args);
                }
            });

        published.SubscribedClientsChangedToken = characteristic.SubscribedClientsChanged(
            [weakThis = weak_from_this(), protocol](gatt::GattLocalCharacteristic const&, foundation::IInspectable const&)
            {
                if (auto self = weakThis.lock())
                {
                    self->OnSubscribedClientsChanged(protocol);
                }
            });

        {
            auto lock = m_providerLock.lock_exclusive();

            if (protocol == MidiBleProtocol::Protocol::Midi2Ump)
            {
                m_umpCharacteristic = published;
            }
            else
            {
                m_midi1Characteristic = published;
            }
        }

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
    PublishedCharacteristic midi1Characteristic{};
    PublishedCharacteristic umpCharacteristic{};
    gatt::GattServiceProvider serviceProvider{ nullptr };

    {
        auto lock = m_providerLock.lock_exclusive();

        midi1Characteristic = std::exchange(m_midi1Characteristic, {});
        umpCharacteristic = std::exchange(m_umpCharacteristic, {});
        serviceProvider = std::exchange(m_serviceProvider, nullptr);
        m_advertisedName = L"";
    }

    for (auto* published : { &midi1Characteristic, &umpCharacteristic })
    {
        try
        {
            auto const& characteristic = published->Characteristic;

            if (characteristic == nullptr)
            {
                continue;
            }

            if (published->ReadRequestedToken)
            {
                characteristic.ReadRequested(published->ReadRequestedToken);
            }

            if (published->WriteRequestedToken)
            {
                characteristic.WriteRequested(published->WriteRequestedToken);
            }

            if (published->SubscribedClientsChangedToken)
            {
                characteristic.SubscribedClientsChanged(published->SubscribedClientsChangedToken);
            }
        }
        CATCH_LOG();
    }

    try
    {
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


_Use_decl_annotations_
gatt::GattLocalCharacteristic
MidiBlePeripheral::Characteristic(MidiBleProtocol::Protocol const protocol) const
{
    auto lock = m_providerLock.lock_shared();

    return protocol == MidiBleProtocol::Protocol::Midi2Ump ?
        m_umpCharacteristic.Characteristic :
        protocol == MidiBleProtocol::Protocol::Midi1 ? m_midi1Characteristic.Characteristic : nullptr;
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
    MidiBleProtocol::Protocol const protocol,
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
            // connection was approved and attached for may feed its endpoint, and only on the
            // Characteristic it subscribed to.
            bool const accepted =
                connection != nullptr &&
                protocol == m_protocol.load() &&
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
    MidiBleProtocol::Protocol const protocol
)
{
    bool clientChanged{ false };
    winrt::hstring clientDeviceId{};

    try
    {
        auto const midi1Characteristic = Characteristic(MidiBleProtocol::Protocol::Midi1);
        auto const umpCharacteristic = Characteristic(MidiBleProtocol::Protocol::Midi2Ump);

        auto const midi1Clients = midi1Characteristic != nullptr ? midi1Characteristic.SubscribedClients() : nullptr;
        auto const umpClients = umpCharacteristic != nullptr ? umpCharacteristic.SubscribedClients() : nullptr;

        auto const clientsFor = [&](MidiBleProtocol::Protocol const which)
            {
                return which == MidiBleProtocol::Protocol::Midi2Ump ? umpClients : midi1Clients;
            };

        auto lock = m_clientLock.lock_exclusive();

        m_subscribedClientCount =
            (midi1Clients != nullptr ? midi1Clients.Size() : 0) +
            (umpClients != nullptr ? umpClients.Size() : 0);

        bool activeClientStillSubscribed{ false };
        auto const activeProtocol = m_protocol.load();

        if (!m_activeClientDeviceId.empty() && activeProtocol != MidiBleProtocol::Protocol::Unknown)
        {
            if (auto const clients = clientsFor(activeProtocol))
            {
                for (auto const& client : clients)
                {
                    if (client.Session().DeviceId().Id() == m_activeClientDeviceId)
                    {
                        activeClientStillSubscribed = true;
                        break;
                    }
                }
            }
        }

        // BLE MIDI permits a single active connection, so the first Central to subscribe keeps
        // the link until it goes away. Its subscription is what just raised this, so that
        // Characteristic is looked at first.
        if (!activeClientStillSubscribed)
        {
            auto const other = protocol == MidiBleProtocol::Protocol::Midi2Ump ?
                MidiBleProtocol::Protocol::Midi1 : MidiBleProtocol::Protocol::Midi2Ump;

            gatt::GattSubscribedClient chosen{ nullptr };
            auto chosenProtocol{ MidiBleProtocol::Protocol::Unknown };

            for (auto const which : { protocol, other })
            {
                auto const clients = clientsFor(which);

                if (clients != nullptr && clients.Size() > 0)
                {
                    chosen = clients.GetAt(0);
                    chosenProtocol = which;
                    break;
                }
            }

            clientChanged = m_activeClient != nullptr || chosen != nullptr;

            m_activeClient = chosen;
            m_activeClientDeviceId = chosen != nullptr ? chosen.Session().DeviceId().Id() : winrt::hstring{};
            m_protocol = chosenProtocol;
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

        auto characteristic = Characteristic(m_protocol.load());
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
