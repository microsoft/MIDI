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


struct KsAggregateEndpointMidiPinDefinition
{
    //std::wstring KSDriverSuppliedName;
    std::wstring FilterDeviceId;
    std::wstring FilterName;

    ULONG PinNumber;
    std::wstring PinName;

    MidiFlow PinDataFlow;

    uint8_t GroupIndex{ 0 };
    uint8_t PortIndexWithinThisFilterAndDirection{ 0 }; // not always the same as the group index. Example: MOTU Express 128 with separate filter for each in/out pair

    internal::Midi1PortNaming::Midi1PortNameEntry PortNames;
};

struct KsAggregateEndpointDefinition
{
    std::wstring EndpointDeviceId{};
    
    uint16_t VID{0};
    uint16_t PID{0};
    std::wstring SerialNumber{};

    std::wstring ManufacturerName{}; 

    std::wstring EndpointName{};
    std::wstring EndpointDeviceInstanceId{};

    std::wstring ParentDeviceName{};
    std::wstring ParentDeviceInstanceId{};

    std::vector<KsAggregateEndpointMidiPinDefinition> MidiPins{ };
};


class CMidi2KSAggregateMidiEndpointManager :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiEndpointManager>
{
public:

    STDMETHOD(Initialize(_In_ IMidiDeviceManager*, _In_ IMidiEndpointProtocolManager*));
    STDMETHOD(Shutdown)();

    // returns the endpointDeviceInterfaceId for a matching endpoint found in m_availableEndpointDefinitions
    winrt::hstring FindMatchingInstantiatedEndpoint(_In_ MidiEndpointMatchCriteria& criteria);

private:
    STDMETHOD(CreateMidiUmpEndpoint)(_In_ KsAggregateEndpointDefinition& masterEndpointDefinition);
    
    HRESULT OnDeviceAdded(_In_ DeviceWatcher, _In_ DeviceInformation);
    HRESULT OnDeviceRemoved(_In_ DeviceWatcher, _In_ DeviceInformationUpdate);
    HRESULT OnDeviceUpdated(_In_ DeviceWatcher, _In_ DeviceInformationUpdate);
    HRESULT OnDeviceStopped(_In_ DeviceWatcher, _In_ winrt::Windows::Foundation::IInspectable);
    HRESULT OnEnumerationCompleted(_In_ DeviceWatcher, _In_ winrt::Windows::Foundation::IInspectable);

    wil::com_ptr_nothrow<IMidiDeviceManager> m_midiDeviceManager;
    wil::com_ptr_nothrow<IMidiEndpointProtocolManager> m_midiProtocolManager;

    wil::critical_section m_availableEndpointDefinitionsLock;
    std::map<std::wstring, KsAggregateEndpointDefinition> m_availableEndpointDefinitions;
    
    DeviceWatcher m_watcher{0};
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Added_revoker m_DeviceAdded;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Removed_revoker m_DeviceRemoved;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Updated_revoker m_DeviceUpdated;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Stopped_revoker m_DeviceStopped;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::EnumerationCompleted_revoker m_DeviceEnumerationCompleted;
    wil::unique_event m_EnumerationCompleted{wil::EventOptions::None};

    HRESULT GetKSDriverSuppliedName(_In_ HANDLE hFilter, _Inout_ std::wstring& name);


};
