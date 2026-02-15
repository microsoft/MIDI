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


using namespace wil;
using namespace winrt::Windows::Devices::Enumeration;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

#define INITIAL_ENUMERATION_TIMEOUT_MS 10000




_Use_decl_annotations_
HRESULT
KsAggregateParentDeviceDefinition2::AddPin(
    std::shared_ptr<KsAggregateEndpointMidiPinDefinition> pin)
{
    UNREFERENCED_PARAMETER(pin);

    // TODO

    // do we have any endpoints with this filter?
    // if so, check to see if it has space for more pins

    // if no existing endpoint with this filter, then check to see if we have any
    // endpoints at all we can add this to

    // if no endpoints at all, or if all other endpoints are filled, create
    // a new endpoint


    // Add this pin to the endpoint



    return S_OK;
}












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
} PinMapEntryStagingEntry;


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
    std::vector<PinMapEntryStagingEntry> pinMapEntries{ };

    for (auto const& pin : masterEndpointDefinition->MidiPins)
    {
        RETURN_HR_IF(E_INVALIDARG, pin->FilterDeviceId.empty());

        internal::GroupTerminalBlockInternal gtb;

        gtb.Number = ++currentBlockNumber;
        gtb.GroupCount = 1; // always a single group for aggregate MIDI 1.0 devices

        PinMapEntryStagingEntry pinMapEntry{ };

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
CMidi2KSAggregateMidiEndpointManager2::CreateMidiUmpEndpoint(
    std::shared_ptr<KsAggregateEndpointDefinition2> masterEndpointDefinition,
    std::shared_ptr<KsAggregateParentDeviceDefinition2> parentDevice
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, masterEndpointDefinition);
    RETURN_HR_IF_NULL(E_INVALIDARG, parentDevice);

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(masterEndpointDefinition->EndpointName.c_str(), "name")
    );

    DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;

    // we require at least one valid pin
    RETURN_HR_IF(E_INVALIDARG, masterEndpointDefinition->MidiPins.size() < 1);

    std::vector<DEVPROPERTY> interfaceDevProperties;

    MIDIENDPOINTCOMMONPROPERTIES commonProperties{};
    commonProperties.TransportId = TRANSPORT_LAYER_GUID;
    commonProperties.EndpointDeviceType = MidiEndpointDeviceType_Normal;
    commonProperties.FriendlyName = masterEndpointDefinition->EndpointName.c_str();
    commonProperties.TransportCode = TRANSPORT_CODE;
    commonProperties.EndpointName = masterEndpointDefinition->EndpointName.c_str();
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
        masterEndpointDefinition, 
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
    }


    // Fold in custom properties, including MIDI 1 port names and naming approach
    // ===============================================================================

    WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria matchCriteria{};
    matchCriteria.DeviceInstanceId = internal::NormalizeDeviceInstanceIdWStringCopy(masterEndpointDefinition->EndpointDeviceInstanceId);
    matchCriteria.UsbVendorId = parentDevice->VID;
    matchCriteria.UsbProductId = parentDevice->PID;
    matchCriteria.UsbSerialNumber = parentDevice->SerialNumber;
    matchCriteria.TransportSuppliedEndpointName = masterEndpointDefinition->EndpointName;

    auto customProperties = TransportState::Current().GetConfigurationManager()->CustomPropertiesCache()->GetProperties(matchCriteria);

    // rebuild the name table, using the custom properties if present
    RETURN_IF_FAILED(UpdateNameTableWithCustomProperties(masterEndpointDefinition, customProperties));

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
            TraceLoggingWideString(masterEndpointDefinition->EndpointDeviceInstanceId.c_str(), MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD),
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
            TraceLoggingWideString(masterEndpointDefinition->EndpointDeviceInstanceId.c_str(), MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD)
        );
    }

    // Write Name table property, folding in the custom names we discovered earlier
    // ===============================================================================================
    RETURN_IF_FAILED(UpdateNameTableWithCustomProperties(masterEndpointDefinition, customProperties));
    masterEndpointDefinition->EndpointNameTable.WriteProperties(interfaceDevProperties);


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
    createInfo.pszInstanceId = masterEndpointDefinition->EndpointDeviceInstanceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = masterEndpointDefinition->EndpointName.c_str();

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
        TraceLoggingWideString(masterEndpointDefinition->EndpointName.c_str(), "name")
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
            TraceLoggingWideString(masterEndpointDefinition->EndpointName.c_str(), "name"),
            TraceLoggingWideString(newDeviceInterfaceId.get(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        // return new device interface id
        masterEndpointDefinition->EndpointDeviceId = internal::NormalizeEndpointInterfaceIdWStringCopy(std::wstring{ newDeviceInterfaceId.get() });

        auto lock = m_availableEndpointDefinitionsLock.lock();

        // Add to internal endpoint manager
        m_availableEndpointDefinitions.insert_or_assign(
            internal::NormalizeDeviceInstanceIdWStringCopy(parentDevice->DeviceInstanceId),
            masterEndpointDefinition);

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
            TraceLoggingWideString(masterEndpointDefinition->EndpointName.c_str(), "name"),
            TraceLoggingHResult(swdCreationResult, MIDI_TRACE_EVENT_HRESULT_FIELD)
        );

        return swdCreationResult;
    }
}



