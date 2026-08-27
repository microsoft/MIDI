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
        auto umpCharacteristics = MidiBleUtilities::AwaitWithTimeout(
            service.GetCharacteristicsForUuidAsync(umpCharacteristicUuid, bt::BluetoothCacheMode::Uncached),
            MidiBleUtilities::BleOperationTimeoutMilliseconds,
            gatt::GattCharacteristicsResult{ nullptr });

        if (umpCharacteristics != nullptr &&
            umpCharacteristics.Status() == gatt::GattCommunicationStatus::Success &&
            umpCharacteristics.Characteristics().Size() > 0)
        {
            selection.Protocol = MidiBleProtocol::Protocol::Midi2Ump;
            selection.Characteristic = umpCharacteristics.Characteristics().GetAt(0);

            return selection;
        }

        winrt::guid midi1CharacteristicUuid{ MidiBleProtocol::Midi1DataIoCharacteristicUuid };
        auto midi1Characteristics = MidiBleUtilities::AwaitWithTimeout(
            service.GetCharacteristicsForUuidAsync(midi1CharacteristicUuid, bt::BluetoothCacheMode::Uncached),
            MidiBleUtilities::BleOperationTimeoutMilliseconds,
            gatt::GattCharacteristicsResult{ nullptr });

        if (midi1Characteristics != nullptr &&
            midi1Characteristics.Status() == gatt::GattCommunicationStatus::Success &&
            midi1Characteristics.Characteristics().Size() > 0)
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

    // Kept distinct from the id built for a device this PC connected out to, so the same phone
    // acting in both directions cannot collide on one device node.
    std::wstring BuildPeripheralEndpointDeviceInstanceId(
        _In_ std::wstring const& remoteName,
        _In_ winrt::hstring const& remoteAddress)
    {
        auto readableName = internal::RemoveInvalidSWDUniqueIdCharacters(remoteName);

        if (readableName.length() > MIDI_BLE_ENDPOINT_INSTANCE_ID_NAME_MAX_CHARS)
        {
            readableName = readableName.substr(0, MIDI_BLE_ENDPOINT_INSTANCE_ID_NAME_MAX_CHARS);
        }

        if (!readableName.empty() && !remoteAddress.empty())
        {
            readableName += L"_";
        }

        return internal::NormalizeDeviceInstanceIdWStringCopy(
            std::wstring{ MIDI_BLE_PERIPHERAL_ENDPOINT_INSTANCE_ID_PREFIX } +
            readableName +
            std::wstring{ remoteAddress });
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

    // Probed before anything is started, so every later failure can say whether the radio was ever
    // capable of it. A machine with no Bluetooth loads the transport and simply finds nothing.
    auto const radioCapabilities = MidiBleUtilities::ProbeRadioCapabilities();

    TransportState::Current().SetRadioCapabilities(radioCapabilities);

    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Bluetooth radio capabilities", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingBool(radioCapabilities.RadioPresent, "radio present"),
        TraceLoggingBool(radioCapabilities.LowEnergySupported, "low energy supported"),
        TraceLoggingBool(radioCapabilities.CentralRoleSupported, "central role supported"),
        TraceLoggingBool(radioCapabilities.PeripheralRoleSupported, "peripheral role supported"),
        TraceLoggingUInt32(radioCapabilities.MaxAdvertisementDataLength, "max advertisement data length")
    );

    RETURN_IF_FAILED(StartBackgroundEndpointCreator());

    // Discovery is pointless without a radio which can act as a central, and starting the watchers
    // anyway would log a failure on every machine which simply has no Bluetooth.
    if (radioCapabilities.CanConnectToDevices())
    {
        // Advertisements are the primary source: they surface unpaired devices too, which is the
        // main gap in the older Windows BLE MIDI 1.0 support. The GATT watcher then fills in the
        // devices Windows already knows about but which are not advertising right now.
        LOG_IF_FAILED(StartAdvertisementWatcher());
        LOG_IF_FAILED(StartGattServiceWatcher());

        // The configuration file may have been pushed before this point, in which case the device
        // ids are already parked and waiting.
        LOG_IF_FAILED(ConnectConfiguredDevices());
    }
    else
    {
        TraceLoggingWrite(
            MidiBle2MidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"No Bluetooth Low Energy central support, so device discovery was not started", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );
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
        m_advertisementStoppedToken = m_advertisementWatcher.Stopped({ this, &CMidi2Ble2MidiEndpointManager::OnAdvertisementWatcherStopped });

        m_advertisementWatcher.Start();

        // Status is the discriminator between "scanning, nothing in range yet" and "never
        // started". Aborted here usually means the Bluetooth radio is off or unavailable to the
        // service account, which otherwise looks identical to a silent room.
        TraceLoggingWrite(
            MidiBle2MidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"BLE advertisement watcher started", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingUInt32(static_cast<uint32_t>(m_advertisementWatcher.Status()), "watcher status")
        );
    }
    CATCH_RETURN();

    return S_OK;
}


_Use_decl_annotations_
void
CMidi2Ble2MidiEndpointManager::OnAdvertisementWatcherStopped(
    bt::Advertisement::BluetoothLEAdvertisementWatcher const&,
    bt::Advertisement::BluetoothLEAdvertisementWatcherStoppedEventArgs const& args
)
{
    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_WARNING,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"BLE advertisement watcher stopped. Discovery of unpaired devices has ended.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt32(static_cast<uint32_t>(args.Error()), "bluetooth error")
    );
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

        TraceLoggingWrite(
            MidiBle2MidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"BLE MIDI GATT service watcher started", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingUInt32(static_cast<uint32_t>(m_deviceWatcher.Status()), "watcher status")
        );
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

        // A device only advertises when it is awake and unconnected, which makes this the exact
        // moment a remembered device becomes connectable.
        QueueConnectIfWanted(device.Id);
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

        // Windows enumerating the GATT service is the other moment a remembered device becomes
        // reachable, and a bonded device which is not advertising only appears this way.
        QueueConnectIfWanted(device.Id);
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

    bool isNewDevice{ false };
    bool needsName{ false };

    {
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

            needsName = existing->second.Name.empty();
        }
        else
        {
            m_discoveredDevices.insert_or_assign(device.Id, device);

            isNewDevice = true;
            needsName = device.Name.empty();
        }
    }

    if (isNewDevice)
    {
        // Only the first sighting is traced. Advertisements repeat several times a second, and
        // this is the event which answers "is discovery working at all".
        TraceLoggingWrite(
            MidiBle2MidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"BLE MIDI device discovered", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(device.Id.c_str(), "device id"),
            TraceLoggingWideString(device.Name.c_str(), "name"),
            TraceLoggingBool(device.IsPaired, "paired"),
            TraceLoggingInt16(device.LastSignalStrengthDbm, "signal strength dbm")
        );
    }

    if (needsName)
    {
        QueueNameResolutionIfNeeded(device.Id);
    }
}


