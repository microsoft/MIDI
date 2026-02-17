// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



#include "pch.h"

#include <sstream>      // for the string stream in parsing of VID/PID/Serial from parent id
#include <iostream>     // for getline for string parsing of VID/PID/Serial from parent id

#include "Feature_Servicing_MIDI2FilterCreations.h"

using namespace wil;
using namespace winrt::Windows::Devices::Enumeration;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

#define INITIAL_ENUMERATION_TIMEOUT_MS 10000
_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager2::Initialize(
    IMidiDeviceManager* midiDeviceManager,
    IMidiEndpointProtocolManager* midiEndpointProtocolManager
)
{
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"), 
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    RETURN_HR_IF(E_INVALIDARG, nullptr == midiDeviceManager);
    RETURN_HR_IF(E_INVALIDARG, nullptr == midiEndpointProtocolManager);

    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManager), (void**)&m_midiDeviceManager));
    RETURN_IF_FAILED(midiEndpointProtocolManager->QueryInterface(__uuidof(IMidiEndpointProtocolManager), (void**)&m_midiProtocolManager));

    // needed for internal consumption. Gary to replace this with feature enablement check
    // defined in pch.h
    DWORD individualInterfaceEnumTimeoutMS{ DEFAULT_KSA_INTERFACE_ENUM_TIMEOUT_MS };
    if (SUCCEEDED(wil::reg::get_value_dword_nothrow(HKEY_LOCAL_MACHINE, MIDI_ROOT_REG_KEY, KSA_INTERFACE_ENUM_TIMEOUT_REG_VALUE, &individualInterfaceEnumTimeoutMS)))
    {
        individualInterfaceEnumTimeoutMS = max(individualInterfaceEnumTimeoutMS, KSA_INTERFACE_ENUM_TIMEOUT_MS_MINIMUM_VALUE);
        individualInterfaceEnumTimeoutMS = min(individualInterfaceEnumTimeoutMS, KSA_INTERFACE_ENUM_TIMEOUT_MS_MAXIMUM_VALUE);

        m_individualInterfaceEnumTimeoutMS = individualInterfaceEnumTimeoutMS;
    }
    else
    {
        m_individualInterfaceEnumTimeoutMS = DEFAULT_KSA_INTERFACE_ENUM_TIMEOUT_MS;
    }


    // the ksa2603 fix enumerates device interfaces instead of parent devices

    winrt::hstring deviceInterfaceSelector(
        L"System.Devices.InterfaceClassGuid:=\"{6994AD04-93EF-11D0-A3CC-00A0C9223196}\" AND " \
        L"System.Devices.InterfaceEnabled: = System.StructuredQueryType.Boolean#True");

    auto additionalProps = winrt::single_threaded_vector<winrt::hstring>();
    additionalProps.Append(L"System.Devices.Parent");

    m_watcher = DeviceInformation::CreateWatcher(deviceInterfaceSelector);

    auto deviceAddedHandler = TypedEventHandler<DeviceWatcher, DeviceInformation>(this, &CMidi2KSAggregateMidiEndpointManager2::OnFilterDeviceInterfaceAdded);
    auto deviceRemovedHandler = TypedEventHandler<DeviceWatcher, DeviceInformationUpdate>(this, &CMidi2KSAggregateMidiEndpointManager2::OnFilterDeviceInterfaceRemoved);
    auto deviceUpdatedHandler = TypedEventHandler<DeviceWatcher, DeviceInformationUpdate>(this, &CMidi2KSAggregateMidiEndpointManager2::OnFilterDeviceInterfaceUpdated);

    auto deviceStoppedHandler = TypedEventHandler<DeviceWatcher, winrt::Windows::Foundation::IInspectable>(this, &CMidi2KSAggregateMidiEndpointManager2::OnDeviceWatcherStopped);
    auto deviceEnumerationCompletedHandler = TypedEventHandler<DeviceWatcher, winrt::Windows::Foundation::IInspectable>(this, &CMidi2KSAggregateMidiEndpointManager2::OnEnumerationCompleted);

    m_DeviceAdded = m_watcher.Added(winrt::auto_revoke, deviceAddedHandler);
    m_DeviceRemoved = m_watcher.Removed(winrt::auto_revoke, deviceRemovedHandler);
    m_DeviceUpdated = m_watcher.Updated(winrt::auto_revoke, deviceUpdatedHandler);
    m_DeviceStopped = m_watcher.Stopped(winrt::auto_revoke, deviceStoppedHandler);
    m_DeviceEnumerationCompleted = m_watcher.EnumerationCompleted(winrt::auto_revoke, deviceEnumerationCompletedHandler);

    // worker thread to handle endpoint creation, since we're enumerating interfaces now and need to aggregate them
    m_endpointCreationThreadWakeup.create(wil::EventOptions::ManualReset);
    std::jthread endpointCreationWorkerThread(std::bind_front(&CMidi2KSAggregateMidiEndpointManager2::EndpointCreationThreadWorker, this));
    m_endpointCreationThread = std::move(endpointCreationWorkerThread);

    m_watcher.Start();

    // Wait for everything to be created so that they're available immediately after service start.
    m_EnumerationCompleted.wait(INITIAL_ENUMERATION_TIMEOUT_MS);

    if (Feature_Servicing_MIDI2VirtualPortDriversFix::IsEnabled())
    {
        if (m_pendingEndpointDefinitions.size() > 0)
        {
            TraceLoggingWrite(
                MidiKSAggregateTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Enumeration completed with endpoint definitions left pending.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingUInt32(static_cast<uint32_t>(m_pendingEndpointDefinitions.size()), "count pending definitions")
            );
        }
    }

    return S_OK;
}


typedef struct {
    BYTE GroupIndex;        // index (0-15) of the group this pin maps to. It's also use when deciding on WinMM names
    UINT32 PinId;           // KS Pin number
    MidiFlow PinDataFlow;   // an input pin is MidiFlowIn, and from the user's perspective, a MIDI Output
    std::wstring FilterId;  // full filter id for this pin
} PinMapEntryStagingEntry2;


