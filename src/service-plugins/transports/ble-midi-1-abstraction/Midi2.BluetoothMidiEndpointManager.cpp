// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "midi2.BluetoothMidiAbstraction.h"

using namespace wil;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

#define MAX_DEVICE_ID_LEN 200 // size in chars

GUID AbstractionLayerGUID = ABSTRACTION_LAYER_GUID;



_Use_decl_annotations_
HRESULT
CMidi2BluetoothMidiEndpointManager::Initialize(
    IUnknown* /*MidiDeviceManager*/,
    IUnknown* /*midiEndpointProtocolManager*/
)
{

    // temporary because this may be crashing the service on start
    return S_OK;

#if 0




    try
    {
        TraceLoggingWrite(
            MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this")
        );

        RETURN_HR_IF_NULL(E_INVALIDARG, MidiDeviceManager);

        RETURN_IF_FAILED(MidiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));

        m_TransportAbstractionId = AbstractionLayerGUID;    // this is needed so MidiSrv can instantiate the correct transport
        m_ContainerId = m_TransportAbstractionId;           // we use the transport ID as the container ID for convenience

        RETURN_IF_FAILED(CreateParentDevice());

        RETURN_IF_FAILED(StartAdvertisementWatcher());
        //RETURN_IF_FAILED(StartDeviceWatcher());

        return S_OK;
    }
    catch (std::exception ex)
    {
        TraceLoggingWrite(
            MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingString(ex.what(), "exception"),
            TraceLoggingWideString(L"Exception initializing BLE MIDI Endpoint Manager", "message")
        );

        return E_FAIL;
    }
    catch (...)
    {
        TraceLoggingWrite(
            MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Unknown exception initializing BLE MIDI Endpoint Manager", "message")
        );

        return E_FAIL;
    }
#endif
}

HRESULT
CMidi2BluetoothMidiEndpointManager::CreateParentDevice()
{
    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    // the parent device parameters are set by the transport (this)
    std::wstring parentDeviceName{ TRANSPORT_PARENT_DEVICE_NAME };
    std::wstring parentDeviceId{ internal::NormalizeDeviceInstanceIdWStringCopy(TRANSPORT_PARENT_ID) };

    SW_DEVICE_CREATE_INFO createInfo = {};
    createInfo.cbSize = sizeof(createInfo);
    createInfo.pszInstanceId = parentDeviceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = parentDeviceName.c_str();
    createInfo.pContainerId = &m_ContainerId;

    const ULONG deviceIdMaxSize = 255;
    wchar_t newDeviceId[deviceIdMaxSize]{ 0 };

    RETURN_IF_FAILED(m_MidiDeviceManager->ActivateVirtualParentDevice(
        0,
        nullptr,
        &createInfo,
        (PWSTR)newDeviceId,
        deviceIdMaxSize
    ));

    m_parentDeviceId = internal::NormalizeDeviceInstanceIdWStringCopy(newDeviceId);


    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(newDeviceId, "New parent device instance id")
    );

    return S_OK;
}