_Use_decl_annotations_
void
CMidi2Ble2MidiEndpointManager::QueueNameResolutionIfNeeded(winrt::hstring const& deviceId)
{
    {
        auto lock = std::scoped_lock{ m_pendingRequestsLock };

        auto& attempts = m_nameResolutionAttempts[deviceId];

        // The stack may not have learned the name from the scan response yet when the first
        // advertisement arrives, so a few spaced retries are allowed. It is only cosmetic, so it
        // gives up rather than asking forever.
        if (attempts.Count >= MIDI_BLE_NAME_RESOLUTION_MAX_ATTEMPTS)
        {
            return;
        }

        auto const now = NowInMilliseconds();

        if (attempts.Count > 0 && attempts.LastAttemptTimestamp + MIDI_BLE_NAME_RESOLUTION_RETRY_INTERVAL_MS > now)
        {
            return;
        }

        attempts.Count++;
        attempts.LastAttemptTimestamp = now;

        m_pendingNameResolutions.push_back(deviceId);
    }

    LOG_IF_FAILED(WakeupBackgroundEndpointCreatorThread());
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


std::set<winrt::hstring>
CMidi2Ble2MidiEndpointManager::GetDeviceIdsWithUnresolvableNames()
{
    auto lock = std::scoped_lock{ m_pendingRequestsLock };

    std::set<winrt::hstring> ids;

    for (auto const& entry : m_nameResolutionAttempts)
    {
        if (entry.second.GaveUp)
        {
            ids.insert(entry.first);
        }
    }

    return ids;
}


std::vector<MidiBleProtocol::DiscoveredDevice>
CMidi2Ble2MidiEndpointManager::GetDiscoveredDevices()
{
    auto const now = NowInMilliseconds();

    // Taken first and released, because the two locks must never be held at the same time.
    auto const unresolvableNames = GetDeviceIdsWithUnresolvableNames();

    std::vector<MidiBleProtocol::DiscoveredDevice> devices;

    {
        auto lock = std::scoped_lock{ m_discoveredDevicesLock };

        devices.reserve(m_discoveredDevices.size());

        for (auto const& entry : m_discoveredDevices)
        {
            // a connected or paired device stays listed even when it is not advertising
            bool const isStale =
                !entry.second.IsConnected &&
                !entry.second.IsPaired &&
                entry.second.LastSeenTimestamp + MidiBleProtocol::DeviceStaleAfterMilliseconds < now;

            if (isStale)
            {
                continue;
            }

            // A device is withheld until it can be named. An address is meaningless to the user,
            // so showing one is a last resort for a device whose name cannot be resolved at all.
            if (entry.second.Name.empty() && unresolvableNames.find(entry.second.Id) == unresolvableNames.end())
            {
                continue;
            }

            devices.push_back(entry.second);
        }
    }

    // Merged outside the discovery lock, because reading a connection must never be done while
    // holding it.
    for (auto& device : devices)
    {
        auto connection = TransportState::Current().GetConnectionByDeviceId(device.Id);

        device.HasEndpoint = connection != nullptr;

        if (connection != nullptr)
        {
            device.MessagesReceived = connection->MessagesReceived();
            device.MessagesSent = connection->MessagesSent();
            device.LastSendErrorHresult = connection->LastSendErrorHresult();

            // The stored flag only records that a connection object exists. Asking the device
            // is what distinguishes a live link from one that went away without telling us,
            // which is the failure the older Windows Bluetooth MIDI support hid from apps.
            device.IsConnected = connection->IsDeviceConnected();
            device.ConnectionIntervalUnits = connection->ConnectionIntervalUnits();
        }
        else
        {
            device.IsConnected = false;
            device.ConnectionIntervalUnits = 0;
        }

        device.LastSeenAgoMilliseconds = now > device.LastSeenTimestamp ? now - device.LastSeenTimestamp : 0;

        // Deterministic, so a client can write a customization for a device it has never
        // connected, and the creation path will find it.
        device.EndpointDeviceInstanceId = winrt::hstring{ BuildEndpointDeviceInstanceId(device) };

        // A connected device stops advertising, so its presence comes from the link instead.
        device.IsPresent =
            device.IsConnected ||
            device.LastSeenAgoMilliseconds <= MIDI_BLE_DEVICE_PRESENT_WITHIN_MS;

        if (!device.IsPresent)
        {
            // a signal strength from minutes ago reads as current and is worse than none
            device.LastSignalStrengthDbm = 0;
        }
    }

    return devices;
}


_Use_decl_annotations_
bool
CMidi2Ble2MidiEndpointManager::IsDeviceNameable(winrt::hstring const& deviceId)
{
    {
        auto lock = std::scoped_lock{ m_discoveredDevicesLock };

        auto entry = m_discoveredDevices.find(deviceId);

        if (entry != m_discoveredDevices.end() && !entry->second.Name.empty())
        {
            return true;
        }
    }

    auto lock = std::scoped_lock{ m_pendingRequestsLock };

    auto attempts = m_nameResolutionAttempts.find(deviceId);

    return attempts != m_nameResolutionAttempts.end() && attempts->second.GaveUp;
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

        // remembered, so a device which is asleep now is picked up when it next advertises
        m_desiredConnections.insert(deviceId);
    }

    // Deferred until the device can be named. Name resolution queues the connection itself when
    // it finishes, so nothing is lost by waiting.
    QueueConnectIfWanted(deviceId);

    return S_OK;
}