_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager2::UpdateNameTableWithCustomProperties(
    std::shared_ptr<KsAggregateEndpointDefinition2> masterEndpointDefinition,
    std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties> customProperties)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, masterEndpointDefinition);
    RETURN_HR_IF_NULL_EXPECTED(S_OK, customProperties);
    RETURN_HR_IF(S_OK, customProperties->Midi1Destinations.size() == 0 && customProperties->Midi1Sources.size() == 0);

    for (auto const& pinEntry : masterEndpointDefinition->MidiPins)
    {
        if (pinEntry->PinDataFlow == MidiFlow::MidiFlowIn)
        {
            // message destination (output port), pin flow is In
            if (auto customConfiguredName = customProperties->Midi1Destinations.find(pinEntry->GroupIndex);
                customConfiguredName != customProperties->Midi1Destinations.end())
            {
                TraceLoggingWrite(
                    MidiKSAggregateTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Found custom name for a Midi 1 destination.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(masterEndpointDefinition->EndpointDeviceInstanceId.c_str(), MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD),
                    TraceLoggingWideString(customConfiguredName->second.Name.c_str(), "custom name"),
                    TraceLoggingUInt8(pinEntry->GroupIndex, "group index")
                );

                masterEndpointDefinition->EndpointNameTable.UpdateDestinationEntryCustomName(pinEntry->GroupIndex, customConfiguredName->second.Name);
            }
        }
        else if (pinEntry->PinDataFlow == MidiFlow::MidiFlowOut)
        {
            // message source (input port), pin flow is Out
            if (auto customConfiguredName = customProperties->Midi1Sources.find(pinEntry->GroupIndex);
                customConfiguredName != customProperties->Midi1Sources.end())
            {
                TraceLoggingWrite(
                    MidiKSAggregateTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Found custom name for a Midi 1 source.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(masterEndpointDefinition->EndpointDeviceInstanceId.c_str(), MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD),
                    TraceLoggingWideString(customConfiguredName->second.Name.c_str(), "custom name"),
                    TraceLoggingUInt8(pinEntry->GroupIndex, "group index")
                );

                masterEndpointDefinition->EndpointNameTable.UpdateSourceEntryCustomName(pinEntry->GroupIndex, customConfiguredName->second.Name);
            }
        }
    }

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager2::BuildPinsAndGroupTerminalBlocksPropertyData(
    std::shared_ptr<KsAggregateEndpointDefinition2> masterEndpointDefinition,
    std::vector<std::byte>& pinMapPropertyData,
    std::vector<internal::GroupTerminalBlockInternal>& groupTerminalBlocks)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, masterEndpointDefinition);

    uint8_t currentBlockNumber{ 0 };
    std::vector<PinMapEntryStagingEntry2> pinMapEntries{ };

    for (auto const& pin : masterEndpointDefinition->MidiPins)
    {
        RETURN_HR_IF(E_INVALIDARG, pin->FilterDeviceId.empty());

        internal::GroupTerminalBlockInternal gtb;

        gtb.Number = ++currentBlockNumber;
        gtb.GroupCount = 1; // always a single group for aggregate MIDI 1.0 devices

        PinMapEntryStagingEntry2 pinMapEntry{ };

        pinMapEntry.PinId = pin->PinNumber;
        pinMapEntry.FilterId = pin->FilterDeviceId;
        pinMapEntry.PinDataFlow = pin->PinDataFlow;

        //MidiFlow flowFromUserPerspective;

        pinMapEntry.GroupIndex = pin->GroupIndex;
        gtb.FirstGroupIndex = pin->GroupIndex;

        if (pin->PinDataFlow == MidiFlow::MidiFlowIn)  // pin in, so user out : A MIDI Destination
        {
            gtb.Direction = MIDI_GROUP_TERMINAL_BLOCK_INPUT;   // from the pin/gtb's perspective

            auto nameTableEntry = masterEndpointDefinition->EndpointNameTable.GetDestinationEntry(gtb.FirstGroupIndex);
            if (nameTableEntry != nullptr && nameTableEntry->NewStyleName[0] != static_cast<wchar_t>(0))
            {
                gtb.Name = internal::TrimmedWStringCopy(nameTableEntry->NewStyleName);
            }
        }
        else if (pin->PinDataFlow == MidiFlow::MidiFlowOut)  // pin out, so user in : A MIDI Source
        {
            gtb.Direction = MIDI_GROUP_TERMINAL_BLOCK_OUTPUT;   // from the pin/gtb's perspective
            auto nameTableEntry = masterEndpointDefinition->EndpointNameTable.GetSourceEntry(gtb.FirstGroupIndex);
            if (nameTableEntry != nullptr && nameTableEntry->NewStyleName[0] != static_cast<wchar_t>(0))
            {
                gtb.Name = internal::TrimmedWStringCopy(nameTableEntry->NewStyleName);
            }
        }
        else
        {
            RETURN_IF_FAILED(E_INVALIDARG);
        }

        // name fallback
        if (gtb.Name.empty())
        {
            gtb.Name = masterEndpointDefinition->EndpointName;

            if (gtb.FirstGroupIndex > 0)
            {
                gtb.Name += L" " + std::wstring{ gtb.FirstGroupIndex };
            }
        }

        // default values as defined in the MIDI 2.0 USB spec
        gtb.Protocol = 0x01;                // midi 1.0
        gtb.MaxInputBandwidth = 0x0001;     // 31.25 kbps
        gtb.MaxOutputBandwidth = 0x0001;    // 31.25 kbps

        groupTerminalBlocks.push_back(gtb);
        pinMapEntries.push_back(pinMapEntry);
    }

    // Write Pin Map Property
    // =====================================================

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Building pin map property", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(masterEndpointDefinition->EndpointName.c_str(), "name")
    );

    // build the pin map property value
    KSAGGMIDI_PIN_MAP_PROPERTY_VALUE pinMap{ };

    size_t totalStringSizesIncludingNulls{ 0 };
    for (auto const& entry : pinMapEntries)
    {
        totalStringSizesIncludingNulls += ((entry.FilterId.length() + 1) * sizeof(wchar_t));
    }

    size_t totalMemoryBytes{
        SIZET_KSAGGMIDI_PIN_MAP_PROPERTY_VALUE_HEADER +
        SIZET_KSAGGMIDI_PIN_MAP_PROPERTY_ENTRY_WITHOUT_STRING * pinMapEntries.size() +
        totalStringSizesIncludingNulls };

    pinMapPropertyData.resize(totalMemoryBytes);
    auto currentPos = pinMapPropertyData.data();

    // header
    auto pinMapHeader = (PKSAGGMIDI_PIN_MAP_PROPERTY_VALUE)currentPos;
    pinMapHeader->TotalByteCount = (UINT32)totalMemoryBytes;
    currentPos += SIZET_KSAGGMIDI_PIN_MAP_PROPERTY_VALUE_HEADER;

    for (auto const& entry : pinMapEntries)
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Processing Pin Map entry", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(masterEndpointDefinition->EndpointName.c_str(), "name"),
            TraceLoggingUInt32(entry.PinId, "Pin Id"),
            TraceLoggingWideString(entry.FilterId.c_str(), "Filter Id")
        );

        PKSAGGMIDI_PIN_MAP_PROPERTY_ENTRY propEntry = (PKSAGGMIDI_PIN_MAP_PROPERTY_ENTRY)currentPos;

        propEntry->ByteCount = (UINT)(SIZET_KSAGGMIDI_PIN_MAP_PROPERTY_ENTRY_WITHOUT_STRING + ((entry.FilterId.length() + 1) * sizeof(wchar_t)));
        propEntry->GroupIndex = entry.GroupIndex;
        propEntry->PinDataFlow = entry.PinDataFlow;
        propEntry->PinId = entry.PinId;

        if (!entry.FilterId.empty())
        {
            wcscpy_s((wchar_t*)propEntry->FilterId, entry.FilterId.length() + 1, entry.FilterId.c_str());
        }

        currentPos += propEntry->ByteCount;
    }

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"All pin map entries copied to property memory", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(masterEndpointDefinition->EndpointName.c_str(), "name")
    );


    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager2::DeviceCreateMidiUmpEndpoint(
    std::shared_ptr<KsAggregateEndpointDefinition2> endpointDefinition    
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, endpointDefinition);

    std::shared_ptr<KsAggregateParentDeviceDefinition2> parentDevice { nullptr };
    RETURN_IF_FAILED(FindExistingParentDeviceDefinitionForEndpoint(endpointDefinition, parentDevice));
    RETURN_HR_IF_NULL(E_UNEXPECTED, parentDevice);

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointDefinition->EndpointName.c_str(), "endpoint name"),
        TraceLoggingWideString(parentDevice->DeviceName.c_str(), "device name")
    );

    DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;

    // we require at least one valid pin, and no more than 32 total pins (16 in, 16 out, max)
    RETURN_HR_IF(E_INVALIDARG, endpointDefinition->MidiPins.size() < 1);
    RETURN_HR_IF(E_INVALIDARG, endpointDefinition->MidiPins.size() > 32);

    std::vector<DEVPROPERTY> interfaceDevProperties;

    MIDIENDPOINTCOMMONPROPERTIES commonProperties{};
    commonProperties.TransportId = TRANSPORT_LAYER_GUID;
    commonProperties.EndpointDeviceType = MidiEndpointDeviceType_Normal;
    commonProperties.FriendlyName = endpointDefinition->EndpointName.c_str();
    commonProperties.TransportCode = TRANSPORT_CODE;
    commonProperties.EndpointName = endpointDefinition->EndpointName.c_str();
    commonProperties.EndpointDescription = nullptr;
    commonProperties.CustomEndpointName = nullptr;
    commonProperties.CustomEndpointDescription = nullptr;
    commonProperties.UniqueIdentifier = parentDevice->SerialNumber.empty() ? nullptr : parentDevice->SerialNumber.c_str();
    commonProperties.ManufacturerName = parentDevice->ManufacturerName.empty() ? nullptr : parentDevice->ManufacturerName.c_str();
    commonProperties.SupportedDataFormats = MidiDataFormats::MidiDataFormats_UMP;
    commonProperties.NativeDataFormat = MidiDataFormats_ByteStream;

    UINT32 capabilities{ 0 };
    capabilities |= MidiEndpointCapabilities_SupportsMultiClient;
    capabilities |= MidiEndpointCapabilities_GenerateIncomingTimestamps;
    capabilities |= MidiEndpointCapabilities_SupportsMidi1Protocol;
    commonProperties.Capabilities = (MidiEndpointCapabilities)capabilities;


    std::vector<std::byte> pinMapPropertyData;
    std::vector<internal::GroupTerminalBlockInternal> groupTerminalBlocks{ };
    std::vector<std::byte> nameTablePropertyData;

    RETURN_IF_FAILED(BuildPinsAndGroupTerminalBlocksPropertyData(
        endpointDefinition,
        pinMapPropertyData,
        groupTerminalBlocks));


    interfaceDevProperties.push_back({ { DEVPKEY_KsAggMidiGroupPinMap, DEVPROP_STORE_SYSTEM, nullptr },
        DEVPROP_TYPE_BINARY, static_cast<uint32_t>(pinMapPropertyData.size()), pinMapPropertyData.data() });


    // Write Group Terminal Block Property
    // =====================================================

    std::vector<std::byte> groupTerminalBlockPropertyData{};

    if (internal::WriteGroupTerminalBlocksToPropertyDataPointer(groupTerminalBlocks, groupTerminalBlockPropertyData))
    {
        interfaceDevProperties.push_back({ { PKEY_MIDI_GroupTerminalBlocks, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BINARY, static_cast<uint32_t>(groupTerminalBlockPropertyData.size()),
            (PVOID)groupTerminalBlockPropertyData.data() });
    }
    else
    {
        // write empty data
        interfaceDevProperties.push_back({ { PKEY_MIDI_GroupTerminalBlocks, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_EMPTY, 0, nullptr });
    }


    // Fold in custom properties, including MIDI 1 port names and naming approach
    // ===============================================================================

    WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria matchCriteria{};
    matchCriteria.DeviceInstanceId = internal::NormalizeDeviceInstanceIdWStringCopy(endpointDefinition->EndpointDeviceInstanceId);
    matchCriteria.UsbVendorId = parentDevice->VID;
    matchCriteria.UsbProductId = parentDevice->PID;
    matchCriteria.UsbSerialNumber = parentDevice->SerialNumber;
    matchCriteria.TransportSuppliedEndpointName = endpointDefinition->EndpointName;

    auto customProperties = TransportState::Current().GetConfigurationManager()->CustomPropertiesCache()->GetProperties(matchCriteria);

    // rebuild the name table, using the custom properties if present
    RETURN_IF_FAILED(UpdateNameTableWithCustomProperties(endpointDefinition, customProperties));

    std::wstring customName{ };
    std::wstring customDescription{ };
    if (customProperties != nullptr)
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Found custom properties cached for this endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(endpointDefinition->EndpointDeviceInstanceId.c_str(), MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD),
            TraceLoggingUInt32(static_cast<uint32_t>(customProperties->Midi1Sources.size()), "MIDI 1 Source count"),
            TraceLoggingUInt32(static_cast<uint32_t>(customProperties->Midi1Destinations.size()), "MIDI 1 Destination count")
        );

        if (!customProperties->Name.empty())
        {
            customName = customProperties->Name;
            commonProperties.CustomEndpointName = customName.c_str();
        }

        if (!customProperties->Description.empty())
        {
            customDescription = customProperties->Description;
            commonProperties.CustomEndpointDescription = customDescription.c_str();
        }

        // this includes image, the Midi 1 naming approach, etc.
        customProperties->WriteNonCommonProperties(interfaceDevProperties);
    }
    else
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"No cached custom properties for this endpoint.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(endpointDefinition->EndpointDeviceInstanceId.c_str(), MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD)
        );
    }

    // Write Name table property, folding in the custom names we discovered earlier
    // ===============================================================================================
    RETURN_IF_FAILED(UpdateNameTableWithCustomProperties(endpointDefinition, customProperties));
    endpointDefinition->EndpointNameTable.WriteProperties(interfaceDevProperties);


    // Write USB VID/PID Data
    // =====================================================

    if (parentDevice->VID > 0)
    {
        interfaceDevProperties.push_back({ { PKEY_MIDI_UsbVID, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_UINT16, static_cast<ULONG>(sizeof(UINT16)), (PVOID)&parentDevice->VID });
    }
    else
    {
        interfaceDevProperties.push_back({ { PKEY_MIDI_UsbVID, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_EMPTY, 0, nullptr });
    }

    if (parentDevice->PID > 0)
    {
        interfaceDevProperties.push_back({ { PKEY_MIDI_UsbPID, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_UINT16, static_cast<ULONG>(sizeof(UINT16)), (PVOID)&parentDevice->PID });
    }
    else
    {
        interfaceDevProperties.push_back({ { PKEY_MIDI_UsbPID, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_EMPTY, 0, nullptr });
    }


    // Despite being a MIDI 1 device, we present as a UMP endpoint, so we need to set 
    // this property so the service can create the MIDI 1 ports without waiting for 
    // function blocks/discovery to complete or timeout (which it never will)
    interfaceDevProperties.push_back({ { PKEY_MIDI_EndpointDiscoveryProcessComplete, DEVPROP_STORE_SYSTEM, nullptr },
        DEVPROP_TYPE_BOOLEAN, (ULONG)sizeof(devPropTrue), (PVOID)&devPropTrue });

    SW_DEVICE_CREATE_INFO createInfo{ };

    createInfo.cbSize = sizeof(createInfo);
    createInfo.pszInstanceId = endpointDefinition->EndpointDeviceInstanceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = endpointDefinition->EndpointName.c_str();

    // Call the device manager and finish the creation

    HRESULT swdCreationResult;
    wil::unique_cotaskmem_string newDeviceInterfaceId;

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Activating endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointDefinition->EndpointName.c_str(), "name")
    );

    // set to true if we only want to create UMP endpoints
    bool umpOnly = false;

    LOG_IF_FAILED(
        swdCreationResult = m_midiDeviceManager->ActivateEndpoint(
            parentDevice->DeviceInstanceId.c_str(),
            umpOnly,
            MidiFlow::MidiFlowBidirectional,            // bidi only for the UMP endpoint
            &commonProperties,
            (ULONG)interfaceDevProperties.size(),
            (ULONG)0,
            interfaceDevProperties.data(),
            nullptr,
            &createInfo,
            &newDeviceInterfaceId)
    );

    if (SUCCEEDED(swdCreationResult))
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Aggregate UMP endpoint created", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(endpointDefinition->EndpointName.c_str(), "name"),
            TraceLoggingWideString(newDeviceInterfaceId.get(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        // return new device interface id
        endpointDefinition->EndpointDeviceId = internal::NormalizeEndpointInterfaceIdWStringCopy(std::wstring{ newDeviceInterfaceId.get() });

        auto lock = m_activatedEndpointDefinitionsLock.lock();

        // Add to internal endpoint manager
        m_activatedEndpointDefinitions.insert_or_assign(
            internal::NormalizeDeviceInstanceIdWStringCopy(parentDevice->DeviceInstanceId),
            endpointDefinition);

        return swdCreationResult;
    }
    else
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Aggregate UMP endpoint creation failed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(endpointDefinition->EndpointName.c_str(), "name"),
            TraceLoggingHResult(swdCreationResult, MIDI_TRACE_EVENT_HRESULT_FIELD)
        );

        return swdCreationResult;
    }
}



