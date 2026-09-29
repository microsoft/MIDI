// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Bluetooth.MidiBluetoothSavedDevice.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    struct MidiBluetoothSavedDevice : MidiBluetoothSavedDeviceT<MidiBluetoothSavedDevice>
    {
        MidiBluetoothSavedDevice() = default;

        winrt::hstring BluetoothDeviceId() const noexcept { return m_bluetoothDeviceId; }
        winrt::hstring Comment() const noexcept { return m_comment; }
        bool IsEnabled() const noexcept { return m_isEnabled; }
        int32_t OfflineRetentionSeconds() const noexcept { return m_offlineRetentionSeconds; }

        void InternalInitialize(
            _In_ winrt::hstring const& bluetoothDeviceId,
            _In_ winrt::hstring const& comment,
            _In_ bool const isEnabled,
            _In_ int32_t const offlineRetentionSeconds) noexcept
        {
            m_bluetoothDeviceId = bluetoothDeviceId;
            m_comment = comment;
            m_isEnabled = isEnabled;
            m_offlineRetentionSeconds = offlineRetentionSeconds;
        }

    private:
        winrt::hstring m_bluetoothDeviceId{};
        winrt::hstring m_comment{};
        bool m_isEnabled{ false };
        int32_t m_offlineRetentionSeconds{ static_cast<int32_t>(MidiBluetoothOfflineRetention::UseTransportDefault) };
    };
}
