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

    //RETURN_IF_FAILED(CreateParentDevice());

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
)
{

    return S_OK;
}



// TODO: This will need to create a device watcher
//
// MIDI Service (UUID: 03B80E5A-EDE8-4B33-A751-6CE34EC4C700) 
// MIDI Data I/O Characteristic(UUID: 7772E5DB-3868-4112-A1A9-F2669D106BF3)
//

#define MIDI_BLE_DATA_CHARACTERISTIC L"{7772E5DB-3868-4112-A1A9-F2669D106BF3}"
#define MIDI_BLE_GATT_SERVICE L"{03B80E5A-EDE8-4B33-A751-6CE34EC4C700}"

namespace bt = ::winrt::Windows::Devices::Bluetooth;

HRESULT
CMidi2BluetoothMidiEndpointManager::EnumCompatibleBluetoothDevices()
{
//    winrt::guid midiServiceGuid(MIDI_BLE_GATT_SERVICE);

//    bt::BluetoothLEDevice::GetGattServicesForUuidAsync(midiServiceGuid);


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


