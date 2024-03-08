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



#define MIDI_BLE_SERVICE L"{03B80E5A-EDE8-4B33-A751-6CE34EC4C700}"
#define MIDI_BLE_DATA_IO_CHARACTERISTIC L"{7772E5DB-3868-4112-A1A9-F2669D106BF3}"

winrt::guid MIDI_BLE_SERVICE_UUID(MIDI_BLE_SERVICE);
winrt::guid MIDI_BLE_DATA_IO_CHARACTERISTIC_UUID(MIDI_BLE_DATA_IO_CHARACTERISTIC);

// Notes:
//      Write (encryption recommended, write without response is required)
//      Read (encryption recommended, respond with no payload)
//      Notify (encryption recommended)
// Max connection interval is 15ms. Lower is better.


enumeration::DeviceWatcher m_Watcher{ nullptr };
bool m_enumerationCompleted{ false };

winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<enumeration::IDeviceWatcher>::Added_revoker m_DeviceAdded;
winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<enumeration::IDeviceWatcher>::Removed_revoker m_DeviceRemoved;
winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<enumeration::IDeviceWatcher>::Updated_revoker m_DeviceUpdated;
winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<enumeration::IDeviceWatcher>::Stopped_revoker m_DeviceStopped;
winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<enumeration::IDeviceWatcher>::EnumerationCompleted_revoker m_DeviceEnumerationCompleted;


void OnDeviceAdded(enumeration::DeviceWatcher, enumeration::DeviceInformation info)
{
    std::cout << "Added: " << winrt::to_string(info.Id()) << std::endl;
    std::cout << "Name: " << winrt::to_string(info.Name()) << std::endl;


    // get the BLE device from this

    auto bleDevice = bt::BluetoothLEDevice::FromIdAsync(info.Id()).get();

    if (bleDevice != nullptr)
    {
        std::cout << "BLE Name: " << winrt::to_string(bleDevice.Name()) << std::endl;

        if (bleDevice.ConnectionStatus() == bt::BluetoothConnectionStatus::Connected)
        {
            std::cout << "Current status: Connected " << std::endl;
        }
        else
        {
            std::cout << "Current status: Disconnected " << std::endl;
        }

        

        auto gattServicesResult = bleDevice.GetGattServicesAsync().get();

        if (gattServicesResult.Status() == gatt::GattCommunicationStatus::Success)
        {
            for (auto service : gattServicesResult.Services())
            {
                auto serviceUuid = service.Uuid();

                if (serviceUuid == MIDI_BLE_SERVICE_UUID)
                {
                    std::cout << "MIDI Service Found: " << winrt::to_string(winrt::to_hstring(serviceUuid)) << std::endl;

                    auto characteristics = service.GetAllCharacteristics();

                    for (auto characteristic : characteristics)
                    {
                        auto uuid = characteristic.Uuid();
                        //auto descriptorResult = characteristic.GetDescriptorsForUuidAsync(uuid).get();

                        //for (auto desc : descriptorResult.Descriptors())
                        //{
                        //    desc.
                        //}

                        if (uuid == MIDI_BLE_DATA_IO_CHARACTERISTIC_UUID)
                        {
                            std::cout << "MIDI Data IO Characteristic Found: " << winrt::to_string(winrt::to_hstring(uuid)) << std::endl;

                            auto properties = characteristic.CharacteristicProperties();

                            if ((properties & gatt::GattCharacteristicProperties::Read) == gatt::GattCharacteristicProperties::Read)
                            {
                                // includes input port
                                std::cout << "- Supports READ (input port)" << std::endl;
                            }

                            if ((properties & gatt::GattCharacteristicProperties::Write) == gatt::GattCharacteristicProperties::Write)
                            {
                                // includes output port
                                std::cout << "- Supports WRITE (output port)" << std::endl;
                            }

                            if ((properties & gatt::GattCharacteristicProperties::Notify) == gatt::GattCharacteristicProperties::Notify)
                            {
                                std::cout << "- Supports NOTIFY" << std::endl;
                            }

                        }


                    }

                    // no need to continue looking
                    break;
                }
            }
        }
        else
        {
            std::cout << "Unable to communicate with device to get gatt services" << std::endl;
        }

        std::cout << "------------------------" << std::endl;

    }

}

