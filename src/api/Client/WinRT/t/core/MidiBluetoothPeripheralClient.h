// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Bluetooth.MidiBluetoothPeripheralClient.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    struct MidiBluetoothPeripheralClient : MidiBluetoothPeripheralClientT<MidiBluetoothPeripheralClient>
    {
        MidiBluetoothPeripheralClient() = default;

        winrt::hstring Name() const noexcept { return m_name; }
        bool HasGenericName() const noexcept { return m_hasGenericName; }
        uint64_t BluetoothAddress() const noexcept { return m_bluetoothAddress; }
        bluetooth::MidiBluetoothAddressType BluetoothAddressType() const noexcept { return m_bluetoothAddressType; }
        bool IsPaired() const noexcept { return m_isPaired; }
        winrt::hstring WindowsDeviceId() const noexcept { return m_windowsDeviceId; }
        foundation::TimeSpan ConnectionInterval() const noexcept { return m_connectionInterval; }

        void InternalInitializeFromJson(_In_ json::JsonObject const& peripheralJson) noexcept;

    private:
        winrt::hstring m_name{};
        bool m_hasGenericName{ false };
        uint64_t m_bluetoothAddress{ 0 };
        bluetooth::MidiBluetoothAddressType m_bluetoothAddressType{ bluetooth::MidiBluetoothAddressType::Unknown };
        bool m_isPaired{ false };
        winrt::hstring m_windowsDeviceId{};
        foundation::TimeSpan m_connectionInterval{};
    };
}
