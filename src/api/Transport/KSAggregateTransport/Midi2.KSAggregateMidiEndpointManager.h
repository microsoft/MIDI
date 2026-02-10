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
#include "MidiEndpointNameTable.h"

using namespace winrt::Windows::Devices::Enumeration;

#define DEFAULT_KSA_INTERFACE_ENUM_TIMEOUT_MS           250
#define KSA_INTERFACE_ENUM_TIMEOUT_MS_MINIMUM_VALUE     50
#define KSA_INTERFACE_ENUM_TIMEOUT_MS_MAXIMUM_VALUE     2500
#define KSA_INTERFACE_ENUM_TIMEOUT_REG_VALUE            L"KsaInterfaceEnumTimeoutMS"

struct KsAggregateEndpointMidiPinDefinition
{
    //std::wstring KSDriverSuppliedName;
    std::wstring FilterDeviceId;                // this is also the value needed by WinMM for DRV_QUERYDEVICEINTERFACE
    std::wstring FilterName;

    ULONG PinNumber;
    std::wstring PinName;

    MidiFlow PinDataFlow;
    MidiFlow DataFlowFromUserPerspective;

    uint8_t GroupIndex{ 0 };
    uint8_t PortIndexWithinThisFilterAndDirection{ 0 }; // not always the same as the group index. Example: MOTU Express 128 with separate filter for each in/out pair

 //   internal::Midi1PortNaming::Midi1PortNameEntry PortNames;
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

    WindowsMidiServicesNamingLib::MidiEndpointNameTable EndpointNameTable{};
};



// new structures because we need to be able to pull together
// virtual endpoints, which have greater than 16 ins and/or outs
// and so need the creation of multiple endpoints. Without the
// new 2603 approach, only 16 in and out ports are available
// per parent device (teVirtualMidi in this case). Also impacts
// loopBE30.

struct KsAggregateEndpointDefinitionV2
{
    std::wstring EndpointDeviceId{};

    std::wstring EndpointName{};
    std::wstring EndpointDeviceInstanceId{};

    std::vector<KsAggregateEndpointMidiPinDefinition> MidiPins{ };

    WindowsMidiServicesNamingLib::MidiEndpointNameTable EndpointNameTable{};

    int8_t CurrentHighestMidiSourceGroupIndex{ -1 };
    int8_t CurrentHighestMidiDestinationGroupIndex{ -1 };
};


struct KsAggregateParentDeviceDefinitionV2
{
    std::wstring DeviceName{};
    std::wstring DeviceInstanceId{};
    std::wstring DriverSuppliedDeviceName{};    // value from registry. Required for WinMM classic naming

    uint16_t VID{ 0 };  // USB-only
    uint16_t PID{ 0 };  // USB-only
    std::wstring SerialNumber{};

    std::wstring ManufacturerName{};

    std::vector<KsAggregateEndpointDefinitionV2> Endpoints{ };  // most devices will have just one endpoint, but virtual can have > 1
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
    winrt::hstring FindMatchingInstantiatedEndpoint(_In_ WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria& criteria);

private:
    STDMETHOD(CreateMidiUmpEndpoint)(_In_ KsAggregateEndpointDefinition& masterEndpointDefinition);
    STDMETHOD(CreateMidiUmpEndpointV2)(_In_ std::shared_ptr<KsAggregateEndpointDefinition> masterEndpointDefinition);

    HRESULT OnDeviceAdded(_In_ DeviceWatcher, _In_ DeviceInformation);
    HRESULT OnDeviceRemoved(_In_ DeviceWatcher, _In_ DeviceInformationUpdate);
    HRESULT OnDeviceUpdated(_In_ DeviceWatcher, _In_ DeviceInformationUpdate);
    HRESULT OnDeviceStopped(_In_ DeviceWatcher, _In_ winrt::Windows::Foundation::IInspectable);
    HRESULT OnEnumerationCompleted(_In_ DeviceWatcher, _In_ winrt::Windows::Foundation::IInspectable);

