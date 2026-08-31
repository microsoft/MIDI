// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Bluetooth.MidiBluetoothPeripheralClientDecisionResponse.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    struct MidiBluetoothPeripheralClientDecisionResponse : MidiBluetoothPeripheralClientDecisionResponseT<MidiBluetoothPeripheralClientDecisionResponse>
    {
        MidiBluetoothPeripheralClientDecisionResponse() = default;

        bool Success() const noexcept { return m_success; }
        bluetooth::MidiBluetoothPeripheralErrorCode ErrorCode() const noexcept { return m_errorCode; }
        winrt::hstring ErrorMessage() const noexcept { return m_errorMessage; }
        bluetooth::MidiBluetoothApprovalScope AppliedScope() const noexcept { return m_appliedScope; }
        winrt::hstring BluetoothAddress() const noexcept { return m_bluetoothAddress; }
        winrt::hstring Name() const noexcept { return m_name; }
        bool PersistRequired() const noexcept { return m_persistRequired; }

        void InternalInitialize(
            _In_ bool const success,
            _In_ bluetooth::MidiBluetoothPeripheralErrorCode const errorCode,
            _In_ winrt::hstring const& errorMessage,
            _In_ bluetooth::MidiBluetoothApprovalScope const appliedScope,
            _In_ winrt::hstring const& bluetoothAddress,
            _In_ winrt::hstring const& name,
            _In_ bool const persistRequired) noexcept;

    private:
        bool m_success{ false };
        bluetooth::MidiBluetoothPeripheralErrorCode m_errorCode{ bluetooth::MidiBluetoothPeripheralErrorCode::Unexpected };
        winrt::hstring m_errorMessage{};
        bluetooth::MidiBluetoothApprovalScope m_appliedScope{ bluetooth::MidiBluetoothApprovalScope::Once };
        winrt::hstring m_bluetoothAddress{};
        winrt::hstring m_name{};
        bool m_persistRequired{ false };
    };
}
