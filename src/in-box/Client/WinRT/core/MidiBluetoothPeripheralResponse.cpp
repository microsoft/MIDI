#include "pch.h"
#include "MidiBluetoothPeripheralResponse.h"
#include "Transports.Bluetooth.MidiBluetoothPeripheralResponse.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    _Use_decl_annotations_
    void MidiBluetoothPeripheralResponse::InternalInitialize(
        bool const success,
        bluetooth::MidiBluetoothPeripheralErrorCode const errorCode,
        winrt::hstring const& errorMessage,
        int32_t const errorHResult,
        bluetooth::MidiBluetoothPeripheralStatus const& status) noexcept
    {
        m_success = success;
        m_errorCode = errorCode;
        m_errorMessage = errorMessage;
        m_errorHResult = errorHResult;
        m_status = status;
    }
}
