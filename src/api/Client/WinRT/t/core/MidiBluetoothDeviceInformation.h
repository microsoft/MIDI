// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Bluetooth.MidiBluetoothDeviceInformation.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    struct MidiBluetoothDeviceInformation : MidiBluetoothDeviceInformationT<MidiBluetoothDeviceInformation>
    {
        MidiBluetoothDeviceInformation() = default;

        winrt::hstring BluetoothDeviceId() const noexcept { return m_bluetoothDeviceId; }
        uint64_t BluetoothAddress() const noexcept { return m_bluetoothAddress; }
        winrt::hstring Name() const noexcept { return m_name; }

        bluetooth::MidiBluetoothProtocol SelectedProtocol() const noexcept { return m_selectedProtocol; }

        bool IsConnected() const noexcept { return m_isConnected; }
        bool IsPaired() const noexcept { return m_isPaired; }
        bool IsPresent() const noexcept { return m_isPresent; }

        int16_t SignalStrengthDecibelMilliwatts() const noexcept { return m_signalStrengthDecibelMilliwatts; }
        foundation::TimeSpan LastSeenAgo() const noexcept { return m_lastSeenAgo; }

        bool HasEndpoint() const noexcept { return m_hasEndpoint; }
        winrt::hstring EndpointDeviceId() const noexcept { return m_endpointDeviceId; }
        winrt::hstring EndpointDeviceInstanceId() const noexcept { return m_endpointDeviceInstanceId; }

        uint64_t MessagesReceived() const noexcept { return m_messagesReceived; }
        uint64_t MessagesSent() const noexcept { return m_messagesSent; }

        foundation::TimeSpan ConnectionInterval() const noexcept { return m_connectionInterval; }

        winrt::hstring LastConnectError() const noexcept { return m_lastConnectError; }
        bluetooth::MidiBluetoothDeviceConnectErrorCode LastConnectErrorCode() const noexcept { return m_lastConnectErrorCode; }
        int32_t LastConnectErrorHResult() const noexcept { return m_lastConnectErrorHResult; }
        int32_t LastSendErrorHResult() const noexcept { return m_lastSendErrorHResult; }

        void InternalInitializeFromJson(_In_ json::JsonObject const& deviceJson) noexcept;

    private:
        winrt::hstring m_bluetoothDeviceId{};
        uint64_t m_bluetoothAddress{ 0 };
        winrt::hstring m_name{};
        bluetooth::MidiBluetoothProtocol m_selectedProtocol{ bluetooth::MidiBluetoothProtocol::Unknown };
        bool m_isConnected{ false };
        bool m_isPaired{ false };
        bool m_isPresent{ false };
        int16_t m_signalStrengthDecibelMilliwatts{ 0 };
        foundation::TimeSpan m_lastSeenAgo{};
        bool m_hasEndpoint{ false };
        winrt::hstring m_endpointDeviceId{};
        winrt::hstring m_endpointDeviceInstanceId{};
        uint64_t m_messagesReceived{ 0 };
        uint64_t m_messagesSent{ 0 };
        foundation::TimeSpan m_connectionInterval{};
        winrt::hstring m_lastConnectError{};
        bluetooth::MidiBluetoothDeviceConnectErrorCode m_lastConnectErrorCode{ bluetooth::MidiBluetoothDeviceConnectErrorCode::Success };
        int32_t m_lastConnectErrorHResult{ 0 };
        int32_t m_lastSendErrorHResult{ 0 };
    };
}