_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager2::DeviceUpdateExistingMidiUmpEndpointWithFilterChanges(
    std::shared_ptr<KsAggregateEndpointDefinition2> endpointDefinition
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, endpointDefinition);

    std::shared_ptr<KsAggregateParentDeviceDefinition2> parentDevice{ nullptr };
    RETURN_IF_FAILED(FindExistingParentDeviceDefinitionForEndpoint(endpointDefinition, parentDevice));
    RETURN_HR_IF_NULL(E_UNEXPECTED, parentDevice);


    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointDefinition->EndpointName.c_str(), "name")
    );

    // we require at least one valid pin
    RETURN_HR_IF(E_INVALIDARG, endpointDefinition->MidiPins.size() < 1);

    std::vector<DEVPROPERTY> interfaceDevProperties{ };

    std::vector<std::byte> pinMapPropertyData;
    std::vector<internal::GroupTerminalBlockInternal> groupTerminalBlocks{ };
    std::vector<std::byte> nameTablePropertyData;

    // update the pin map to have all the existing pins
    // plus the new pins. Update Group Terminal Blocks at the same time.
    RETURN_IF_FAILED(BuildPinsAndGroupTerminalBlocksPropertyData(
        endpointDefinition,
        pinMapPropertyData,
        groupTerminalBlocks));


    // Write Pin Map Property
    // =====================================================
    interfaceDevProperties.push_back({ { DEVPKEY_KsAggMidiGroupPinMap, DEVPROP_STORE_SYSTEM, nullptr },
        DEVPROP_TYPE_BINARY, static_cast<uint32_t>(pinMapPropertyData.size()), pinMapPropertyData.data() });


    // Write Group Terminal Block Property
    // =====================================================

    std::vector<std::byte> groupTerminalBlockData;

    if (internal::WriteGroupTerminalBlocksToPropertyDataPointer(groupTerminalBlocks, groupTerminalBlockData))
    {
        interfaceDevProperties.push_back({ { PKEY_MIDI_GroupTerminalBlocks, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BINARY, (ULONG)groupTerminalBlockData.size(), (PVOID)groupTerminalBlockData.data() });
    }
    else
    {
        // write empty data
    }


    // Fold in custom properties, including MIDI 1 port names and naming approach
    // ===============================================================================

    WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria matchCriteria{};
    matchCriteria.DeviceInstanceId = internal::NormalizeDeviceInstanceIdWStringCopy(endpointDefinition->EndpointDeviceInstanceId);
    matchCriteria.UsbVendorId = parentDevice->VID;
    matchCriteria.UsbProductId = parentDevice->PID;
    matchCriteria.UsbSerialNumber = parentDevice->SerialNumber;
    matchCriteria.TransportSuppliedEndpointName = endpointDefinition->EndpointName;

    auto customProperties = TransportState::Current().GetConfigurationManager()->CustomPropertiesCache()->GetProperties(matchCriteria);

    std::wstring customName{ };
    std::wstring customDescription{ };
    if (customProperties != nullptr)
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Found custom properties cached for this endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(endpointDefinition->EndpointDeviceInstanceId.c_str(), MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD),
            TraceLoggingUInt32(static_cast<uint32_t>(customProperties->Midi1Sources.size()), "MIDI 1 Source count"),
            TraceLoggingUInt32(static_cast<uint32_t>(customProperties->Midi1Destinations.size()), "MIDI 1 Destination count")
        );

        // this includes image, the Midi 1 naming approach, etc.
        customProperties->WriteNonCommonProperties(interfaceDevProperties);
    }
    else
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"No cached custom properties for this endpoint.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(endpointDefinition->EndpointDeviceInstanceId.c_str(), MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD)
        );
    }

    // store the property data for the name table
    endpointDefinition->EndpointNameTable.WriteProperties(interfaceDevProperties);


    // Write Name table property, folding in the custom names we discovered earlier
    // ===============================================================================================
    RETURN_IF_FAILED(UpdateNameTableWithCustomProperties(endpointDefinition, customProperties));
    endpointDefinition->EndpointNameTable.WriteProperties(interfaceDevProperties);

    HRESULT updateResult{};

    LOG_IF_FAILED(updateResult = m_midiDeviceManager->UpdateEndpointProperties(
        endpointDefinition->EndpointDeviceId.c_str(),
        static_cast<ULONG>(interfaceDevProperties.size()),
        interfaceDevProperties.data()
    ));


    if (SUCCEEDED(updateResult))
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Aggregate UMP endpoint updated with new filter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(endpointDefinition->EndpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        auto lock = m_activatedEndpointDefinitionsLock.lock();

        // Add to internal endpoint manager
        m_activatedEndpointDefinitions.insert_or_assign(
            internal::NormalizeDeviceInstanceIdWStringCopy(parentDevice->DeviceInstanceId),
            endpointDefinition);

    }
    else
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Aggregate UMP endpoint update failed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(endpointDefinition->EndpointName.c_str(), "name"),
            TraceLoggingHResult(updateResult, MIDI_TRACE_EVENT_HRESULT_FIELD)
        );
    }

    return updateResult;
}

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager2::GetPinName(HANDLE const hFilter, UINT const pinIndex, std::wstring& pinName)
{
    std::unique_ptr<WCHAR> pinNameData;
    ULONG pinNameDataSize{ 0 };

    auto pinNameHR = PinPropertyAllocate(
        hFilter,
        pinIndex,
        KSPROPSETID_Pin,
        KSPROPERTY_PIN_NAME,
        (PVOID*)&pinNameData,
        &pinNameDataSize
    );

    if (SUCCEEDED(pinNameHR) || pinNameHR == HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND))
    {
        // Check to see if the pin has an iJack name
        if (pinNameDataSize > 0)
        {
            pinName = pinNameData.get();
            
            return S_OK;
        }
    }

    return E_FAIL;
}

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager2::GetPinDataFlow(_In_ HANDLE const hFilter, _In_ UINT const pinIndex, _Inout_ KSPIN_DATAFLOW& dataFlow)
{
    auto dataFlowHR = PinPropertySimple(
        hFilter,
        pinIndex,
        KSPROPSETID_Pin,
        KSPROPERTY_PIN_DATAFLOW,
        &dataFlow,
        sizeof(KSPIN_DATAFLOW)
    );

    if (SUCCEEDED(dataFlowHR))
    {
        return S_OK;
    }

    return E_FAIL;
}


