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

GUID AbstractionLayerGUID = __uuidof(Midi2BluetoothMidiAbstraction);



#define MIDI_BLE_SERVICE_CHARACTERISTIC L"{03B80E5A-EDE8-4B33-A751-6CE34EC4C700}"
#define MIDI_BLE_DATA_IO_CHARACTERISTIC L"{7772E5DB-3868-4112-A1A9-F2669D106BF3}"
// Notes:
//      Write (encryption recommended, write without response is required)
//      Read (encryption recommended, respond with no payload)
//      Notify (encryption recommended)
// Max connection interval is 15ms. Lower is better.



_Use_decl_annotations_
HRESULT
CMidi2BluetoothMidiEndpointManager::Initialize(
    IUnknown* MidiDeviceManager, 
    IUnknown* /*midiEndpointProtocolManager*/
)
{
    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF(E_INVALIDARG, nullptr == MidiDeviceManager);

    RETURN_IF_FAILED(MidiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));

    m_TransportAbstractionId = AbstractionLayerGUID;   // this is needed so MidiSrv can instantiate the correct transport
    m_ContainerId = m_TransportAbstractionId;                           // we use the transport ID as the container ID for convenience


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
CMidi2BluetoothMidiEndpointManager::CreateParentDevice()
{

    return S_OK;
}


_Use_decl_annotations_
HRESULT 
CMidi2BluetoothMidiEndpointManager::CreateEndpoint(
    MidiBluetoothDeviceDefinition& definition
)
{
    UNREFERENCED_PARAMETER(definition);



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


    return S_OK;
}






_Use_decl_annotations_
HRESULT 
CMidi2BluetoothMidiEndpointManager::OnDeviceAdded(enumeration::DeviceWatcher, enumeration::DeviceInformation)
{

    return S_OK;
}

_Use_decl_annotations_
HRESULT 
CMidi2BluetoothMidiEndpointManager::OnDeviceRemoved(enumeration::DeviceWatcher, enumeration::DeviceInformationUpdate)
{

    return S_OK;
}

_Use_decl_annotations_
HRESULT 
CMidi2BluetoothMidiEndpointManager::OnDeviceUpdated(enumeration::DeviceWatcher, enumeration::DeviceInformationUpdate)
{

    return S_OK;
}


_Use_decl_annotations_
HRESULT 
CMidi2BluetoothMidiEndpointManager::OnDeviceStopped(enumeration::DeviceWatcher, foundation::IInspectable)
{

    return S_OK;
}


_Use_decl_annotations_
HRESULT 
CMidi2BluetoothMidiEndpointManager::OnEnumerationCompleted(enumeration::DeviceWatcher, foundation::IInspectable)
{

    return S_OK;
}
