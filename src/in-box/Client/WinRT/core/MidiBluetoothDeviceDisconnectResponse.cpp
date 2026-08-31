#include "pch.h"
#include "MidiBluetoothDeviceDisconnectResponse.h"
#include "Transports.Bluetooth.MidiBluetoothDeviceDisconnectResponse.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    _Use_decl_annotations_
    void MidiBluetoothDeviceDisconnectResponse::InternalInitialize(
        bool const success,
        bluetooth::MidiBluetoothDeviceDisconnectErrorCode const errorCode,
        winrt::hstring const& errorMessage,
        int32_t const errorHResult) noexcept
    {
        m_success = success;
        m_errorCode = errorCode;
        m_errorMessage = errorMessage;
        m_errorHResult = errorHResult;
    }
}