_Use_decl_annotations_
HRESULT 
CMidi2KSAggregateMidiEndpointManager2::GetKSDriverSuppliedName(HANDLE hInstantiatedFilter, std::wstring& name)
{
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // get the name GUID

    KSCOMPONENTID componentId{};
    KSPROPERTY prop{};
    ULONG countBytesReturned{};

    prop.Id = KSPROPERTY_GENERAL_COMPONENTID;
    prop.Set = KSPROPSETID_General;
    prop.Flags = KSPROPERTY_TYPE_GET;

    auto hrComponent = SyncIoctl(
        hInstantiatedFilter,
        IOCTL_KS_PROPERTY,
        &prop,
        sizeof(KSPROPERTY),
        &componentId,
        sizeof(KSCOMPONENTID),
        &countBytesReturned
    );

    if (Feature_Servicing_MIDI2VirtualPortDriversFix::IsEnabled())
    {
        // changed to not log the failure here. Failures are expected for many devices, and it's adding noise to error logs
        if (FAILED(hrComponent))
        {
            return hrComponent;
        }
    }
    else
    {
        RETURN_IF_FAILED(hrComponent);
    }

    componentId.Name;   // this is the GUID which points to the registry location with the driver-supplied name

    if (componentId.Name != GUID_NULL)
    {
        // we have the GUID where this name is stored, so get the driver-supplied name from the registry

        WCHAR nameFromRegistry[MAX_PATH]{ 0 };   // this should only be MAXPNAMELEN, but if someone tampered with it, could be larger, hence MAX_PATH

        std::wstring regKey = L"SYSTEM\\CurrentControlSet\\Control\\MediaCategories\\" + internal::GuidToString(componentId.Name);

        if (SUCCEEDED(wil::reg::get_value_string_nothrow(HKEY_LOCAL_MACHINE, regKey.c_str(), L"Name", nameFromRegistry)))
        {
            name = nameFromRegistry;
        }

        return S_OK;
    }

    return E_NOTFOUND;
}



#define KS_CATEGORY_AUDIO_GUID L"{6994AD04-93EF-11D0-A3CC-00A0C9223196}"



_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager2::ParseParentIdIntoVidPidSerial(
    std::wstring systemDevicesParentValue, 
    std::shared_ptr<KsAggregateParentDeviceDefinition2> parentDevice)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, parentDevice);

    if (systemDevicesParentValue.empty())
    {
        RETURN_IF_FAILED(E_INVALIDARG);
    }


    // Examples
    // ---------------------------------------------------------------------------
    // Parent values with serial:    USB\VID_16C0&PID_05E4\ContinuuMini_SN024066
    //                               USB\VID_2573&PID_008A\no_serial_number (yes, this is the iSerialNumber in USB :/
    //                                        0x03	iSerialNumber   "no serial number"
    //                               USB\VID_12E6&PID_002C\251959d4f21fc283
    // 
    // Parent values without serial: USB\VID_F055&PID_0069\8&2858bbac&0&4
    //                               USB\VID_2662&PID_000D\8&24eb0394&0&4
    //                               USB\VID_05E3&PID_0610\7&2f028fc9&0&4
    //                               ROOT\MOTUBUS\0000

    std::wstring parentVal = systemDevicesParentValue.c_str();

    std::wstringstream ss(parentVal);
    std::wstring usbSection{};

    std::getline(ss, usbSection, static_cast<wchar_t>('\\'));

    if (usbSection == L"USB")
    {
        // get the VID/PID section

        std::wstring vidPidSection{};

        std::getline(ss, vidPidSection, static_cast<wchar_t>('\\'));

        if (!vidPidSection.empty())
        {
            std::wstring serialSection{};
            std::getline(ss, serialSection, static_cast<wchar_t>('\\'));

            std::wstring vidPidString1{};
            std::wstring vidPidString2{};

            std::wstringstream ssVidPid(vidPidSection);
            std::getline(ssVidPid, vidPidString1, static_cast<wchar_t>('&'));
            std::getline(ssVidPid, vidPidString2, static_cast<wchar_t>('&'));

            wchar_t* end{ nullptr };

            // find the VID
            if (vidPidString1.starts_with(L"VID_"))
            {               
                parentDevice->VID = static_cast<uint16_t>(wcstol(vidPidString1.substr(4).c_str(), &end, 16));
            }
            else if (vidPidString2.starts_with(L"VID_"))
            {
                parentDevice->VID = static_cast<uint16_t>(wcstol(vidPidString2.substr(4).c_str(), &end, 16));
            }

            // find the PID
            if (vidPidString1.starts_with(L"PID_"))
            {
                parentDevice->PID = static_cast<uint16_t>(wcstol(vidPidString1.substr(4).c_str(), &end, 16));
            }
            else if (vidPidString2.starts_with(L"PID_"))
            {
                parentDevice->PID = static_cast<uint16_t>(wcstol(vidPidString2.substr(4).c_str(), &end, 16));
            }

            // serial numbers with a & in them, are generated by our system
            // it's possible a vendor may have a serial number with this in it,
            // but in that case, we just ditch it.
            if (serialSection.find_first_of('&') == serialSection.npos)
            {
                // Windows replaces spaces in the serial number with the underscore.
                // yes, this will end up catching the few (if any) serials that do 
                // actually include an underscore. However, there are a bunch with spaces.
                std::replace(serialSection.begin(), serialSection.end(), '_', ' ');
                parentDevice->SerialNumber = serialSection;
            }
        }
    }
    else
    {
        // not a USB device, or otherwise uses a custom driver. We can't count
        // on being able to parse the parent id. Example: MOTU has
        // ROOT\MOTUBUS\0000 as the parent
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager2::FindPendingEndpointDefinitionForParentDevice(
    std::wstring parentDeviceInstanceId,
    std::shared_ptr<KsAggregateEndpointDefinition2>& endpointDefinition)
{
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(parentDeviceInstanceId.c_str(), "parent deviec instance id")
    );

    auto cleanParentDeviceInstanceId = internal::NormalizeDeviceInstanceIdWStringCopy(parentDeviceInstanceId);

    for (auto const& endpoint : m_pendingEndpointDefinitions)
    {
        if (internal::NormalizeDeviceInstanceIdWStringCopy(endpoint->ParentDeviceInstanceId) == cleanParentDeviceInstanceId)
        {
            TraceLoggingWrite(
                MidiKSAggregateTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_VERBOSE,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Match found", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(parentDeviceInstanceId.c_str(), "parent deviec instance id")
            );

            endpointDefinition = endpoint;
            return S_OK;
        }
    }

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"No match found", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(parentDeviceInstanceId.c_str(), "parent deviec instance id")
    );

    endpointDefinition = nullptr;

    return E_NOTFOUND;
}