_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager2::UpdateExistingMidiUmpEndpointWithFilterChanges(
    std::shared_ptr<KsAggregateEndpointDefinition2> masterEndpointDefinition,
    std::shared_ptr<KsAggregateParentDeviceDefinition2> parentDevice
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, masterEndpointDefinition);
    RETURN_HR_IF_NULL(E_INVALIDARG, parentDevice);

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(masterEndpointDefinition->EndpointName.c_str(), "name")
    );

    // we require at least one valid pin
    RETURN_HR_IF(E_INVALIDARG, masterEndpointDefinition->MidiPins.size() < 1);

    std::vector<DEVPROPERTY> interfaceDevProperties{ };

    std::vector<std::byte> pinMapPropertyData;
    std::vector<internal::GroupTerminalBlockInternal> groupTerminalBlocks{ };
    std::vector<std::byte> nameTablePropertyData;

    // update the pin map to have all the existing pins
    // plus the new pins. Update Group Terminal Blocks at the same time.
    RETURN_IF_FAILED(BuildPinsAndGroupTerminalBlocksPropertyData(
        masterEndpointDefinition,
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
    matchCriteria.DeviceInstanceId = internal::NormalizeDeviceInstanceIdWStringCopy(masterEndpointDefinition->EndpointDeviceInstanceId);
    matchCriteria.UsbVendorId = parentDevice->VID;
    matchCriteria.UsbProductId = parentDevice->PID;
    matchCriteria.UsbSerialNumber = parentDevice->SerialNumber;
    matchCriteria.TransportSuppliedEndpointName = masterEndpointDefinition->EndpointName;

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
            TraceLoggingWideString(masterEndpointDefinition->EndpointDeviceInstanceId.c_str(), MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD),
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
            TraceLoggingWideString(masterEndpointDefinition->EndpointDeviceInstanceId.c_str(), MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD)
        );
    }

    // store the property data for the name table
    masterEndpointDefinition->EndpointNameTable.WriteProperties(interfaceDevProperties);


    // Write Name table property, folding in the custom names we discovered earlier
    // ===============================================================================================
    RETURN_IF_FAILED(UpdateNameTableWithCustomProperties(masterEndpointDefinition, customProperties));
    masterEndpointDefinition->EndpointNameTable.WriteProperties(interfaceDevProperties);

    HRESULT updateResult{};

    LOG_IF_FAILED(updateResult = m_midiDeviceManager->UpdateEndpointProperties(
        masterEndpointDefinition->EndpointDeviceId.c_str(),
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
            TraceLoggingWideString(masterEndpointDefinition->EndpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        auto lock = m_availableEndpointDefinitionsLock.lock();

        // Add to internal endpoint manager
        m_availableEndpointDefinitions.insert_or_assign(
            internal::NormalizeDeviceInstanceIdWStringCopy(parentDevice->DeviceInstanceId),
            masterEndpointDefinition);

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
            TraceLoggingWideString(masterEndpointDefinition->EndpointName.c_str(), "name"),
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
    winrt::hstring systemDevicesParentValue, 
    std::shared_ptr<KsAggregateParentDeviceDefinition2>& parentDevice)
{

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
CMidi2KSAggregateMidiEndpointManager2::FindActivatedEndpointDefinitionForFilterDevice(
    std::wstring parentDeviceInstanceId,
    std::shared_ptr<KsAggregateEndpointDefinition2>& endpointDefinition
)
{
    for (auto const& entry : m_availableEndpointDefinitions)
    {
        if (internal::NormalizeDeviceInstanceIdWStringCopy(entry.second->DeviceInstanceId) ==
            internal::NormalizeDeviceInstanceIdWStringCopy(parentDeviceInstanceId.c_str()))
        {
            endpointDefinition = entry.second;

            return S_OK;
        }
    }

    return E_NOTFOUND;
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
        TraceLoggingWideString(L"Enter.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    // we require that the System.Devices.DeviceInstanceId property was requested for the passed-in filter device
    auto deviceInstanceId = internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(L"System.Devices.DeviceInstanceId", filterDevice, L"");
    RETURN_HR_IF(E_FAIL, deviceInstanceId.empty());

    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
    additionalProperties.Append(L"System.Devices.DeviceManufacturer");
    additionalProperties.Append(L"System.Devices.Manufacturer");
    additionalProperties.Append(L"System.Devices.Parent");

    auto parentDevice = DeviceInformation::CreateFromIdAsync(deviceInstanceId,
        additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::Device).get();

    // See if we already have a pending master endpoint definition for this parent device

    auto lock = m_pendingEndpointDefinitionsLock.lock();    // we lock to avoid having one inserted while we're processing
    
    auto parentInstanceIdToFind = internal::NormalizeDeviceInstanceIdWStringCopy(parentDevice.Id().c_str());
    auto it = std::find_if(
        m_pendingEndpointDefinitions.begin(),
        m_pendingEndpointDefinitions.end(),
        [&parentInstanceIdToFind](const std::shared_ptr<KsAggregateEndpointDefinition> def){return internal::NormalizeDeviceInstanceIdWStringCopy(def->ParentDeviceInstanceId) == parentInstanceIdToFind; });

    if (it != m_pendingEndpointDefinitions.end())
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_VERBOSE,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Found existing aggregate UMP endpoint definition.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(parentInstanceIdToFind.c_str(), "parent")
        );

        endpointDefinition = *it;
        return S_OK;
    }

    // We don't have one, create one and add, and get all the parent device information for it
    // we still have the map locked, so keep this code fast
    auto newEndpointDefinition = std::make_shared<KsAggregateEndpointDefinition2>();
    RETURN_HR_IF_NULL(E_OUTOFMEMORY, newEndpointDefinition);

    //newEndpointDefinition->ParentDeviceName = parentDevice.Name();
    //newEndpointDefinition->EndpointName = parentDevice.Name();
    //newEndpointDefinition->ParentDeviceInstanceId = parentDevice.Id();

    //LOG_IF_FAILED(ParseParentIdIntoVidPidSerial(newEndpointDefinition->ParentDeviceInstanceId.c_str(), *newEndpointDefinition));

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Creating new aggregate UMP endpoint definition.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(parentDevice.Id().c_str(), "parent")
    );

    // only some vendor drivers provide an actual manufacturer
    // and all the in-box drivers just provide the Generic USB Audio string
    // TODO: Is "Generic USB Audio" a string that is localized? If so, this
    // code will not have the intended effect outside of en-US
    //auto manufacturer = internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(L"System.Devices.DeviceManufacturer", parentDevice, L"");
    //if (!manufacturer.empty() && manufacturer != L"(Generic USB Audio)" && manufacturer != L"Microsoft")
    //{
    //    newEndpointDefinition->ManufacturerName = manufacturer;
    //}

    //// default hash is the device id.
    //std::hash<std::wstring> hasher;
    //std::wstring hash;
    //hash = std::to_wstring(hasher(newEndpointDefinition->ParentDeviceInstanceId));

    //newEndpointDefinition->EndpointDeviceInstanceId = TRANSPORT_INSTANCE_ID_PREFIX + hash;

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

//_Use_decl_annotations_
//HRESULT
//CMidi2KSAggregateMidiEndpointManager::IncrementAndGetNextGroupIndex(
//    std::shared_ptr<KsAggregateEndpointDefinition> definition,
//    MidiFlow dataFlowFromUserPerspective,
//    uint8_t& groupIndex)
//{
//    if (dataFlowFromUserPerspective == MidiFlow::MidiFlowIn)
//    {
//        definition->CurrentHighestMidiSourceGroupIndex++;
//        groupIndex = definition->CurrentHighestMidiSourceGroupIndex;
//    }
//    else
//    {
//        definition->CurrentHighestMidiDestinationGroupIndex++;
//        groupIndex = definition->CurrentHighestMidiDestinationGroupIndex;
//    }
//
//    return S_OK;
//}

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
                    LOG_IF_FAILED(CreateMidiUmpEndpoint(ep));
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

#ifdef _DEBUG
            else
            {
                if (m_pendingEndpointDefinitions.size() == 0)
                {
                    TraceLoggingWrite(
                        MidiKSAggregateTransportTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_VERBOSE,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"EndpointCreationWorker: Woken up, but no work to do. Pending count == 0.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                    );
                }
                else
                {
                    TraceLoggingWrite(
                        MidiKSAggregateTransportTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_VERBOSE,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"EndpointCreationWorker: Woken up, but thread is no longer signaled", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                    );
                }
            }
#endif
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
    for (auto const& entry : m_availableEndpointDefinitions) 
    {
        if (internal::NormalizeDeviceInstanceIdWStringCopy(entry.second->DeviceInstanceId) ==
            internal::NormalizeDeviceInstanceIdWStringCopy(parentDeviceInstanceId.c_str()))
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
    std::vector<KsAggregateEndpointMidiPinDefinition2>& pinListToAddTo
)
{
    // Wrapper opens the handle internally.
    KsHandleWrapper deviceHandleWrapper(filterDevice.Id().c_str());
    RETURN_IF_FAILED(deviceHandleWrapper.Open());

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

        // Duplicate the handle to safely pass it to another component or store it.
        wil::unique_handle handleDupe(deviceHandleWrapper.GetHandle());
        RETURN_IF_NULL_ALLOC(handleDupe);

        // we try to open UMP only so we understand the device
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_VERBOSE,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Checking for UMP pin. This will fallback error fail for non-UMP devices.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(filterDevice.Id().c_str(), "filter device id")
        );

        KsHandleWrapper m_PinHandleWrapperUmp(filterDevice.Id().c_str(), pinIndex, MidiTransport_CyclicUMP, handleDupe.get());
        if (SUCCEEDED(m_PinHandleWrapperUmp.Open()))
        {
            // this is a UMP pin. The KS transport will handle it, so we skip it here.
            // In the future, we may want to bail on the first UMP pin we find.

            TraceLoggingWrite(
                MidiKSAggregateTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_VERBOSE,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Found UMP/MIDI2 pin. Skipping for this transport.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(filterDevice.Id().c_str(), "filter device id")
            );

            continue;
        }


        // try to open as a MIDI 1 bytestream pin
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_VERBOSE,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Checking for MIDI 1 pin. This will fallback error fail for non-MIDI devices.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(filterDevice.Id().c_str(), "filter device id")
        );

        KsHandleWrapper pinHandleWrapperMidi1(filterDevice.Id().c_str(), pinIndex, MidiTransport_StandardByteStream, handleDupe.get());
        if (SUCCEEDED(pinHandleWrapperMidi1.Open()))
        {
            // this is a MIDI 1.0 byte format pin, so let's process it
            KsAggregateEndpointMidiPinDefinition pinDefinition{ };

            pinDefinition.PinNumber = pinIndex;
            pinDefinition.FilterDeviceId = std::wstring{ filterDevice.Id() };
            pinDefinition.FilterName = std::wstring{ filterDevice.Name() };

            // find the name of the pin (supplied by iJack, and if not available, generated by the stack)
            std::wstring pinName{ };
            HRESULT pinNameHr = deviceHandleWrapper.Execute([&](HANDLE h) -> HRESULT {
                return GetPinName(h, pinIndex, pinName);
                });

            if (SUCCEEDED(pinNameHr))
            {
                pinDefinition.PinName = pinName;

                TraceLoggingWrite(
                    MidiKSAggregateTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_VERBOSE,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Pin has name", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(filterDevice.Id().c_str(), "filter device id"),
                    TraceLoggingWideString(pinDefinition.PinName.c_str(), "pin name")
                );
            }

            // get the data flow so we know if this is a MIDI Input (Source) or a MIDI Output (Destination)
            KSPIN_DATAFLOW dataFlow = (KSPIN_DATAFLOW)0;
            RETURN_IF_FAILED(deviceHandleWrapper.Execute([&](HANDLE h) -> HRESULT {
                return GetPinDataFlow(h, pinIndex, dataFlow);
                }));

            if (dataFlow == KSPIN_DATAFLOW_IN)
            {
                // MIDI Out (input to device)
                pinDefinition.PinDataFlow = MidiFlow::MidiFlowIn;
                pinDefinition.DataFlowFromUserPerspective = MidiFlow::MidiFlowOut;    // opposite
                pinDefinition.PortIndexWithinThisFilterAndDirection = static_cast<uint8_t>(midiOutputPinIndexForThisFilter);

                midiOutputPinIndexForThisFilter++;
            }
            else if (dataFlow == KSPIN_DATAFLOW_OUT)
            {
                // MIDI In (output from device)
                pinDefinition.PinDataFlow = MidiFlow::MidiFlowOut;
                pinDefinition.DataFlowFromUserPerspective = MidiFlow::MidiFlowIn;     // opposite
                pinDefinition.PortIndexWithinThisFilterAndDirection = static_cast<uint8_t>(midiInputPinIndexForThisFilter);

                midiInputPinIndexForThisFilter++;
            }

            pinListToAddTo.push_back(pinDefinition);

            TraceLoggingWrite(
                MidiKSAggregateTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_VERBOSE,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"MIDI 1.0 pin added", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(filterDevice.Id().c_str(), "filter device id")
            );
        }
    }


    return S_OK;
}