void OnDeviceRemoved(enumeration::DeviceWatcher, enumeration::DeviceInformationUpdate upd)
{
    std::cout << "Removed: " << winrt::to_string(upd.Id()) << std::endl;
}

void OnDeviceUpdated(enumeration::DeviceWatcher, enumeration::DeviceInformationUpdate upd)
{
    std::cout << "-----------" << std::endl;

    std::cout << "Updated: " << winrt::to_string(upd.Id()).c_str() << std::endl;

    auto bleDevice = bt::BluetoothLEDevice::FromIdAsync(upd.Id()).get();

    if (bleDevice != nullptr)
    {
        std::cout << "Name: " << winrt::to_string(bleDevice.Name()) << " : ";

        if (bleDevice.ConnectionStatus() == bt::BluetoothConnectionStatus::Connected)
        {
            std::cout << "Connected " << std::endl;
        }
        else
        {
            std::cout << "Disconnected " << std::endl;
        }
    }

    // spit out the updated properties
    for (auto prop : upd.Properties())
    {
        std::cout << winrt::to_string(prop.Key()) << std::endl;
    }

    std::cout << "-----------" << std::endl;
}

void OnDeviceStopped(enumeration::DeviceWatcher, foundation::IInspectable)
{

}

void OnEnumerationCompleted(enumeration::DeviceWatcher, foundation::IInspectable)
{
    m_enumerationCompleted = true;

}

void TestEnumeration()
{
    // enumerate ble MIDI devices

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

    auto deviceAddedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, enumeration::DeviceInformation>(OnDeviceAdded);
    auto deviceRemovedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, enumeration::DeviceInformationUpdate>(OnDeviceRemoved);
    auto deviceUpdatedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, enumeration::DeviceInformationUpdate>(OnDeviceUpdated);
    auto deviceStoppedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, foundation::IInspectable>(OnDeviceStopped);
    auto deviceEnumerationCompletedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, foundation::IInspectable>(OnEnumerationCompleted);

    m_DeviceAdded = m_Watcher.Added(winrt::auto_revoke, deviceAddedHandler);
    m_DeviceRemoved = m_Watcher.Removed(winrt::auto_revoke, deviceRemovedHandler);
    m_DeviceUpdated = m_Watcher.Updated(winrt::auto_revoke, deviceUpdatedHandler);
    m_DeviceStopped = m_Watcher.Stopped(winrt::auto_revoke, deviceStoppedHandler);
    m_DeviceEnumerationCompleted = m_Watcher.EnumerationCompleted(winrt::auto_revoke, deviceEnumerationCompletedHandler);

    // this blocks
    m_Watcher.Start();

}