_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager2::FindActivatedEndpointDefinitionForFilterDevice(
    std::wstring filterDeviceId,
    std::shared_ptr<KsAggregateEndpointDefinition2>& endpointDefinition
)
{
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(filterDeviceId.c_str(), "filter device id")
    );

    auto cleanFilterDeviceId = internal::NormalizeEndpointInterfaceIdWStringCopy(filterDeviceId);

    for (auto const& endpoint : m_activatedEndpointDefinitions)
    {
        for (auto const& pin: endpoint.second->MidiPins)
        {
            TraceLoggingWrite(
                MidiKSAggregateTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_VERBOSE,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Matching Endpoint found", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(cleanFilterDeviceId.c_str(), "filter device id")
            );

            if (internal::NormalizeEndpointInterfaceIdWStringCopy(pin->FilterDeviceId) == cleanFilterDeviceId)
            {
                endpointDefinition = endpoint.second;
                return S_OK;
            }
        }
    }

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"No match found", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(cleanFilterDeviceId.c_str(), "filter device id")
    );

    endpointDefinition = nullptr;

    return E_NOTFOUND;
}

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager2::FindAllActivatedEndpointDefinitionsForParentDevice(
    std::wstring parentDeviceInstanceId,
    std::vector<std::shared_ptr<KsAggregateEndpointDefinition2>>& endpointDefinitions
)
{
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(parentDeviceInstanceId.c_str(), "parent device instance id")
    );

    auto cleanParentDeviceInstanceId = internal::NormalizeDeviceInstanceIdWStringCopy(parentDeviceInstanceId);

    bool found { false };

    for (auto const& endpoint : m_activatedEndpointDefinitions)
    {
        if (internal::NormalizeDeviceInstanceIdWStringCopy(endpoint.second->ParentDeviceInstanceId) == cleanParentDeviceInstanceId)
        {
            endpointDefinitions.push_back(endpoint.second);
            found = true;
        }
    }


    if (found)
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_VERBOSE,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"One or more matching endpoints found", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(cleanParentDeviceInstanceId.c_str(), "parent device instance id")
        );

        return S_OK;
    }
    else
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_VERBOSE,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"No matches found", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(cleanParentDeviceInstanceId.c_str(), "parent device instance id")
        );

        return E_NOTFOUND;
    }

}



_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager2::FindExistingParentDeviceDefinitionForEndpoint(
    std::shared_ptr<KsAggregateEndpointDefinition2> endpointDefinition,
    std::shared_ptr<KsAggregateParentDeviceDefinition2>& parentDeviceDefinition
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, endpointDefinition);

    auto cleanDeviceInstanceId = internal::NormalizeDeviceInstanceIdWStringCopy(endpointDefinition->ParentDeviceInstanceId);

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Looking for matching parent", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(cleanDeviceInstanceId.c_str(), "parent device instance id")
    );

    if (auto parent = m_allParentDeviceDefinitions.find(cleanDeviceInstanceId); parent != m_allParentDeviceDefinitions.end())
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_VERBOSE,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Match found.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(cleanDeviceInstanceId.c_str(), "parent device instance id")
        );

        parentDeviceDefinition = parent->second;

        return S_OK;
    }

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"No match found", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(cleanDeviceInstanceId.c_str(), "parent device instance id")
    );

    return E_NOTFOUND;
}


_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager2::FindOrCreateParentDeviceDefinitionForFilterDevice(
    DeviceInformation filterDevice,
    std::shared_ptr<KsAggregateParentDeviceDefinition2>& parentDeviceDefinition
)
{
    // we require that the System.Devices.DeviceInstanceId property was requested for the passed-in filter device
    auto deviceInstanceId = internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(L"System.Devices.DeviceInstanceId", filterDevice, L"");
    RETURN_HR_IF(E_FAIL, deviceInstanceId.empty());

    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
    additionalProperties.Append(L"System.Devices.DeviceManufacturer");
    additionalProperties.Append(L"System.Devices.Manufacturer");
    additionalProperties.Append(L"System.Devices.Parent");

    auto parentDevice = DeviceInformation::CreateFromIdAsync(
        deviceInstanceId,
        additionalProperties, 
        winrt::Windows::Devices::Enumeration::DeviceInformationKind::Device).get();

   
    auto lock = m_allParentDeviceDefinitionsLock.lock();    // we lock to avoid having one inserted while we're processing

    auto cleanParentDeviceInstanceId = internal::NormalizeDeviceInstanceIdWStringCopy(parentDevice.Id().c_str());

    if (auto it = m_allParentDeviceDefinitions.find(cleanParentDeviceInstanceId); it != m_allParentDeviceDefinitions.end())
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_VERBOSE,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Found existing parent device.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(cleanParentDeviceInstanceId.c_str(), "parent")
        );

        // we found a matching parent device. Return it.
        parentDeviceDefinition = it->second;

        return S_OK;
    }

    // We don't have one, create one and add, and get all the parent device information for it
    // we still have the map locked, so keep this code fast

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Parent device definition not already created. Creating now.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(cleanParentDeviceInstanceId.c_str(), "parent")
    );

    auto newParentDeviceDefinition = std::make_shared<KsAggregateParentDeviceDefinition2>();
    RETURN_HR_IF_NULL(E_OUTOFMEMORY, newParentDeviceDefinition);

    newParentDeviceDefinition->DeviceName = parentDevice.Name();
    newParentDeviceDefinition->DeviceInstanceId = cleanParentDeviceInstanceId;

    LOG_IF_FAILED(ParseParentIdIntoVidPidSerial(newParentDeviceDefinition->DeviceInstanceId, newParentDeviceDefinition));

    // only some vendor drivers provide an actual manufacturer
    // and all the in-box drivers just provide the Generic USB Audio string
    // TODO: Is "Generic USB Audio" a string that is localized? If so, this code will not have the intended effect outside of en-US
    auto manufacturer = internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(L"System.Devices.DeviceManufacturer", parentDevice, L"");
    auto manufacturer2 = internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(L"System.Devices.Manufacturer", parentDevice, L"");
    if (!manufacturer.empty() && manufacturer != L"(Generic USB Audio)" && manufacturer != L"Microsoft")
    {
        newParentDeviceDefinition->ManufacturerName = manufacturer;
    }
    else if (!manufacturer2.empty() && manufacturer2 != L"(Generic USB Audio)" && manufacturer2 != L"Microsoft")
    {
        newParentDeviceDefinition->ManufacturerName = manufacturer2;
    }

    // Do we need to disambiguate this parent because another of the same device already exists?

    uint32_t currentMaxIndex { 0 };
    bool otherParentsWithSameNameExist{ false };

    for (auto const& existingParent : m_allParentDeviceDefinitions)
    {
        if (existingParent.second->DeviceName == newParentDeviceDefinition->DeviceName)
        {
            currentMaxIndex = max(currentMaxIndex, existingParent.second->IndexOfDevicesWithThisSameName);
            otherParentsWithSameNameExist = true;
        }
    }

    if (otherParentsWithSameNameExist)
    {
        parentDeviceDefinition->IndexOfDevicesWithThisSameName = currentMaxIndex + 1;
    }


    m_allParentDeviceDefinitions[newParentDeviceDefinition->DeviceInstanceId] = newParentDeviceDefinition;
    parentDeviceDefinition = newParentDeviceDefinition;

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Parent device definition added.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(newParentDeviceDefinition->DeviceInstanceId.c_str(), "key (device instance id)")
    );

    return S_OK;
}


_Use_decl_annotations_
HRESULT 
CMidi2KSAggregateMidiEndpointManager2::FindCurrentMaxEndpointIndexForParentDevice(
    std::shared_ptr<KsAggregateParentDeviceDefinition2> parentDeviceDefinition,
    uint32_t& currentMaxIndex)
{
    auto cleanParentDeviceInstanceId = internal::NormalizeDeviceInstanceIdWStringCopy(parentDeviceDefinition->DeviceInstanceId);

    int32_t maxIndex{ -1 };
    bool found{ false };

    auto activatedLock = m_activatedEndpointDefinitionsLock.lock();
    auto pendingLock = m_pendingEndpointDefinitionsLock.lock();

    // look through all pending and activated endpoints and find the max. 
    // If the max is 0 or greater, return it and set S_OK.
    // if no endpoints found, return E_NOTFOUND so the calling code
    // knows that the 0 is not in use

    for (auto const& ep : m_activatedEndpointDefinitions)
    {
        if (ep.second->ParentDeviceInstanceId == cleanParentDeviceInstanceId)
        {
            maxIndex++;
            found = true;
        }
    }

    for (auto const& ep : m_pendingEndpointDefinitions)
    {
        if (ep->ParentDeviceInstanceId == cleanParentDeviceInstanceId)
        {
            maxIndex++;
            found = true;
        }
    }


    if (found)
    {
        currentMaxIndex = static_cast<uint32_t>(maxIndex);
        return S_OK;
    }
    else
    {
        return E_NOTFOUND;
    }

}


