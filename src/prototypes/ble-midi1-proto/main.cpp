// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// NOTE: This is prototype code for working through some BLE approaches before
// implementing it in the Windows service. Eventually, this code will go stale, 
// but we'll keep it around. This is not production code.

#include "pch.h"

using namespace winrt;
using namespace Windows::Foundation;

// some refs
// https://learn.microsoft.com/en-us/windows/uwp/devices-sensors/gatt-client


void TestFindingDevices()
{
    std::map<uint64_t, winrt::hstring> foundMidiDevices;

    GattCharacteristic characteristic{ nullptr };


    winrt::guid MIDI_BLE_SERVICE_UUID{ MIDI_BLE_SERVICE };

    BluetoothLEAdvertisementWatcher watcher;

    watcher.Received([&](BluetoothLEAdvertisementWatcher /*source*/, BluetoothLEAdvertisementReceivedEventArgs const& args)
        {
            //std::cout
            //    << "Ad received" << std::endl
            //    << " - Timestamp:               " << args.Timestamp().time_since_epoch().count() << std::endl
            //    << " - Address:                 " << args.BluetoothAddress() << std::endl
            //    << " - IsConnectable:           " << args.IsConnectable() << std::endl
            //    << " - IsAnonymous:             " << args.IsAnonymous() << std::endl
            //    << " - IsDirected:              " << args.IsDirected() << std::endl
            //    << " - IsScannable:             " << args.IsScannable() << std::endl
            //    << " - RawSignalStrengthInDBm:  " << args.RawSignalStrengthInDBm() << std::endl;

            auto device = BluetoothLEDevice::FromBluetoothAddressAsync(args.BluetoothAddress()).get();

            if (device != nullptr)
            {
                if (device.ConnectionStatus() == BluetoothConnectionStatus::Connected)
                {
                    //                   std::cout << "Connection status: Connected" << std::endl;
                }
                else
                {
                    //                    std::cout << "Connection status: Disconnected" << std::endl;
                }

                auto service = GetBleMidiServiceFromDevice(device);

                if (service != nullptr)
                {
                    //std::cout << "Found service" << std::endl;

                    if (foundMidiDevices.find(args.BluetoothAddress()) == foundMidiDevices.end())
                    {
                        foundMidiDevices[args.BluetoothAddress()] = device.Name();

                        std::cout
                            << "MIDI Device at Address: " << args.BluetoothAddress()
                            << ", Name: " << winrt::to_string(device.Name())
                            << std::endl;
                    }
                }
                else
                {
                    //std::cout << "Not a BLE MIDI Device" << std::endl;
                }
            }
        }
    );

    watcher.AdvertisementFilter().Advertisement().ServiceUuids().Append(MIDI_BLE_SERVICE_UUID);

    watcher.Start();

    system("pause > nul");
}

void ReportAdapterCapabilities()
{
    auto adapter = BluetoothAdapter::GetDefaultAsync().get();

    if (adapter != nullptr)
    {
        std::cout << "IsCentralRoleSupported                 : " << adapter.IsCentralRoleSupported() << std::endl;
        std::cout << "IsPeripheralRoleSupported              : " << adapter.IsPeripheralRoleSupported() << std::endl;
        std::cout << "IsClassicSupported                     : " << adapter.IsClassicSupported() << std::endl;
        std::cout << "IsLowEnergySupported                   : " << adapter.IsLowEnergySupported() << std::endl;
        std::cout << "IsExtendedAdvertisingSupported         : " << adapter.IsExtendedAdvertisingSupported() << std::endl;
        std::cout << "IsAdvertisementOffloadSupported        : " << adapter.IsAdvertisementOffloadSupported() << std::endl;
        std::cout << "AreClassicSecureConnectionsSupported   : " << adapter.AreClassicSecureConnectionsSupported() << std::endl;
        std::cout << "AreLowEnergySecureConnectionsSupported : " << adapter.AreLowEnergySecureConnectionsSupported() << std::endl;
    }
    else
    {
        std::cout << "No BT adapter present" << std::endl;
    }

    std::cout << "------------------------------------------" << std::endl;
}

#define BLUETOOTH_COMPANY_CODE_MICROSOFT 0xFE08

void TestAdvertisingAsPeripheral()
{
//    std::cout << "Setting up ad..." << std::endl;
//
//    winrt::guid MIDI_BLE_SERVICE_UUID{ MIDI_BLE_SERVICE };
//
//    BluetoothLEManufacturerData manufacturer;
//    manufacturer.CompanyId(BLUETOOTH_COMPANY_CODE_MICROSOFT);
//
//    //streams::DataWriter writer;
//    //writer.WriteString(L"Test");
//    //manufacturer.Data(writer.DetachBuffer());
//
//    BluetoothLEAdvertisement ad;
//    //ad.ServiceUuids().Append(MIDI_BLE_SERVICE_UUID);
//    ad.ManufacturerData().Append(manufacturer);
//
//    BluetoothLEAdvertisementPublisher publisher(ad);
////    publisher.IncludeTransmitPowerLevel(true);    // can only do this if we support extended advertising, a BLE 5+ thing
//
//    std::cout << "Starting publisher..." << std::endl;
//
//    publisher.Start();

    winrt::guid MIDI_BLE_SERVICE_UUID{ MIDI_BLE_SERVICE };

    auto result = GattServiceProvider::CreateAsync(MIDI_BLE_SERVICE_UUID).get();

    if (result.Error() == BluetoothError::Success)
    {
        auto provider = result.ServiceProvider();

        GattServiceProviderAdvertisingParameters params;
        params.IsConnectable(true);
        params.IsDiscoverable(true);

        provider.StartAdvertising(params);

        system("pause");
    }
}


int main()
{
    init_apartment();


    //TestFindingDevices();

    ReportAdapterCapabilities();

    TestAdvertisingAsPeripheral();

    
}

