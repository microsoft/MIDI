// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Bluetooth.MidiBluetoothPeripheralResponse.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    struct MidiBluetoothPeripheralResponse : MidiBluetoothPeripheralResponseT<MidiBluetoothPeripheralResponse>
    {
        MidiBluetoothPeripheralResponse() = default;

        bool Success() const noexcept { return m_success; }
        bluetooth::MidiBluetoothPeripheralErrorCode ErrorCode() const noexcept { return m_errorCode; }
        winrt::hstring ErrorMessage() const noexcept { return m_errorMessage; }
        int32_t ErrorHResult() const noexcept { return m_errorHResult; }
        bluetooth::MidiBluetoothPeripheralStatus Status() const noexcept { return m_status; }

        void InternalInitialize(
            _In_ bool const success,
            _In_ bluetooth::MidiBluetoothPeripheralErrorCode const errorCode,
            _In_ winrt::hstring const& errorMessage,
            _In_ int32_t const errorHResult,
            _In_ bluetooth::MidiBluetoothPeripheralStatus const& status) noexcept;

    private:
        bool m_success{ false };
        bluetooth::MidiBluetoothPeripheralErrorCode m_errorCode{ bluetooth::MidiBluetoothPeripheralErrorCode::Unexpected };
        winrt::hstring m_errorMessage{};
        int32_t m_errorHResult{ 0 };
        bluetooth::MidiBluetoothPeripheralStatus m_status{ nullptr };
    };
}