_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager2::FindOrCreatePendingEndpointDefinitionForFilterDevice(
    DeviceInformation filterDevice,
    std::shared_ptr<KsAggregateEndpointDefinition2>& endpointDefinition
)
{
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(filterDevice.Id().c_str(), "filter device id")
    );

    std::shared_ptr<KsAggregateParentDeviceDefinition2> parentDeviceDefinition{ nullptr };

    // this function locks the parent device list for the duration of the call
    RETURN_IF_FAILED(FindOrCreateParentDeviceDefinitionForFilterDevice(
        filterDevice,
        parentDeviceDefinition
        ));

    RETURN_HR_IF_NULL(E_POINTER, parentDeviceDefinition);


    // See if we already have an endpoint with space for the number of groups we're going to add

    std::shared_ptr<KsAggregateEndpointDefinition2> existingEndpointDefinition { nullptr };
    if (SUCCEEDED(FindPendingEndpointDefinitionForParentDevice(parentDeviceDefinition->DeviceInstanceId, existingEndpointDefinition)))
    {
        RETURN_HR_IF_NULL(E_UNEXPECTED, existingEndpointDefinition);

        // TODO: Need to check to see if this endpoint has enough space for the new pins

        endpointDefinition = existingEndpointDefinition;

        return S_OK;
    }
    else
    {

        // create a new endpoint
        auto newEndpointDefinition = std::make_shared<KsAggregateEndpointDefinition2>();
        RETURN_HR_IF_NULL(E_POINTER, newEndpointDefinition);

        // We need to ensure each endpoint has a unique id. They can't all use the ParentDeviceInstanceId as the
        // instance id because now some devices will have multiple endpoints. Instead, we need to add a suffix to 
        // that. We need this to be deterministic and not just a random GUID/number, so that device ids have a 
        // chance to match up when next enumerated after a restart or connect/disconnect.

        auto parentLock = m_allParentDeviceDefinitionsLock.lock();

        uint32_t endpointIndexForThisParent{ 0 };
        if (SUCCEEDED(FindCurrentMaxEndpointIndexForParentDevice(parentDeviceDefinition, endpointIndexForThisParent)))
        {
            // increment the number here
            endpointIndexForThisParent++;
        }

        newEndpointDefinition->ParentDeviceInstanceId = parentDeviceDefinition->DeviceInstanceId;
        newEndpointDefinition->EndpointIndexForThisParentDevice = endpointIndexForThisParent;

        // default hash is the device id.
        std::hash<std::wstring> hasher;
        std::wstring hash;
        hash = std::to_wstring(hasher(parentDeviceDefinition->DeviceInstanceId));

        if (endpointIndexForThisParent == 0)
        {
            newEndpointDefinition->EndpointName = parentDeviceDefinition->DeviceName;
            newEndpointDefinition->EndpointDeviceInstanceId = TRANSPORT_INSTANCE_ID_PREFIX + hash;
        }
        else
        {
            // pad the string with "0" characters to the left of the number, up to 3 places total.
            // we +1 so the second one is _002 and not _001
            newEndpointDefinition->EndpointDeviceInstanceId = std::format(L"{0}{1}_{2:0>3}", TRANSPORT_INSTANCE_ID_PREFIX, hash, endpointIndexForThisParent + 1);

            // add the name disambiguator to the endpoint. We +1 to the index for the same reasons as above. 
            newEndpointDefinition->EndpointName = std::format(L"{0} ({1})", parentDeviceDefinition->DeviceName, endpointIndexForThisParent + 1);
            //newEndpointDefinition->EndpointName = std::format(L"{1} - {0}", parentDeviceDefinition->DeviceName, endpointIndexForThisParent + 1);
        }

        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Adding pending aggregate UMP endpoint.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        m_pendingEndpointDefinitions.push_back(newEndpointDefinition);
        endpointDefinition = newEndpointDefinition;

        return S_OK;
    }

}

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager2::IncrementAndGetNextGroupIndex(
    std::shared_ptr<KsAggregateEndpointDefinition2> definition,
    MidiFlow dataFlowFromUserPerspective,
    uint8_t& groupIndex)
{
    // the structure is initialized with -1 for the current group index in each direction

    if (dataFlowFromUserPerspective == MidiFlow::MidiFlowIn)
    {
        definition->CurrentHighestMidiSourceGroupIndex++;
        groupIndex = definition->CurrentHighestMidiSourceGroupIndex;
    }
    else
    {
        definition->CurrentHighestMidiDestinationGroupIndex++;
        groupIndex = definition->CurrentHighestMidiDestinationGroupIndex;
    }

    return S_OK;
}

#define MAX_THREAD_WORKER_WAIT_TIME_MS 20000

