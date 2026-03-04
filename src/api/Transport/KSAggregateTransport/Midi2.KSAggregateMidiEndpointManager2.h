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

#define DEFAULT_KSA_INTERFACE_ENUM_TIMEOUT_MS           300 //250 is the minimum for some of my 16 port USB devices
#define KSA_INTERFACE_ENUM_TIMEOUT_MS_MINIMUM_VALUE     20
#define KSA_INTERFACE_ENUM_TIMEOUT_MS_MAXIMUM_VALUE     2500
#define KSA_INTERFACE_ENUM_TIMEOUT_REG_VALUE            L"KsaInterfaceEnumTimeoutMS"

#define KSA_EMPTY_PENDING_ENDPOINT_QUEUE_WAIT_MS        10000       // this is just to avoid an infinite wait

struct KsAggregateEndpointMidiPinDefinition2
{
    std::wstring FilterDeviceId{ };                // this is also the value needed by WinMM for DRV_QUERYDEVICEINTERFACE
    std::wstring FilterName{ };

    std::wstring DriverSuppliedName{};          // value from registry. Required for WinMM classic naming. This was at parent level efore, but loopMIDI and similar register per-interface

    ULONG PinNumber{ 0 };
    std::wstring PinName;

    MidiFlow PinDataFlow{ MidiFlow::MidiFlowOut };
    MidiFlow DataFlowFromUserPerspective{ MidiFlow::MidiFlowOut };

    bool NeedsGroupIndexAssigned{ true };

    uint8_t GroupIndex{ 0 };
    uint8_t PortIndexWithinThisFilterAndDirection{ 0 }; // not always the same as the group index. Example: MOTU Express 128 with separate filter for each in/out pair
};

struct KsAggregateEndpointDefinition2
{
    std::wstring ParentDeviceInstanceId{};

    std::wstring EndpointDeviceId{};

    std::wstring EndpointName{};
    std::wstring EndpointDeviceInstanceId{};

    std::vector<std::shared_ptr<KsAggregateEndpointMidiPinDefinition2>> MidiSourcePins{ };
    std::vector<std::shared_ptr<KsAggregateEndpointMidiPinDefinition2>> MidiDestinationPins{ };

    inline std::vector<std::shared_ptr<KsAggregateEndpointMidiPinDefinition2>> GetAllPins()
    {
        std::vector<std::shared_ptr<KsAggregateEndpointMidiPinDefinition2>> allPins;
        allPins.reserve(MidiSourcePins.size() + MidiDestinationPins.size());
        allPins.insert(allPins.end(), MidiSourcePins.begin(), MidiSourcePins.end());
        allPins.insert(allPins.end(), MidiDestinationPins.begin(), MidiDestinationPins.end());

        return allPins;
    }

 //   inline void RemoveAllPinsForFilter(_In_ std::wstring const& filterId);

    WindowsMidiServicesNamingLib::MidiEndpointNameTable EndpointNameTable{ };

    uint32_t EndpointIndexForThisParentDevice{ 0 };  

    //wil::slim_event_manual_reset UpdateTimeout;
    //std::atomic<bool> UpdatedSinceLastCheck{ true };

    std::atomic<uint64_t> LastUpdatedTimestamp{ 0 };
    std::atomic<bool> LockedForUpdating { false };

};


