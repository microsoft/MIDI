// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

//#include "MidiVirtualDevice.h"

// thread-safe meyers singleton for storing the devices we'll use


struct MidiBluetoothEndpointEntry
{
    MidiBluetoothDeviceDefinition Definition{};

    bt::BluetoothLEDevice Device{ nullptr };
    gatt::GattDeviceService Service{ nullptr };

    wil::com_ptr_nothrow<IMidiBiDi> MidiDeviceBiDi{ nullptr };

    //winrt::event_token ConnectionStatusChangedRevoker;

    //bt::BluetoothLEDevice::ConnectionParametersChanged_revoker ConnectionStatusChangedRevoker{ nullptr };
    //winrt::impl::consume_Windows_Devices_Bluetooth_IBluetoothLEDevice<bt::IBluetoothLEDevice>::ConnectionStatusChanged_revoker ConnectionStatusChangedRevoker{ nullptr };
//    winrt::impl::consume_Windows_Devices_Bluetooth_IBluetoothLEDevice<bt::IBluetoothLEDevice>::ConnectionStatusChanged_revoker ConnectionStatusChangedRevoker;

    winrt::event_token ConnectionStatusChangedRevoker;

    MidiBluetoothEndpointEntry() = default;
    ~MidiBluetoothEndpointEntry()
    {
        if (MidiDeviceBiDi)
        {
            MidiDeviceBiDi->Cleanup();
            MidiDeviceBiDi.reset();
        }

        if (Device != nullptr)
        {
            Device.ConnectionStatusChanged(ConnectionStatusChangedRevoker);
        }
    }
};


class MidiEndpointTable
{
public:
    static MidiEndpointTable& Current();

    // no copying
    MidiEndpointTable(_In_ const MidiEndpointTable&) = delete;
    MidiEndpointTable& operator=(_In_ const MidiEndpointTable&) = delete;

//    wil::com_ptr_nothrow<IMidiBiDi> GetEndpointInterfaceForId(_In_ std::wstring const EndpointDeviceId) const noexcept;

    std::shared_ptr<MidiBluetoothEndpointEntry> GetEndpointEntryForBluetoothAddress(_In_ uint64_t const bluetoothAddress) const noexcept;

    std::shared_ptr<MidiBluetoothEndpointEntry> CreateAndAddNewEndpointEntry(_In_ MidiBluetoothDeviceDefinition definition, _In_  bt::BluetoothLEDevice device, _In_ gatt::GattDeviceService service) noexcept;
   
    void RemoveEndpointEntry(_In_ uint64_t bluetoothAddress) noexcept;

private:
    MidiEndpointTable();
    ~MidiEndpointTable();

    std::map<uint64_t, std::shared_ptr<MidiBluetoothEndpointEntry>> m_endpoints;
    //std::vector<MidiBluetoothEndpointEntry> m_endpoints;
};