HRESULT
CMidi2Ble2MidiEndpointManager::ConnectConfiguredDevices()
{
    RETURN_HR_IF(E_UNEXPECTED, !m_initialized);

    for (auto const& deviceId : TransportState::Current().TakeConfiguredDeviceIds())
    {
        TraceLoggingWrite(
            MidiBle2MidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Connecting a device from the configuration file", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(deviceId.c_str(), "device id")
        );

        LOG_IF_FAILED(ConnectDevice(deviceId));
    }

    if (auto const protocol = TransportState::Current().TakeConfiguredPeripheralProtocol();
        protocol != MidiBleProtocol::Protocol::Unknown)
    {
        TraceLoggingWrite(
            MidiBle2MidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Publishing the peripheral from the configuration file", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingUInt8(static_cast<uint8_t>(protocol), "protocol")
        );

        LOG_IF_FAILED(StartPeripheral(protocol));
    }

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2Ble2MidiEndpointManager::StartPeripheral(MidiBleProtocol::Protocol const protocol)
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

    RETURN_HR_IF(E_UNEXPECTED, !m_initialized);
    RETURN_HR_IF(E_INVALIDARG, protocol == MidiBleProtocol::Protocol::Unknown);

    // Plenty of radios cannot advertise at all. Refusing here means the caller is told why,
    // instead of the GATT service provider failing later with nothing to explain it.
    auto const radioCapabilities = TransportState::Current().GetRadioCapabilities();

    if (!radioCapabilities.CanPublishPeripheral())
    {
        TraceLoggingWrite(
            MidiBle2MidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"This radio cannot act as a Bluetooth LE peripheral", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingBool(radioCapabilities.RadioPresent, "radio present"),
            TraceLoggingBool(radioCapabilities.LowEnergySupported, "low energy supported"),
            TraceLoggingBool(radioCapabilities.PeripheralRoleSupported, "peripheral role supported")
        );

        RETURN_HR(HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));
    }

    RETURN_IF_FAILED(TransportState::Current().StartPeripheral(protocol));

    auto cleanupOnFailure = wil::scope_exit([&]() { LOG_IF_FAILED(StopPeripheral()); });

    auto peripheral = TransportState::Current().GetPeripheral();
    RETURN_HR_IF_NULL(E_UNEXPECTED, peripheral);

    // No endpoint yet. Like a Network MIDI 2.0 host, the endpoint is the remote device which
    // connected, so it is created when a Central subscribes.
    peripheral->SetClientChangedCallback([this]() { OnPeripheralClientChanged(); });

    cleanupOnFailure.release();

    return S_OK;
}


void
CMidi2Ble2MidiEndpointManager::OnPeripheralClientChanged()
{
    {
        auto lock = std::scoped_lock{ m_pendingRequestsLock };

        m_peripheralClientChangePending = true;
    }

    LOG_IF_FAILED(WakeupBackgroundEndpointCreatorThread());
}


HRESULT
CMidi2Ble2MidiEndpointManager::StopPeripheral()
{
    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    LOG_IF_FAILED(RemovePeripheralEndpoint());

    LOG_IF_FAILED(TransportState::Current().StopPeripheral());

    return S_OK;
}


HRESULT
CMidi2Ble2MidiEndpointManager::RemovePeripheralEndpoint()
{
    winrt::hstring const peripheralDeviceId{ MIDI_BLE_PERIPHERAL_DEVICE_ID };

    if (auto connection = TransportState::Current().GetConnectionByDeviceId(peripheralDeviceId))
    {
        // the callback is released before the endpoint goes away, because removal re-enters
        // synchronously through the bidi's Shutdown
        LOG_IF_FAILED(connection->DisconnectMidiCallback());
    }

    {
        auto lock = std::scoped_lock{ m_createdEndpointsLock };

        std::erase_if(
            m_createdEndpoints,
            [&peripheralDeviceId](CreatedEndpointRecord const& record) { return record.DeviceId == peripheralDeviceId; });
    }

    // Removing it from the table shuts it down; detaching only drops the peripheral's reference.
    LOG_IF_FAILED(TransportState::Current().RemoveConnection(peripheralDeviceId));

    if (auto peripheral = TransportState::Current().GetPeripheral())
    {
        LOG_IF_FAILED(peripheral->DetachConnection());
    }

    if (!m_peripheralEndpointInstanceId.empty())
    {
        LOG_IF_FAILED(DeleteEndpoint(m_peripheralEndpointInstanceId));
        m_peripheralEndpointInstanceId.clear();
    }

    m_peripheralClientDeviceId = L"";

    return S_OK;
}