_Use_decl_annotations_
HRESULT 
CMidi2KSAggregateMidiEndpointManager2::UpdateNewPinDefinitions(
    std::wstring filterDeviceid, 
    std::wstring driverSuppliedName, 
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
//        RETURN_IF_FAILED(IncrementAndGetNextGroupIndex(endpointDefinition, pinDefinition.DataFlowFromUserPerspective, pinDefinition.GroupIndex));

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

        std::wstring customName = L"";  // This is blank here. It gets folded in later during endpoint creation/update

        // Build the name table entry for this individual pin
        endpointDefinition->EndpointNameTable.PopulateEntryForMidi1DeviceUsingMidi1Driver(
            pinDefinition->GroupIndex,
            pinDefinition->DataFlowFromUserPerspective,
            customName,
            driverSuppliedName,
            pinDefinition->FilterName,
            pinDefinition->PinName,
            pinDefinition->PortIndexWithinThisFilterAndDirection
        );
    }

    return S_OK;
}




//HRESULT
//PopulatePinKSDataFormats(HANDLE filterHandle/*, Some_vector_of_pin_format_structs*/)
//{
//    //Try this, it should be a fairly easy thing to add to your change.
//    //    retrieve the :
//    //KSPROPSETID_Pin,
//    //    KSPROPERTY_PIN_DATARANGES,
//
//    //    limit to pins with(pKsDataFormat->MajorFormat == KSDATAFORMAT_TYPE_MUSIC)
//    //
//    // Retrieval is going to follow the same ksmultipleitemp pattern as KSPROPERTY_MIDI2_GROUP_TERMINAL_BLOCKS
//}


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

    std::wstring transportCode(TRANSPORT_CODE);

    // Wrapper opens the handle internally.
    KsHandleWrapper deviceHandleWrapper(filterDevice.Id().c_str());
    RETURN_IF_FAILED(deviceHandleWrapper.Open());

    std::shared_ptr<KsAggregateEndpointDefinition2> endpointDefinition{ nullptr };

    // get all the MIDI 1 pins. These are only partially processed because some things
    // like group index and naming require the full context of all filters/pins on the
    // parent device.
    std::vector<KsAggregateEndpointMidiPinDefinition2> pinList{ };
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

    auto parentInstanceId = internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(L"System.Devices.DeviceInstanceId", filterDevice, L"");
    RETURN_HR_IF(E_FAIL, parentInstanceId.empty());

    // Driver-supplied name. This is needed for WinMM backwards compatibility
    std::wstring driverSuppliedName{};

    // Using lamba function to prevent handle from dissapearing when being used. 
    // we don't log the HRESULT because this is not critical and will often fail,
    // adding unnecessary noise to the error logs
    deviceHandleWrapper.Execute([&](HANDLE h) -> HRESULT {
        return GetKSDriverSuppliedName(h, driverSuppliedName);
        });


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

        // first MIDI 1 pin we're processing for this interface
        RETURN_IF_FAILED(FindActivatedMasterEndpointDefinitionForFilterDevice(parentInstanceId.c_str(), existingActivatedEndpointDefinition));
        RETURN_HR_IF_NULL(E_POINTER, existingActivatedEndpointDefinition);

        // add our new pins into the existing endpoint definition
        existingActivatedEndpointDefinition->MidiPins.insert(existingActivatedEndpointDefinition->MidiPins.end(), pinList.begin(), pinList.end());
        RETURN_IF_FAILED(UpdateNewPinDefinitions(filterDevice.Id().c_str(), driverSuppliedName, existingActivatedEndpointDefinition));

        RETURN_IF_FAILED(UpdateExistingMidiUmpEndpointWithFilterChanges(existingActivatedEndpointDefinition));

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
        RETURN_IF_FAILED(FindOrCreatePendingMasterEndpointDefinitionForFilterDevice(filterDevice, endpointDefinition));
        RETURN_HR_IF_NULL(E_POINTER, endpointDefinition);

        // add our new pins into the existing endpoint definition
        endpointDefinition->MidiPins.insert(endpointDefinition->MidiPins.end(), pinList.begin(), pinList.end());
        pinList.clear();    // just make sure we don't use this one, accidentally
    }


