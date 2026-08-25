// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "midi2.Ble2MidiTransport.h"

using namespace wil;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

namespace
{
    struct BleMidiCharacteristicSelection
    {
        MidiBleProtocol::Protocol Protocol{ MidiBleProtocol::Protocol::Unknown };
        gatt::GattCharacteristic Characteristic{ nullptr };
    };

    // BLE MIDI 2.0 section 3.3: a Central which discovers both Characteristics subscribes to the
    // UMP Characteristic. A Peripheral rejects a subscription to the second one anyway, so this
    // is also the only order that can succeed.
    BleMidiCharacteristicSelection SelectPreferredMidiCharacteristic(_In_ gatt::GattDeviceService const& service)
    {
        BleMidiCharacteristicSelection selection{};

        if (service == nullptr)
        {
            return selection;
        }

        winrt::guid umpCharacteristicUuid{ MidiBleProtocol::Midi2UmpCharacteristicUuid };
        auto umpCharacteristics = service.GetCharacteristicsForUuidAsync(umpCharacteristicUuid, bt::BluetoothCacheMode::Uncached).get();

        if (umpCharacteristics.Status() == gatt::GattCommunicationStatus::Success && umpCharacteristics.Characteristics().Size() > 0)
        {
            selection.Protocol = MidiBleProtocol::Protocol::Midi2Ump;
            selection.Characteristic = umpCharacteristics.Characteristics().GetAt(0);

            return selection;
        }

        winrt::guid midi1CharacteristicUuid{ MidiBleProtocol::Midi1DataIoCharacteristicUuid };
        auto midi1Characteristics = service.GetCharacteristicsForUuidAsync(midi1CharacteristicUuid, bt::BluetoothCacheMode::Uncached).get();

        if (midi1Characteristics.Status() == gatt::GattCommunicationStatus::Success && midi1Characteristics.Characteristics().Size() > 0)
        {
            selection.Protocol = MidiBleProtocol::Protocol::Midi1;
            selection.Characteristic = midi1Characteristics.Characteristics().GetAt(0);
        }

        return selection;
    }

    // Stable across builds, processes and reboots, unlike std::hash. Not used for identity here,
    // only to keep names readable in the instance id.
    std::wstring BuildEndpointDeviceInstanceId(_In_ MidiBleProtocol::DiscoveredDevice const& device)
    {
        // The Bluetooth address is the device's identity, so an endpoint keeps the same device
        // node across disconnects and reconnects and any user configuration survives with it.
        auto readableName = internal::RemoveInvalidSWDUniqueIdCharacters(std::wstring{ device.Name });

        if (readableName.length() > MIDI_BLE_ENDPOINT_INSTANCE_ID_NAME_MAX_CHARS)
        {
            readableName = readableName.substr(0, MIDI_BLE_ENDPOINT_INSTANCE_ID_NAME_MAX_CHARS);
        }

        if (!readableName.empty())
        {
            readableName += L"_";
        }

        return internal::NormalizeDeviceInstanceIdWStringCopy(
            std::wstring{ MIDI_BLE_ENDPOINT_INSTANCE_ID_PREFIX } +
            readableName +
            std::wstring{ device.Id });
    }

    uint64_t NowInMilliseconds()
    {
        return internal::ConvertTimestampToWholeMilliseconds(
            internal::GetCurrentMidiTimestamp(),
            internal::GetMidiTimestampFrequency());
    }
}


_Use_decl_annotations_
HRESULT
CMidi2Ble2MidiEndpointManager::Initialize(
    IMidiDeviceManager* midiDeviceManager,
    IMidiEndpointProtocolManager* midiEndpointProtocolManager
)
{
    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    RETURN_HR_IF(E_INVALIDARG, nullptr == midiDeviceManager);
    RETURN_HR_IF(E_INVALIDARG, nullptr == midiEndpointProtocolManager);

    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManager), (void**)&m_midiDeviceManager));
    RETURN_IF_FAILED(midiEndpointProtocolManager->QueryInterface(__uuidof(IMidiEndpointProtocolManager), (void**)&m_midiProtocolManager));

    m_transportId = TRANSPORT_LAYER_GUID;   // this is needed so MidiSrv can instantiate the correct transport
    m_containerId = m_transportId;          // we use the transport ID as the container ID for convenience

    RETURN_IF_FAILED(CreateParentDevice());

    m_initialized = true;

    RETURN_IF_FAILED(StartBackgroundEndpointCreator());

    // Advertisements are the primary source: they surface unpaired devices too, which is the
    // main gap in the older Windows BLE MIDI 1.0 support. The GATT watcher then fills in the
    // devices Windows already knows about but which are not advertising right now.
    LOG_IF_FAILED(StartAdvertisementWatcher());
    LOG_IF_FAILED(StartGattServiceWatcher());

    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}