HRESULT
CMidi2Ble2MidiEndpointManager::ProcessPeripheralClientChange()
{
    auto peripheral = TransportState::Current().GetPeripheral();

    auto const clientDeviceId = peripheral != nullptr ? peripheral->ActiveClientDeviceId() : winrt::hstring{};

    if (clientDeviceId == m_peripheralClientDeviceId)
    {
        return S_OK;
    }

    // A different Central, or none, means the endpoint no longer represents what is connected.
    LOG_IF_FAILED(RemovePeripheralEndpoint());

    if (peripheral == nullptr || clientDeviceId.empty())
    {
        return S_OK;
    }

    // The remote device's own name is what the endpoint is called, so the user sees the phone or
    // tablet which connected rather than this PC.
    winrt::hstring remoteName{};
    winrt::hstring remoteAddress{};

    // Every identity WinRT offers is recorded, because which of them survives a resolvable private
    // address rotation is what decides the endpoint's identity, and that cannot be reasoned out
    // from the API surface alone.
    winrt::hstring remoteBluetoothDeviceId{};
    winrt::hstring remoteAddressType{ L"unspecified" };
    bool remoteIsPaired{ false };
    bt::BluetoothLEDevice remoteDevice{ nullptr };

    try
    {
        if (auto device = MidiBleUtilities::AwaitWithTimeout(
            bt::BluetoothLEDevice::FromIdAsync(clientDeviceId),
            MidiBleUtilities::BleOperationTimeoutMilliseconds,
            bt::BluetoothLEDevice{ nullptr }))
        {
            remoteDevice = device;
            remoteName = device.Name();
            remoteAddress = MidiBleUtilities::FormatBluetoothAddress(device.BluetoothAddress());
            remoteAddressType = MidiBleUtilities::BluetoothAddressTypeToString(device.BluetoothAddressType());

            if (auto bluetoothDeviceId = device.BluetoothDeviceId())
            {
                remoteBluetoothDeviceId = bluetoothDeviceId.Id();
            }

            if (auto deviceInformation = device.DeviceInformation())
            {
                if (auto pairing = deviceInformation.Pairing())
                {
                    remoteIsPaired = pairing.IsPaired();
                }
            }
        }
    }
    CATCH_LOG();

    auto const hasGenericName = MidiBleUtilities::IsGenericDeviceName(remoteName);

    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"A Central connected to the BLE MIDI peripheral", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(clientDeviceId.c_str(), "client device id"),
        TraceLoggingWideString(remoteName.c_str(), "remote name"),
        TraceLoggingWideString(remoteAddress.c_str(), "remote address"),
        TraceLoggingWideString(remoteAddressType.c_str(), "remote address type"),
        TraceLoggingWideString(remoteBluetoothDeviceId.c_str(), "remote bluetooth device id"),
        TraceLoggingBool(remoteIsPaired, "remote is paired"),
        TraceLoggingBool(hasGenericName, "remote name is generic")
    );

    std::wstring endpointName{ remoteName };

    if (endpointName.empty())
    {
        endpointName = remoteAddress.empty() ?
            std::wstring{ MIDI_BLE_PERIPHERAL_UNKNOWN_CLIENT_NAME } :
            std::wstring{ MIDI_BLE_PERIPHERAL_UNKNOWN_CLIENT_NAME } + L" " + std::wstring{ remoteAddress };
    }

    std::shared_ptr<MidiBleConnection> connection{ nullptr };
    RETURN_IF_FAILED(peripheral->AttachConnection(clientDeviceId, winrt::hstring{ endpointName }, connection));
    RETURN_HR_IF_NULL(E_UNEXPECTED, connection);

    peripheral->SetRemoteClientInfo(MidiBleRemoteClientInfo{
        remoteName,
        remoteAddress,
        remoteAddressType,
        remoteBluetoothDeviceId,
        remoteIsPaired,
        hasGenericName });

    peripheral->SetRemoteDevice(remoteDevice);

    RETURN_IF_FAILED(TransportState::Current().AddConnection(connection));

    // A bonded device reports the identity address recorded at bonding, which survives both the
    // address rotation and a reconnect, so it is what the endpoint is keyed on. Without a bond
    // there is no stable identity to key on at all.
    auto const instanceId = remoteIsPaired ?
        BuildPeripheralEndpointDeviceInstanceId(endpointName, remoteAddress) :
        internal::NormalizeDeviceInstanceIdWStringCopy(MIDI_BLE_PERIPHERAL_UNPAIRED_ENDPOINT_INSTANCE_ID);

    RETURN_IF_FAILED(CreateEndpoint(
        connection,
        endpointName,
        peripheral->Protocol() == MidiBleProtocol::Protocol::Midi2Ump ?
            MIDI_BLE_PERIPHERAL_MIDI2_ENDPOINT_DESCRIPTION :
            MIDI_BLE_PERIPHERAL_MIDI1_ENDPOINT_DESCRIPTION,
        instanceId,
        remoteAddress.empty() ? std::wstring{ MIDI_BLE_PERIPHERAL_DEVICE_ID } : std::wstring{ remoteAddress }));

    m_peripheralEndpointInstanceId = connection->EndpointDeviceInstanceId();
    m_peripheralClientDeviceId = clientDeviceId;

    return S_OK;
}


_Use_decl_annotations_
void
CMidi2Ble2MidiEndpointManager::QueueConnectIfWanted(winrt::hstring const& deviceId)
{
    if (TransportState::Current().GetConnectionByDeviceId(deviceId) != nullptr)
    {
        return;
    }

    // An endpoint is named after its device, so connecting before the name is known would
    // publish one named after the Bluetooth address.
    if (!IsDeviceNameable(deviceId))
    {
        return;
    }

    {
        auto lock = std::scoped_lock{ m_pendingRequestsLock };

        if (m_desiredConnections.find(deviceId) == m_desiredConnections.end())
        {
            return;
        }

        // Advertisements arrive several times a second. Without this the queue would fill with
        // duplicate attempts faster than the worker can drain them.
        auto const now = NowInMilliseconds();

        if (auto attempt = m_lastConnectAttemptTimestamp.find(deviceId); attempt != m_lastConnectAttemptTimestamp.end())
        {
            if (attempt->second + MIDI_BLE_CONNECT_RETRY_INTERVAL_MS > now)
            {
                return;
            }
        }

        if (std::find(m_pendingConnectRequests.begin(), m_pendingConnectRequests.end(), deviceId) != m_pendingConnectRequests.end())
        {
            return;
        }

        m_lastConnectAttemptTimestamp[deviceId] = now;
        m_pendingConnectRequests.push_back(deviceId);
    }

    LOG_IF_FAILED(WakeupBackgroundEndpointCreatorThread());
}


