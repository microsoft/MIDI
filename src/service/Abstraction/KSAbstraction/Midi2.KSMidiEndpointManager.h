// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

using namespace winrt::Windows::Devices::Enumeration;

typedef class _MIDI_PIN_INFO
{
public:
    std::wstring Id; // the filter InterfaceId
    std::wstring InstanceId; // the MIDI instance id
    std::wstring ParentInstanceId; // The instance id of the parent device
    std::wstring Name; // friendly name for this device
    INT32 PinId{ 0 }; // contains the pin id, for bidi it is the MidiflowOut pin id
    INT32 PinIdIn{ 0 }; // only used for bidi, contains the MidiFlowIn pin id
    BOOL StandardUmp{ FALSE };
    BOOL CyclicUmp{ FALSE };
    BOOL MidiOne{ FALSE };
    MidiFlow Flow{ MidiFlowOut };
    BOOL CreateUMPOnly{ FALSE };
} MIDI_PIN_INFO, *PMIDI_PIN_INFO;

class CMidi2KSMidiEndpointManager : 
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiEndpointManager>
{
public:

    STDMETHOD(Initialize(_In_ IUnknown *));
    STDMETHOD(Cleanup)();

private:
    
    HRESULT OnDeviceAdded(_In_ DeviceWatcher, _In_ DeviceInformation);
    HRESULT OnDeviceRemoved(_In_ DeviceWatcher, _In_ DeviceInformationUpdate);
    HRESULT OnDeviceUpdated(_In_ DeviceWatcher, _In_ DeviceInformationUpdate);
    HRESULT OnDeviceStopped(_In_ DeviceWatcher, _In_ winrt::Windows::Foundation::IInspectable);
    HRESULT OnEnumerationCompleted(_In_ DeviceWatcher, _In_ winrt::Windows::Foundation::IInspectable);
    wil::com_ptr_nothrow<IMidiDeviceManagerInterface> m_MidiDeviceManager;

    std::vector<std::unique_ptr<MIDI_PIN_INFO>> m_AvailableMidiPins;
    
    DeviceWatcher m_Watcher{0};
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Added_revoker m_DeviceAdded;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Removed_revoker m_DeviceRemoved;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Updated_revoker m_DeviceUpdated;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Stopped_revoker m_DeviceStopped;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::EnumerationCompleted_revoker m_DeviceEnumerationCompleted;
    wil::unique_event m_EnumerationCompleted{wil::EventOptions::None};

};
