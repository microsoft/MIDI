// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once


// class for communicating with the endpoint. Not for enumeration

class MidiBleDevice
{
public:
    HRESULT Initialize(
        _In_ std::wstring const endpointDeviceInterfaceId,
        _In_ std::wstring const bleDeviceId,
        _In_ bool const supportsMidiOut,
        _In_ bool const supportsMidiIn,
        _In_ bool const supportsNotify);

    HRESULT Connect();

    HRESULT Disconnect();

private:

    GattSession m_session{ nullptr };

    std::wstring m_endpointDeviceInterfaceId{};
    std::wstring m_bleDeviceId{};

    BluetoothLEDevice m_bleDevice{ nullptr };
    GattDeviceService m_midiService{ nullptr };
    GattCharacteristic m_midiDataIOCharacteristic{ nullptr };

    bool m_supportsMidiOut{ false };
    bool m_supportsMidiIn{ false };
    bool m_supportsNotify{ false };


    void OnConnected();
    void OnDisconnected();


    void OnConnectionStatusChanged(_In_ BluetoothLEDevice const& sender, _In_ foundation::IInspectable const& args);
    void OnValueChanged(_In_ GattCharacteristic const& sender, _In_ GattValueChangedEventArgs const& args);

};