#ifdef _DEBUG
    if (!driverSuppliedName.empty())
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_VERBOSE,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Driver-supplied name found", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(filterDevice.Id().c_str(), "filter device id"),
            TraceLoggingWideString(driverSuppliedName.c_str(), "driver-supplied name")
        );
    }
#endif

    // At this point, we need to have *all* the pins for the endpoint, not just this filter
    for (auto& pinDefinition : endpointDefinition->MidiPins)
    {
        if (internal::NormalizeDeviceInstanceIdWStringCopy(pinDefinition.FilterDeviceId) != 
            internal::NormalizeDeviceInstanceIdWStringCopy(filterDevice.Id().c_str()))
        {
            // only process the pins for this filter interface. We don't want to 
            // change anything that has already been built. But we do need the 
            // context of all pins when getting the group index.
            continue;
        }

        // Figure out the group index for the pin. This needs the context of the 
        // entire device. Failure to get the group index is fatal
        RETURN_IF_FAILED(IncrementAndGetNextGroupIndex(endpointDefinition, pinDefinition.DataFlowFromUserPerspective, pinDefinition.GroupIndex));

        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_VERBOSE,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Assigned Group Index to pin", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(filterDevice.Id().c_str(), "filter device id"),
            TraceLoggingUInt8(pinDefinition.GroupIndex, "group index"),
            TraceLoggingWideString(pinDefinition.DataFlowFromUserPerspective ==
                MidiFlow::MidiFlowOut ? L"MidiFlowOut" : L"MidiFlowIn", "data flow from user's perspective")
        );

        std::wstring customName = L"";  // This is blank here. It gets folded in later during endpoint creation/update

        // Build the name table entry for this individual pin
        endpointDefinition->EndpointNameTable.PopulateEntryForMidi1DeviceUsingMidi1Driver(
            pinDefinition.GroupIndex,
            pinDefinition.DataFlowFromUserPerspective,
            customName,
            driverSuppliedName,
            pinDefinition.FilterName,
            pinDefinition.PinName,
            pinDefinition.PortIndexWithinThisFilterAndDirection
        );
    }

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

    std::shared_ptr<KsAggregateEndpointDefinition> endpointDefinition{ nullptr };

  

    for (auto& endpointListIterator : m_availableEndpointDefinitions)
    {
        // check pins for this filter
        for (auto& pin: endpointListIterator.second->MidiPins)
        {
            if (internal::NormalizeDeviceInstanceIdWStringCopy(pin.FilterDeviceId) == removedFilterDeviceId)
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
                [&removedFilterDeviceId](KsAggregateEndpointMidiPinDefinition& pin) { return internal::NormalizeDeviceInstanceIdWStringCopy(pin.FilterDeviceId) == removedFilterDeviceId; }
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

            // update remaining pins in existing endpoint definition
            RETURN_IF_FAILED(UpdateNewPinDefinitions(removedFilterDeviceId, L"", endpointDefinition));
            RETURN_IF_FAILED(UpdateExistingMidiUmpEndpointWithFilterChanges(endpointDefinition));
        }
        else
        {
            auto lock = m_availableEndpointDefinitionsLock.lock();

            // notify the device manager using the InstanceId for this midi device
            RETURN_IF_FAILED(m_midiDeviceManager->RemoveEndpoint(
                internal::NormalizeDeviceInstanceIdWStringCopy(endpointDefinition->EndpointDeviceInstanceId).c_str()));

            // remove the endpoint from the list

            m_availableEndpointDefinitions.erase(internal::NormalizeDeviceInstanceIdWStringCopy(endpointDefinition->ParentDeviceInstanceId));
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
winrt::hstring CMidi2KSAggregateMidiEndpointManager2::FindMatchingInstantiatedEndpoint(WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria& criteria)
{
    criteria.Normalize();

    for (auto const& def : m_availableEndpointDefinitions)
    {
        WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria available{};

        available.DeviceInstanceId = def.second->DeviceInstanceId;
        available.EndpointDeviceId = def.second->EndpointDeviceId;
        available.UsbVendorId = def.second->VID;
        available.UsbProductId = def.second->PID;
        available.UsbSerialNumber = def.second->SerialNumber;
        available.TransportSuppliedEndpointName = def.second->EndpointName;
        available.DeviceManufacturerName = def.second->ManufacturerName;

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