class KsAggregateParentDeviceDefinition2
{
public:
    std::wstring DeviceName{};
    std::wstring DeviceInstanceId{};

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
    uint64_t m_individualInterfaceEnumTimeoutTicks{ 0 };
    DWORD m_individualInterfaceEnumTimeoutMilliseconds{ 0 };

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
        _In_ std::shared_ptr<KsAggregateParentDeviceDefinition2> parentDevice);


    HRESULT FindActivatedEndpointDefinitionForFilterDevice(
        _In_ std::wstring filterDeviceId,
        _Inout_ std::shared_ptr<KsAggregateEndpointDefinition2>&);

    HRESULT FindAllActivatedEndpointDefinitionsForParentDevice(
        _In_ std::wstring parentDeviceInstanceId,
        _Inout_ std::vector<std::shared_ptr<KsAggregateEndpointDefinition2>>& endpointDefinitions);

    HRESULT FindAllPendingEndpointDefinitionsForParentDevice(
        _In_ std::wstring parentDeviceInstanceId,
        _Inout_ std::vector<std::shared_ptr<KsAggregateEndpointDefinition2>>& endpointDefinitions);


    bool ActivatedEndpointContainsPinsForFilter(
        _In_ std::wstring filterDeviceId,
        _In_ std::shared_ptr<KsAggregateEndpointDefinition2> endpoint);


    HRESULT FindParentDeviceForActivatedFilter(
        _In_ std::wstring filterDeviceId,
        _Inout_ std::shared_ptr<KsAggregateParentDeviceDefinition2>& parent);



    HRESULT FindPendingEndpointDefinitionForParentDevice(
        _In_ std::wstring parentDeviceInstanceId,
        _Inout_ std::shared_ptr<KsAggregateEndpointDefinition2>&);

    HRESULT FindExistingParentDeviceDefinitionForEndpoint(
        _In_ std::shared_ptr<KsAggregateEndpointDefinition2> endpointDefinition,
        _Inout_ std::shared_ptr<KsAggregateParentDeviceDefinition2>& parentDeviceDefinition);

    HRESULT FindOrCreateParentDeviceDefinitionForFilterDevice(
        _In_ DeviceInformation filterDevice,
        _Inout_ std::shared_ptr<KsAggregateParentDeviceDefinition2>& parentDeviceDefinition);

    HRESULT CreatePendingEndpointDefinitionForFilterDevice(
        _In_ DeviceInformation,
        _Inout_ std::shared_ptr<KsAggregateEndpointDefinition2>&
    );
    

    HRESULT FindUnusedEndpointIndexForParentDevice(
        _In_ std::shared_ptr<KsAggregateParentDeviceDefinition2> parentDeviceDefinition, 
        _In_ uint32_t& unusedIndex);

    bool AddPinToEndpoint(
        _In_ std::shared_ptr<KsAggregateEndpointDefinition2> endpoint,
        _In_ std::shared_ptr<KsAggregateEndpointMidiPinDefinition2> pin
    );


    HRESULT GetPinName(_In_ HANDLE const hFilter, _In_ UINT const pinIndex, _Inout_ std::wstring& pinName);
    HRESULT GetPinDataFlow(_In_ HANDLE const hFilter, _In_ UINT const pinIndex, _Inout_ KSPIN_DATAFLOW& dataFlow);

    HRESULT GetMidi1FilterPins(
        _In_ DeviceInformation,
        _In_ std::vector<std::shared_ptr<KsAggregateEndpointMidiPinDefinition2>>&,
        _Inout_ uint32_t& countMidiSourcePinsAdded,
        _Inout_ uint32_t& countMidiDestinationPinsAdded);

    HRESULT GetKSDriverSuppliedName(_In_ HANDLE hFilter, _Inout_ std::wstring& name);


    //HRESULT IncrementAndGetNextGroupIndex(
    //    _In_ std::shared_ptr<KsAggregateEndpointDefinition2> definition,
    //    _In_ MidiFlow dataFlowFromUserPerspective,
    //    _In_ uint8_t& groupIndex);

    HRESULT UpdateNewPinDefinitions(
        _In_ std::shared_ptr<KsAggregateEndpointDefinition2> endpointDefinition,
        _In_ std::shared_ptr<KsAggregateParentDeviceDefinition2> parentDevice);

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


    wil::unique_event_nothrow m_EnumerationCompleted;
    wil::unique_event_nothrow m_initialEndpointCreationCompleted;

    wil::unique_event_nothrow m_endpointCreationThreadWakeup;
    std::jthread m_endpointCreationThread;
    void EndpointCreationThreadWorker(_In_ std::stop_token token);


    DeviceWatcher m_watcher{0};
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Added_revoker m_DeviceAdded;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Removed_revoker m_DeviceRemoved;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Updated_revoker m_DeviceUpdated;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Stopped_revoker m_DeviceStopped;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::EnumerationCompleted_revoker m_DeviceEnumerationCompleted;

};