_Use_decl_annotations_
void CMidi2KSAggregateMidiEndpointManager2::EndpointCreationThreadWorker(
    std::stop_token token)
{
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"EndpointCreationWorker: Enter.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    while (!token.stop_requested())
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_VERBOSE,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"EndpointCreationWorker: Waiting to be woken up.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        // wait to be woken up
        m_endpointCreationThreadWakeup.wait(MAX_THREAD_WORKER_WAIT_TIME_MS);

        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_VERBOSE,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"EndpointCreationWorker: I'm awake now.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        if (!token.stop_requested())
        {
            TraceLoggingWrite(
                MidiKSAggregateTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_VERBOSE,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"EndpointCreationWorker: Short nap before checking.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingUInt32(m_individualInterfaceEnumTimeoutMS, "nap period (ms)")
            );

            // we sleep for this timeout before we check to see if the thread is signaled.
            // this gives time for an additional interface notification to cause the event
            // to be reset
            Sleep(m_individualInterfaceEnumTimeoutMS);

            // if we're still signaled, that means no other pnp notifications came in during the nap, or if they
            // did, they completed within that nap period
            if (m_endpointCreationThreadWakeup.is_signaled() && m_pendingEndpointDefinitions.size() > 0)
            {
                m_endpointCreationThreadWakeup.ResetEvent();    // it's a manual reset event

                TraceLoggingWrite(
                    MidiKSAggregateTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_VERBOSE,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"EndpointCreationWorker: Thread was signaled and pending definition count > 0. Proceed to processing queue.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                );


                // lock the definitions so we can process them
                auto lock = m_pendingEndpointDefinitionsLock.lock();

                TraceLoggingWrite(
                    MidiKSAggregateTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_VERBOSE,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"EndpointCreationWorker: Processing pending endpoint definitions.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                );

                while (m_pendingEndpointDefinitions.size() > 0)
                {
                    // effectively a queue, but because we have to iterate and search 
                    // this in other functions, a vector is more appropriate
                    auto ep = m_pendingEndpointDefinitions[0];
                    m_pendingEndpointDefinitions.erase(m_pendingEndpointDefinitions.begin());

                    // create the endpoint
                    LOG_IF_FAILED(DeviceCreateMidiUmpEndpoint(ep));
                }

                TraceLoggingWrite(
                    MidiKSAggregateTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_VERBOSE,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"EndpointCreationWorker: Processed all pending endpoint definitions.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                );
            }
        }
    }

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


}

_Use_decl_annotations_
bool CMidi2KSAggregateMidiEndpointManager2::ActiveKSAEndpointForDeviceExists(
    _In_ std::wstring parentDeviceInstanceId)
{
    auto cleanParentDeviceInstanceId = internal::NormalizeDeviceInstanceIdWStringCopy(parentDeviceInstanceId.c_str());

    for (auto const& entry : m_activatedEndpointDefinitions) 
    {
        if (internal::NormalizeDeviceInstanceIdWStringCopy(entry.second->ParentDeviceInstanceId) == cleanParentDeviceInstanceId)
        {
            return true;
        }
    }

    return false;
}


bool ShouldSkipOpeningKsPin(_In_ KsHandleWrapper& deviceHandleWrapper, _In_ UINT pinIndex)
{
    if (Feature_Servicing_MIDI2FilterCreations::IsEnabled())
    {
        std::unique_ptr<KSMULTIPLE_ITEM> dataRanges;
        ULONG dataRangesSize{ 0 };

        // skip this pin if for some reason data ranges aren't valid
        if (FAILED(deviceHandleWrapper.Execute([&](HANDLE h) -> HRESULT {
            return RetrieveDataRanges(h, pinIndex, (PKSMULTIPLE_ITEM*)&dataRanges, &dataRangesSize);
            })))
        {
            return true;
        }

        // Skip this pin if it supports cyclic ump, or if it doesn't support bytestream
        if (SUCCEEDED(DataRangeSupportsTransport(dataRanges.get(), MidiTransport_CyclicUMP)) ||
            FAILED(DataRangeSupportsTransport(dataRanges.get(), MidiTransport_StandardByteStream)))
        {
            return true;
        }
    }

    return false;
}



_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager2::GetMidi1FilterPins(
    DeviceInformation filterDevice,
    std::vector<std::shared_ptr<KsAggregateEndpointMidiPinDefinition2>>& pinListToAddTo
)
{
    // Wrapper opens the handle internally.
    KsHandleWrapper deviceHandleWrapper(filterDevice.Id().c_str());
    RETURN_IF_FAILED(deviceHandleWrapper.Open());



    // Driver-supplied name. This is needed for WinMM backwards compatibility
    std::wstring driverSuppliedName{};

    // Using lamba function to prevent handle from dissapearing when being used. 
    // we don't log the HRESULT because this is not critical and will often fail,
    // adding unnecessary noise to the error logs
    deviceHandleWrapper.Execute([&](HANDLE h) -> HRESULT {
        return GetKSDriverSuppliedName(h, driverSuppliedName);
        });

    // =============================================================================================
    // Go through all the enumerated pins, looking for a MIDI 1.0 pin

    // enumerate all the pins for this filter
    ULONG cPins{ 0 };

    RETURN_IF_FAILED(deviceHandleWrapper.Execute([&](HANDLE h) -> HRESULT {
        return PinPropertySimple(h, 0, KSPROPSETID_Pin, KSPROPERTY_PIN_CTYPES, &cPins, sizeof(cPins));
        }));

    ULONG midiInputPinIndexForThisFilter{ 0 };
    ULONG midiOutputPinIndexForThisFilter{ 0 };

    // process the pins for this filter. Not all will be MIDI pins
    for (UINT pinIndex = 0; pinIndex < cPins; pinIndex++)
    {
        // Check the communication capabilities of the pin so we can fail fast
        KSPIN_COMMUNICATION communication = (KSPIN_COMMUNICATION)0;

        RETURN_IF_FAILED(deviceHandleWrapper.Execute([&](HANDLE h) -> HRESULT {
            return PinPropertySimple(h, pinIndex, KSPROPSETID_Pin, KSPROPERTY_PIN_COMMUNICATION, &communication, sizeof(KSPIN_COMMUNICATION));
            }));

        // The external connector pin representing the physical connection
        // has KSPIN_COMMUNICATION_NONE. We can only create on software IO pins which
        // have a valid communication. Skip connector pins.
        if (communication == KSPIN_COMMUNICATION_NONE)
        {
            continue;
        }

        if (ShouldSkipOpeningKsPin(deviceHandleWrapper, pinIndex))
        {
            continue;
        }

        // Duplicate the handle to safely pass it to another component or store it.
        wil::unique_handle handleDupe(deviceHandleWrapper.GetHandle());
        RETURN_IF_NULL_ALLOC(handleDupe);

        KsHandleWrapper pinHandleWrapper(
            filterDevice.Id().c_str(), pinIndex, MidiTransport_StandardByteStream, handleDupe.get());

        if (SUCCEEDED(pinHandleWrapper.Open()))
        {
            auto pinDefinition = std::make_shared<KsAggregateEndpointMidiPinDefinition2>();
            RETURN_HR_IF_NULL(E_POINTER, pinDefinition);

            //pinDefinition.KSDriverSuppliedName = driverSuppliedName;
            pinDefinition->PinNumber = pinIndex;
            pinDefinition->FilterDeviceId = std::wstring{ filterDevice.Id() };
            pinDefinition->FilterName = std::wstring{ filterDevice.Name() };

            // find the name of the pin (supplied by iJack, and if not available, generated by the stack)
            std::wstring pinName{ };

            HRESULT pinNameHr = deviceHandleWrapper.Execute([&](HANDLE h) -> HRESULT {
                return GetPinName(h, pinIndex, pinName);
                });

            if (SUCCEEDED(pinNameHr))
            {
                pinDefinition->PinName = pinName;
            }

            // get the data flow so we know if this is a MIDI Input or a MIDI Output
            KSPIN_DATAFLOW dataFlow = (KSPIN_DATAFLOW)0;

            HRESULT dataFlowHr = deviceHandleWrapper.Execute([&](HANDLE h) -> HRESULT {
                return GetPinDataFlow(h, pinIndex, dataFlow);
                });



            pinDefinition->DriverSuppliedName = driverSuppliedName;


            if (SUCCEEDED(dataFlowHr))
            {
                if (dataFlow == KSPIN_DATAFLOW_IN)
                {
                    // MIDI Out (input to device)
                    pinDefinition->PinDataFlow = MidiFlow::MidiFlowIn;
                    pinDefinition->DataFlowFromUserPerspective = MidiFlow::MidiFlowOut;    // opposite
                    pinDefinition->PortIndexWithinThisFilterAndDirection = static_cast<uint8_t>(midiOutputPinIndexForThisFilter);

                    midiOutputPinIndexForThisFilter++;
                }
                else if (dataFlow == KSPIN_DATAFLOW_OUT)
                {
                    // MIDI In (output from device)
                    pinDefinition->PinDataFlow = MidiFlow::MidiFlowOut;
                    pinDefinition->DataFlowFromUserPerspective = MidiFlow::MidiFlowIn;     // opposite
                    pinDefinition->PortIndexWithinThisFilterAndDirection = static_cast<uint8_t>(midiInputPinIndexForThisFilter);

                    midiInputPinIndexForThisFilter++;
                }

                pinListToAddTo.push_back(pinDefinition);
            }
            else
            {
                // this is a failure condition. Move on to next pin
                LOG_IF_FAILED(dataFlowHr);
                continue;
            }
        }
    }

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"MIDI 1.0 pins enumerated", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(filterDevice.Id().c_str(), "filter device id"),
        TraceLoggingUInt32(static_cast<uint32_t>(pinListToAddTo.size()), "Total size of pin list including new pins.")
    );


    return S_OK;
}


_Use_decl_annotations_
HRESULT 
CMidi2KSAggregateMidiEndpointManager2::UpdateNewPinDefinitions(
    std::wstring filterDeviceid, 
    std::shared_ptr<KsAggregateEndpointDefinition2> endpointDefinition)
{
    // At this point, we need to have *all* the pins for the endpoint, not just this filter
    for (auto& pinDefinition : endpointDefinition->MidiPins)
    {
        if (internal::NormalizeDeviceInstanceIdWStringCopy(pinDefinition->FilterDeviceId) !=
            internal::NormalizeDeviceInstanceIdWStringCopy(filterDeviceid))
        {
            // only process the pins for this filter interface. We don't want to 
            // change anything that has already been built. But we do need the 
            // context of all pins when getting the group index.
            continue;
        }

        // Figure out the group index for the pin. This needs the context of the 
        // entire device. Failure to get the group index is fatal
        RETURN_IF_FAILED(IncrementAndGetNextGroupIndex(
            endpointDefinition, 
            pinDefinition->DataFlowFromUserPerspective, 
            pinDefinition->GroupIndex
        ));

        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_VERBOSE,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Assigned Group Index to pin", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(filterDeviceid.c_str(), "filter device id"),
            TraceLoggingUInt8(pinDefinition->GroupIndex, "group index"),
            TraceLoggingWideString(pinDefinition->DataFlowFromUserPerspective ==
                MidiFlow::MidiFlowOut ? L"MidiFlowOut" : L"MidiFlowIn", "data flow from user's perspective")
        );

        // guard against errors resulting in invalid group indexes
        RETURN_HR_IF(E_UNEXPECTED, !IS_VALID_GROUP_INDEX(pinDefinition->GroupIndex));


        std::wstring customName = L"";  // This is blank here. It gets folded in later during endpoint creation/update

        // Build the name table entry for this individual pin
        endpointDefinition->EndpointNameTable.PopulateEntryForMidi1DeviceUsingMidi1Driver(
            pinDefinition->GroupIndex,
            pinDefinition->DataFlowFromUserPerspective,
            customName,
            pinDefinition->DriverSuppliedName,
            pinDefinition->FilterName,
            pinDefinition->PinName,
            pinDefinition->PortIndexWithinThisFilterAndDirection
        );
    }

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager2::OnFilterDeviceInterfaceAdded(
    DeviceWatcher /* watcher */,
    DeviceInformation filterDevice
)
{
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(filterDevice.Id().c_str(), "added interface")
    );


    // 1. Get the list of pins that are the right category for us to try to activate
    // 2. Loop through and build final list of all MIDI 1.0 source and destination pins
    // 3. Do we already have an activated endpoint for this device?
    //    3.1 If we do, then see if it has room for these pins.
    //        3.1.1 If it has room, then add these pins to the endpoint
    //        3.1.2 If it doesn't have room, then build a new endpoint for this device
    //              and add that endpoint to the pending endpoints list
    //    3.2 If we do not have an activated endpoint, see if we have a pending endpoint
    //    


    std::wstring transportCode(TRANSPORT_CODE);

    // Wrapper opens the handle internally.
    KsHandleWrapper deviceHandleWrapper(filterDevice.Id().c_str());
    RETURN_IF_FAILED(deviceHandleWrapper.Open());

    // get all the MIDI 1 pins. These are only partially processed because some things
    // like group index and naming require the full context of all filters/pins on the
    // parent device. We want to get these before we try creating parents or endpoints
    std::vector<std::shared_ptr<KsAggregateEndpointMidiPinDefinition2>> pinList{ };
    RETURN_IF_FAILED(GetMidi1FilterPins(filterDevice, pinList));

    if (pinList.size() == 0)
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"No MIDI 1.0 pins for this filter device", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(filterDevice.Id().c_str(), "filter device id")
        );

        return S_OK;
    }
    else if (pinList.size() > 32)
    {
        // we don't support more than 32 pins, 16 in, 16 out, on a single endpoint. 
        // We also don't support splitting a filter across more than one endpoint
        // so we can only enumerate the first 16 in each direction

        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Too many MIDI pins for this filter. Maximum of 16 in and 16 out allowed per KS filter.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(filterDevice.Id().c_str(), "filter device id")
        );

        RETURN_IF_FAILED(E_FAIL);
    }




    // We have MIDI 1.0 pins to process, so we'll need to find or create a parent device
    // and also find or create an endpoint under that parent, which has sufficient room
    // for these pins.


    // ===================================================================
    // Find or create a parent device definition

    auto parentInstanceId = internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(L"System.Devices.DeviceInstanceId", filterDevice, L"");
    RETURN_HR_IF(E_FAIL, parentInstanceId.empty());

    std::shared_ptr<KsAggregateParentDeviceDefinition2> parentDeviceDefinition{ nullptr };

    RETURN_IF_FAILED(FindOrCreateParentDeviceDefinitionForFilterDevice(
        filterDevice,
        parentDeviceDefinition
        ));


    // ===================================================================
    // Find or create the endpoint

    std::shared_ptr<KsAggregateEndpointDefinition2> endpointDefinition{ nullptr };

    // check to see if we already have an *activated* endpoint for this filter
    if (ActiveKSAEndpointForDeviceExists(parentInstanceId.c_str()))
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_VERBOSE,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"KSA endpoint for this filter already activated. Updating it.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(filterDevice.Id().c_str(), "filter device id"),
            TraceLoggingWideString(parentInstanceId.c_str(), "parent instance id")
        );

        std::shared_ptr<KsAggregateEndpointDefinition2> existingActivatedEndpointDefinition { nullptr };

        // Get all existing endpoint definitions for this parent device
        // If the existing endpoint definition doesn't have room for this set of pins, we need to create a new one

        std::vector<std::shared_ptr<KsAggregateEndpointDefinition2>> foundEndpoints {};

        if (SUCCEEDED(FindAllActivatedEndpointDefinitionsForParentDevice(parentInstanceId.c_str(), foundEndpoints)))
        {
            // find an endpoint with room for another interface with pins.
            // We're going by the highest group index


            // TEMP
            existingActivatedEndpointDefinition = foundEndpoints[0];
        }


        //// first MIDI 1 pin we're processing for this interface
        //RETURN_IF_FAILED(FindActivatedEndpointDefinitionForFilterDevice(parentInstanceId.c_str(), existingActivatedEndpointDefinition));
        //RETURN_HR_IF_NULL(E_POINTER, existingActivatedEndpointDefinition);

        // add our new pins into the existing endpoint definition
        existingActivatedEndpointDefinition->MidiPins.insert(existingActivatedEndpointDefinition->MidiPins.end(), pinList.begin(), pinList.end());
        RETURN_IF_FAILED(UpdateNewPinDefinitions(filterDevice.Id().c_str(), existingActivatedEndpointDefinition));

        RETURN_IF_FAILED(DeviceUpdateExistingMidiUmpEndpointWithFilterChanges(existingActivatedEndpointDefinition));

        return S_OK;
    }
    else
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_VERBOSE,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Endpoint for this device does not already exist. Proceed to creating new one.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(filterDevice.Id().c_str(), "filter device id"),
            TraceLoggingWideString(parentInstanceId.c_str(), "parent instance id")
        );
    }

    // if the endpointDefinition is null, that means we haven't found an existing
    // activated endpoint definition we need to use, and so we proceed to check
    // for an existing pending endpoint definition. If found, it's used. If not
    // found, the function will create a new one for us to use, with all the
    // endpoint-specific details (excluding pins) populated.
    if (endpointDefinition == nullptr)
    {
        // first MIDI 1 pin we're processing for this interface
        RETURN_IF_FAILED(FindOrCreatePendingEndpointDefinitionForFilterDevice(filterDevice, endpointDefinition));
        RETURN_HR_IF_NULL(E_POINTER, endpointDefinition);



        // TODO: If the existing endpoint definition doesn't have room for this set of pins, we need to create a new one




        // add our new pins into the existing endpoint definition
        endpointDefinition->MidiPins.insert(endpointDefinition->MidiPins.end(), pinList.begin(), pinList.end());
        pinList.clear();    // just make sure we don't use this one, accidentally
    }

    RETURN_IF_FAILED(UpdateNewPinDefinitions(filterDevice.Id().c_str(), endpointDefinition));

    // we have an endpoint definition
    m_endpointCreationThreadWakeup.SetEvent();

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager2::OnFilterDeviceInterfaceRemoved(
    DeviceWatcher watcher,
    DeviceInformationUpdate deviceInterfaceUpdate
)
{
    UNREFERENCED_PARAMETER(watcher);

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(deviceInterfaceUpdate.Id().c_str(), "removed interface")
    );

    std::wstring removedFilterDeviceId{ internal::NormalizeDeviceInstanceIdWStringCopy(deviceInterfaceUpdate.Id().c_str()) };

    // find an active device with this filter

    std::shared_ptr<KsAggregateEndpointDefinition2> endpointDefinition{ nullptr };

    for (auto& endpointListIterator : m_activatedEndpointDefinitions)
    {
        // check pins for this filter
        for (auto& pin: endpointListIterator.second->MidiPins)
        {
            if (internal::NormalizeDeviceInstanceIdWStringCopy(pin->FilterDeviceId) == removedFilterDeviceId)
            {
                endpointDefinition = endpointListIterator.second;
                break;
            }
        }

    }

    if (endpointDefinition != nullptr)
    {
        bool done { false };

        while (!done)
        {
            auto foundIt = std::find_if(
                endpointDefinition->MidiPins.begin(),
                endpointDefinition->MidiPins.end(),
                [&removedFilterDeviceId](std::shared_ptr<KsAggregateEndpointMidiPinDefinition2> pin) { return internal::NormalizeDeviceInstanceIdWStringCopy(pin->FilterDeviceId) == removedFilterDeviceId; }
            );

            if (foundIt != endpointDefinition->MidiPins.end())
            {
                // erase the pin definition with this 
                endpointDefinition->MidiPins.erase(foundIt);
            }
            else
            {
                // we've removed all the pins for this interface
                done = true;
            }
        }

        if (endpointDefinition->MidiPins.size() > 0)
        {
            // we've removed all the pins for this interface, but there are still
            // pins left, so now it's time to update the endpoint


            // TODO: Need to cache the name from the driver/registry so we don't have to do a lookup here.

            std::shared_ptr<KsAggregateParentDeviceDefinition2> parentDeviceDefinition{ nullptr };

            if (SUCCEEDED(FindExistingParentDeviceDefinitionForEndpoint(endpointDefinition, parentDeviceDefinition)))
            {
                RETURN_HR_IF_NULL(E_UNEXPECTED, parentDeviceDefinition);

                // update remaining pins in existing endpoint definition
                RETURN_IF_FAILED(UpdateNewPinDefinitions(removedFilterDeviceId, endpointDefinition));
                RETURN_IF_FAILED(DeviceUpdateExistingMidiUmpEndpointWithFilterChanges(endpointDefinition));
            }
            else
            {
                RETURN_IF_FAILED(E_NOTFOUND);
            }
        }
        else
        {
            auto lock = m_activatedEndpointDefinitionsLock.lock();

            // notify the device manager using the InstanceId for this midi device
            RETURN_IF_FAILED(m_midiDeviceManager->RemoveEndpoint(
                internal::NormalizeDeviceInstanceIdWStringCopy(endpointDefinition->EndpointDeviceInstanceId).c_str()));

            // remove the endpoint from the list

            m_activatedEndpointDefinitions.erase(internal::NormalizeDeviceInstanceIdWStringCopy(endpointDefinition->ParentDeviceInstanceId));
        }
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager2::OnFilterDeviceInterfaceUpdated(
    DeviceWatcher watcher,
    DeviceInformationUpdate deviceInterfaceUpdate
)
{
    UNREFERENCED_PARAMETER(watcher);

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(deviceInterfaceUpdate.Id().c_str(), "updated interface")
    );

    // Flow for interface UPDATED
    // - Check for any name changes
    // - If any relevant changes recalculate GTBs and Name table as above and update properties
    // 



    return S_OK;
}