_Use_decl_annotations_
HRESULT 
CMidi2BluetoothMidiEndpointManager::CreateEndpoint(
    MidiBluetoothDeviceDefinition* definition
)
{
    RETURN_HR_IF_NULL_MSG(E_INVALIDARG, definition, "Empty endpoint definition");

    //RETURN_HR_IF_MSG(E_INVALIDARG, definition->EndpointName.empty(), "Empty endpoint name");
    //RETURN_HR_IF_MSG(E_INVALIDARG, definition->InstanceIdPrefix.empty(), "Empty endpoint prefix");
    //RETURN_HR_IF_MSG(E_INVALIDARG, definition->EndpointUniqueIdentifier.empty(), "Empty endpoint unique id");

    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingUInt64(definition->BluetoothAddress, "bluetooth address")
    );

    std::wstring mnemonic(TRANSPORT_MNEMONIC);
  
    // TODO: Need to fold in user data here
    std::wstring endpointName = definition->TransportSuppliedName.c_str();
    std::wstring endpointDescription = definition->TransportSuppliedDescription.c_str();

    std::vector<DEVPROPERTY> interfaceDeviceProperties{};

    // no user or in-protocol data in this case
    std::wstring friendlyName = internal::CalculateEndpointDevicePrimaryName(endpointName, L"", L"");


    bool requiresMetadataHandler = false;
    bool multiClient = true;
    bool generateIncomingTimestamps = true;

    std::wstring uniqueIdentifier = std::to_wstring(definition->BluetoothAddress);

    // Device properties


    SW_DEVICE_CREATE_INFO createInfo = {};
    createInfo.cbSize = sizeof(createInfo);

    // build the instance id, which becomes the middle of the SWD id
    std::wstring instanceId = internal::NormalizeDeviceInstanceIdWStringCopy(
        MIDI_BLE_INSTANCE_ID_PREFIX +
        uniqueIdentifier);

    createInfo.pszInstanceId = instanceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = friendlyName.c_str();

    const ULONG deviceInterfaceIdMaxSize = 255;
    wchar_t newDeviceInterfaceId[deviceInterfaceIdMaxSize]{ 0 };

    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(instanceId.c_str(), "instance id"),
        TraceLoggingWideString(L"Activating endpoint", "message")
    );

    MIDIENDPOINTCOMMONPROPERTIES commonProperties{};
    commonProperties.AbstractionLayerGuid = m_TransportAbstractionId;
    commonProperties.EndpointPurpose = MidiEndpointDevicePurposePropertyValue::NormalMessageEndpoint;
    commonProperties.FriendlyName = friendlyName.c_str();
    commonProperties.TransportMnemonic = mnemonic.c_str();
    commonProperties.TransportSuppliedEndpointName = endpointName.c_str();
    commonProperties.TransportSuppliedEndpointDescription = endpointDescription.c_str();
    commonProperties.UserSuppliedEndpointName = nullptr;
    commonProperties.UserSuppliedEndpointDescription = nullptr;
    commonProperties.UniqueIdentifier = uniqueIdentifier.c_str();
    commonProperties.SupportedDataFormats = MidiDataFormat::MidiDataFormat_ByteStream;
    commonProperties.NativeDataFormat = MIDI_PROP_NATIVEDATAFORMAT_BYTESTREAM;
    commonProperties.SupportsMultiClient = multiClient;
    commonProperties.RequiresMetadataHandler = requiresMetadataHandler;
    commonProperties.GenerateIncomingTimestamps = generateIncomingTimestamps;
    commonProperties.SupportsMidi1ProtocolDefaultValue = true;
    commonProperties.SupportsMidi2ProtocolDefaultValue = false;

    if (definition->ManufacturerName.empty())
    {
        commonProperties.ManufacturerName = nullptr;
    }
    else
    {
        commonProperties.ManufacturerName = definition->ManufacturerName.c_str();
    }

    RETURN_IF_FAILED(m_MidiDeviceManager->ActivateEndpoint(
        (PCWSTR)m_parentDeviceId.c_str(),                       // parent instance Id
        true,                                                   // UMP-only
        MidiFlow::MidiFlowBidirectional,                        // MIDI Flow is bidirectional for a BLE MIDI 1.0 device
        &commonProperties,
        (ULONG)interfaceDeviceProperties.size(),
        (ULONG)0,
        (PVOID)interfaceDeviceProperties.data(),
        (PVOID)nullptr,
        (PVOID)&createInfo,
        (LPWSTR)&newDeviceInterfaceId,
        deviceInterfaceIdMaxSize));

    // we need this for removal later
    definition->CreatedMidiDeviceInstanceId = instanceId;
    definition->CreatedEndpointInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(newDeviceInterfaceId);

    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(newDeviceInterfaceId, "new device interface id"),
        TraceLoggingWideString(instanceId.c_str(), "instance id"),
        TraceLoggingWideString(uniqueIdentifier.c_str(), "unique identifier"),
        TraceLoggingWideString(L"Endpoint activated", "message")
    );

    return S_OK;
}




HRESULT
CMidi2BluetoothMidiEndpointManager::Cleanup()
{
    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    if (m_bleAdWatcher != nullptr)
    {
        m_bleAdWatcher.Stop();
        m_bleAdWatcher = nullptr;

        m_AdvertisementReceived.revoke();
        m_AdvertisementWatcherStopped.revoke();
    }

    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Cleanup complete", "message")
    );

    return S_OK;
}






_Use_decl_annotations_
HRESULT 
CMidi2BluetoothMidiEndpointManager::OnDeviceAdded(enumeration::DeviceWatcher /*source*/, enumeration::DeviceInformation /*args*/)
{
    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    return S_OK;
}

_Use_decl_annotations_
HRESULT 
CMidi2BluetoothMidiEndpointManager::OnDeviceRemoved(enumeration::DeviceWatcher /*source*/, enumeration::DeviceInformationUpdate /*args*/)
{
    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    return S_OK;
}