HRESULT
CMidi2Ble2MidiEndpointManager::StartAdvertisementWatcher()
{
    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    try
    {
        m_advertisementWatcher = bt::Advertisement::BluetoothLEAdvertisementWatcher();

        // Active scanning also collects scan response data, which is where some peripherals put
        // the service UUID and the complete local name.
        m_advertisementWatcher.ScanningMode(bt::Advertisement::BluetoothLEScanningMode::Active);

        winrt::guid midiServiceUuid{ MidiBleProtocol::MidiServiceUuid };
        m_advertisementWatcher.AdvertisementFilter().Advertisement().ServiceUuids().Append(midiServiceUuid);

        m_advertisementReceivedToken = m_advertisementWatcher.Received({ this, &CMidi2Ble2MidiEndpointManager::OnAdvertisementReceived });

        m_advertisementWatcher.Start();
    }
    CATCH_RETURN();

    return S_OK;
}


HRESULT
CMidi2Ble2MidiEndpointManager::StartGattServiceWatcher()
{
    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    try
    {
        winrt::guid midiBleServiceUuid{ MidiBleProtocol::MidiServiceUuid };
        winrt::hstring query = gatt::GattDeviceService::GetDeviceSelectorFromUuid(midiBleServiceUuid);

        auto props = winrt::single_threaded_vector<winrt::hstring>();

        props.Append(L"System.DeviceInterface.Bluetooth.DeviceAddress");
        props.Append(L"System.Devices.Aep.IsPaired");
        props.Append(L"System.Devices.Connected");

        m_deviceWatcher = enumeration::DeviceInformation::CreateWatcher(query, props);

        m_deviceWatcherAddedToken = m_deviceWatcher.Added({ this, &CMidi2Ble2MidiEndpointManager::OnDeviceWatcherAdded });
        m_deviceWatcherUpdatedToken = m_deviceWatcher.Updated({ this, &CMidi2Ble2MidiEndpointManager::OnDeviceWatcherUpdated });
        m_deviceWatcherRemovedToken = m_deviceWatcher.Removed({ this, &CMidi2Ble2MidiEndpointManager::OnDeviceWatcherRemoved });
        m_deviceWatcherStoppedToken = m_deviceWatcher.Stopped({ this, &CMidi2Ble2MidiEndpointManager::OnDeviceWatcherStopped });

        m_deviceWatcher.Start();
    }
    CATCH_RETURN();

    return S_OK;
}


_Use_decl_annotations_
void
CMidi2Ble2MidiEndpointManager::OnAdvertisementReceived(
    bt::Advertisement::BluetoothLEAdvertisementWatcher const&,
    bt::Advertisement::BluetoothLEAdvertisementReceivedEventArgs const& args
)
{
    try
    {
        auto const address = args.BluetoothAddress();

        if (address == 0)
        {
            return;
        }

        MidiBleProtocol::DiscoveredDevice device{};

        device.BluetoothAddress = address;
        device.Id = MidiBleUtilities::FormatBluetoothAddress(address);
        device.Name = args.Advertisement().LocalName();
        device.LastSignalStrengthDbm = args.RawSignalStrengthInDBm();
        device.LastSeenTimestamp = NowInMilliseconds();

        UpsertDiscoveredDevice(device);
    }
    CATCH_LOG();
}


