// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Bluetooth.MidiBluetoothConfiguredDevice.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    struct MidiBluetoothConfiguredDevice : MidiBluetoothConfiguredDeviceT<MidiBluetoothConfiguredDevice>
    {
        MidiBluetoothConfiguredDevice() = default;

        winrt::hstring BluetoothDeviceId() const noexcept { return m_bluetoothDeviceId; }
        winrt::hstring Comment() const noexcept { return m_comment; }
        bool IsEnabled() const noexcept { return m_isEnabled; }

        void InternalInitialize(
            _In_ winrt::hstring const& bluetoothDeviceId,
            _In_ winrt::hstring const& comment,
            _In_ bool const isEnabled) noexcept
        {
            m_bluetoothDeviceId = bluetoothDeviceId;
            m_comment = comment;
            m_isEnabled = isEnabled;
        }

    private:
        winrt::hstring m_bluetoothDeviceId{};
        winrt::hstring m_comment{};
        bool m_isEnabled{ false };
    };
}
