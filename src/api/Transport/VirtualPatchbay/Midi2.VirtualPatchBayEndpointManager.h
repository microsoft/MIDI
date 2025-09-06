// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

using namespace winrt::Windows::Devices::Enumeration;

class CMidi2VirtualPatchBayEndpointManager :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiEndpointManager>

{
public:
    STDMETHOD(Initialize(_In_ IMidiDeviceManager*, _In_ IMidiEndpointProtocolManager*));
    STDMETHOD(Shutdown)();


private:
    GUID m_ContainerId{};
    GUID m_transportId;   // kept for convenience

    HRESULT CreateEndpoint(
    );

    //HRESULT CreateParentDevice();


    HRESULT OnDeviceAdded(_In_ DeviceWatcher, _In_ DeviceInformation);
    HRESULT OnDeviceRemoved(_In_ DeviceWatcher, _In_ DeviceInformationUpdate);
    HRESULT OnDeviceUpdated(_In_ DeviceWatcher, _In_ DeviceInformationUpdate);
    HRESULT OnDeviceStopped(_In_ DeviceWatcher, _In_ winrt::Windows::Foundation::IInspectable);
    HRESULT OnEnumerationCompleted(_In_ DeviceWatcher, _In_ winrt::Windows::Foundation::IInspectable);

    wil::com_ptr_nothrow<IMidiDeviceManager> m_MidiDeviceManager;
    wil::com_ptr_nothrow<IMidiEndpointProtocolManager> m_MidiProtocolManager;

    DeviceWatcher m_Watcher{ 0 };
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Added_revoker m_DeviceAdded;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Removed_revoker m_DeviceRemoved;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Updated_revoker m_DeviceUpdated;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Stopped_revoker m_DeviceStopped;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::EnumerationCompleted_revoker m_DeviceEnumerationCompleted;
    //wil::unique_event m_EnumerationCompleted{ wil::EventOptions::None };


};
