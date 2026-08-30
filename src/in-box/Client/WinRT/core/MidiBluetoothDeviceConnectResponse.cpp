#include "pch.h"
#include "MidiBluetoothDeviceConnectResponse.h"
#include "Transports.Bluetooth.MidiBluetoothDeviceConnectResponse.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    _Use_decl_annotations_
    void MidiBluetoothDeviceConnectResponse::InternalInitialize(
        bool const success,
        bluetooth::MidiBluetoothDeviceConnectErrorCode const errorCode,
        winrt::hstring const& errorMessage,
        int32_t const errorHResult,
        bool const isKnown,
        bluetooth::MidiBluetoothDeviceInformation const& device) noexcept
    {
        m_success = success;
        m_errorCode = errorCode;
        m_errorMessage = errorMessage;
        m_errorHResult = errorHResult;
        m_isKnown = isKnown;
        m_device = device;
    }
}
