// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Bluetooth.MidiBluetoothDeviceDisconnectConfig.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    struct MidiBluetoothDeviceDisconnectConfig : MidiBluetoothDeviceDisconnectConfigT<MidiBluetoothDeviceDisconnectConfig>
    {
        MidiBluetoothDeviceDisconnectConfig() = default;
        MidiBluetoothDeviceDisconnectConfig(_In_ winrt::hstring const& bluetoothDeviceId);
        MidiBluetoothDeviceDisconnectConfig(_In_ winrt::hstring const& bluetoothDeviceId, _In_ bool const removeFromConfiguration);

        winrt::guid TransportId() const noexcept { return bluetooth::MidiBluetoothTransportManager::TransportId(); }

        json::JsonObject ConfigJson();

        winrt::hstring BluetoothDeviceId() const noexcept { return m_bluetoothDeviceId; }
        void BluetoothDeviceId(_In_ winrt::hstring const& value) noexcept { m_bluetoothDeviceId = value; }

        bool RemoveFromConfiguration() const noexcept { return m_removeFromConfiguration; }
        void RemoveFromConfiguration(_In_ bool const value) noexcept { m_removeFromConfiguration = value; }

    private:
        winrt::hstring m_bluetoothDeviceId{};
        bool m_removeFromConfiguration{ false };
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::factory_implementation
{
    struct MidiBluetoothDeviceDisconnectConfig : MidiBluetoothDeviceDisconnectConfigT<MidiBluetoothDeviceDisconnectConfig, implementation::MidiBluetoothDeviceDisconnectConfig>
    {
    };
}
