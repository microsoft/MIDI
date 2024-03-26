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
            auto device = BluetoothLEDevice::FromBluetoothAddressAsync(args.BluetoothAddress()).get();

            if (device != nullptr)
            {
                auto service = GetBleMidiServiceFromDevice(device);

                if (service != nullptr)
                {
                    if (foundMidiDevices.find(args.BluetoothAddress()) == foundMidiDevices.end())
                    {
                        foundMidiDevices[args.BluetoothAddress()] = device.Name();

                        std::cout
                            << "MIDI Device Found" << std::endl
                            << " - Name:                    " << winrt::to_string(device.Name()) << std::endl
                            << " - Timestamp:               " << args.Timestamp().time_since_epoch().count() << std::endl
                            << " - Address:                 " << args.BluetoothAddress() << std::endl
                            << " - IsConnectable:           " << args.IsConnectable() << std::endl
                            << " - IsAnonymous:             " << args.IsAnonymous() << std::endl
                            << " - IsDirected:              " << args.IsDirected() << std::endl
                            << " - IsScannable:             " << args.IsScannable() << std::endl
                            << " - RawSignalStrengthInDBm:  " << args.RawSignalStrengthInDBm() << std::endl;

                        if (device.ConnectionStatus() == BluetoothConnectionStatus::Connected)
                        {
                            std::cout << " - Connection status:       Connected" << std::endl;
                        }
                        else
                        {
                            std::cout << " - Connection status:       Disconnected" << std::endl;
                        }
                    }
                }
                //else
                //{
                //    std::cout << "Not a BLE MIDI Device" << std::endl;
                //}
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




GattServiceProvider m_provider{ nullptr };
winrt::event_token Revoke_AdvertisementStatusChanged;
winrt::event_token Revoke_GattReadRequest;
winrt::event_token Revoke_GattWriteRequest;
GattLocalCharacteristic m_dataIOCharacteristic{ nullptr };


void OnDataIOReadRequested(GattLocalCharacteristic const& source, GattReadRequestedEventArgs const& args)
{
    //std::cout << __FUNCTION__ << std::endl;

    auto request = args.GetRequestAsync().get();

    args.Session().MaintainConnection(true);

    std::cout << "Read Request: " << winrt::to_string(args.Session().DeviceId().Id()) 
        << ", length=" << request.Length()
        << std::endl;

    // should respond with zero only the first time
    streams::DataWriter writer;
    writer.WriteByte(0);

    request.RespondWithValue(writer.DetachBuffer());
}

void ProcessIncomingBuffer(streams::IBuffer buffer)
{
    auto reader = streams::DataReader::FromBuffer(buffer);

    reader.ByteOrder(streams::ByteOrder::LittleEndian);

    // TODO: Process the data. We'll just display for now

    // Read header bytes
    auto headerByte = reader.ReadByte();

    if (headerByte & 0x80)
    {
        // valid header byte. Get the rest of the info
        auto timestampByte = reader.ReadByte();

        if (timestampByte & 0x80)
        {
            // timestamp high is bits 5-0 of header (6 bits)
            uint16_t timestampHigh = headerByte & 0x3F;

            // timestamp low is bites 6-0 of timestamp byte (7 bits)
            uint16_t timestampLow = timestampByte & 0x7F;

            uint16_t fullTimestamp = timestampLow | (timestampHigh << 7);

            std::cout
                << "TS:"
                << std::setw(5) << std::setfill('0')
                << std::dec
                << fullTimestamp 
                << std::nouppercase
                << " Data: ";

            // now read the message data
            // In the real impl, this needs to account for
            // timestamp (ts low) embedded in here between
            // messages, as well as things like running
            // status messages. This is just quick and dirty.
            while (reader.UnconsumedBufferLength() > 0)
            {
                std::cout 
                    << std::hex << std::setw(2) << std::setfill('0') << std::noshowbase 
                    << std::uppercase
                    << (unsigned)reader.ReadByte()
                    << std::nouppercase
                    << " ";
            }

            std::cout << std::endl;
        }
        else
        {
            std::cout << "Invalid timestamp byte" << std::endl;
        }
    }
    else
    {
        std::cout << "Invalid data" << std::endl;
    }
}


void OnDataIOWriteRequested(GattLocalCharacteristic const& source, GattWriteRequestedEventArgs const& args)
{
    //std::cout << __FUNCTION__ << std::endl;

    auto request = args.GetRequestAsync().get();

    //std::cout << "Write Request: " << winrt::to_string(args.Session().DeviceId().Id())
    //    << ", offset=" << request.Offset()
    //    << std::endl;

    ProcessIncomingBuffer(request.Value());

}

void OnDataIOSubscribedClientsChanged(GattLocalCharacteristic const& source, foundation::IInspectable const& args)
{
    std::cout << __FUNCTION__ << std::endl;

}


void OnGattServiceProviderAdvertisementStatusChanged(GattServiceProvider const& source, GattServiceProviderAdvertisementStatusChangedEventArgs const& args)
{
//    std::cout << __FUNCTION__ << std::endl;

    std::cout << "Provider ad status: ";

    switch (args.Status())
    {
    case GattServiceProviderAdvertisementStatus::StartedWithoutAllAdvertisementData:
        std::cout << "Started without all ad data";
        break;
    case GattServiceProviderAdvertisementStatus::Started:
        std::cout << "Started";
        break;
    case GattServiceProviderAdvertisementStatus::Created:
        std::cout << "Created";
        break;
    case GattServiceProviderAdvertisementStatus::Stopped:
        std::cout << "Stopped";
        break;
    }

    std::cout << std::endl;

}

void StartAsPeripheral()
{
    winrt::guid MIDI_BLE_SERVICE_UUID{ MIDI_BLE_SERVICE };
    winrt::guid MIDI_BLE_DATA_IO_CHARACTERISTIC_UUID{ MIDI_BLE_DATA_IO_CHARACTERISTIC };

    auto result = GattServiceProvider::CreateAsync(MIDI_BLE_SERVICE_UUID).get();

    if (result.Error() == BluetoothError::Success)
    {
        m_provider = result.ServiceProvider();

        Revoke_AdvertisementStatusChanged = m_provider.AdvertisementStatusChanged(OnGattServiceProviderAdvertisementStatusChanged);

        GattServiceProviderAdvertisingParameters params;
        params.IsConnectable(true);
        params.IsDiscoverable(true);

        // BLE MIDI 1.0 requires WriteWithoutResponse
        GattCharacteristicProperties dataIOProperties = 
            GattCharacteristicProperties::WriteWithoutResponse | 
            GattCharacteristicProperties::Read | 
            GattCharacteristicProperties::Notify;

        // BLE MIDI 1.0 supports encryption, and recommends it, but it isn't required. 
        // We'll have this as a user setting later so the user can decide if, when the
        // PC is acting as a peripheral, encryption and/or auth is required.
        GattLocalCharacteristicParameters dataIOParameters;
        dataIOParameters.ReadProtectionLevel(GattProtectionLevel::Plain);   
        dataIOParameters.WriteProtectionLevel(GattProtectionLevel::Plain);  // same
        dataIOParameters.UserDescription(L"MIDI Prototype");
        dataIOParameters.CharacteristicProperties(dataIOProperties);

        auto characteristicResult = m_provider.Service().CreateCharacteristicAsync(
            MIDI_BLE_DATA_IO_CHARACTERISTIC_UUID, 
            dataIOParameters
        ).get();

        if (characteristicResult.Error() == BluetoothError::Success)
        {
            m_dataIOCharacteristic = characteristicResult.Characteristic();

            Revoke_GattReadRequest = m_dataIOCharacteristic.ReadRequested(OnDataIOReadRequested);
            Revoke_GattWriteRequest = m_dataIOCharacteristic.WriteRequested(OnDataIOWriteRequested);

            m_dataIOCharacteristic.SubscribedClientsChanged(OnDataIOSubscribedClientsChanged);

            m_dataIOCharacteristic.StaticValue();

            std::cout << "Start Advertising..." << std::endl;
            m_provider.StartAdvertising(params);
        }
        else
        {
            std::cout << "Failed to create characteristic" << std::endl;
        }
    }
}

void StopPeripheral()
{
    std::cout << "Stopping peripheral..." << std::endl;

    if (m_provider != nullptr)
    {
        m_provider.StopAdvertising();

        // clean up event handler
        m_provider.AdvertisementStatusChanged(Revoke_AdvertisementStatusChanged);

        m_dataIOCharacteristic.ReadRequested(Revoke_GattReadRequest);
        m_dataIOCharacteristic.WriteRequested(Revoke_GattWriteRequest);


        m_provider == nullptr;
    }
}


int main()
{
    init_apartment();


    TestFindingDevices();

    //ReportAdapterCapabilities();

    //StartAsPeripheral();

    //system("pause");

    //StopPeripheral();

    system("pause");
}