_Use_decl_annotations_
HRESULT 
CMidi2BluetoothMidiEndpointManager::OnDeviceUpdated(enumeration::DeviceWatcher /*source*/, enumeration::DeviceInformationUpdate /*args*/)
{
    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    return S_OK;
}


_Use_decl_annotations_
HRESULT 
CMidi2BluetoothMidiEndpointManager::OnDeviceStopped(enumeration::DeviceWatcher /*source*/, foundation::IInspectable /*args*/)
{
    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    return S_OK;
}


_Use_decl_annotations_
HRESULT 
CMidi2BluetoothMidiEndpointManager::OnEnumerationCompleted(enumeration::DeviceWatcher /*source*/, foundation::IInspectable /*args*/)
{
    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    return S_OK;
}




// TODO: This likely isn't the correct event signature

_Use_decl_annotations_
HRESULT
CMidi2BluetoothMidiEndpointManager::OnBleDeviceNameChanged(bt::BluetoothLEDevice device, foundation::IInspectable /*args*/)
{
    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2BluetoothMidiEndpointManager::OnBleDeviceConnectionStatusChanged(bt::BluetoothLEDevice device, foundation::IInspectable /*args*/)
{
    bool connected = (device.ConnectionStatus() == bt::BluetoothConnectionStatus::Connected);

    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingUInt64(device.BluetoothAddress(), "address"),
        TraceLoggingWideString(device.DeviceId().c_str(), "device id"),
        TraceLoggingWideString(device.Name().c_str(), "name"),
        TraceLoggingBool(connected, "is connected")
    );

    // TODO: When the device was first seen by the service, it may have already been connected, so 
    // we need to handle that even though we won't have a connection status changed event

    auto entry = MidiEndpointTable::Current().GetEndpointEntryForBluetoothAddress(device.BluetoothAddress());

    if (entry == nullptr)
    {
        // this device is new to us. This should never happen
        TraceLoggingWrite(
            MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingUInt64(device.BluetoothAddress(), "address"),
            TraceLoggingWideString(device.DeviceId().c_str(), "device id"),
            TraceLoggingWideString(device.Name().c_str(), "name"),
            TraceLoggingBool(connected, "is connected"),
            TraceLoggingWideString(L"Received a connection changed notification for a device we didn't have recorded.", "message")
        );

        return E_FAIL;
    }

    if (connected)
    {
        // Connected. check to see if we've already created the SWD. If not, create it.
        if (entry->Definition.CreatedEndpointInterfaceId.empty())
        {
            TraceLoggingWrite(
                MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingUInt64(device.BluetoothAddress(), "address"),
                TraceLoggingWideString(device.DeviceId().c_str(), "device id"),
                TraceLoggingWideString(device.Name().c_str(), "name"),
                TraceLoggingBool(connected, "is connected"),
                TraceLoggingWideString(L"Creating new SWD / endpoint", "message")
            );

            RETURN_IF_FAILED(CreateEndpoint(&(entry->Definition)));

            TraceLoggingWrite(
                MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingUInt64(device.BluetoothAddress(), "address"),
                TraceLoggingWideString(device.DeviceId().c_str(), "device id"),
                TraceLoggingWideString(device.Name().c_str(), "name"),
                TraceLoggingBool(connected, "is connected"),
                TraceLoggingWideString(L"Endpoint created", "message")
            );
        }
        else
        {
            // we've already created this device and it is currently connected. This shouldn't happen.

            TraceLoggingWrite(
                MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingUInt64(device.BluetoothAddress(), "address"),
                TraceLoggingWideString(device.DeviceId().c_str(), "device id"),
                TraceLoggingWideString(device.Name().c_str(), "name"),
                TraceLoggingBool(connected, "is connected"),
                TraceLoggingWideString(L"The last state we captured was that this device was connected. Shouldn't have received a change notification.", "message")
            );
        }
    }
    else
    {
        TraceLoggingWrite(
            MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingUInt64(device.BluetoothAddress(), "address"),
            TraceLoggingWideString(device.DeviceId().c_str(), "device id"),
            TraceLoggingWideString(device.Name().c_str(), "name"),
            TraceLoggingBool(connected, "is connected"),
            TraceLoggingWideString(L"Deactivating SWD", "message")
        );

        // Disconnected. Remove the SWD.

        std::wstring instanceId = entry->Definition.CreatedMidiDeviceInstanceId;

        if (!instanceId.empty())
        {
            RETURN_IF_FAILED(m_MidiDeviceManager->DeactivateEndpoint(instanceId.c_str()));

            entry->Definition.SetDeactivated();
            entry->MidiDeviceBiDi = nullptr;
        }
        else
        {
            // TODO Log
        }
    }

    return S_OK;
}