_Use_decl_annotations_
HRESULT
CMidi2Ble2MidiEndpointManager::DisconnectDevice(winrt::hstring const& deviceId)
{
    RETURN_HR_IF(E_INVALIDARG, deviceId.empty());

    {
        auto lock = std::scoped_lock{ m_pendingRequestsLock };

        // the user no longer wants this device, so stop retrying it
        m_desiredConnections.erase(deviceId);
        m_lastConnectAttemptTimestamp.erase(deviceId);

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
        std::deque<winrt::hstring> nameResolutions;
        bool peripheralClientChanged{ false };

        bool haveWork{ false };

        {
            auto lock = std::scoped_lock{ m_pendingRequestsLock };

            connectRequests.swap(m_pendingConnectRequests);
            disconnectRequests.swap(m_pendingDisconnectRequests);
            negotiations.swap(m_pendingNegotiations);
            nameResolutions.swap(m_pendingNameResolutions);

            peripheralClientChanged = m_peripheralClientChangePending;
            m_peripheralClientChangePending = false;

            haveWork =
                !connectRequests.empty() ||
                !disconnectRequests.empty() ||
                !negotiations.empty() ||
                !nameResolutions.empty() ||
                peripheralClientChanged;

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

        if (peripheralClientChanged && !stopToken.stop_requested())
        {
            LOG_IF_FAILED(ProcessPeripheralClientChange());
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

        // last, because it is cosmetic and must never delay a connection
        for (auto const& deviceId : nameResolutions)
        {
            if (stopToken.stop_requested())
            {
                break;
            }

            LOG_IF_FAILED(ResolveDeviceNameInternal(deviceId));
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
    winrt::hstring failureDetail{};
    uint32_t errorCode{ BLUETOOTH_MIDI_ERROR_CODE_UNKNOWN_ERROR };

    auto hr = ConnectDeviceCore(deviceId, failureDetail, errorCode);

    RecordConnectResult(deviceId, hr, failureDetail, errorCode);

    return hr;
}


_Use_decl_annotations_
HRESULT
CMidi2Ble2MidiEndpointManager::ConnectDeviceCore(
    winrt::hstring const& deviceId,
    winrt::hstring& failureDetail,
    uint32_t& errorCode
)
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

    failureDetail = L"";
    errorCode = BLUETOOTH_MIDI_ERROR_CODE_UNKNOWN_ERROR;

    if (TransportState::Current().GetConnectionByDeviceId(deviceId) != nullptr)
    {
        return S_OK;
    }

    MidiBleProtocol::DiscoveredDevice discoveredDevice{};

    if (!TryGetDiscoveredDevice(deviceId, discoveredDevice) || discoveredDevice.BluetoothAddress == 0)
    {
        failureDetail = L"This device has not been discovered. Wake it so it advertises, then try again.";
        errorCode = BLUETOOTH_MIDI_ERROR_CODE_DEVICE_NOT_DISCOVERED;
        RETURN_IF_FAILED(E_NOTFOUND);
    }

    bt::BluetoothLEDevice bleDevice{ nullptr };
    gatt::GattDeviceService service{ nullptr };
    gatt::GattSession session{ nullptr };
    BleMidiCharacteristicSelection selection{};

    try
    {
        bleDevice = MidiBleUtilities::AwaitWithTimeout(
            bt::BluetoothLEDevice::FromBluetoothAddressAsync(discoveredDevice.BluetoothAddress),
            MidiBleUtilities::BleOperationTimeoutMilliseconds,
            bt::BluetoothLEDevice{ nullptr });

        if (bleDevice == nullptr)
        {
            failureDetail = L"Windows could not open this Bluetooth device.";
            errorCode = BLUETOOTH_MIDI_ERROR_CODE_DEVICE_NOT_AVAILABLE;
            RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_AVAILABLE));
        }

        if (discoveredDevice.Name.empty())
        {
            discoveredDevice.Name = bleDevice.Name();
        }

        auto serviceStatus = gatt::GattCommunicationStatus::Unreachable;
        service = MidiBleUtilities::GetBleMidiServiceFromDevice(bleDevice, serviceStatus);

        if (IsStopping())
        {
            failureDetail = L"The transport is shutting down.";
            errorCode = BLUETOOTH_MIDI_ERROR_CODE_OPERATION_ABORTED;
            RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_OPERATION_ABORTED));
        }

        if (service == nullptr)
        {
            switch (serviceStatus)
            {
            case gatt::GattCommunicationStatus::Unreachable:
                // by far the most common outcome: BLE peripherals sleep aggressively
                failureDetail = L"The device did not respond. Wake it up, keep it in range, and make sure it is not already connected to another host.";
                errorCode = BLUETOOTH_MIDI_ERROR_CODE_DEVICE_UNREACHABLE;
                break;

            case gatt::GattCommunicationStatus::AccessDenied:
                failureDetail = L"Windows denied access to this device's GATT services.";
                errorCode = BLUETOOTH_MIDI_ERROR_CODE_GATT_ACCESS_DENIED;
                break;

            case gatt::GattCommunicationStatus::ProtocolError:
                failureDetail = L"The device reported a GATT protocol error.";
                errorCode = BLUETOOTH_MIDI_ERROR_CODE_GATT_PROTOCOL_ERROR;
                break;

            default:
                failureDetail = L"The device does not expose the Bluetooth MIDI service.";
                errorCode = BLUETOOTH_MIDI_ERROR_CODE_MIDI_SERVICE_NOT_FOUND;
                break;
            }

            RETURN_IF_FAILED(E_NOTFOUND);
        }

        auto openStatus = MidiBleUtilities::AwaitWithTimeout(
            service.OpenAsync(gatt::GattSharingMode::SharedReadAndWrite),
            MidiBleUtilities::BleOperationTimeoutMilliseconds,
            gatt::GattOpenStatus::Unspecified);

        if (openStatus == gatt::GattOpenStatus::AccessDenied)
        {
            failureDetail = L"Access to the device's MIDI service was denied.";
            errorCode = BLUETOOTH_MIDI_ERROR_CODE_GATT_ACCESS_DENIED;
            RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED));
        }

        if (openStatus == gatt::GattOpenStatus::SharingViolation)
        {
            // the in-box Bluetooth MIDI 1.0 stack claims paired devices and holds them exclusively
            failureDetail = L"Another component already has this device's MIDI service open. The older Windows Bluetooth MIDI support may be holding it. Remove the device's Bluetooth MIDI entry in Device Manager, then try again.";
            errorCode = BLUETOOTH_MIDI_ERROR_CODE_DEVICE_IN_USE;
            RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION));
        }

        if (openStatus != gatt::GattOpenStatus::Success && openStatus != gatt::GattOpenStatus::AlreadyOpened)
        {
            failureDetail = L"The device's MIDI service could not be opened.";
            errorCode = BLUETOOTH_MIDI_ERROR_CODE_SESSION_CREATION_FAILED;
            RETURN_IF_FAILED(E_FAIL);
        }

        selection = SelectPreferredMidiCharacteristic(service);

        if (IsStopping())
        {
            failureDetail = L"The transport is shutting down.";
            errorCode = BLUETOOTH_MIDI_ERROR_CODE_OPERATION_ABORTED;
            RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_OPERATION_ABORTED));
        }

        if (selection.Protocol == MidiBleProtocol::Protocol::Unknown || selection.Characteristic == nullptr)
        {
            failureDetail = L"The device's MIDI service has neither a MIDI 1.0 nor a UMP characteristic.";
            errorCode = BLUETOOTH_MIDI_ERROR_CODE_MIDI_CHARACTERISTIC_NOT_FOUND;
            RETURN_IF_FAILED(E_NOTFOUND);
        }

        session = MidiBleUtilities::AwaitWithTimeout(
            gatt::GattSession::FromDeviceIdAsync(bleDevice.BluetoothDeviceId()),
            MidiBleUtilities::BleOperationTimeoutMilliseconds,
            gatt::GattSession{ nullptr });

        if (session == nullptr)
        {
            failureDetail = L"A Bluetooth session could not be created for this device.";
            errorCode = BLUETOOTH_MIDI_ERROR_CODE_SESSION_CREATION_FAILED;
            RETURN_IF_FAILED(E_FAIL);
        }

        // BLE devices drop out constantly. This asks the system to re-establish the link rather
        // than leaving an app holding an endpoint that has silently stopped working.
        session.MaintainConnection(true);

        // Both specifications make a connection interval of 15 ms or less mandatory and prefer the
        // lowest both ends support, with 7.5 ms recommended for live performance. WinRT offers
        // only presets and no way to name an interval, and the throughput-optimized one asks for
        // 15 ms as both floor and ceiling, so which preset is actually best is a measurement.
        try
        {
            auto const preference = TransportState::Current().GetConnectionParameterPreference();
            auto const preferredParameters = MidiBleUtilities::GetPreferredConnectionParameters(preference);

            if (preferredParameters != nullptr)
            {
                auto connectionParametersRequest = bleDevice.RequestPreferredConnectionParameters(preferredParameters);

                // Connection intervals are in units of 1.25 ms, so these are logged raw and
                // converted, because the preset's name says nothing about what it asks for.
                TraceLoggingWrite(
                    MidiBle2MidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Requested preferred connection parameters", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(deviceId.c_str(), "device id"),
                    TraceLoggingWideString(
                        MidiBleUtilities::ConnectionParameterPreferenceToJsonString(preference).c_str(), "preference"),
                    TraceLoggingUInt16(preferredParameters.MinConnectionInterval(), "min interval units"),
                    TraceLoggingUInt16(preferredParameters.MaxConnectionInterval(), "max interval units"),
                    TraceLoggingFloat32(preferredParameters.MinConnectionInterval() * 1.25f, "min interval ms"),
                    TraceLoggingFloat32(preferredParameters.MaxConnectionInterval() * 1.25f, "max interval ms"),
                    TraceLoggingUInt32(
                        connectionParametersRequest != nullptr ? static_cast<uint32_t>(connectionParametersRequest.Status()) : 0xFFFFFFFF,
                        "request status")
                );

                // The request object must outlive the connection, because releasing it withdraws
                // the preference and the link reverts to the system default.
                if (connectionParametersRequest != nullptr)
                {
                    auto lock = std::scoped_lock{ m_connectionParameterRequestsLock };

                    m_connectionParameterRequests.insert_or_assign(deviceId, connectionParametersRequest);
                }
            }
            else
            {
                TraceLoggingWrite(
                    MidiBle2MidiTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Leaving connection parameters to the system", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(deviceId.c_str(), "device id")
                );
            }
        }
        CATCH_LOG();
    }
    catch (...)
    {
        failureDetail = L"An unexpected Bluetooth error occurred.";
        errorCode = BLUETOOTH_MIDI_ERROR_CODE_UNKNOWN_ERROR;
        RETURN_CAUGHT_EXCEPTION();
    }

    auto connection = std::make_shared<MidiBleConnection>();
    RETURN_IF_NULL_ALLOC(connection);

    errorCode = BLUETOOTH_MIDI_ERROR_CODE_NOTIFY_FAILED;
    failureDetail = L"The device's MIDI characteristic could not be subscribed to.";

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

    errorCode = BLUETOOTH_MIDI_ERROR_CODE_ENDPOINT_CREATION_FAILED;
    failureDetail = L"The MIDI endpoint for this device could not be created.";

    hr = CreateEndpointForConnection(connection);

    if (FAILED(hr))
    {
        LOG_IF_FAILED(TransportState::Current().RemoveConnection(deviceId));
        RETURN_IF_FAILED(hr);
    }

    failureDetail = L"";
    errorCode = BLUETOOTH_MIDI_ERROR_CODE_UNKNOWN_ERROR;

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
void
CMidi2Ble2MidiEndpointManager::RecordConnectResult(
    winrt::hstring const& deviceId,
    HRESULT const hr,
    winrt::hstring const& detail,
    uint32_t const errorCode
)
{
    {
        auto lock = std::scoped_lock{ m_discoveredDevicesLock };

        if (auto entry = m_discoveredDevices.find(deviceId); entry != m_discoveredDevices.end())
        {
            entry->second.LastConnectErrorHresult = SUCCEEDED(hr) ? 0 : static_cast<int32_t>(hr);
            entry->second.LastConnectErrorDetail = SUCCEEDED(hr) ? winrt::hstring{} : detail;
            entry->second.LastConnectErrorCode = SUCCEEDED(hr) ? 0 : errorCode;
        }
    }

    if (FAILED(hr))
    {
        TraceLoggingWrite(
            MidiBle2MidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"BLE MIDI device connection failed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(deviceId.c_str(), "device id"),
            TraceLoggingWideString(detail.c_str(), "detail"),
            TraceLoggingHResult(hr, MIDI_TRACE_EVENT_HRESULT_FIELD)
        );
    }
}


