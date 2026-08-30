// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Bluetooth.MidiBluetoothPeripheralClientListConfig.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    struct MidiBluetoothPeripheralClientListConfig : MidiBluetoothPeripheralClientListConfigT<MidiBluetoothPeripheralClientListConfig>
    {
        MidiBluetoothPeripheralClientListConfig() = default;
        MidiBluetoothPeripheralClientListConfig(_In_ bluetooth::MidiBluetoothPeripheralStatus const& currentStatus);

        winrt::guid TransportId() const noexcept { return bluetooth::MidiBluetoothTransportManager::TransportId(); }

        json::JsonObject ConfigJson();

    private:
        collections::IVectorView<bluetooth::MidiBluetoothRememberedClient> m_allowed{ nullptr };
        collections::IVectorView<bluetooth::MidiBluetoothRememberedClient> m_denied{ nullptr };
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::factory_implementation
{
    struct MidiBluetoothPeripheralClientListConfig : MidiBluetoothPeripheralClientListConfigT<MidiBluetoothPeripheralClientListConfig, implementation::MidiBluetoothPeripheralClientListConfig>
    {
    };
}
