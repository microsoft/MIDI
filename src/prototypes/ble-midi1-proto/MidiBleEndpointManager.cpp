// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================



#include "pch.h"


HRESULT MidiBleEndpointManager::Initialize()
{
    winrt::hstring deviceSelector = BluetoothLEDevice::GetDeviceSelector();

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

    auto deviceAddedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, enumeration::DeviceInformation>(this, &MidiBleEndpointManager::OnDeviceAdded);
    auto deviceRemovedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, enumeration::DeviceInformationUpdate>(this, &MidiBleEndpointManager::OnDeviceRemoved);
    auto deviceUpdatedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, enumeration::DeviceInformationUpdate>(this, &MidiBleEndpointManager::OnDeviceUpdated);
    auto deviceStoppedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, foundation::IInspectable>(this, &MidiBleEndpointManager::OnDeviceWatcherStopped);
    auto deviceEnumerationCompletedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, foundation::IInspectable>(this, &MidiBleEndpointManager::OnEnumerationCompleted);

    m_DeviceAdded = m_Watcher.Added(winrt::auto_revoke, deviceAddedHandler);
    m_DeviceRemoved = m_Watcher.Removed(winrt::auto_revoke, deviceRemovedHandler);
    m_DeviceUpdated = m_Watcher.Updated(winrt::auto_revoke, deviceUpdatedHandler);
    m_DeviceStopped = m_Watcher.Stopped(winrt::auto_revoke, deviceStoppedHandler);
    m_DeviceEnumerationCompleted = m_Watcher.EnumerationCompleted(winrt::auto_revoke, deviceEnumerationCompletedHandler);


    std::thread watcherThread(&MidiBleEndpointManager::WatcherThreadWorker, this);

    m_watcherThread = std::move(watcherThread);

    m_watcherThread.detach();

    return S_OK;
}

void MidiBleEndpointManager::WatcherThreadWorker()
{
    m_Watcher.Start();
}


HRESULT MidiBleEndpointManager::Cleanup()
{
    std::cout << "MidiBleEndpointManager::Cleanup" << std::endl;

    // terminate background thread
    m_Watcher.Stop();

//    if (m_watcherThread.joinable()) m_watcherThread.join();

    return S_OK;
}

_Use_decl_annotations_
MidiBleDevice* MidiBleEndpointManager::GetDevice(std::wstring endpointDeviceInterfaceId)
{
    // todo: clean the id
    if (auto deviceIter = m_devices.find(endpointDeviceInterfaceId); deviceIter != m_devices.end())
    {
        return &(deviceIter->second);
    }

    return nullptr;
}








_Use_decl_annotations_
void MidiBleEndpointManager::OnDeviceAdded(enumeration::DeviceWatcher, enumeration::DeviceInformation const& info)
{
    std::hash<std::wstring> hasher;
    std::wstring hash;

    std::cout << "Added: " << winrt::to_string(info.Id()) << std::endl;
    std::cout << "Name: " << winrt::to_string(info.Name()) << std::endl;

    // get the BLE device from this

    auto bleDevice = BluetoothLEDevice::FromIdAsync(info.Id()).get();

    if (bleDevice != nullptr)
    {
        std::cout << "BLE Name: " << winrt::to_string(bleDevice.Name()) << std::endl;

        if (bleDevice.ConnectionStatus() == BluetoothConnectionStatus::Connected)
        {
            std::cout << "Current status: Connected " << std::endl;
        }
        else
        {
            std::cout << "Current status: Disconnected " << std::endl;
        }

        auto midiService = GetBleMidiServiceFromDevice(bleDevice);

        if (midiService != nullptr)
        {
            auto characteristic = GetBleMidiDataIOCharacteristicFromService(midiService);

            if (characteristic != nullptr)
            {
                auto properties = characteristic.CharacteristicProperties();

                bool supportsMidiIn{ false };
                bool supportsMidiOut{ false };
                bool supportsNotify{ false };

                if ((properties & GattCharacteristicProperties::Read) == GattCharacteristicProperties::Read)
                {
                    // includes input port
                    std::cout << "- Supports READ (input port)" << std::endl;

                    supportsMidiIn = true;
                }

                if ((properties & GattCharacteristicProperties::Write) == GattCharacteristicProperties::Write)
                {
                    // includes output port
                    std::cout << "- Supports WRITE (output port)" << std::endl;

                    supportsMidiOut = true;
                }

                if ((properties & GattCharacteristicProperties::Notify) == GattCharacteristicProperties::Notify)
                {
                    std::cout << "- Supports NOTIFY" << std::endl;

                    supportsNotify = true;
                }

                // lock the map and create and store the device

                MidiBleDevice endpointDevice;

                // we'll create a real id in the actual implementation
                    // todo: clean the id


                hash = std::to_wstring(hasher(std::wstring{ bleDevice.DeviceId() }));

                std::wstring endpointDeviceId = std::wstring{ L"\\\\?\\SWD&MIDIU_BLE_" } + hash;

                endpointDevice.Initialize(
                    endpointDeviceId,
                    bleDevice.DeviceId().c_str(),
                    supportsMidiOut,
                    supportsMidiIn,
                    supportsNotify);

                m_devices.emplace(endpointDeviceId, endpointDevice);

                std::wcout << L"Added device: " << endpointDeviceId << std::endl;
            }
        }

    }

    std::cout << "------------------------" << std::endl;
}

_Use_decl_annotations_
void MidiBleEndpointManager::OnDeviceRemoved(enumeration::DeviceWatcher, enumeration::DeviceInformationUpdate const& upd)
{
    std::cout << "Removed: " << winrt::to_string(upd.Id()) << std::endl;
}

_Use_decl_annotations_
void MidiBleEndpointManager::OnDeviceUpdated(enumeration::DeviceWatcher, enumeration::DeviceInformationUpdate const& upd)
{
    std::cout << "-----------" << std::endl;

    std::cout << "Updated: " << winrt::to_string(upd.Id()).c_str() << std::endl;

    auto bleDevice = BluetoothLEDevice::FromIdAsync(upd.Id()).get();

    if (bleDevice != nullptr)
    {
        std::cout << "Name: " << winrt::to_string(bleDevice.Name()) << " : ";

        if (bleDevice.ConnectionStatus() == BluetoothConnectionStatus::Connected)
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

        if (upd.Id() == bleDevice.DeviceId())
        {
            if (prop.Key() == L"System.DeviceInterface.Bluetooth.Flags")
            {
                // TODO: get the property value and decipher


            }
        }

    }

    std::cout << "-----------" << std::endl;
}

_Use_decl_annotations_
void MidiBleEndpointManager::OnDeviceWatcherStopped(enumeration::DeviceWatcher, foundation::IInspectable)
{
    std::cout << "Device watcher stopped" << std::endl;
}

_Use_decl_annotations_
void MidiBleEndpointManager::OnEnumerationCompleted(enumeration::DeviceWatcher, foundation::IInspectable)
{
    std::cout << "Enumeration completed" << std::endl;

}