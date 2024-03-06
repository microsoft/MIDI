// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

using namespace winrt::Windows::Devices::Enumeration;

typedef class _MIDI_PIN_INFO
{
public:
    std::wstring Id;                // the filter InterfaceId
    std::wstring InstanceId;        // the MIDI instance id
    std::wstring ParentInstanceId;  // The instance id of the parent device
    std::wstring Name;              // friendly name for this device
    INT32 PinId{ 0 };               // contains the pin id, for bidi it is the MidiflowOut pin id
    INT32 PinIdIn{ 0 };             // only used for bidi, contains the MidiFlowIn pin id
    MidiTransport TransportCapability {MidiTransport_Invalid};
    MidiDataFormat DataFormatCapability {MidiDataFormat_Invalid};
    MidiFlow Flow{ MidiFlowOut };
    BOOL CreateUMPOnly{ FALSE };
    HRESULT SwdCreation{ S_OK };
    std::unique_ptr<BYTE> GroupTerminalBlockDataOut;
    ULONG GroupTerminalBlockDataSizeOut {0};
    std::unique_ptr<BYTE> GroupTerminalBlockDataIn;
    ULONG GroupTerminalBlockDataSizeIn {0};
    GUID NativeDataFormat{0};

    std::wstring SerialNumber;
    std::wstring ManufacturerName;
    UINT16 VID{ 0 };
    UINT16 PID{ 0 };
} MIDI_PIN_INFO, *PMIDI_PIN_INFO;

class CMidi2KSMidiEndpointManager : 
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiEndpointManager>
{
public:

    STDMETHOD(Initialize(_In_ IUnknown*, _In_ IUnknown*));
    STDMETHOD(Cleanup)();

private:

    
    HRESULT OnDeviceAdded(_In_ DeviceWatcher, _In_ DeviceInformation);
    HRESULT OnDeviceRemoved(_In_ DeviceWatcher, _In_ DeviceInformationUpdate);
    HRESULT OnDeviceUpdated(_In_ DeviceWatcher, _In_ DeviceInformationUpdate);
    HRESULT OnDeviceStopped(_In_ DeviceWatcher, _In_ winrt::Windows::Foundation::IInspectable);
    HRESULT OnEnumerationCompleted(_In_ DeviceWatcher, _In_ winrt::Windows::Foundation::IInspectable);

    wil::com_ptr_nothrow<IMidiDeviceManagerInterface> m_MidiDeviceManager;
    wil::com_ptr_nothrow<IMidiEndpointProtocolManagerInterface> m_MidiProtocolManager;

    std::vector<std::unique_ptr<MIDI_PIN_INFO>> m_AvailableMidiPins;
    
    DeviceWatcher m_Watcher{0};
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Added_revoker m_DeviceAdded;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Removed_revoker m_DeviceRemoved;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Updated_revoker m_DeviceUpdated;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Stopped_revoker m_DeviceStopped;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::EnumerationCompleted_revoker m_DeviceEnumerationCompleted;
    wil::unique_event m_EnumerationCompleted{wil::EventOptions::None};


};
