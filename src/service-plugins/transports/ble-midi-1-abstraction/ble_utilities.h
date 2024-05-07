// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#define MIDI_BLE_SERVICE_UUID                   L"03B80E5A-EDE8-4B33-A751-6CE34EC4C700"
#define MIDI_BLE_DATA_IO_CHARACTERISTIC_UUID    L"7772E5DB-3868-4112-A1A9-F2669D106BF3"


inline bt::BluetoothLEDevice GetBleDeviceFromEnumerationDeviceId(_In_ std::wstring deviceId)
{
    //    std::cout << __FUNCTION__ << std::endl;

    auto device = bt::BluetoothLEDevice::FromIdAsync(winrt::to_hstring(deviceId.c_str())).get();

    return device;
}


inline gatt::GattDeviceService GetBleMidiServiceFromDevice(_In_ bt::BluetoothLEDevice bleDevice)
{
    //    std::cout << __FUNCTION__ << std::endl;

    winrt::guid serviceUuid{ MIDI_BLE_SERVICE_UUID };

    auto gattServicesResult = bleDevice.GetGattServicesAsync(bt::BluetoothCacheMode::Uncached).get();

    if (gattServicesResult.Status() == gatt::GattCommunicationStatus::Success)
    {
        auto services = gattServicesResult.Services();

        // find the MIDI BLE service
        if (auto it = std::find_if(services.begin(), services.end(), [&](gatt::GattDeviceService const& x) { return x.Uuid() == serviceUuid; }); it != services.end())
        {
            // just grab the first one. Shouldn't be more than one match.
            auto midiService = *it;

            return midiService;
        }
    }

    return nullptr;
}



inline gatt::GattCharacteristic GetBleMidiDataIOCharacteristicFromService(_In_ gatt::GattDeviceService midiService)
{
    //    std::cout << __FUNCTION__ << std::endl;

    winrt::guid dataIOCharacteristicUuid{ MIDI_BLE_DATA_IO_CHARACTERISTIC_UUID };

    if (midiService != nullptr)
    {
        auto characteristicsResult = midiService.GetCharacteristicsForUuidAsync(dataIOCharacteristicUuid).get();

        if (characteristicsResult.Status() == gatt::GattCommunicationStatus::Success && characteristicsResult.Characteristics().Size() > 0)
        {
            // we're just getting the first one
            auto characteristic = characteristicsResult.Characteristics().GetAt(0);

            return characteristic;
        }
    }

    return nullptr;
}
