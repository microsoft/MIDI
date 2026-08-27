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

        // Midi2.BluetoothMidiTransport.dll, registered as Midi2BluetoothMidiTransport
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

        static collections::IVectorView<bluetooth::MidiBluetoothPeripheralClient> GetPendingPeripheralClients() noexcept;

        static foundation::IAsyncOperation<bluetooth::MidiBluetoothPeripheralClientDecisionResponse> ApprovePeripheralClientAsync(
            _In_ winrt::hstring bluetoothAddress,
            _In_ bluetooth::MidiBluetoothApprovalScope scope) noexcept;

        static foundation::IAsyncOperation<bluetooth::MidiBluetoothPeripheralClientDecisionResponse> DenyPeripheralClientAsync(
            _In_ winrt::hstring bluetoothAddress,
            _In_ bluetooth::MidiBluetoothApprovalScope scope) noexcept;

        static foundation::IAsyncOperation<bluetooth::MidiBluetoothPeripheralClientDecisionResponse> ForgetPeripheralClientAsync(
            _In_ winrt::hstring bluetoothAddress) noexcept;

        static bluetooth::MidiBluetoothRadioInformation GetRadioInformation() noexcept;

    private:
        static bluetooth::MidiBluetoothPeripheralClientDecisionResponse SendClientDecision(
            _In_ std::wstring const& verb,
            _In_ winrt::hstring const& bluetoothAddress,
            _In_ bluetooth::MidiBluetoothApprovalScope const scope) noexcept;
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::factory_implementation
{
    struct MidiBluetoothTransportManager : MidiBluetoothTransportManagerT<MidiBluetoothTransportManager, implementation::MidiBluetoothTransportManager>
    {
    };
}
