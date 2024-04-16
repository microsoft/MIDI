// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// NOTE: This is prototype code for working through some BLE approaches before
// implementing it in the Windows service. Eventually, this code will go stale, 
// but we'll keep it around. This is not production code.

#include "pch.h"


_Use_decl_annotations_
HRESULT MidiBleDevice::Initialize(
    std::wstring const endpointDeviceInterfaceId,
    std::wstring const bleDeviceId,
    bool const supportsMidiOut,
    bool const supportsMidiIn,
    bool const supportsNotify)
{
    std::cout << __FUNCTION__ << std::endl;

    m_endpointDeviceInterfaceId = endpointDeviceInterfaceId;
    m_bleDeviceId = bleDeviceId;

    m_supportsMidiOut = supportsMidiOut;
    m_supportsMidiIn = supportsMidiIn;
    m_supportsNotify = supportsNotify;

    return S_OK;
}




HRESULT MidiBleDevice::Connect()
{
    std::cout << __FUNCTION__ << std::endl;

    m_bleDevice = GetBleDeviceFromEnumerationDeviceId(m_bleDeviceId);

    if (m_bleDevice != nullptr)
    {
        m_bleDevice.ConnectionStatusChanged({ this, &MidiBleDevice::OnConnectionStatusChanged });

        //std::cout << "Bluetooth device connected: " << winrt::to_string(m_bleDevice.Name()) << std::endl;

        // force connection by, as our docs say, calling an uncached service discovery method

        auto service = GetBleMidiServiceFromDevice(m_bleDevice);

        OnConnected();

        return S_OK;
    }

    //std::cout << "Couldn't connect" << std::endl;

    return E_FAIL;
}


HRESULT MidiBleDevice::Disconnect()
{
    std::cout << __FUNCTION__ << std::endl;

    // terminate the session
    if (m_bleDevice != nullptr && m_session != nullptr)
    {
        std::cout << "Disconnecting from " << winrt::to_string(m_bleDevice.Name()) << std::endl;

        m_session.Close();
    }

    return S_OK;
}

void MidiBleDevice::OnDisconnected()
{
    std::cout << __FUNCTION__ << std::endl;

}

void MidiBleDevice::OnConnected()
{
    std::cout << __FUNCTION__ << std::endl;

    m_midiService = GetBleMidiServiceFromDevice(m_bleDevice);

    if (m_midiService != nullptr)
    {
        auto openStatus = m_midiService.OpenAsync(GattSharingMode::SharedReadAndWrite).get();

        if (openStatus == GattOpenStatus::Success)
        {
            std::cout << "Service opened" << std::endl;

            auto session = GattSession::FromDeviceIdAsync(m_bleDevice.BluetoothDeviceId()).get();

            session.MaintainConnection(true);

            m_session = std::move(session);
        }
        else if (openStatus == GattOpenStatus::AccessDenied)
        {
            std::cout << "Open returned AccessDenied" << std::endl;
        }
        else if (openStatus == GattOpenStatus::AlreadyOpened)
        {
            std::cout << "Open returned AlreadyOpened" << std::endl;
        }
        else if (openStatus == GattOpenStatus::NotFound)
        {
            std::cout << "Open returned NotFound" << std::endl;
        }
        else if (openStatus == GattOpenStatus::SharingViolation)
        {
            std::cout << "Open returned SharingViolation" << std::endl;
        }
        else if (openStatus == GattOpenStatus::Unspecified)
        {
            std::cout << "Open returned Unspecified" << std::endl;
        }

        m_midiDataIOCharacteristic = GetBleMidiDataIOCharacteristicFromService(m_midiService);

        if (m_midiDataIOCharacteristic != nullptr)
        {
            m_midiDataIOCharacteristic.WriteClientCharacteristicConfigurationDescriptorAsync(
                GattClientCharacteristicConfigurationDescriptorValue::Notify).get();


            // todo: keep token to unwire this on disconnect
            m_midiDataIOCharacteristic.ValueChanged({ this, &MidiBleDevice::OnValueChanged });

            std::cout << "ValueChanged wired up" << std::endl;

            return;
        }
    }
    else
    {
        std::cout << "Service is nullptr" << std::endl;
    }



}


_Use_decl_annotations_
void MidiBleDevice::OnConnectionStatusChanged(BluetoothLEDevice const& sender, foundation::IInspectable const& /*args*/)
{
    std::cout << __FUNCTION__ << std::endl;

    if (sender.ConnectionStatus() == BluetoothConnectionStatus::Connected)
    {
        OnConnected();
    }
    else
    {
        OnDisconnected();
    }
}





_Use_decl_annotations_
void MidiBleDevice::OnValueChanged(GattCharacteristic const& /*sender*/, GattValueChangedEventArgs const& args)
{
    std::cout << __FUNCTION__ << std::endl;

    std::cout << "> ";

    for (unsigned i = 0; i < args.CharacteristicValue().Length(); i++)
    {
        // read next byte
        auto b = *(args.CharacteristicValue().data() + i);

        std::cout << "0x" << std::hex << (unsigned)b << " ";
    }

    std::cout << std::endl;
}