_Use_decl_annotations_
HRESULT
CMidi2Ble2MidiEndpointManager::ResolveDeviceNameInternal(winrt::hstring const& deviceId)
{
    MidiBleProtocol::DiscoveredDevice discoveredDevice{};

    RETURN_HR_IF(S_FALSE, !TryGetDiscoveredDevice(deviceId, discoveredDevice));
    RETURN_HR_IF(S_FALSE, !discoveredDevice.Name.empty());
    RETURN_HR_IF(S_FALSE, discoveredDevice.BluetoothAddress == 0);

    winrt::hstring resolvedName{};
    bool isPaired{ false };

    try
    {
        // Works for unpaired devices too: this reads what the Bluetooth stack already knows
        // about the device rather than connecting to it.
        auto bleDevice = MidiBleUtilities::AwaitWithTimeout(
            bt::BluetoothLEDevice::FromBluetoothAddressAsync(discoveredDevice.BluetoothAddress),
            MidiBleUtilities::BleOperationTimeoutMilliseconds,
            bt::BluetoothLEDevice{ nullptr });

        if (bleDevice == nullptr)
        {
            return S_FALSE;
        }

        resolvedName = bleDevice.Name();

        if (auto deviceInformation = bleDevice.DeviceInformation())
        {
            if (auto pairing = deviceInformation.Pairing())
            {
                isPaired = pairing.IsPaired();
            }
        }
    }
    CATCH_RETURN();

    if (resolvedName.empty())
    {
        bool gaveUp{ false };

        {
            auto lock = std::scoped_lock{ m_pendingRequestsLock };

            if (auto attempts = m_nameResolutionAttempts.find(deviceId); attempts != m_nameResolutionAttempts.end())
            {
                if (attempts->second.Count >= MIDI_BLE_NAME_RESOLUTION_MAX_ATTEMPTS && !attempts->second.GaveUp)
                {
                    attempts->second.GaveUp = true;
                    gaveUp = true;
                }
            }
        }

        if (gaveUp)
        {
            TraceLoggingWrite(
                MidiBle2MidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Could not resolve a name for this device. It will be listed by address.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(deviceId.c_str(), "device id")
            );

            // it is listable now, so a connection which was waiting on the name can proceed
            QueueConnectIfWanted(deviceId);
        }

        return S_FALSE;
    }

    {
        auto lock = std::scoped_lock{ m_discoveredDevicesLock };

        if (auto entry = m_discoveredDevices.find(deviceId); entry != m_discoveredDevices.end())
        {
            if (entry->second.Name.empty())
            {
                entry->second.Name = resolvedName;
            }

            entry->second.IsPaired = entry->second.IsPaired || isPaired;
        }
    }

    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Resolved a BLE MIDI device name which was not advertised", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(deviceId.c_str(), "device id"),
        TraceLoggingWideString(resolvedName.c_str(), "name")
    );

    // a connection which was waiting on the name can proceed now that the endpoint can be named
    QueueConnectIfWanted(deviceId);

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

    {
        auto lock = std::scoped_lock{ m_connectionParameterRequestsLock };

        m_connectionParameterRequests.erase(deviceId);
    }

    {
        auto lock = std::scoped_lock{ m_createdEndpointsLock };

        std::erase_if(m_createdEndpoints, [&deviceId](CreatedEndpointRecord const& record) { return record.DeviceId == deviceId; });
    }

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

    return CreateEndpoint(
        connection,
        endpointName,
        isUmpNative ? MIDI_BLE_MIDI2_ENDPOINT_DESCRIPTION : MIDI_BLE_MIDI1_ENDPOINT_DESCRIPTION,
        BuildEndpointDeviceInstanceId(discoveredDevice),
        std::wstring{ discoveredDevice.Id });
}


