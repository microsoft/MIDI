// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once


class CMidi2BluetoothMidiEndpointManager :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiEndpointManager>

{
public:
    STDMETHOD(Initialize(_In_ IUnknown*, _In_ IUnknown*));
    STDMETHOD(Cleanup)();


private:
    enumeration::DeviceWatcher m_Watcher{ nullptr };

    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<enumeration::IDeviceWatcher>::Added_revoker m_DeviceAdded;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<enumeration::IDeviceWatcher>::Removed_revoker m_DeviceRemoved;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<enumeration::IDeviceWatcher>::Updated_revoker m_DeviceUpdated;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<enumeration::IDeviceWatcher>::Stopped_revoker m_DeviceStopped;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<enumeration::IDeviceWatcher>::EnumerationCompleted_revoker m_DeviceEnumerationCompleted;

    HRESULT OnDeviceAdded(_In_ enumeration::DeviceWatcher, _In_ enumeration::DeviceInformation);
    HRESULT OnDeviceRemoved(_In_ enumeration::DeviceWatcher, _In_ enumeration::DeviceInformationUpdate);
    HRESULT OnDeviceUpdated(_In_ enumeration::DeviceWatcher, _In_ enumeration::DeviceInformationUpdate);
    HRESULT OnDeviceStopped(_In_ enumeration::DeviceWatcher, _In_ foundation::IInspectable);
    HRESULT OnEnumerationCompleted(_In_ enumeration::DeviceWatcher, _In_ foundation::IInspectable);


    GUID m_ContainerId{};
    GUID m_TransportAbstractionId{};

    HRESULT CreateEndpoint(
        _In_ MidiBluetoothDeviceDefinition& definition
    );

    HRESULT EnumCompatibleBluetoothDevices();

    HRESULT CreateParentDevice();

    wil::com_ptr_nothrow<IMidiDeviceManagerInterface> m_MidiDeviceManager;
};
