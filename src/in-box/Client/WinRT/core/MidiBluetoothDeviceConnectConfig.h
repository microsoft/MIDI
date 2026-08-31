// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Bluetooth.MidiBluetoothDeviceConnectConfig.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    struct MidiBluetoothDeviceConnectConfig : MidiBluetoothDeviceConnectConfigT<MidiBluetoothDeviceConnectConfig>
    {
        MidiBluetoothDeviceConnectConfig() = default;
        MidiBluetoothDeviceConnectConfig(_In_ winrt::hstring const& bluetoothDeviceId);

        winrt::guid TransportId() const noexcept { return bluetooth::MidiBluetoothTransportManager::TransportId(); }

        json::JsonObject ConfigJson();

        winrt::hstring BluetoothDeviceId() const noexcept { return m_bluetoothDeviceId; }
        void BluetoothDeviceId(_In_ winrt::hstring const& value) noexcept { m_bluetoothDeviceId = value; }

        winrt::hstring Comment() const noexcept { return m_comment; }
        void Comment(_In_ winrt::hstring const& value) noexcept { m_comment = value; }

    private:
        winrt::hstring m_bluetoothDeviceId{};
        winrt::hstring m_comment{};
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::factory_implementation
{
    struct MidiBluetoothDeviceConnectConfig : MidiBluetoothDeviceConnectConfigT<MidiBluetoothDeviceConnectConfig, implementation::MidiBluetoothDeviceConnectConfig>
    {
    };
}
