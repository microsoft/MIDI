// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Bluetooth.MidiBluetoothRememberedClient.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    struct MidiBluetoothRememberedClient : MidiBluetoothRememberedClientT<MidiBluetoothRememberedClient>
    {
        MidiBluetoothRememberedClient() = default;

        winrt::hstring BluetoothAddress() const noexcept { return m_bluetoothAddress; }
        winrt::hstring Name() const noexcept { return m_name; }

        void InternalInitialize(
            _In_ winrt::hstring const& bluetoothAddress,
            _In_ winrt::hstring const& name) noexcept
        {
            m_bluetoothAddress = bluetoothAddress;
            m_name = name;
        }

    private:
        winrt::hstring m_bluetoothAddress{};
        winrt::hstring m_name{};
    };
}
