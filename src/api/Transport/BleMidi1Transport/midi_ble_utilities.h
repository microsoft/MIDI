// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#ifndef MIDI_BLE_UTILITIES_H
#define MIDI_BLE_UTILITIES_H

#define MIDI_BLE_SERVICE_UUID                   L"{03B80E5A-EDE8-4B33-A751-6CE34EC4C700}"
#define MIDI_BLE_DATA_IO_CHARACTERISTIC_UUID    L"{7772E5DB-3868-4112-A1A9-F2669D106BF3}"
#define MIDI_BLE_COMPANY_CODE_MICROSOFT         0xFE08



namespace MidiBleUtilities
{
    using namespace ::winrt::Windows::Devices::Bluetooth;
    using namespace ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;


    inline BluetoothLEDevice GetBleDeviceFromEnumerationDeviceId(_In_ std::wstring deviceId)
    {
        //    std::cout << __FUNCTION__ << std::endl;

        auto device = BluetoothLEDevice::FromIdAsync(winrt::to_hstring(deviceId.c_str())).get();

        return device;
    }


    inline GattDeviceService GetBleMidiServiceFromDevice(_In_ BluetoothLEDevice bleDevice)
    {
        //    std::cout << __FUNCTION__ << std::endl;

        winrt::guid bleServiceUUID { MIDI_BLE_SERVICE_UUID };

        auto gattServicesResult = bleDevice.GetGattServicesAsync(BluetoothCacheMode::Uncached).get();
        if (gattServicesResult.Status() == GattCommunicationStatus::Success)
        {
            auto services = gattServicesResult.Services();

            // find the MIDI BLE service
            if (auto it = std::find_if(services.begin(), services.end(), [&](GattDeviceService const& x) { return x.Uuid() == bleServiceUUID; }); it != services.end())
            {
                // just grab the first one. Shouldn't be more than one match.
                auto midiService = *it;

                return midiService;
            }
        }

        return nullptr;
    }



    inline GattCharacteristic GetBleMidiDataIOCharacteristicFromService(_In_ GattDeviceService midiService)
    {
        //    std::cout << __FUNCTION__ << std::endl;

        winrt::guid characteristicUUID { MIDI_BLE_DATA_IO_CHARACTERISTIC_UUID };

        if (midiService != nullptr)
        {
            auto characteristicsResult = midiService.GetCharacteristicsForUuidAsync(characteristicUUID).get();

            if (characteristicsResult.Status() == GattCommunicationStatus::Success && characteristicsResult.Characteristics().Size() > 0)
            {
                // we're just getting the first one
                auto characteristic = characteristicsResult.Characteristics().GetAt(0);

                return characteristic;
            }
        }

        return nullptr;
    }
}

#endif