IAsyncAction TestReceivingData()
{
    std::cout << "Test Receiving Data ---------------------------------------------------------" << std::endl;

    winrt::hstring id = L"BluetoothLE#BluetoothLE3c:6a:a7:f0:4e:b0-48:b6:20:1a:71:9d";

    try
    {
        gatt::GattSession session{ nullptr };

        std::cout << "Creating session" << std::endl;

        auto bleId = bt::BluetoothDeviceId::FromId(id);

        session = co_await gatt::GattSession::FromDeviceIdAsync(bleId);

        std::cout << "Session created" << std::endl;

        session.MaintainConnection(true);

        auto bleDevice = bt::BluetoothLEDevice::FromIdAsync(id).get();

        if (bleDevice != nullptr)
        {
            std::cout << "Using device " << winrt::to_string(bleDevice.Name()) << std::endl;

            auto service = bleDevice.GetGattService(MIDI_BLE_SERVICE_UUID);

            if (service != nullptr)
            {
                std::cout << "Found service" << std::endl;;

                auto openStatus = co_await service.OpenAsync(gatt::GattSharingMode::SharedReadAndWrite);

                if (openStatus == gatt::GattOpenStatus::Success)
                {
                    std::cout << "Service opened" << std::endl;
                }
                else
                {
                    std::cout << "Unable to open service" << std::endl;
                    co_return;
                }

                std::cout << "Getting MIDI Data IO characteristic" << std::endl;

                auto characteristicsResult = co_await service.GetCharacteristicsForUuidAsync(MIDI_BLE_DATA_IO_CHARACTERISTIC_UUID);

                if (characteristicsResult.Status() == gatt::GattCommunicationStatus::Success)
                {
                    if (characteristicsResult.Characteristics().Size() == 0)
                    {
                        // this is unexpected
                        std::cout << "Returned no characteristics for the required UUID" << std::endl;
                    }
                    else if (characteristicsResult.Characteristics().Size() > 1)
                    {
                        // TODO: Need to find out under which cases this happens
                        std::cout << "Returned more than one characteristic for the same UUID" << std::endl;
                    }
                    else
                    {
                        std::cout << "Returned just the one characteristic for the UUID" << std::endl;

                        auto characteristic = characteristicsResult.Characteristics().GetAt(0);

                        co_await characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(
                            gatt::GattClientCharacteristicConfigurationDescriptorValue::Notify);


                        auto valueChangedHandler = [&](gatt::GattCharacteristic const& sender, gatt::GattValueChangedEventArgs const& args)
                            {
                                //std::cout << "Value changed" << std::endl;

                                // we get an IBuffer here

                                //std::cout << "Data size is " << args.CharacteristicValue().Length() << " bytes" << std::endl;

                                std::cout << "> ";

                                for (int i = 0; i < args.CharacteristicValue().Length(); i++)
                                {
                                    // read next byte
                                    auto b = *(args.CharacteristicValue().data() + i);

                                    std::cout << "0x" << std::hex << (unsigned)b << " ";
                                }

                                std::cout << std::endl;
                            };

                        // wire up ValueChanged so we actually get data
                        characteristic.ValueChanged(valueChangedHandler);

                        while (true)
                        {
                            ::Sleep(100);
                        }

                        session.Close();

                    }
                }


                service.Close();
            }
        }

        std::cout << "Done" << std::endl;


        // wait for incoming data indefinitely

        //auto characteristic = characteristicsResult.Characteristics().GetAt(0);

        //auto readResult = characteristic.ReadValueAsync().get();

        //if (readResult.Status() == gatt::GattCommunicationStatus::Success)
        //{
        //    // we get an IBuffer here

        //    for (int i = 0; i < readResult.Value().Length(); i++)
        //    {
        //        // read next byte
        //        auto b = *(readResult.Value().data() + i);

        //        std::cout << std::hex << b << " ";
        //    }

        //    std::cout << std::endl;

        //}
        //session.Close();








    }
    catch (winrt::hresult_error err)
    {
        // Important: using .get() means we don't actually get to handle these exceptions.

        if (err.code() == HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION))
        {
            std::cout << "Cannot access the BLE device because it's being used by another process" << std::endl;
        }
        else
        {
            std::cout << "HRESULT exception 0x" << std::hex << err.code() << std::endl;
        }

        co_return;
    }
    catch (...)
    {
        std::cout << "Exception" << std::endl;

        co_return;
    }

}



int main()
{
    init_apartment();

    //TestEnumeration();

    std::thread enumerationThread(TestEnumeration);

    enumerationThread.detach();


    while (!m_enumerationCompleted)
    {
        Sleep(1000);
    }

    TestReceivingData().get();

    //system("pause > nul");
    system("pause");


}