_Use_decl_annotations_
HRESULT
CMidi2Ble2MidiEndpointManager::OnDeviceWatcherAdded(
    enumeration::DeviceWatcher const&,
    enumeration::DeviceInformation const& args
)
{
    try
    {
        uint64_t address{ 0 };

        auto const addressPropertyKey = winrt::hstring{ L"System.DeviceInterface.Bluetooth.DeviceAddress" };

        if (args.Properties().HasKey(addressPropertyKey))
        {
            auto property = args.Properties().Lookup(addressPropertyKey);

            if (property != nullptr)
            {
                // the property system has historically supplied this both as a formatted string
                // and as a raw integer, so both are accepted
                if (auto stringValue = property.try_as<foundation::IReference<winrt::hstring>>())
                {
                    MidiBleUtilities::TryParseBluetoothAddress(std::wstring{ stringValue.Value() }, address);
                }
                else if (auto uint64Value = property.try_as<foundation::IReference<uint64_t>>())
                {
                    address = uint64Value.Value();
                }
            }
        }

        if (address == 0)
        {
            // without an address there is nothing to correlate this with, and nothing to connect to
            TraceLoggingWrite(
                MidiBle2MidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"BLE MIDI GATT service has no readable Bluetooth address", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(args.Id().c_str(), "id")
            );

            return S_OK;
        }

        MidiBleProtocol::DiscoveredDevice device{};

        device.BluetoothAddress = address;
        device.Id = MidiBleUtilities::FormatBluetoothAddress(address);
        device.Name = args.Name();
        device.GattServiceDeviceId = args.Id();
        device.IsPaired = true;
        device.LastSeenTimestamp = NowInMilliseconds();

        TraceLoggingWrite(
            MidiBle2MidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"BLE MIDI GATT service found", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(args.Id().c_str(), "id"),
            TraceLoggingWideString(device.Name.c_str(), "name"),
            TraceLoggingWideString(device.Id.c_str(), "device id")
        );

        UpsertDiscoveredDevice(device);
    }
    CATCH_LOG();

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2Ble2MidiEndpointManager::OnDeviceWatcherUpdated(
    enumeration::DeviceWatcher const&,
    enumeration::DeviceInformationUpdate const& /*args*/
)
{
    // deliberately quiet. Connection state is tracked through the GATT session, not here, and
    // these updates are extremely noisy.
    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2Ble2MidiEndpointManager::OnDeviceWatcherRemoved(
    enumeration::DeviceWatcher const&,
    enumeration::DeviceInformationUpdate const& args
)
{
    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"BLE MIDI GATT service removed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(args.Id().c_str(), "id")
    );

    // The service interface disappearing does not mean the device is gone for good, and BLE
    // devices come and go constantly. The GATT service id is cleared so a later connect goes
    // through the address, but the entry and any live connection are left alone.
    auto lock = std::scoped_lock{ m_discoveredDevicesLock };

    for (auto& entry : m_discoveredDevices)
    {
        if (entry.second.GattServiceDeviceId == args.Id())
        {
            entry.second.GattServiceDeviceId = L"";
            break;
        }
    }

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2Ble2MidiEndpointManager::OnDeviceWatcherStopped(
    enumeration::DeviceWatcher const&,
    foundation::IInspectable const&
)
{
    return S_OK;
}


_Use_decl_annotations_
void
CMidi2Ble2MidiEndpointManager::UpsertDiscoveredDevice(MidiBleProtocol::DiscoveredDevice const& device)
{
    if (device.Id.empty())
    {
        return;
    }

    auto lock = std::scoped_lock{ m_discoveredDevicesLock };

    if (auto existing = m_discoveredDevices.find(device.Id); existing != m_discoveredDevices.end())
    {
        // Advertisements and the GATT watcher each know only part of the picture, so an update
        // from one never erases what came from the other.
        if (!device.Name.empty())
        {
            existing->second.Name = device.Name;
        }

        if (!device.GattServiceDeviceId.empty())
        {
            existing->second.GattServiceDeviceId = device.GattServiceDeviceId;
        }

        if (device.IsPaired)
        {
            existing->second.IsPaired = true;
        }

        if (device.LastSignalStrengthDbm != 0)
        {
            existing->second.LastSignalStrengthDbm = device.LastSignalStrengthDbm;
        }

        existing->second.LastSeenTimestamp = device.LastSeenTimestamp;

        return;
    }

    m_discoveredDevices.insert_or_assign(device.Id, device);
}


_Use_decl_annotations_
bool
CMidi2Ble2MidiEndpointManager::TryGetDiscoveredDevice(
    winrt::hstring const& deviceId,
    MidiBleProtocol::DiscoveredDevice& device
)
{
    auto lock = std::scoped_lock{ m_discoveredDevicesLock };

    if (auto entry = m_discoveredDevices.find(deviceId); entry != m_discoveredDevices.end())
    {
        device = entry->second;
        return true;
    }

    return false;
}


_Use_decl_annotations_
void
CMidi2Ble2MidiEndpointManager::UpdateDiscoveredDeviceConnectionState(
    winrt::hstring const& deviceId,
    bool const isConnected,
    MidiBleProtocol::Protocol const protocol,
    winrt::hstring const& endpointDeviceId
)
{
    auto lock = std::scoped_lock{ m_discoveredDevicesLock };

    if (auto entry = m_discoveredDevices.find(deviceId); entry != m_discoveredDevices.end())
    {
        entry->second.IsConnected = isConnected;
        entry->second.SelectedProtocol = protocol;
        entry->second.NativeDataFormat = protocol == MidiBleProtocol::Protocol::Midi2Ump ?
            MidiBleProtocol::NativeDataFormat::UniversalMidiPacket :
            protocol == MidiBleProtocol::Protocol::Midi1 ?
            MidiBleProtocol::NativeDataFormat::TimestampedMidi1ByteStream :
            MidiBleProtocol::NativeDataFormat::Unknown;
        entry->second.EndpointDeviceId = endpointDeviceId;
    }
}


std::vector<MidiBleProtocol::DiscoveredDevice>
CMidi2Ble2MidiEndpointManager::GetDiscoveredDevices()
{
    auto const now = NowInMilliseconds();

    auto lock = std::scoped_lock{ m_discoveredDevicesLock };

    std::vector<MidiBleProtocol::DiscoveredDevice> devices;
    devices.reserve(m_discoveredDevices.size());

    for (auto const& entry : m_discoveredDevices)
    {
        // a connected or paired device stays listed even when it is not advertising
        bool const isStale =
            !entry.second.IsConnected &&
            !entry.second.IsPaired &&
            entry.second.LastSeenTimestamp + MidiBleProtocol::DeviceStaleAfterMilliseconds < now;

        if (!isStale)
        {
            devices.push_back(entry.second);
        }
    }

    return devices;
}


_Use_decl_annotations_
HRESULT
CMidi2Ble2MidiEndpointManager::ConnectDevice(winrt::hstring const& deviceId)
{
    RETURN_HR_IF(E_INVALIDARG, deviceId.empty());
    RETURN_HR_IF(E_UNEXPECTED, !m_initialized);

    if (TransportState::Current().GetConnectionByDeviceId(deviceId) != nullptr)
    {
        return S_OK;
    }

    {
        auto lock = std::scoped_lock{ m_pendingRequestsLock };
        m_pendingConnectRequests.push_back(deviceId);
    }

    RETURN_IF_FAILED(WakeupBackgroundEndpointCreatorThread());

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2Ble2MidiEndpointManager::DisconnectDevice(winrt::hstring const& deviceId)
{
    RETURN_HR_IF(E_INVALIDARG, deviceId.empty());

    {
        auto lock = std::scoped_lock{ m_pendingRequestsLock };
        m_pendingDisconnectRequests.push_back(deviceId);
    }

    RETURN_IF_FAILED(WakeupBackgroundEndpointCreatorThread());

    return S_OK;
}


HRESULT
CMidi2Ble2MidiEndpointManager::WakeupBackgroundEndpointCreatorThread()
{
    m_backgroundEndpointCreatorThreadWakeup.SetEvent();

    return S_OK;
}


HRESULT
CMidi2Ble2MidiEndpointManager::StartBackgroundEndpointCreator()
{
    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    m_backgroundEndpointCreatorThread = std::jthread([this](std::stop_token stopToken)
        {
            LOG_IF_FAILED(EndpointCreatorWorker(stopToken));
        });

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2Ble2MidiEndpointManager::EndpointCreatorWorker(std::stop_token stopToken)
{
    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // COM and WinRT calls from this thread fail CO_E_NOTINITIALIZED without this
    winrt::init_apartment();

    // Without this the thread parks in wait() and never sees the stop request, so the jthread
    // destructor's join would hang forever.
    std::stop_callback wakeOnStop{ stopToken, [this]() { m_backgroundEndpointCreatorThreadWakeup.SetEvent(); } };

    while (!stopToken.stop_requested())
    {
        std::deque<winrt::hstring> connectRequests;
        std::deque<winrt::hstring> disconnectRequests;
        std::deque<std::wstring> negotiations;

        bool haveWork{ false };

        {
            auto lock = std::scoped_lock{ m_pendingRequestsLock };

            connectRequests.swap(m_pendingConnectRequests);
            disconnectRequests.swap(m_pendingDisconnectRequests);
            negotiations.swap(m_pendingNegotiations);

            haveWork = !connectRequests.empty() || !disconnectRequests.empty() || !negotiations.empty();

            // Reset under the same lock the producers queue under. Resetting outside it can
            // clear a signal raised for work queued after the swap, and the worker then sleeps
            // with a request sitting in the queue.
            if (!haveWork)
            {
                m_backgroundEndpointCreatorThreadWakeup.ResetEvent();
            }
        }

        if (!haveWork)
        {
            m_backgroundEndpointCreatorThreadWakeup.wait();

            continue;
        }

        // disconnects run first so a reconnect request in the same pass gets a clean slate
        for (auto const& deviceId : disconnectRequests)
        {
            if (stopToken.stop_requested())
            {
                break;
            }

            LOG_IF_FAILED(DisconnectDeviceInternal(deviceId));
        }

        for (auto const& deviceId : connectRequests)
        {
            if (stopToken.stop_requested())
            {
                break;
            }

            LOG_IF_FAILED(ConnectDeviceInternal(deviceId));
        }

        for (auto const& endpointDeviceInterfaceId : negotiations)
        {
            if (stopToken.stop_requested())
            {
                break;
            }

            LOG_IF_FAILED(InitiateDiscoveryAndNegotiation(endpointDeviceInterfaceId));
        }
    }

    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2Ble2MidiEndpointManager::ConnectDeviceInternal(winrt::hstring const& deviceId)
{
    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(deviceId.c_str(), "device id")
    );

    if (TransportState::Current().GetConnectionByDeviceId(deviceId) != nullptr)
    {
        return S_OK;
    }

    MidiBleProtocol::DiscoveredDevice discoveredDevice{};
    RETURN_HR_IF(E_NOTFOUND, !TryGetDiscoveredDevice(deviceId, discoveredDevice));
    RETURN_HR_IF(E_NOTFOUND, discoveredDevice.BluetoothAddress == 0);

    bt::BluetoothLEDevice bleDevice{ nullptr };
    gatt::GattDeviceService service{ nullptr };
    gatt::GattSession session{ nullptr };
    BleMidiCharacteristicSelection selection{};

    try
    {
        bleDevice = bt::BluetoothLEDevice::FromBluetoothAddressAsync(discoveredDevice.BluetoothAddress).get();
        RETURN_HR_IF_NULL(HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_AVAILABLE), bleDevice);

        if (discoveredDevice.Name.empty())
        {
            discoveredDevice.Name = bleDevice.Name();
        }

        service = MidiBleUtilities::GetBleMidiServiceFromDevice(bleDevice);
        RETURN_HR_IF_NULL(E_NOTFOUND, service);

        auto openStatus = service.OpenAsync(gatt::GattSharingMode::SharedReadAndWrite).get();
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED), openStatus == gatt::GattOpenStatus::AccessDenied);
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION), openStatus == gatt::GattOpenStatus::SharingViolation);
        RETURN_HR_IF(E_NOTFOUND, openStatus == gatt::GattOpenStatus::NotFound);
        RETURN_HR_IF(E_FAIL, openStatus == gatt::GattOpenStatus::Unspecified);

        selection = SelectPreferredMidiCharacteristic(service);
        RETURN_HR_IF(E_NOTFOUND, selection.Protocol == MidiBleProtocol::Protocol::Unknown);
        RETURN_HR_IF_NULL(E_NOTFOUND, selection.Characteristic);

        session = gatt::GattSession::FromDeviceIdAsync(bleDevice.BluetoothDeviceId()).get();
        RETURN_HR_IF_NULL(E_FAIL, session);

        // BLE devices drop out constantly. This asks the system to re-establish the link rather
        // than leaving an app holding an endpoint that has silently stopped working.
        session.MaintainConnection(true);
    }
    CATCH_RETURN();

    auto connection = std::make_shared<MidiBleConnection>();
    RETURN_IF_NULL_ALLOC(connection);

    RETURN_IF_FAILED(connection->Initialize(
        deviceId,
        discoveredDevice.Name,
        selection.Protocol,
        bleDevice,
        service,
        selection.Characteristic,
        session));

    auto hr = connection->Start();

    if (FAILED(hr))
    {
        LOG_IF_FAILED(connection->Shutdown());
        RETURN_IF_FAILED(hr);
    }

    // Registered before the endpoint exists, because activating it can bring a client in through
    // the bidi immediately and the bidi resolves its connection out of this table.
    RETURN_IF_FAILED(TransportState::Current().AddConnection(connection));

    hr = CreateEndpointForConnection(connection);

    if (FAILED(hr))
    {
        LOG_IF_FAILED(TransportState::Current().RemoveConnection(deviceId));
        RETURN_IF_FAILED(hr);
    }

    UpdateDiscoveredDeviceConnectionState(
        deviceId,
        true,
        selection.Protocol,
        winrt::hstring{ connection->EndpointDeviceInterfaceId() });

    // A BLE MIDI 2.0 endpoint is a real UMP endpoint, so it goes through the normal endpoint
    // discovery and protocol negotiation. A BLE MIDI 1.0 endpoint has nothing to negotiate.
    if (selection.Protocol == MidiBleProtocol::Protocol::Midi2Ump)
    {
        LOG_IF_FAILED(InitiateDiscoveryAndNegotiation(connection->EndpointDeviceInterfaceId()));
    }

    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"BLE MIDI device connected", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(deviceId.c_str(), "device id"),
        TraceLoggingWideString(discoveredDevice.Name.c_str(), "name"),
        TraceLoggingUInt8(static_cast<uint8_t>(selection.Protocol), "protocol")
    );

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2Ble2MidiEndpointManager::DisconnectDeviceInternal(winrt::hstring const& deviceId)
{
    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(deviceId.c_str(), "device id")
    );

    auto connection = TransportState::Current().GetConnectionByDeviceId(deviceId);

    if (connection == nullptr)
    {
        UpdateDiscoveredDeviceConnectionState(deviceId, false, MidiBleProtocol::Protocol::Unknown, L"");

        return S_OK;
    }

    auto const deviceInstanceId = connection->EndpointDeviceInstanceId();

    // the callback is released before the endpoint goes away, because removal re-enters
    // synchronously through the bidi's Shutdown
    LOG_IF_FAILED(connection->DisconnectMidiCallback());

    LOG_IF_FAILED(TransportState::Current().RemoveConnection(deviceId));

    if (!deviceInstanceId.empty())
    {
        LOG_IF_FAILED(DeleteEndpoint(deviceInstanceId));
    }

    UpdateDiscoveredDeviceConnectionState(deviceId, false, MidiBleProtocol::Protocol::Unknown, L"");

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2Ble2MidiEndpointManager::CreateEndpointForConnection(std::shared_ptr<MidiBleConnection> connection)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, connection);
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_midiDeviceManager);
    RETURN_HR_IF(E_UNEXPECTED, m_parentDeviceId.empty());

    MidiBleProtocol::DiscoveredDevice discoveredDevice{};
    RETURN_HR_IF(E_NOTFOUND, !TryGetDiscoveredDevice(connection->DeviceId(), discoveredDevice));

    if (discoveredDevice.Name.empty())
    {
        discoveredDevice.Name = connection->DeviceName();
    }

    std::wstring endpointName{ discoveredDevice.Name };

    if (endpointName.empty())
    {
        endpointName = std::wstring{ L"BLE MIDI " } + std::wstring{ discoveredDevice.Id };
    }

    bool const isUmpNative = connection->Protocol() == MidiBleProtocol::Protocol::Midi2Ump;

    std::wstring endpointDescription{ isUmpNative ? MIDI_BLE_MIDI2_ENDPOINT_DESCRIPTION : MIDI_BLE_MIDI1_ENDPOINT_DESCRIPTION };
    std::wstring transportCode{ TRANSPORT_CODE };
    std::wstring uniqueIdentifier{ discoveredDevice.Id };

    std::wstring instanceId = BuildEndpointDeviceInstanceId(discoveredDevice);

    std::vector<DEVPROPERTY> interfaceDevProperties;

    DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;

    if (!isUmpNative)
    {
        // A BLE MIDI 1.0 device presents as a UMP endpoint but has no endpoint discovery to run,
        // so the service is told discovery is done and can create the MIDI 1.0 ports immediately
        // instead of waiting for a negotiation that will never happen.
        interfaceDevProperties.push_back({ { PKEY_MIDI_EndpointDiscoveryProcessComplete, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)), (PVOID)&devPropTrue });
    }

    MIDIENDPOINTCOMMONPROPERTIES commonProperties{};

    commonProperties.TransportId = TRANSPORT_LAYER_GUID;
    commonProperties.EndpointDeviceType = MidiEndpointDeviceType::MidiEndpointDeviceType_Normal;
    commonProperties.FriendlyName = endpointName.c_str();
    commonProperties.TransportCode = transportCode.c_str();
    commonProperties.EndpointName = endpointName.c_str();
    commonProperties.EndpointDescription = endpointDescription.c_str();
    commonProperties.UniqueIdentifier = uniqueIdentifier.c_str();

    // Either way the service sees UMP. The native format is what tells apps, and the service's
    // own transform selection, that the wire format is really a MIDI 1.0 byte stream.
    commonProperties.SupportedDataFormats = MidiDataFormats::MidiDataFormats_UMP;
    commonProperties.NativeDataFormat = isUmpNative ? MidiDataFormats::MidiDataFormats_UMP : MidiDataFormats::MidiDataFormats_ByteStream;

    UINT32 capabilities{ 0 };
    capabilities |= MidiEndpointCapabilities_SupportsMidi1Protocol;
    capabilities |= MidiEndpointCapabilities_SupportsMultiClient;
    capabilities |= MidiEndpointCapabilities_GenerateIncomingTimestamps;

    if (isUmpNative)
    {
        capabilities |= MidiEndpointCapabilities_SupportsMidi2Protocol;
    }

    commonProperties.Capabilities = (MidiEndpointCapabilities)capabilities;

    SW_DEVICE_CREATE_INFO createInfo{};
    createInfo.cbSize = sizeof(createInfo);
    createInfo.pszInstanceId = instanceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = endpointName.c_str();

    wil::unique_cotaskmem_string newDeviceInterfaceId;

    auto activateHR = m_midiDeviceManager->ActivateEndpoint(
        m_parentDeviceId.c_str(),
        false,                                          // when false, WinMM MIDI 1.0 ports are created as well
        MidiFlow::MidiFlowBidirectional,
        &commonProperties,
        (ULONG)interfaceDevProperties.size(),
        (ULONG)0,
        interfaceDevProperties.size() > 0 ? interfaceDevProperties.data() : nullptr,
        nullptr,
        &createInfo,
        &newDeviceInterfaceId);

    RETURN_IF_FAILED(activateHR);

    // S_FALSE means the instance id is already active and no interface id was returned, which
    // RETURN_IF_FAILED does not catch
    if (activateHR == S_FALSE || newDeviceInterfaceId.get() == nullptr)
    {
        TraceLoggingWrite(
            MidiBle2MidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"This device already has an active endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(instanceId.c_str(), "instance id")
        );

        RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_DEVICE_ALREADY_ATTACHED));
    }

    connection->SetEndpointDeviceInstanceId(internal::NormalizeDeviceInstanceIdWStringCopy(instanceId));
    connection->SetEndpointDeviceInterfaceId(internal::NormalizeEndpointInterfaceIdWStringCopy(newDeviceInterfaceId.get()));

    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"BLE MIDI endpoint activated", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(instanceId.c_str(), "instance id"),
        TraceLoggingWideString(newDeviceInterfaceId.get(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    return S_OK;
}


HRESULT
CMidi2Ble2MidiEndpointManager::CreateParentDevice()
{
    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    RETURN_HR_IF_NULL(E_UNEXPECTED, m_midiDeviceManager);

    std::wstring parentDeviceName{ TRANSPORT_PARENT_DEVICE_NAME };
    std::wstring parentDeviceId{ internal::NormalizeDeviceInstanceIdWStringCopy(TRANSPORT_PARENT_ID) };

    SW_DEVICE_CREATE_INFO createInfo = {};
    createInfo.cbSize = sizeof(createInfo);
    createInfo.pszInstanceId = parentDeviceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = parentDeviceName.c_str();
    createInfo.pContainerId = &m_containerId;

    LPWSTR newDeviceId;

    RETURN_IF_FAILED(m_midiDeviceManager->ActivateVirtualParentDevice(
        0,
        nullptr,
        &createInfo,
        (LPWSTR*)&newDeviceId
    ));

    m_parentDeviceId = internal::NormalizeDeviceInstanceIdWStringCopy(newDeviceId);

    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(newDeviceId, "New parent device instance id")
    );

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2Ble2MidiEndpointManager::DeleteEndpoint(
    std::wstring deviceInstanceId)
{
    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(deviceInstanceId.c_str(), "deviceShortInstanceId")
    );

    RETURN_HR_IF_NULL(E_UNEXPECTED, m_midiDeviceManager);

    auto instanceId = internal::NormalizeDeviceInstanceIdWStringCopy(deviceInstanceId);

    RETURN_HR_IF(E_INVALIDARG, instanceId.empty());
    RETURN_IF_FAILED(m_midiDeviceManager->RemoveEndpoint(instanceId.c_str()));

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2Ble2MidiEndpointManager::InitiateDiscoveryAndNegotiation(
    std::wstring const& endpointDeviceInterfaceId
)
{
    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    RETURN_HR_IF_NULL(E_UNEXPECTED, m_midiProtocolManager);
    RETURN_HR_IF(E_INVALIDARG, endpointDeviceInterfaceId.empty());

    ENDPOINTPROTOCOLNEGOTIATIONPARAMS negotiationParams{ };
    negotiationParams.PreferredMidiProtocol = MIDI_PROP_CONFIGURED_PROTOCOL_MIDI2;

    // The transport does not implement JR timestamps yet. Section 5.5 makes them optional and
    // the service has no JR clock, so they are not requested.
    negotiationParams.PreferToSendJitterReductionTimestampsToEndpoint = false;
    negotiationParams.PreferToReceiveJitterReductionTimestampsFromEndpoint = false;

    RETURN_IF_FAILED(m_midiProtocolManager->DiscoverAndNegotiate(
        m_transportId,
        endpointDeviceInterfaceId.c_str(),
        negotiationParams
    ));

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2Ble2MidiEndpointManager::QueueDiscoveryAndNegotiation(
    std::wstring const& endpointDeviceInterfaceId
)
{
    RETURN_HR_IF(E_INVALIDARG, endpointDeviceInterfaceId.empty());

    {
        auto lock = std::scoped_lock{ m_pendingRequestsLock };
        m_pendingNegotiations.push_back(endpointDeviceInterfaceId);
    }

    RETURN_IF_FAILED(WakeupBackgroundEndpointCreatorThread());

    return S_OK;
}


HRESULT
CMidi2Ble2MidiEndpointManager::Shutdown()
{
    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    m_initialized = false;

    m_backgroundEndpointCreatorThread.request_stop();
    m_backgroundEndpointCreatorThreadWakeup.SetEvent();

    if (m_backgroundEndpointCreatorThread.joinable() && m_backgroundEndpointCreatorThread.get_id() != std::this_thread::get_id())
    {
        m_backgroundEndpointCreatorThread.join();
    }

    try
    {
        if (m_advertisementWatcher != nullptr)
        {
            if (m_advertisementReceivedToken)
            {
                m_advertisementWatcher.Received(m_advertisementReceivedToken);
                m_advertisementReceivedToken = {};
            }

            m_advertisementWatcher.Stop();
            m_advertisementWatcher = nullptr;
        }
    }
    CATCH_LOG();

    try
    {
        if (m_deviceWatcher != nullptr)
        {
            if (m_deviceWatcherStoppedToken)
            {
                m_deviceWatcher.Stopped(m_deviceWatcherStoppedToken);
            }

            if (m_deviceWatcherAddedToken)
            {
                m_deviceWatcher.Added(m_deviceWatcherAddedToken);
            }

            if (m_deviceWatcherRemovedToken)
            {
                m_deviceWatcher.Removed(m_deviceWatcherRemovedToken);
            }

            if (m_deviceWatcherUpdatedToken)
            {
                m_deviceWatcher.Updated(m_deviceWatcherUpdatedToken);
            }

            m_deviceWatcher.Stop();
            m_deviceWatcher = nullptr;
        }
    }
    CATCH_LOG();

    TransportState::Current().ShutdownAllConnections();

    {
        auto lock = std::scoped_lock{ m_discoveredDevicesLock };
        m_discoveredDevices.clear();
    }

    {
        auto lock = std::scoped_lock{ m_pendingRequestsLock };
        m_pendingConnectRequests.clear();
        m_pendingDisconnectRequests.clear();
        m_pendingNegotiations.clear();
    }

    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}
