// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

using namespace winrt::Windows::Devices::Enumeration;


struct KsAggregateEndpointPinDefinition
{
    ULONG PinNumber;
    std::wstring Name;
    MidiFlow DataFlowFromClientPerspective;
};

struct KsAggregateEndpointDefinition
{
    std::wstring EndpointName;
    std::wstring FilterName;
    std::wstring ParentDeviceName;

    std::wstring FilterDeviceId;
    std::wstring ParentDeviceInstanceId;
    std::wstring EndpointDeviceInstanceId;

    MidiTransport TransportCapability;

    std::vector<KsAggregateEndpointPinDefinition> Pins;
};


class CMidi2KSAggregateMidiEndpointManager :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiEndpointManager>
{
public:

    STDMETHOD(Initialize(_In_ IMidiDeviceManagerInterface*, _In_ IMidiEndpointProtocolManagerInterface*));
    STDMETHOD(Shutdown)();

private:
    STDMETHOD(CreateMidiUmpEndpoint)(_In_ KsAggregateEndpointDefinition& MasterEndpointDefinition);
    STDMETHOD(CreateMidiBytestreamEndpoints)(_In_ KsAggregateEndpointDefinition& MasterEndpointDefinition);

    
    HRESULT OnDeviceAdded(_In_ DeviceWatcher, _In_ DeviceInformation);
    HRESULT OnDeviceRemoved(_In_ DeviceWatcher, _In_ DeviceInformationUpdate);
    HRESULT OnDeviceUpdated(_In_ DeviceWatcher, _In_ DeviceInformationUpdate);
    HRESULT OnDeviceStopped(_In_ DeviceWatcher, _In_ winrt::Windows::Foundation::IInspectable);
    HRESULT OnEnumerationCompleted(_In_ DeviceWatcher, _In_ winrt::Windows::Foundation::IInspectable);

    wil::com_ptr_nothrow<IMidiDeviceManagerInterface> m_MidiDeviceManager;
    wil::com_ptr_nothrow<IMidiEndpointProtocolManagerInterface> m_MidiProtocolManager;

    std::vector<KsAggregateEndpointDefinition> m_availableEndpointDefinitions;
    
    DeviceWatcher m_Watcher{0};
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Added_revoker m_DeviceAdded;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Removed_revoker m_DeviceRemoved;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Updated_revoker m_DeviceUpdated;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Stopped_revoker m_DeviceStopped;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::EnumerationCompleted_revoker m_DeviceEnumerationCompleted;
    wil::unique_event m_EnumerationCompleted{wil::EventOptions::None};


};