_Use_decl_annotations_
HRESULT
CMidi2BluetoothMidiEndpointManager::OnBleAdvertisementReceived(
    ad::BluetoothLEAdvertisementWatcher /*source*/,
    ad::BluetoothLEAdvertisementReceivedEventArgs const& args)
{
    // Check to see if we already know about this device. If so, ignore

    if (MidiEndpointTable::Current().GetEndpointEntryForBluetoothAddress(args.BluetoothAddress()) != nullptr)
    {
        // we already know about this device. Exit

        return S_OK;
    }

    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingUInt64(args.BluetoothAddress(), "address"),
        TraceLoggingWideString(L"New BLE device found via advertisement", "message")
    );

    // TODO
    // If we don't know about it, check to see if it's a device on our allowed list
    // If so, we can create a device entry for it. If not, ignore.
    // We may want a ble-wide policy option to allow any MIDI devices we see, vs deny by default. Default is deny all.


    auto device = bt::BluetoothLEDevice::FromBluetoothAddressAsync(args.BluetoothAddress()).get();
    RETURN_HR_IF_NULL_MSG(E_UNEXPECTED, device, "Bluetooth device address is invalid.");

    // get the GATT MIDI service
    auto service = GetBleMidiServiceFromDevice(device);
   
    if (service != nullptr)
    {
        auto connectionStatusChangedHandler = foundation::TypedEventHandler <bt::BluetoothLEDevice , foundation::IInspectable>
            (this, &CMidi2BluetoothMidiEndpointManager::OnBleDeviceConnectionStatusChanged);

        // create the device definition
        MidiBluetoothDeviceDefinition definition;

        definition.BluetoothAddress = device.BluetoothAddress();
        definition.TransportSuppliedName = device.Name();           // need to ensure we also wire up the name changed event handler for this 

        std::shared_ptr<MidiBluetoothEndpointEntry> entry{ nullptr };

        // todo: Should lock the entry list during this



        
        // add the new endpoint to the device table
        entry = MidiEndpointTable::Current().CreateAndAddNewEndpointEntry(definition, device, service);

        if (entry != nullptr)
        {
            // create the MIDI endpoint if this is connected. 
            if (entry->Device.ConnectionStatus() == bt::BluetoothConnectionStatus::Connected)
            {
                RETURN_IF_FAILED(CreateEndpoint(&(entry->Definition)));
            }

            // wire up connection status changed event and store the token for revocation
//            entry->ConnectionStatusChangedToken = device.ConnectionStatusChanged(winrt::auto_revoke, connectionStatusChangedHandler);
            //entry->ConnectionStatusChangedRevoker = entry->Device.ConnectionStatusChanged(winrt::auto_revoke, connectionStatusChangedHandler);
            entry->ConnectionStatusChangedRevoker = entry->Device.ConnectionStatusChanged(connectionStatusChangedHandler);
        }
        else
        {
            // failed to just add a new entry. Log

            TraceLoggingWrite(
                MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingUInt64(device.BluetoothAddress(), "address"),
                TraceLoggingWideString(device.DeviceId().c_str(), "device id"),
                TraceLoggingWideString(device.Name().c_str(), "name"),
                TraceLoggingWideString(L"Failed to add entry to endpoint table", "message")
            );
        }
    }
    else
    {
        // no BLE MIDI service found. Log this because the filter should have prevented this.

        TraceLoggingWrite(
            MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingUInt64(device.BluetoothAddress(), "address"),
            TraceLoggingWideString(device.DeviceId().c_str(), "device id"),
            TraceLoggingWideString(device.Name().c_str(), "name"),
            TraceLoggingWideString(L"BLE device does not have MIDI service", "message")
        );
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2BluetoothMidiEndpointManager::OnBleAdvertisementWatcherStopped(ad::BluetoothLEAdvertisementWatcher, ad::BluetoothLEAdvertisementWatcherStoppedEventArgs args)
{
    // error enum vals: https://learn.microsoft.com/uwp/api/windows.devices.bluetooth.bluetootherror?view=winrt-22621

    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingUInt32((uint32_t)args.Error(), "BluetoothError enum value")
    );

    // todo: consider if we should restart the watcher or not

    return S_OK;
}


