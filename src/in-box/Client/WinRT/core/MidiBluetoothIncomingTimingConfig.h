// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Bluetooth.MidiBluetoothIncomingTimingConfig.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    struct MidiBluetoothIncomingTimingConfig : MidiBluetoothIncomingTimingConfigT<MidiBluetoothIncomingTimingConfig>
    {
        MidiBluetoothIncomingTimingConfig() = default;
        MidiBluetoothIncomingTimingConfig(_In_ bluetooth::MidiBluetoothIncomingTiming const incomingTiming);
        MidiBluetoothIncomingTimingConfig(_In_ winrt::hstring const& bluetoothDeviceId, _In_ bluetooth::MidiBluetoothIncomingTiming const incomingTiming);

        winrt::guid TransportId() const noexcept { return bluetooth::MidiBluetoothTransportManager::TransportId(); }

        json::JsonObject ConfigJson();

    private:
        winrt::hstring m_bluetoothDeviceId{};
        bluetooth::MidiBluetoothIncomingTiming m_incomingTiming{ bluetooth::MidiBluetoothIncomingTiming::Smooth };
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::factory_implementation
{
    struct MidiBluetoothIncomingTimingConfig : MidiBluetoothIncomingTimingConfigT<MidiBluetoothIncomingTimingConfig, implementation::MidiBluetoothIncomingTimingConfig>
    {
    };
}