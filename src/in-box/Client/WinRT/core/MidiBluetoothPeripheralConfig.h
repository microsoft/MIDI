// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Bluetooth.MidiBluetoothPeripheralConfig.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    struct MidiBluetoothPeripheralConfig : MidiBluetoothPeripheralConfigT<MidiBluetoothPeripheralConfig>
    {
        MidiBluetoothPeripheralConfig() = default;
        MidiBluetoothPeripheralConfig(_In_ bluetooth::MidiBluetoothProtocol const protocol);

        winrt::guid TransportId() const noexcept { return bluetooth::MidiBluetoothTransportManager::TransportId(); }

        json::JsonObject ConfigJson();

        bluetooth::MidiBluetoothProtocol Protocol() const noexcept { return m_protocol; }
        void Protocol(_In_ bluetooth::MidiBluetoothProtocol const value) noexcept { m_protocol = value; }

        bool IsEnabled() const noexcept { return m_isEnabled; }
        void IsEnabled(_In_ bool const value) noexcept { m_isEnabled = value; }

    private:
        bluetooth::MidiBluetoothProtocol m_protocol{ bluetooth::MidiBluetoothProtocol::BluetoothLowEnergyMidi1 };
        bool m_isEnabled{ true };
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::factory_implementation
{
    struct MidiBluetoothPeripheralConfig : MidiBluetoothPeripheralConfigT<MidiBluetoothPeripheralConfig, implementation::MidiBluetoothPeripheralConfig>
    {
    };
}
