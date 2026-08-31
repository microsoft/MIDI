// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Bluetooth.MidiBluetoothDeviceDisconnectResponse.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    struct MidiBluetoothDeviceDisconnectResponse : MidiBluetoothDeviceDisconnectResponseT<MidiBluetoothDeviceDisconnectResponse>
    {
        MidiBluetoothDeviceDisconnectResponse() = default;

        bool Success() const noexcept { return m_success; }
        bluetooth::MidiBluetoothDeviceDisconnectErrorCode ErrorCode() const noexcept { return m_errorCode; }
        winrt::hstring ErrorMessage() const noexcept { return m_errorMessage; }
        int32_t ErrorHResult() const noexcept { return m_errorHResult; }

        void InternalInitialize(
            _In_ bool const success,
            _In_ bluetooth::MidiBluetoothDeviceDisconnectErrorCode const errorCode,
            _In_ winrt::hstring const& errorMessage,
            _In_ int32_t const errorHResult) noexcept;

    private:
        bool m_success{ false };
        bluetooth::MidiBluetoothDeviceDisconnectErrorCode m_errorCode{ bluetooth::MidiBluetoothDeviceDisconnectErrorCode::Unexpected };
        winrt::hstring m_errorMessage{};
        int32_t m_errorHResult{ 0 };
    };
}