HRESULT 
CMidi2BluetoothMidiEndpointManager::StartAdvertisementWatcher()
{
    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    auto adReceivedHandler = foundation::TypedEventHandler<ad::BluetoothLEAdvertisementWatcher, ad::BluetoothLEAdvertisementReceivedEventArgs>
        (this, &CMidi2BluetoothMidiEndpointManager::OnBleAdvertisementReceived);

    auto adWatcherStoppedHandler = foundation::TypedEventHandler<ad::BluetoothLEAdvertisementWatcher, ad::BluetoothLEAdvertisementWatcherStoppedEventArgs>
        (this, &CMidi2BluetoothMidiEndpointManager::OnBleAdvertisementWatcherStopped);


    // wire up the event handler so we're notified when advertising messages are received. 
    // This will fire for every message received, even if we already know about the device.
    m_AdvertisementReceived = m_bleAdWatcher.Received(winrt::auto_revoke, adReceivedHandler);
    m_AdvertisementWatcherStopped = m_bleAdWatcher.Stopped(winrt::auto_revoke, adWatcherStoppedHandler);

    winrt::guid serviceUuid{ MIDI_BLE_SERVICE_UUID };

    m_bleAdWatcher.AdvertisementFilter().Advertisement().ServiceUuids().Append(serviceUuid);
    m_bleAdWatcher.Start();

    if (m_bleAdWatcher.Status() == ad::BluetoothLEAdvertisementWatcherStatus::Aborted)
    {
        // if the status is Aborted, it means there was an error

        TraceLoggingWrite(
            MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Error starting BLE ad watcher", "message")
        );

        return E_FAIL;
    }
    else
    {
        TraceLoggingWrite(
            MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"BLE Ad watcher started", "message")
        );

        return S_OK;
    }
}

HRESULT 
CMidi2BluetoothMidiEndpointManager::StartDeviceWatcher()
{
    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    winrt::hstring deviceSelector = bt::BluetoothLEDevice::GetDeviceSelector();


    auto requestedProperties = winrt::single_threaded_vector<winrt::hstring>(
        {
            L"System.DeviceInterface.Bluetooth.DeviceAddress",
            L"System.DeviceInterface.Bluetooth.Flags",
            L"System.DeviceInterface.Bluetooth.LastConnectedTime",
            L"System.DeviceInterface.Bluetooth.Manufacturer",
            L"System.DeviceInterface.Bluetooth.ModelNumber",
            L"System.DeviceInterface.Bluetooth.ProductId",
            L"System.DeviceInterface.Bluetooth.ProductVersion",
            L"System.DeviceInterface.Bluetooth.ServiceGuid",
            L"System.DeviceInterface.Bluetooth.VendorId",
            L"System.DeviceInterface.Bluetooth.VendorIdSource",
            L"System.Devices.Connected",
            L"System.Devices.DeviceCapabilities",
            L"System.Devices.DeviceCharacteristics"
        }
    );


    m_Watcher = enumeration::DeviceInformation::CreateWatcher(deviceSelector, requestedProperties);

    auto deviceAddedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, enumeration::DeviceInformation>(this, &CMidi2BluetoothMidiEndpointManager::OnDeviceAdded);
    auto deviceRemovedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, enumeration::DeviceInformationUpdate>(this, &CMidi2BluetoothMidiEndpointManager::OnDeviceRemoved);
    auto deviceUpdatedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, enumeration::DeviceInformationUpdate>(this, &CMidi2BluetoothMidiEndpointManager::OnDeviceUpdated);
    auto deviceStoppedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, foundation::IInspectable>(this, &CMidi2BluetoothMidiEndpointManager::OnDeviceStopped);
    auto deviceEnumerationCompletedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, foundation::IInspectable>(this, &CMidi2BluetoothMidiEndpointManager::OnEnumerationCompleted);

    m_DeviceAdded = m_Watcher.Added(winrt::auto_revoke, deviceAddedHandler);
    m_DeviceRemoved = m_Watcher.Removed(winrt::auto_revoke, deviceRemovedHandler);
    m_DeviceUpdated = m_Watcher.Updated(winrt::auto_revoke, deviceUpdatedHandler);
    m_DeviceStopped = m_Watcher.Stopped(winrt::auto_revoke, deviceStoppedHandler);
    m_DeviceEnumerationCompleted = m_Watcher.EnumerationCompleted(winrt::auto_revoke, deviceEnumerationCompletedHandler);


    m_Watcher.Start();

    return S_OK;
}

HRESULT 
CMidi2BluetoothMidiEndpointManager::CreateSelfPeripheralEndpoint()
{



    return S_OK;
}