_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager2::OnDeviceWatcherStopped(DeviceWatcher watcher, winrt::Windows::Foundation::IInspectable)
{
    UNREFERENCED_PARAMETER(watcher);

    m_EnumerationCompleted.SetEvent();
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager2::OnEnumerationCompleted(DeviceWatcher watcher, winrt::Windows::Foundation::IInspectable)
{
    UNREFERENCED_PARAMETER(watcher);

    m_EnumerationCompleted.SetEvent();
    return S_OK;
}


_Use_decl_annotations_
winrt::hstring CMidi2KSAggregateMidiEndpointManager2::FindMatchingInstantiatedEndpoint(
    WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria& criteria)
{
    criteria.Normalize();

    for (auto const& def : m_activatedEndpointDefinitions)
    {
        WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria available{};

        available.DeviceInstanceId = def.second->EndpointDeviceInstanceId;
        available.EndpointDeviceId = def.second->EndpointDeviceId;
        available.TransportSuppliedEndpointName = def.second->EndpointName;

        std::shared_ptr<KsAggregateParentDeviceDefinition2> parent { nullptr };

        if (SUCCEEDED(FindExistingParentDeviceDefinitionForEndpoint(def.second, parent)))
        {
            available.UsbVendorId = parent->VID;
            available.UsbProductId = parent->PID;
            available.UsbSerialNumber = parent->SerialNumber;
            available.DeviceManufacturerName = parent->ManufacturerName;
        }


        if (available.Matches(criteria))
        {
            return available.EndpointDeviceId;
        }
    }

    return L"";
}

HRESULT
CMidi2KSAggregateMidiEndpointManager2::Shutdown()
{
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    m_endpointCreationThread.request_stop();
    m_endpointCreationThreadWakeup.SetEvent();

    m_DeviceAdded.revoke();
    m_DeviceRemoved.revoke();
    m_DeviceUpdated.revoke();
    m_DeviceStopped.revoke();

    m_DeviceEnumerationCompleted.revoke();

    m_watcher.Stop();

    uint8_t tries{ 0 };
    while (m_watcher.Status() != DeviceWatcherStatus::Stopped && tries < 50)
    {
        Sleep(100);
        tries++;
    }

    TransportState::Current().Shutdown();

    m_midiDeviceManager.reset();
    m_midiProtocolManager.reset();

    return S_OK;
}