#pragma once

inline BluetoothLEDevice GetBleDeviceFromEnumerationDeviceId(_In_ std::wstring deviceId)
{
//    std::cout << __FUNCTION__ << std::endl;

    auto device = BluetoothLEDevice::FromIdAsync(winrt::to_hstring(deviceId.c_str())).get();

    return device;
}


inline GattDeviceService GetBleMidiServiceFromDevice(_In_ BluetoothLEDevice bleDevice)
{
//    std::cout << __FUNCTION__ << std::endl;

    winrt::guid MIDI_BLE_SERVICE_UUID{ MIDI_BLE_SERVICE };

    auto gattServicesResult = bleDevice.GetGattServicesAsync(BluetoothCacheMode::Uncached).get();
    if (gattServicesResult.Status() == GattCommunicationStatus::Success)
    {
        auto services = gattServicesResult.Services();

        // find the MIDI BLE service
        if (auto it = std::find_if(services.begin(), services.end(), [&](GattDeviceService const& x) { return x.Uuid() == MIDI_BLE_SERVICE_UUID; }); it != services.end())
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

    winrt::guid MIDI_BLE_DATA_IO_CHARACTERISTIC_UUID{ MIDI_BLE_DATA_IO_CHARACTERISTIC };

    if (midiService != nullptr)
    {
        auto characteristicsResult = midiService.GetCharacteristicsForUuidAsync(MIDI_BLE_DATA_IO_CHARACTERISTIC_UUID).get();

        if (characteristicsResult.Status() == GattCommunicationStatus::Success && characteristicsResult.Characteristics().Size() > 0)
        {
            // we're just getting the first one
            auto characteristic = characteristicsResult.Characteristics().GetAt(0);

            return characteristic;
        }
    }

    return nullptr;
}
