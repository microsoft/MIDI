// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiEndpointCustomProperties.h"
#include "MidiEndpointMatchCriteria.h"

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
    MidiDataFormats DataFormatCapability {MidiDataFormats_Invalid};
    MidiFlow Flow{ MidiFlowOut };
    BOOL CreateUMPOnly{ FALSE };
    HRESULT SwdCreation{ S_OK };
    std::unique_ptr<BYTE> GroupTerminalBlockData;
    ULONG GroupTerminalBlockDataSize {0};

    std::vector<internal::GroupTerminalBlockInternal> blocks{ }; // we need this for naming

    GUID NativeDataFormat{0};


    // the following fields are used for matching when customizing
    std::wstring SerialNumber;          // USB serial number, if provided       
    std::wstring ManufacturerName;      
    UINT16 VID{ 0 };                    // Vendor ID
    UINT16 PID{ 0 };                    // Product Id
    std::wstring EndpointDeviceId{};    // the swd id that is created when the endpoint is activated

} MIDI_PIN_INFO, *PMIDI_PIN_INFO;

class CMidi2KSMidiEndpointManager : 
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiEndpointManager>
{
public:

    STDMETHOD(Initialize(_In_ IMidiDeviceManager*, _In_ IMidiEndpointProtocolManager*));
    STDMETHOD(Shutdown)();

    // returns the endpointDeviceInterfaceId for a matching endpoint found in m_AvailableMidiPins
    winrt::hstring FindMatchingInstantiatedEndpoint(_In_ WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria& criteria);

private:

    
    HRESULT OnDeviceAdded(_In_ DeviceWatcher, _In_ DeviceInformation);
    HRESULT OnDeviceRemoved(_In_ DeviceWatcher, _In_ DeviceInformationUpdate);
    HRESULT OnDeviceUpdated(_In_ DeviceWatcher, _In_ DeviceInformationUpdate);
    HRESULT OnDeviceStopped(_In_ DeviceWatcher, _In_ winrt::Windows::Foundation::IInspectable);
    HRESULT OnEnumerationCompleted(_In_ DeviceWatcher, _In_ winrt::Windows::Foundation::IInspectable);

    wil::com_ptr_nothrow<IMidiDeviceManager> m_midiDeviceManager;
    wil::com_ptr_nothrow<IMidiEndpointProtocolManager> m_midiProtocolManager;

    std::vector<std::unique_ptr<MIDI_PIN_INFO>> m_AvailableMidiPins;
    
    DeviceWatcher m_Watcher{0};
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Added_revoker m_DeviceAdded;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Removed_revoker m_DeviceRemoved;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Updated_revoker m_DeviceUpdated;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Stopped_revoker m_DeviceStopped;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::EnumerationCompleted_revoker m_DeviceEnumerationCompleted;
    wil::unique_event m_EnumerationCompleted{wil::EventOptions::None};


};
