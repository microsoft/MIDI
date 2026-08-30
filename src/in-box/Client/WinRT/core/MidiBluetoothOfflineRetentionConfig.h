// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Bluetooth.MidiBluetoothOfflineRetentionConfig.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    struct MidiBluetoothOfflineRetentionConfig : MidiBluetoothOfflineRetentionConfigT<MidiBluetoothOfflineRetentionConfig>
    {
        MidiBluetoothOfflineRetentionConfig() = default;
        MidiBluetoothOfflineRetentionConfig(_In_ int32_t const retentionSeconds);
        MidiBluetoothOfflineRetentionConfig(_In_ winrt::hstring const& bluetoothDeviceId, _In_ int32_t const retentionSeconds);

        winrt::guid TransportId() const noexcept { return bluetooth::MidiBluetoothTransportManager::TransportId(); }

        json::JsonObject ConfigJson();

    private:
        winrt::hstring m_bluetoothDeviceId{};
        int32_t m_retentionSeconds{ static_cast<int32_t>(bluetooth::MidiBluetoothOfflineRetention::KeepAlways) };
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::factory_implementation
{
    struct MidiBluetoothOfflineRetentionConfig : MidiBluetoothOfflineRetentionConfigT<MidiBluetoothOfflineRetentionConfig, implementation::MidiBluetoothOfflineRetentionConfig>
    {
    };
}
