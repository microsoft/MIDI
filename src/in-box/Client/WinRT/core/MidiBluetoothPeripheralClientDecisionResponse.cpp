#include "pch.h"
#include "MidiBluetoothPeripheralClientDecisionResponse.h"
#include "Transports.Bluetooth.MidiBluetoothPeripheralClientDecisionResponse.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    _Use_decl_annotations_
    void MidiBluetoothPeripheralClientDecisionResponse::InternalInitialize(
        bool const success,
        bluetooth::MidiBluetoothPeripheralErrorCode const errorCode,
        winrt::hstring const& errorMessage,
        bluetooth::MidiBluetoothApprovalScope const appliedScope,
        winrt::hstring const& bluetoothAddress,
        winrt::hstring const& name,
        bool const persistRequired) noexcept
    {
        m_success = success;
        m_errorCode = errorCode;
        m_errorMessage = errorMessage;
        m_appliedScope = appliedScope;
        m_bluetoothAddress = bluetoothAddress;
        m_name = name;
        m_persistRequired = persistRequired;
    }
}