_Use_decl_annotations_
HRESULT
CMidi2Ble2MidiEndpointManager::CreateEndpoint(
    std::shared_ptr<MidiBleConnection> connection,
    std::wstring const& endpointName,
    std::wstring const& endpointDescription,
    std::wstring const& instanceId,
    std::wstring const& uniqueIdentifier
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, connection);
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_midiDeviceManager);
    RETURN_HR_IF(E_UNEXPECTED, m_parentDeviceId.empty());
    RETURN_HR_IF(E_INVALIDARG, endpointName.empty());
    RETURN_HR_IF(E_INVALIDARG, instanceId.empty());

    bool const isUmpNative = connection->Protocol() == MidiBleProtocol::Protocol::Midi2Ump;

    std::wstring transportCode{ TRANSPORT_CODE };

    std::vector<DEVPROPERTY> interfaceDevProperties;

    // The user's customization is looked up before the device node is created, so an endpoint is
    // never published under the wrong name and then renamed a moment later.
    WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria matchCriteria{};
    matchCriteria.DeviceInstanceId = winrt::hstring{ instanceId };
    matchCriteria.TransportSuppliedEndpointName = winrt::hstring{ endpointName };

    std::wstring customName{};
    std::wstring customDescription{};
    std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties> customProperties{ nullptr };

    if (auto configurationManager = TransportState::Current().GetConfigurationManager())
    {
        customProperties = configurationManager->CustomPropertiesCache()->GetProperties(matchCriteria);

        if (customProperties != nullptr)
        {
            if (!customProperties->Name.empty())
            {
                customName = customProperties->Name;
            }

            if (!customProperties->Description.empty())
            {
                customDescription = customProperties->Description;
            }

            // image, MIDI 1.0 port naming, and the device capability hints
            customProperties->WriteNonCommonProperties(interfaceDevProperties);
        }
    }

    // The user's name is what every app shows, including those which know nothing about MIDI
    // properties, so it becomes the device node name and not only a MIDI property.
    std::wstring friendlyName = customName.empty() ? endpointName : customName;

    DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;

    // These two own the memory the group terminal block and name table properties point at, so
    // they have to outlive the ActivateEndpoint call below.
    std::vector<std::byte> groupTerminalBlockData{ };
    WindowsMidiServicesNamingLib::MidiEndpointNameTable nameTable{ };

    if (!isUmpNative)
    {
        // A BLE MIDI 1.0 device presents as a UMP endpoint but has no endpoint discovery to run,
        // so the service is told discovery is done and can create the MIDI 1.0 ports immediately
        // instead of waiting for a negotiation that will never happen.
        interfaceDevProperties.push_back({ { PKEY_MIDI_EndpointDiscoveryProcessComplete, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)), (PVOID)&devPropTrue });

        LOG_IF_FAILED(MidiBleUtilities::BuildMidi1PortProperties(
            friendlyName,
            customProperties,
            groupTerminalBlockData,
            nameTable,
            interfaceDevProperties));
    }

    MIDIENDPOINTCOMMONPROPERTIES commonProperties{};

    commonProperties.TransportId = TRANSPORT_LAYER_GUID;
    commonProperties.EndpointDeviceType = MidiEndpointDeviceType::MidiEndpointDeviceType_Normal;
    commonProperties.FriendlyName = friendlyName.c_str();
    commonProperties.TransportCode = transportCode.c_str();
    commonProperties.EndpointName = endpointName.c_str();
    commonProperties.EndpointDescription = endpointDescription.c_str();
    commonProperties.CustomEndpointName = customName.empty() ? nullptr : customName.c_str();
    commonProperties.CustomEndpointDescription = customDescription.empty() ? nullptr : customDescription.c_str();
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
    createInfo.pszDeviceDescription = friendlyName.c_str();

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

    {
        auto lock = std::scoped_lock{ m_createdEndpointsLock };

        m_createdEndpoints.push_back(CreatedEndpointRecord{
            connection->DeviceId(),
            winrt::hstring{ connection->EndpointDeviceInterfaceId() },
            winrt::hstring{ connection->EndpointDeviceInstanceId() },
            winrt::hstring{ endpointName } });
    }

    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"BLE MIDI endpoint activated", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(instanceId.c_str(), "instance id"),
        TraceLoggingWideString(friendlyName.c_str(), "friendly name"),
        TraceLoggingWideString(newDeviceInterfaceId.get(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2Ble2MidiEndpointManager::RefreshMidi1PortsForRenamedEndpoint(
    winrt::hstring const& endpointDeviceInterfaceId,
    std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties> const customProperties
)
{
    RETURN_HR_IF(E_INVALIDARG, endpointDeviceInterfaceId.empty());
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_midiDeviceManager);

    auto connection = TransportState::Current().GetConnectionByEndpointDeviceInterfaceId(
        std::wstring{ endpointDeviceInterfaceId });

    // Only BLE MIDI 1.0 endpoints get their blocks from this transport. A BLE MIDI 2.0 endpoint
    // declares function blocks of its own during discovery, and the service names the ports
    // from those.
    RETURN_HR_IF(S_FALSE, connection == nullptr);
    RETURN_HR_IF(S_FALSE, connection->Protocol() != MidiBleProtocol::Protocol::Midi1);

    std::wstring portName{ };

    if (customProperties != nullptr && !customProperties->Name.empty())
    {
        portName = customProperties->Name;
    }
    else
    {
        // the customization was cleared, so the name reverts to the one the device reported
        auto lock = std::scoped_lock{ m_createdEndpointsLock };

        auto const record = std::find_if(
            m_createdEndpoints.begin(),
            m_createdEndpoints.end(),
            [&connection](CreatedEndpointRecord const& entry) { return entry.DeviceId == connection->DeviceId(); });

        RETURN_HR_IF(S_FALSE, record == m_createdEndpoints.end());

        portName = record->TransportSuppliedEndpointName;
    }

    RETURN_HR_IF(S_FALSE, portName.empty());

    std::vector<std::byte> groupTerminalBlockData{ };
    WindowsMidiServicesNamingLib::MidiEndpointNameTable nameTable{ };
    std::vector<DEVPROPERTY> properties{ };

    RETURN_IF_FAILED(MidiBleUtilities::BuildMidi1PortProperties(
        portName,
        customProperties,
        groupTerminalBlockData,
        nameTable,
        properties));

    RETURN_IF_FAILED(m_midiDeviceManager->UpdateEndpointProperties(
        endpointDeviceInterfaceId.c_str(),
        static_cast<ULONG>(properties.size()),
        properties.data()));

    TraceLoggingWrite(
        MidiBle2MidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Rebuilt MIDI 1.0 ports for renamed endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingWideString(portName.c_str(), "port name")
    );

    return S_OK;
}


_Use_decl_annotations_
winrt::hstring
CMidi2Ble2MidiEndpointManager::FindMatchingInstantiatedEndpoint(
    WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria& criteria)
{
    criteria.Normalize();

    auto lock = std::scoped_lock{ m_createdEndpointsLock };

    for (auto const& record : m_createdEndpoints)
    {
        WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria available{};

        available.EndpointDeviceId = record.EndpointDeviceId;
        available.DeviceInstanceId = record.DeviceInstanceId;
        available.TransportSuppliedEndpointName = record.TransportSuppliedEndpointName;

        if (available.Matches(criteria))
        {
            return available.EndpointDeviceId;
        }
    }

    return L"";
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

    // Stopped before the worker is joined, so its client-changed callback cannot queue work for a
    // thread which is going away.
    if (auto peripheral = TransportState::Current().GetPeripheral())
    {
        peripheral->SetClientChangedCallback(nullptr);
    }

    // midisrv never calls TransportState::Shutdown, so without this the GATT service provider
    // keeps advertising after the service has stopped and is only torn down by the static
    // TransportState destructor, which runs after main returns and drags a thread join and WinRT
    // teardown into process exit.
    LOG_IF_FAILED(TransportState::Current().StopPeripheral());

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

            if (m_advertisementStoppedToken)
            {
                m_advertisementWatcher.Stopped(m_advertisementStoppedToken);
                m_advertisementStoppedToken = {};
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
        auto lock = std::scoped_lock{ m_connectionParameterRequestsLock };
        m_connectionParameterRequests.clear();
    }

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