    wil::com_ptr_nothrow<IMidiDeviceManager> m_midiDeviceManager;
    wil::com_ptr_nothrow<IMidiEndpointProtocolManager> m_midiProtocolManager;

    wil::critical_section m_availableEndpointDefinitionsLock;
    std::map<std::wstring, KsAggregateEndpointDefinition> m_availableEndpointDefinitions;

    wil::critical_section m_pendingEndpointDefinitionsLock;
    std::vector<std::shared_ptr<KsAggregateEndpointDefinition>> m_pendingEndpointDefinitions;



    // new interface-based approachfor 2603 CFR update
    HRESULT OnFilterDeviceInterfaceAdded(_In_ DeviceWatcher, _In_ DeviceInformation);
    HRESULT OnFilterDeviceInterfaceRemoved(_In_ DeviceWatcher, _In_ DeviceInformationUpdate);
    HRESULT OnFilterDeviceInterfaceUpdated(_In_ DeviceWatcher, _In_ DeviceInformationUpdate);

    std::map<std::wstring, std::shared_ptr<KsAggregateParentDeviceDefinitionV2>> m_availableEndpointDefinitionsV2;
    std::vector<std::shared_ptr<KsAggregateParentDeviceDefinitionV2>> m_pendingEndpointDefinitionsV2;


    HRESULT FindActivatedEndpointDefinitionForFilterDevice(
        _In_ std::wstring parentDeviceInstanceId,
        _In_ std::shared_ptr<KsAggregateEndpointDefinitionV2>&);

    HRESULT FindOrCreatePendingEndpointDefinitionForFilterDevice(
        _In_ DeviceInformation,
        _In_ std::shared_ptr<KsAggregateEndpointDefinitionV2>&);
    
    HRESULT GetMidi1FilterPins(
        _In_ DeviceInformation,
        _In_ std::vector<KsAggregateEndpointMidiPinDefinition>&);

    bool KSAEndpointForDeviceExists(
        _In_ std::wstring deviceInstanceId);

    HRESULT IncrementAndGetNextGroupIndex(
        _In_ std::shared_ptr<KsAggregateEndpointDefinitionV2> definition,
        _In_ MidiFlow dataFlowFromUserPerspective,
        _In_ uint8_t& groupIndex);

    HRESULT UpdateNewPinDefinitions(
        _In_ std::wstring filterDeviceid,
        _In_ std::wstring driverSuppliedName,
        _In_ std::shared_ptr<KsAggregateEndpointDefinitionV2> endpointDefinition);

    HRESULT BuildPinsAndGroupTerminalBlocksPropertyData(
        _In_ std::shared_ptr<KsAggregateEndpointDefinitionV2> masterEndpointDefinition,
        _In_ std::vector<std::byte>& pinMapPropertyData,
        _In_ std::vector<internal::GroupTerminalBlockInternal>& groupTerminalBlocks);

    HRESULT UpdateNameTableWithCustomProperties(
        _In_ std::shared_ptr<KsAggregateEndpointDefinitionV2> masterEndpointDefinition,
        _In_ std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties> customProperties);

    wil::unique_event_nothrow m_endpointCreationThreadWakeup;
    std::jthread m_endpointCreationThread;
    void EndpointCreationThreadWorker(_In_ std::stop_token token);

    HRESULT UpdateExistingMidiUmpEndpointWithFilterChanges(
        _In_ std::shared_ptr<KsAggregateEndpointDefinition> masterEndpointDefinition);




    DeviceWatcher m_watcher{0};
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Added_revoker m_DeviceAdded;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Removed_revoker m_DeviceRemoved;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Updated_revoker m_DeviceUpdated;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Stopped_revoker m_DeviceStopped;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::EnumerationCompleted_revoker m_DeviceEnumerationCompleted;
    wil::unique_event m_EnumerationCompleted{wil::EventOptions::None};

    HRESULT GetKSDriverSuppliedName(_In_ HANDLE hFilter, _Inout_ std::wstring& name);

    DWORD m_individualInterfaceEnumTimeoutMS { DEFAULT_KSA_INTERFACE_ENUM_TIMEOUT_MS };
};
