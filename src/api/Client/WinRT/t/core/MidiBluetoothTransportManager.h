// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Bluetooth.MidiBluetoothTransportManager.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    struct MidiBluetoothTransportManager
    {
        MidiBluetoothTransportManager() = default;

        static bool IsTransportAvailable() noexcept;

        // Midi2.Ble2MidiTransport.dll, registered as Midi2BluetoothMidiTransport
        static const winrt::guid TransportId() noexcept { return internal::StringToGuid(L"{5dc87270-f318-4838-a4f9-6aadc63e925f}"); }

        static collections::IVectorView<bluetooth::MidiBluetoothDeviceInformation> GetAvailableDevices() noexcept;

        static bluetooth::MidiBluetoothDeviceInformation GetDevice(_In_ winrt::hstring const& bluetoothDeviceId) noexcept;

        static foundation::IAsyncOperation<bluetooth::MidiBluetoothDeviceConnectResponse> ConnectDeviceAsync(
            _In_ bluetooth::MidiBluetoothDeviceConnectConfig connectConfig) noexcept;

        static foundation::IAsyncOperation<bluetooth::MidiBluetoothDeviceDisconnectResponse> DisconnectDeviceAsync(
            _In_ bluetooth::MidiBluetoothDeviceDisconnectConfig disconnectConfig) noexcept;

        static foundation::IAsyncOperation<bluetooth::MidiBluetoothPeripheralResponse> StartPeripheralAsync(
            _In_ bluetooth::MidiBluetoothPeripheralConfig peripheralConfig) noexcept;

        static foundation::IAsyncOperation<bluetooth::MidiBluetoothPeripheralResponse> StopPeripheralAsync() noexcept;

        static bluetooth::MidiBluetoothPeripheralStatus GetPeripheralStatus() noexcept;

        static bluetooth::MidiBluetoothRadioInformation GetRadioInformation() noexcept;
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::factory_implementation
{
    struct MidiBluetoothTransportManager : MidiBluetoothTransportManagerT<MidiBluetoothTransportManager, implementation::MidiBluetoothTransportManager>
    {
    };
}
