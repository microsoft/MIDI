// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Bluetooth.MidiBluetoothDeviceConnectResponse.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    struct MidiBluetoothDeviceConnectResponse : MidiBluetoothDeviceConnectResponseT<MidiBluetoothDeviceConnectResponse>
    {
        MidiBluetoothDeviceConnectResponse() = default;

        bool Success() const noexcept { return m_success; }
        bluetooth::MidiBluetoothDeviceConnectErrorCode ErrorCode() const noexcept { return m_errorCode; }
        winrt::hstring ErrorMessage() const noexcept { return m_errorMessage; }
        int32_t ErrorHResult() const noexcept { return m_errorHResult; }
        bool IsKnown() const noexcept { return m_isKnown; }
        bluetooth::MidiBluetoothDeviceInformation Device() const noexcept { return m_device; }

        void InternalInitialize(
            _In_ bool const success,
            _In_ bluetooth::MidiBluetoothDeviceConnectErrorCode const errorCode,
            _In_ winrt::hstring const& errorMessage,
            _In_ int32_t const errorHResult,
            _In_ bool const isKnown,
            _In_ bluetooth::MidiBluetoothDeviceInformation const& device) noexcept;

    private:
        bool m_success{ false };
        bluetooth::MidiBluetoothDeviceConnectErrorCode m_errorCode{ bluetooth::MidiBluetoothDeviceConnectErrorCode::Unexpected };
        winrt::hstring m_errorMessage{};
        int32_t m_errorHResult{ 0 };
        bool m_isKnown{ false };
        bluetooth::MidiBluetoothDeviceInformation m_device{ nullptr };
    };
}
