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

struct KsAggregateEndpointMidiPinDefinition2
{
    std::wstring FilterDeviceId;                // this is also the value needed by WinMM for DRV_QUERYDEVICEINTERFACE
    std::wstring FilterName;

    ULONG PinNumber;
    std::wstring PinName;

    MidiFlow PinDataFlow;
    MidiFlow DataFlowFromUserPerspective;

    uint8_t GroupIndex{ 0 };
    uint8_t PortIndexWithinThisFilterAndDirection{ 0 }; // not always the same as the group index. Example: MOTU Express 128 with separate filter for each in/out pair
};

struct KsAggregateEndpointDefinition2
{
    std::wstring ParentDeviceInstanceId{};

    std::wstring EndpointDeviceId{};

    std::wstring EndpointName{};
    std::wstring EndpointDeviceInstanceId{};

    std::vector<std::shared_ptr<KsAggregateEndpointMidiPinDefinition2>> MidiPins{ };

    WindowsMidiServicesNamingLib::MidiEndpointNameTable EndpointNameTable{ };

    uint32_t EndpointIndexForThisParentDevice{ 0 };  


    int8_t CurrentHighestMidiSourceGroupIndex{ -1 };
    int8_t CurrentHighestMidiDestinationGroupIndex{ -1 };

};


class KsAggregateParentDeviceDefinition2
{
public:
    std::wstring DeviceName{};
    std::wstring DeviceInstanceId{};
    std::wstring DriverSuppliedDeviceName{};    // value from registry. Required for WinMM classic naming

    uint32_t IndexOfDevicesWithThisSameName{ 0 };   // for when there are multiple of the same device


    uint16_t VID{ 0 };  // USB-only
    uint16_t PID{ 0 };  // USB-only
    std::wstring SerialNumber{};

    std::wstring ManufacturerName{};
};


