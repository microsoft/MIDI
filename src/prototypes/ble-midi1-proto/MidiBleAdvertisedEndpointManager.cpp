// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

HRESULT MidiBleAdvertisedEndpointManager::Initialize()
{
    winrt::guid MIDI_BLE_SERVICE_UUID{ MIDI_BLE_SERVICE };

    BluetoothLEAdvertisementWatcher watcher;
    watcher.AdvertisementFilter().Advertisement().ServiceUuids().Append(MIDI_BLE_SERVICE_UUID);


    BluetoothLEAdvertisementFilter ad;

    BluetoothLEAdvertisementBytePattern pattern;

    // filter by our service UUID
    pattern.DataType(BluetoothLEAdvertisementDataTypes::CompleteService128BitUuids());



//    ad.BytePatterns().Append();


    return S_OK;
}

MidiBleDevice* MidiBleAdvertisedEndpointManager::GetDevice(_In_ std::wstring endpointDeviceInterfaceId)
{

    return nullptr;
}

HRESULT MidiBleAdvertisedEndpointManager::Cleanup()
{

    return S_OK;
}

