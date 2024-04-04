// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// Notes:
//      Write (encryption recommended, write without response is required)
//      Read (encryption recommended, respond with no payload)
//      Notify (encryption recommended)
// Max connection interval is 15ms. Lower is better.


class CMidi2BluetoothMidiEndpointManager :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiEndpointManager>

{
public:
    STDMETHOD(Initialize(_In_ IUnknown*, _In_ IUnknown*));
    STDMETHOD(Cleanup)();


private:
//    winrt::guid m_midiBleServiceUuid{ MIDI_BLE_SERVICE_UUID };
    wil::com_ptr_nothrow<IMidiDeviceManagerInterface> m_MidiDeviceManager;

    GUID m_ContainerId{};
    GUID m_TransportAbstractionId{};
    std::wstring m_parentDeviceId{};

    enumeration::DeviceWatcher m_Watcher{ nullptr };        // for non-advertising but paired devices
    ad::BluetoothLEAdvertisementWatcher m_bleAdWatcher;     // for advertised devices, which is common for BLE MIDI

    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<enumeration::IDeviceWatcher>::Added_revoker m_DeviceAdded;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<enumeration::IDeviceWatcher>::Removed_revoker m_DeviceRemoved;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<enumeration::IDeviceWatcher>::Updated_revoker m_DeviceUpdated;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<enumeration::IDeviceWatcher>::Stopped_revoker m_DeviceStopped;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<enumeration::IDeviceWatcher>::EnumerationCompleted_revoker m_DeviceEnumerationCompleted;

    winrt::impl::consume_Windows_Devices_Bluetooth_Advertisement_IBluetoothLEAdvertisementWatcher<ad::IBluetoothLEAdvertisementWatcher>::Received_revoker m_AdvertisementReceived;


    HRESULT OnDeviceAdded(_In_ enumeration::DeviceWatcher, _In_ enumeration::DeviceInformation);
    HRESULT OnDeviceRemoved(_In_ enumeration::DeviceWatcher, _In_ enumeration::DeviceInformationUpdate);
    HRESULT OnDeviceUpdated(_In_ enumeration::DeviceWatcher, _In_ enumeration::DeviceInformationUpdate);
    HRESULT OnDeviceStopped(_In_ enumeration::DeviceWatcher, _In_ foundation::IInspectable);
    HRESULT OnEnumerationCompleted(_In_ enumeration::DeviceWatcher, _In_ foundation::IInspectable);

    HRESULT OnBleAdvertisementReceived(_In_ ad::BluetoothLEAdvertisementWatcher, _In_ ad::BluetoothLEAdvertisementReceivedEventArgs const&);

    HRESULT OnBleDeviceConnectionStatusChanged(_In_ bt::BluetoothLEDevice /*device*/, _In_ foundation::IInspectable /*args*/);
    HRESULT OnBleDeviceNameChanged(_In_ bt::BluetoothLEDevice /*device*/, _In_ foundation::IInspectable /*args*/);

    HRESULT CreateEndpoint(
        _In_ MidiBluetoothDeviceDefinition* definition
    );

//    HRESULT EnumCompatibleBluetoothDevices();

    HRESULT StartAdvertisementWatcher();
    HRESULT StartDeviceWatcher();

    HRESULT CreateSelfPeripheralEndpoint();

    HRESULT CreateParentDevice();
};