class CMidi2KSAggregateMidiEndpointManager2 :
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
    DWORD m_individualInterfaceEnumTimeoutMS{ DEFAULT_KSA_INTERFACE_ENUM_TIMEOUT_MS };

    wil::com_ptr_nothrow<IMidiDeviceManager> m_midiDeviceManager;
    wil::com_ptr_nothrow<IMidiEndpointProtocolManager> m_midiProtocolManager;

    HRESULT OnFilterDeviceInterfaceAdded(_In_ DeviceWatcher, _In_ DeviceInformation);
    HRESULT OnFilterDeviceInterfaceRemoved(_In_ DeviceWatcher, _In_ DeviceInformationUpdate);
    HRESULT OnFilterDeviceInterfaceUpdated(_In_ DeviceWatcher, _In_ DeviceInformationUpdate);

    HRESULT OnEnumerationCompleted(_In_ DeviceWatcher, _In_ winrt::Windows::Foundation::IInspectable);
    HRESULT OnDeviceWatcherStopped(_In_ DeviceWatcher, _In_ winrt::Windows::Foundation::IInspectable);


    // key is parent device instance id. These don't get "activated" so we keep a single list
    std::map<std::wstring, std::shared_ptr<KsAggregateParentDeviceDefinition2>> m_allParentDeviceDefinitions;
    wil::critical_section m_allParentDeviceDefinitionsLock;

    // these are all endpoints which have not yet been activated
    std::vector<std::shared_ptr<KsAggregateEndpointDefinition2>> m_pendingEndpointDefinitions;
    wil::critical_section m_pendingEndpointDefinitionsLock;

    // key is parent device instance id
    std::map<std::wstring, std::shared_ptr<KsAggregateEndpointDefinition2>> m_activatedEndpointDefinitions;
    wil::critical_section m_activatedEndpointDefinitionsLock;



    bool ActiveKSAEndpointForDeviceExists(
        _In_ std::wstring deviceInstanceId);

    HRESULT ParseParentIdIntoVidPidSerial(
        _In_ std::wstring systemDevicesParentValue,
        _In_ std::shared_ptr<KsAggregateParentDeviceDefinition2>& parentDevice);


    HRESULT FindActivatedEndpointDefinitionForFilterDevice(
        _In_ std::wstring filterDeviceId,
        _In_ std::shared_ptr<KsAggregateEndpointDefinition2>&);

    HRESULT FindExistingParentDeviceDefinitionForEndpoint(
        _In_ std::shared_ptr<KsAggregateEndpointDefinition2> endpointDefinition,
        _In_ std::shared_ptr<KsAggregateParentDeviceDefinition2>& parentDeviceDefinition);

    HRESULT FindOrCreateParentDeviceDefinitionForFilterDevice(
        _In_ DeviceInformation filterDevice,
        _In_ KsHandleWrapper& filterDeviceWrapper,
        _In_ std::shared_ptr<KsAggregateParentDeviceDefinition2>& parentDeviceDefinition);

    HRESULT FindOrCreatePendingEndpointDefinitionForFilterDevice(
        _In_ DeviceInformation,
        _In_ KsHandleWrapper& filterDeviceHandleWrapper,
        _In_ std::shared_ptr<KsAggregateEndpointDefinition2>&);
    

    HRESULT FindCurrentMaxEndpointIndexForParentDevice(
        _In_ std::shared_ptr<KsAggregateParentDeviceDefinition2> parentDeviceDefinition, 
        _In_ uint32_t& currentMaxIndex);


    HRESULT GetPinName(_In_ HANDLE const hFilter, _In_ UINT const pinIndex, _Inout_ std::wstring& pinName);
    HRESULT GetPinDataFlow(_In_ HANDLE const hFilter, _In_ UINT const pinIndex, _Inout_ KSPIN_DATAFLOW& dataFlow);

    HRESULT GetMidi1FilterPins(
        _In_ DeviceInformation,
        _In_ std::vector<std::shared_ptr<KsAggregateEndpointMidiPinDefinition2>>&);

    HRESULT GetKSDriverSuppliedName(_In_ HANDLE hFilter, _Inout_ std::wstring& name);


    HRESULT IncrementAndGetNextGroupIndex(
        _In_ std::shared_ptr<KsAggregateEndpointDefinition2> definition,
        _In_ MidiFlow dataFlowFromUserPerspective,
        _In_ uint8_t& groupIndex);

    HRESULT UpdateNewPinDefinitions(
        _In_ std::wstring filterDeviceid,
        _In_ std::wstring driverSuppliedName,
        _In_ std::shared_ptr<KsAggregateEndpointDefinition2> endpointDefinition);

    HRESULT BuildPinsAndGroupTerminalBlocksPropertyData(
        _In_ std::shared_ptr<KsAggregateEndpointDefinition2> masterEndpointDefinition,
        _In_ std::vector<std::byte>& pinMapPropertyData,
        _In_ std::vector<internal::GroupTerminalBlockInternal>& groupTerminalBlocks);

    HRESULT UpdateNameTableWithCustomProperties(
        _In_ std::shared_ptr<KsAggregateEndpointDefinition2> masterEndpointDefinition,
        _In_ std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties> customProperties);


    // these two functions actually update the software devices in Windows

    HRESULT DeviceCreateMidiUmpEndpoint(
        _In_ std::shared_ptr<KsAggregateEndpointDefinition2> masterEndpointDefinition);

    HRESULT DeviceUpdateExistingMidiUmpEndpointWithFilterChanges(
        _In_ std::shared_ptr<KsAggregateEndpointDefinition2> masterEndpointDefinition);


    wil::unique_event_nothrow m_endpointCreationThreadWakeup;
    std::jthread m_endpointCreationThread;
    void EndpointCreationThreadWorker(_In_ std::stop_token token);


    DeviceWatcher m_watcher{0};
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Added_revoker m_DeviceAdded;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Removed_revoker m_DeviceRemoved;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Updated_revoker m_DeviceUpdated;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Stopped_revoker m_DeviceStopped;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::EnumerationCompleted_revoker m_DeviceEnumerationCompleted;
    wil::unique_event m_EnumerationCompleted{wil::EventOptions::None};


};
