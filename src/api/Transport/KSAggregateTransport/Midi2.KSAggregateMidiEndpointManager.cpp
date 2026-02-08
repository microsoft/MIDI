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
CMidi2KSAggregateMidiEndpointManager::Initialize(
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
    if (NewMidiFeatureUpdateKsa2603Enabled())
    {
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

        auto deviceAddedHandler = TypedEventHandler<DeviceWatcher, DeviceInformation>(this, &CMidi2KSAggregateMidiEndpointManager::OnFilterDeviceInterfaceAdded);
        auto deviceRemovedHandler = TypedEventHandler<DeviceWatcher, DeviceInformationUpdate>(this, &CMidi2KSAggregateMidiEndpointManager::OnFilterDeviceInterfaceRemoved);
        auto deviceUpdatedHandler = TypedEventHandler<DeviceWatcher, DeviceInformationUpdate>(this, &CMidi2KSAggregateMidiEndpointManager::OnFilterDeviceInterfaceUpdated);

        auto deviceStoppedHandler = TypedEventHandler<DeviceWatcher, winrt::Windows::Foundation::IInspectable>(this, &CMidi2KSAggregateMidiEndpointManager::OnDeviceStopped);
        auto deviceEnumerationCompletedHandler = TypedEventHandler<DeviceWatcher, winrt::Windows::Foundation::IInspectable>(this, &CMidi2KSAggregateMidiEndpointManager::OnEnumerationCompleted);

        m_DeviceAdded = m_watcher.Added(winrt::auto_revoke, deviceAddedHandler);
        m_DeviceRemoved = m_watcher.Removed(winrt::auto_revoke, deviceRemovedHandler);
        m_DeviceUpdated = m_watcher.Updated(winrt::auto_revoke, deviceUpdatedHandler);
        m_DeviceStopped = m_watcher.Stopped(winrt::auto_revoke, deviceStoppedHandler);
        m_DeviceEnumerationCompleted = m_watcher.EnumerationCompleted(winrt::auto_revoke, deviceEnumerationCompletedHandler);

        // worker thread to handle endpoint creation, since we're enumerating interfaces now and need to aggregate them
        m_endpointCreationThreadWakeup.create(wil::EventOptions::ManualReset);
        std::jthread endpointCreationWorkerThread(std::bind_front(&CMidi2KSAggregateMidiEndpointManager::EndpointCreationThreadWorker, this));
        m_endpointCreationThread = std::move(endpointCreationWorkerThread);
    }
    else
    {
        winrt::hstring parentDeviceSelector(
            L"System.Devices.ClassGuid:=\"{4d36e96c-e325-11ce-bfc1-08002be10318}\" AND " \
            L"System.Devices.Present:=System.StructuredQueryType.Boolean#True");

        // :=System.StructuredQueryType.Boolean#True

        auto additionalProps = winrt::single_threaded_vector<winrt::hstring>();

        additionalProps.Append(L"System.Devices.DeviceManufacturer");
        additionalProps.Append(L"System.Devices.Manufacturer");
        additionalProps.Append(L"System.Devices.Parent");

        m_watcher = DeviceInformation::CreateWatcher(parentDeviceSelector, additionalProps, DeviceInformationKind::Device);

        auto deviceAddedHandler = TypedEventHandler<DeviceWatcher, DeviceInformation>(this, &CMidi2KSAggregateMidiEndpointManager::OnDeviceAdded);
        auto deviceRemovedHandler = TypedEventHandler<DeviceWatcher, DeviceInformationUpdate>(this, &CMidi2KSAggregateMidiEndpointManager::OnDeviceRemoved);
        auto deviceUpdatedHandler = TypedEventHandler<DeviceWatcher, DeviceInformationUpdate>(this, &CMidi2KSAggregateMidiEndpointManager::OnDeviceUpdated);
        auto deviceStoppedHandler = TypedEventHandler<DeviceWatcher, winrt::Windows::Foundation::IInspectable>(this, &CMidi2KSAggregateMidiEndpointManager::OnDeviceStopped);
        auto deviceEnumerationCompletedHandler = TypedEventHandler<DeviceWatcher, winrt::Windows::Foundation::IInspectable>(this, &CMidi2KSAggregateMidiEndpointManager::OnEnumerationCompleted);

        m_DeviceAdded = m_watcher.Added(winrt::auto_revoke, deviceAddedHandler);
        m_DeviceRemoved = m_watcher.Removed(winrt::auto_revoke, deviceRemovedHandler);
        m_DeviceUpdated = m_watcher.Updated(winrt::auto_revoke, deviceUpdatedHandler);
        m_DeviceStopped = m_watcher.Stopped(winrt::auto_revoke, deviceStoppedHandler);
        m_DeviceEnumerationCompleted = m_watcher.EnumerationCompleted(winrt::auto_revoke, deviceEnumerationCompletedHandler);
    }


    m_watcher.Start();

    // Wait for everything to be created so that they're available immediately after service start.
    m_EnumerationCompleted.wait(INITIAL_ENUMERATION_TIMEOUT_MS);

    if (NewMidiFeatureUpdateKsa2603Enabled())
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
CMidi2KSAggregateMidiEndpointManager::CreateMidiUmpEndpoint(
    KsAggregateEndpointDefinition& masterEndpointDefinition
)
{
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(masterEndpointDefinition.EndpointName.c_str(), "name")
        );

    DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;

    // we require at least one valid pin
    RETURN_HR_IF(E_INVALIDARG, masterEndpointDefinition.MidiPins.size() < 1);

    std::vector<DEVPROPERTY> interfaceDevProperties;

    MIDIENDPOINTCOMMONPROPERTIES commonProperties{};
    commonProperties.TransportId = TRANSPORT_LAYER_GUID;
    commonProperties.EndpointDeviceType = MidiEndpointDeviceType_Normal;
    commonProperties.FriendlyName = masterEndpointDefinition.EndpointName.c_str();
    commonProperties.TransportCode = TRANSPORT_CODE;
    commonProperties.EndpointName = masterEndpointDefinition.EndpointName.c_str();
    commonProperties.EndpointDescription = nullptr;
    commonProperties.CustomEndpointName = nullptr;
    commonProperties.CustomEndpointDescription = nullptr;
    commonProperties.UniqueIdentifier = masterEndpointDefinition.SerialNumber.empty() ? nullptr : masterEndpointDefinition.SerialNumber.c_str();
    commonProperties.ManufacturerName = masterEndpointDefinition.ManufacturerName.empty() ? nullptr : masterEndpointDefinition.ManufacturerName.c_str();
    commonProperties.SupportedDataFormats = MidiDataFormats::MidiDataFormats_UMP;
    commonProperties.NativeDataFormat = MidiDataFormats_ByteStream;
    
    UINT32 capabilities { 0 };
    capabilities |= MidiEndpointCapabilities_SupportsMultiClient;
    capabilities |= MidiEndpointCapabilities_GenerateIncomingTimestamps;
    capabilities |= MidiEndpointCapabilities_SupportsMidi1Protocol;
    commonProperties.Capabilities = (MidiEndpointCapabilities) capabilities;

    uint8_t currentBlockNumber{ 0 };

    std::vector<internal::GroupTerminalBlockInternal> blocks{ };
    std::vector<PinMapEntryStagingEntry> pinMapEntries{ };

    for (auto const& pin : masterEndpointDefinition.MidiPins)
    {
        RETURN_HR_IF(E_INVALIDARG, pin.FilterDeviceId.empty());

        internal::GroupTerminalBlockInternal gtb;

        gtb.Number = ++currentBlockNumber;
        gtb.GroupCount = 1; // always a single group for aggregate MIDI 1.0 devices

        PinMapEntryStagingEntry pinMapEntry{ };

        pinMapEntry.PinId = pin.PinNumber;
        pinMapEntry.FilterId = pin.FilterDeviceId;
        pinMapEntry.PinDataFlow = pin.PinDataFlow;

        //MidiFlow flowFromUserPerspective;

        pinMapEntry.GroupIndex = pin.GroupIndex;
        gtb.FirstGroupIndex = pin.GroupIndex;

        if (pin.PinDataFlow == MidiFlow::MidiFlowIn)  // pin in, so user out : A MIDI Destination
        {
            gtb.Direction = MIDI_GROUP_TERMINAL_BLOCK_INPUT;   // from the pin/gtb's perspective

            auto nameTableEntry = masterEndpointDefinition.EndpointNameTable.GetDestinationEntry(gtb.FirstGroupIndex);
            if (nameTableEntry != nullptr && nameTableEntry->NewStyleName[0] != static_cast<wchar_t>(0))
            {
                gtb.Name = internal::TrimmedWStringCopy(nameTableEntry->NewStyleName);
            }
        }
        else if (pin.PinDataFlow == MidiFlow::MidiFlowOut)  // pin out, so user in : A MIDI Source
        {
            gtb.Direction = MIDI_GROUP_TERMINAL_BLOCK_OUTPUT;   // from the pin/gtb's perspective
            auto nameTableEntry = masterEndpointDefinition.EndpointNameTable.GetSourceEntry(gtb.FirstGroupIndex);
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
            gtb.Name = masterEndpointDefinition.EndpointName;

            if (gtb.FirstGroupIndex > 0)
            {
                gtb.Name += L" " + std::wstring{ gtb.FirstGroupIndex };
            }
        }

        // default values as defined in the MIDI 2.0 USB spec
        gtb.Protocol = 0x01;                // midi 1.0
        gtb.MaxInputBandwidth = 0x0001;     // 31.25 kbps
        gtb.MaxOutputBandwidth = 0x0001;    // 31.25 kbps

        blocks.push_back(gtb);
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
        TraceLoggingWideString(masterEndpointDefinition.EndpointName.c_str(), "name")
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

    auto pinMapData = std::make_shared<byte[]>(totalMemoryBytes);
    RETURN_IF_NULL_ALLOC(pinMapData);
    memset(pinMapData.get(), 0, totalMemoryBytes);

    auto currentPos = pinMapData.get();

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
            TraceLoggingWideString(masterEndpointDefinition.EndpointName.c_str(), "name"),
            TraceLoggingUInt32(entry.PinId, "Pin Id"),
            TraceLoggingWideString(entry.FilterId.c_str(), "Filter Id")
        );

        PKSAGGMIDI_PIN_MAP_PROPERTY_ENTRY propEntry = (PKSAGGMIDI_PIN_MAP_PROPERTY_ENTRY)currentPos;

        propEntry->ByteCount = (UINT)(SIZET_KSAGGMIDI_PIN_MAP_PROPERTY_ENTRY_WITHOUT_STRING + ((entry.FilterId.length()+1) * sizeof(wchar_t)));
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
        TraceLoggingWideString(masterEndpointDefinition.EndpointName.c_str(), "name")
    );

    interfaceDevProperties.push_back({ { DEVPKEY_KsAggMidiGroupPinMap, DEVPROP_STORE_SYSTEM, nullptr },
        DEVPROP_TYPE_BINARY, (UINT32)totalMemoryBytes, pinMapData.get() });


    // Write USB Data
    // =====================================================

    // the serialnumber was already added to the common properties object

    if (masterEndpointDefinition.VID > 0)
    {
        interfaceDevProperties.push_back({ { PKEY_MIDI_UsbVID, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_UINT16, static_cast<ULONG>(sizeof(UINT16)), (PVOID)&masterEndpointDefinition.VID });
    }
    else
    {
        interfaceDevProperties.push_back({ { PKEY_MIDI_UsbVID, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_EMPTY, 0, nullptr });
    }

    if (masterEndpointDefinition.PID > 0)
    {
        interfaceDevProperties.push_back({ { PKEY_MIDI_UsbPID, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_UINT16, static_cast<ULONG>(sizeof(UINT16)), (PVOID)&masterEndpointDefinition.PID });
    }
    else
    {
        interfaceDevProperties.push_back({ { PKEY_MIDI_UsbPID, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_EMPTY, 0, nullptr });
    }



    // Write Group Terminal Block Property
    // =====================================================

    std::vector<std::byte> groupTerminalBlockData;

    if (internal::WriteGroupTerminalBlocksToPropertyDataPointer(blocks, groupTerminalBlockData))
    {
        interfaceDevProperties.push_back({ { PKEY_MIDI_GroupTerminalBlocks, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BINARY, (ULONG)groupTerminalBlockData.size(), (PVOID)groupTerminalBlockData.data()});
    }
    else
    {
        // write empty data
    }


    // Fold in custom properties, including MIDI 1 port names and naming approach
    // ===============================================================================

    WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria matchCriteria{};
    matchCriteria.DeviceInstanceId = internal::NormalizeDeviceInstanceIdWStringCopy(masterEndpointDefinition.EndpointDeviceInstanceId);
    matchCriteria.UsbVendorId = masterEndpointDefinition.VID;
    matchCriteria.UsbProductId = masterEndpointDefinition.PID;
    matchCriteria.UsbSerialNumber = masterEndpointDefinition.SerialNumber;
    matchCriteria.TransportSuppliedEndpointName = masterEndpointDefinition.EndpointName;

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
            TraceLoggingWideString(masterEndpointDefinition.EndpointDeviceInstanceId.c_str(), MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD),
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
            TraceLoggingWideString(masterEndpointDefinition.EndpointDeviceInstanceId.c_str(), MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD)
        );
    }


    // Write Name table property, folding in the custom names we discovered earlier
    // ===============================================================================================

    for (auto const& pinEntry : masterEndpointDefinition.MidiPins)
    {
        if (customProperties != nullptr && 
            (customProperties->Midi1Destinations.size() > 0 || customProperties->Midi1Sources.size() > 0))
        {
            if (pinEntry.PinDataFlow == MidiFlow::MidiFlowIn)
            {
                // message destination (output port), pin flow is In
                if (auto customConfiguredName = customProperties->Midi1Destinations.find(pinEntry.GroupIndex);
                    customConfiguredName != customProperties->Midi1Destinations.end())
                {
                    TraceLoggingWrite(
                        MidiKSAggregateTransportTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_INFO,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Found custom name for a Midi 1 destination.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingWideString(masterEndpointDefinition.EndpointDeviceInstanceId.c_str(), MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD),
                        TraceLoggingWideString(customConfiguredName->second.Name.c_str(), "custom name"),
                        TraceLoggingUInt8(pinEntry.GroupIndex, "group index")
                    );

                    masterEndpointDefinition.EndpointNameTable.UpdateDestinationEntryCustomName(pinEntry.GroupIndex, customConfiguredName->second.Name);
                }
            }
            else if (pinEntry.PinDataFlow == MidiFlow::MidiFlowOut)
            {
                // message source (input port), pin flow is Out
                if (auto customConfiguredName = customProperties->Midi1Sources.find(pinEntry.GroupIndex);
                    customConfiguredName != customProperties->Midi1Sources.end())
                {
                    TraceLoggingWrite(
                        MidiKSAggregateTransportTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_INFO,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Found custom name for a Midi 1 source.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingWideString(masterEndpointDefinition.EndpointDeviceInstanceId.c_str(), MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD),
                        TraceLoggingWideString(customConfiguredName->second.Name.c_str(), "custom name"),
                        TraceLoggingUInt8(pinEntry.GroupIndex, "group index")
                    );

                    masterEndpointDefinition.EndpointNameTable.UpdateSourceEntryCustomName(pinEntry.GroupIndex, customConfiguredName->second.Name);
                }
            }
        }

    }

    masterEndpointDefinition.EndpointNameTable.WriteProperties(interfaceDevProperties);


    // Despite being a MIDI 1 device, we present as a UMP endpoint, so we need to set 
    // this property so the service can create the MIDI 1 ports without waiting for 
    // function blocks/discovery to complete or timeout (which it never will)
    interfaceDevProperties.push_back({ { PKEY_MIDI_EndpointDiscoveryProcessComplete, DEVPROP_STORE_SYSTEM, nullptr },
        DEVPROP_TYPE_BOOLEAN, (ULONG)sizeof(devPropTrue), (PVOID)&devPropTrue });

        SW_DEVICE_CREATE_INFO createInfo{ };

    createInfo.cbSize = sizeof(createInfo);
    createInfo.pszInstanceId = masterEndpointDefinition.EndpointDeviceInstanceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = masterEndpointDefinition.EndpointName.c_str();

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
        TraceLoggingWideString(masterEndpointDefinition.EndpointName.c_str(), "name")
    );

    // set to true if we only want to create UMP endpoints
    bool umpOnly = false;

    LOG_IF_FAILED(
        swdCreationResult = m_midiDeviceManager->ActivateEndpoint(
            masterEndpointDefinition.ParentDeviceInstanceId.c_str(),
            umpOnly,
            MidiFlow::MidiFlowBidirectional,            // bidi only for the UMP endpoint
            &commonProperties,
            (ULONG) interfaceDevProperties.size(),
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
            TraceLoggingWideString(masterEndpointDefinition.EndpointName.c_str(), "name"),
            TraceLoggingWideString(newDeviceInterfaceId.get(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        // return new device interface id
        masterEndpointDefinition.EndpointDeviceId = internal::NormalizeEndpointInterfaceIdWStringCopy(std::wstring{ newDeviceInterfaceId.get() });

        auto lock = m_availableEndpointDefinitionsLock.lock();

        // Add to internal endpoint manager
        m_availableEndpointDefinitions.insert_or_assign(
            internal::NormalizeDeviceInstanceIdWStringCopy(masterEndpointDefinition.ParentDeviceInstanceId), 
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
            TraceLoggingWideString(masterEndpointDefinition.EndpointName.c_str(), "name"),
            TraceLoggingHResult(swdCreationResult, MIDI_TRACE_EVENT_HRESULT_FIELD)
        );

        return swdCreationResult;
    }
}


HRESULT
GetPinName(_In_ HANDLE const hFilter, _In_ UINT const pinIndex, _Inout_ std::wstring& pinName)
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

HRESULT
GetPinDataFlow(_In_ HANDLE const hFilter, _In_ UINT const pinIndex, _Inout_ KSPIN_DATAFLOW& dataFlow)
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
CMidi2KSAggregateMidiEndpointManager::GetKSDriverSuppliedName(HANDLE hInstantiatedFilter, std::wstring& name)
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

    if (NewMidiFeatureUpdateKsa2603Enabled())
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



HRESULT
ParseParentIdIntoVidPidSerial(
    _In_ winrt::hstring systemDevicesParentValue, 
    _In_ KsAggregateEndpointDefinition& endpointDefinition)
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
                endpointDefinition.VID = static_cast<uint16_t>(wcstol(vidPidString1.substr(4).c_str(), &end, 16));
            }
            else if (vidPidString2.starts_with(L"VID_"))
            {
                endpointDefinition.VID = static_cast<uint16_t>(wcstol(vidPidString2.substr(4).c_str(), &end, 16));
            }

            // find the PID
            if (vidPidString1.starts_with(L"PID_"))
            {
                endpointDefinition.PID = static_cast<uint16_t>(wcstol(vidPidString1.substr(4).c_str(), &end, 16));
            }
            else if (vidPidString2.starts_with(L"PID_"))
            {
                endpointDefinition.PID = static_cast<uint16_t>(wcstol(vidPidString2.substr(4).c_str(), &end, 16));
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
                endpointDefinition.SerialNumber = serialSection;
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
CMidi2KSAggregateMidiEndpointManager::FindOrCreateMasterEndpointDefinitionForFilterDevice(
    DeviceInformation filterDevice,
    std::shared_ptr<KsAggregateEndpointDefinition>& endpointDefinition
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
    auto newEndpointDefinition = std::make_shared<KsAggregateEndpointDefinition>();
    RETURN_HR_IF_NULL(E_OUTOFMEMORY, newEndpointDefinition);

//    auto systemDevicesParent = internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(L"System.Devices.Parent", parentDevice, L"");

    newEndpointDefinition->ParentDeviceName = parentDevice.Name();
    newEndpointDefinition->EndpointName = parentDevice.Name();
    newEndpointDefinition->ParentDeviceInstanceId = parentDevice.Id();

    LOG_IF_FAILED(ParseParentIdIntoVidPidSerial(newEndpointDefinition->ParentDeviceInstanceId.c_str(), *newEndpointDefinition));

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Creating new aggregate UMP endpoint definition.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(newEndpointDefinition->ParentDeviceInstanceId.c_str(), "parent")
    );

    // only some vendor drivers provide an actual manufacturer
    // and all the in-box drivers just provide the Generic USB Audio string
    // TODO: Is "Generic USB Audio" a string that is localized? If so, this
    // code will not have the intended effect outside of en-US
    auto manufacturer = internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(L"System.Devices.DeviceManufacturer", parentDevice, L"");
    if (!manufacturer.empty() && manufacturer != L"(Generic USB Audio)" && manufacturer != L"Microsoft")
    {
        newEndpointDefinition->ManufacturerName = manufacturer;
    }

    // default hash is the device id.
    std::hash<std::wstring> hasher;
    std::wstring hash;
    hash = std::to_wstring(hasher(newEndpointDefinition->ParentDeviceInstanceId));

    newEndpointDefinition->EndpointDeviceInstanceId = TRANSPORT_INSTANCE_ID_PREFIX + hash;

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

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager::GetNextGroupIndex(
    std::shared_ptr<KsAggregateEndpointDefinition> definition,
    MidiFlow dataFlowFromUserPerspective,
    uint8_t& groupIndex)
{
    // iterate through the pin definitions which match this data flow
    // return last + 1 for the group index for this endpoint
    // we could cache this value in the structure, but trying not to 
    // change the structs with this CFR bugfix

    uint8_t nextGroupIndex{ 0 };
    for (auto const& pin : definition->MidiPins)
    {
        if (pin.DataFlowFromUserPerspective == dataFlowFromUserPerspective)
        {
            nextGroupIndex = max(pin.GroupIndex + 1, nextGroupIndex);
        }
    }

    groupIndex = nextGroupIndex;

    return S_OK;
}


_Use_decl_annotations_
void CMidi2KSAggregateMidiEndpointManager::EndpointCreationThreadWorker(
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
        m_endpointCreationThreadWakeup.wait();

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
                    LOG_IF_FAILED(CreateMidiUmpEndpoint(*ep));
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
bool CMidi2KSAggregateMidiEndpointManager::KSAEndpointForDeviceExists(
    _In_ std::wstring parentDeviceInstanceId)
{
    for (auto const& entry : m_availableEndpointDefinitions) 
    {
        if (internal::NormalizeDeviceInstanceIdWStringCopy(entry.second.ParentDeviceInstanceId) ==
            internal::NormalizeDeviceInstanceIdWStringCopy(parentDeviceInstanceId.c_str()))
        {
            return true;
        }
    }

    return false;
}

// TODO: If this is a new filter for an existing device, we need to update properties on that
// device, not recreate the endpoint

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager::OnFilterDeviceInterfaceAdded(
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

    std::shared_ptr<KsAggregateEndpointDefinition> endpointDefinition{ nullptr };

    // =============================================================================================
    // Go through all the enumerated pins, looking for a MIDI 1.0 pin

    // enumerate all the pins for this filter
    ULONG cPins{ 0 };

    RETURN_IF_FAILED(deviceHandleWrapper.Execute([&](HANDLE h) -> HRESULT {
        return PinPropertySimple(h, 0, KSPROPSETID_Pin, KSPROPERTY_PIN_CTYPES, &cPins, sizeof(cPins));
        }));

    std::wstring driverSuppliedName{};
    ULONG midiInputPinIndexForThisFilter{ 0 };
    ULONG midiOutputPinIndexForThisFilter{ 0 };

    bool checkedForDriverSuppliedName{ false };

    // process the pins for this filter. Not all will be MIDI pins
    for (UINT pinIndex = 0; pinIndex < cPins; pinIndex++)
    {
        //    bool isMidi1Pin{ false };

        // TODO
        std::wstring customPortName{};

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

        KsHandleWrapper m_PinHandleWrapperMidi1(filterDevice.Id().c_str(), pinIndex, MidiTransport_StandardByteStream, handleDupe.get());
        if (SUCCEEDED(m_PinHandleWrapperMidi1.Open()))
        {
            // don't wakeup right now. We've found a midi1 pin
            m_endpointCreationThreadWakeup.ResetEvent();






            // TODO ==============================================================

            // check to see if this filter is for an endpoint device we've already created.
            // if it is, we have to take a different approach to updating it. We don't want
            // to just tear down and rebuild the current device, because that churns ports
            // and can cause disconnections.

            //auto deviceInstanceId = internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(L"System.Devices.DeviceInstanceId", filterDevice, L"");
            auto parentInstanceId = internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(L"System.Devices.DeviceInstanceId", filterDevice, L"");
            RETURN_HR_IF(E_FAIL, parentInstanceId.empty());
            if (KSAEndpointForDeviceExists(parentInstanceId.c_str()))
            {
                TraceLoggingWrite(
                    MidiKSAggregateTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_VERBOSE,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"KSA endpoint for this filter already activated. TEMP skipping.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(filterDevice.Id().c_str(), "filter device id"),
                    TraceLoggingWideString(parentInstanceId.c_str(), "parent instance id")
                );

                return S_OK;
            }

            // END TODO ==============================================================





            if (endpointDefinition == nullptr)
            {
                // first MIDI 1 pin we're processing for this interface
                RETURN_IF_FAILED(FindOrCreateMasterEndpointDefinitionForFilterDevice(filterDevice, endpointDefinition));
                RETURN_HR_IF_NULL(E_POINTER, endpointDefinition);
            }


            if (driverSuppliedName.empty() && !checkedForDriverSuppliedName)
            {
                // Using lamba function to prevent handle from dissapearing when being used. 
                // we don't log the HRESULT because this is not critical and will often fail
                deviceHandleWrapper.Execute([&](HANDLE h) -> HRESULT {
                    return GetKSDriverSuppliedName(h, driverSuppliedName);
                    });

                checkedForDriverSuppliedName = true;

                if (driverSuppliedName.empty())
                {
                    TraceLoggingWrite(
                        MidiKSAggregateTransportTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_VERBOSE,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"No driver-supplied name", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingWideString(filterDevice.Id().c_str(), "filter device id")
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
                        TraceLoggingWideString(L"Driver-supplied name found", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingWideString(filterDevice.Id().c_str(), "filter device id"),
                        TraceLoggingWideString(driverSuppliedName.c_str(), "driver-supplied name")
                    );
                }
            }

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

            // not being able to get the group index is fatal
            RETURN_IF_FAILED(GetNextGroupIndex(endpointDefinition, pinDefinition.DataFlowFromUserPerspective, pinDefinition.GroupIndex));

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

            // This is where we build the proposed names
            // =================================================

            std::wstring customName = L"";  // TODO

            endpointDefinition->EndpointNameTable.PopulateEntryForMidi1DeviceUsingMidi1Driver(
                pinDefinition.GroupIndex,
                pinDefinition.DataFlowFromUserPerspective,
                customName,
                driverSuppliedName,
                pinDefinition.FilterName,
                pinDefinition.PinName,
                pinDefinition.PortIndexWithinThisFilterAndDirection
            );

            endpointDefinition->MidiPins.push_back(pinDefinition);

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


    if (endpointDefinition == nullptr || endpointDefinition->MidiPins.size() == 0)
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"No MIDI 1.0 pins found", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(filterDevice.Id().c_str(), "filter device id")
        );
    }
    else if (endpointDefinition->MidiPins.size() > 0)
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Filter pin Enumeration Complete", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(filterDevice.Id().c_str(), "filter device id"),
            TraceLoggingUInt32(static_cast<uint32_t>(endpointDefinition->MidiPins.size()), "total MIDI 1.0 pin count")
        );

        m_endpointCreationThreadWakeup.SetEvent();
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager::OnFilterDeviceInterfaceRemoved(
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
        TraceLoggingWideString(deviceInterfaceUpdate.Id().c_str(), "added interface")
    );

    // Flow for interface REMOVED
    // - Check for the interface on the existing device
    // - Update pin map to remove all entries with that interface
    // - If this is the last interface, then remove the device. 
    // - If not the last interface:
    //   - Reset the UPDATE_TIMEOUT timeout for this device. If no other removals come through for this device during the timeout:
    //     - Rebuild pin map, maintaining existing numbers where possible
    //     - Rebuild GTBs, maintaining existing numbers where possible
    //     - Recalculate name table
    //     - call MidiDeviceManager::UpdateEndpointProperties. That will also recalculate MIDI 1 ports
    //



    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager::OnFilterDeviceInterfaceUpdated(
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
CMidi2KSAggregateMidiEndpointManager::OnDeviceAdded(
    DeviceWatcher watcher, 
    DeviceInformation parentDevice
)
{
    UNREFERENCED_PARAMETER(watcher);

    // HACK: this is super annoying (and embarrassing) to do. There's some race condition 
    // here that we need to sort through. Maybe queue the device add updates and return 
    // from this function immediately? That will require a queue worker that processes 
    // the device adds and whatnot.
    // Maybe better to use the non-WinRT versions of add/remove notification?
    Sleep(500);


    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(parentDevice.Id().c_str(), "added parent device")
    );

    std::wstring transportCode(TRANSPORT_CODE);

    //auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
    auto properties = parentDevice.Properties();

    KsAggregateEndpointDefinition endpointDefinition{ };

    auto deviceInstanceId = internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(L"System.Devices.DeviceInstanceId", parentDevice, L"");
    RETURN_HR_IF(E_FAIL, deviceInstanceId.empty());


    auto systemDevicesParent = internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(L"System.Devices.Parent", parentDevice, L"");

    endpointDefinition.ParentDeviceName = parentDevice.Name();
    endpointDefinition.EndpointName = parentDevice.Name();
    endpointDefinition.ParentDeviceInstanceId = parentDevice.Id();

    if (!systemDevicesParent.empty())
    {
        LOG_IF_FAILED(ParseParentIdIntoVidPidSerial(systemDevicesParent, endpointDefinition));
    }

    // we set this if we find any compatible MIDI 1.0 byte format pins
    bool isCompatibleMidi1Device{ false };

    // enumerate all KS_CATEGORY_AUDIO filters for this parent media device
    winrt::hstring filterDeviceSelector(
        L"System.Devices.InterfaceClassGuid:=\"" + winrt::hstring(KS_CATEGORY_AUDIO_GUID) + "\""\
        L" AND System.Devices.InterfaceEnabled:= System.StructuredQueryType.Boolean#True"\
        L" AND System.Devices.DeviceInstanceId:= \"" + deviceInstanceId + L"\"");


    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enumerating Filters", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(parentDevice.Id().c_str(), "parent device id")
    );

    auto filterDevices = DeviceInformation::FindAllAsync(filterDeviceSelector).get();

    ULONG midiInputGroupIndexForDevice{ 0 };
    ULONG midiOutputGroupIndexForDevice{ 0 };

    if (filterDevices.Size() > 0)
    {
        for (auto const& filterDevice : filterDevices)
        {
            ULONG midiInputPinIndexForThisFilter{ 0 };
            ULONG midiOutputPinIndexForThisFilter{ 0 };

            if (filterDevice.Id().empty())
            {
                TraceLoggingWrite(
                    MidiKSAggregateTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Filter Id is empty.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(parentDevice.Id().c_str(), "device id")
                );

                continue;
            }

            // Wrapper opens the handle internally.
            KsHandleWrapper deviceHandleWrapper(filterDevice.Id().c_str());
            if (FAILED(deviceHandleWrapper.Open()))
            {
                // couldn't instantiate this filter for some reason.
                continue;
            }

            std::wstring driverSuppliedName{};

            // Using lamba function to prevent handle from dissapearing when being used. 
            LOG_IF_FAILED(deviceHandleWrapper.Execute([&](HANDLE h) -> HRESULT {
                return GetKSDriverSuppliedName(h, driverSuppliedName);
            }));

            // enumerate all the pins for this filter
            ULONG cPins{ 0 };

            HRESULT hr = deviceHandleWrapper.Execute([&](HANDLE h) -> HRESULT {
                return PinPropertySimple(h, 0, KSPROPSETID_Pin, KSPROPERTY_PIN_CTYPES, &cPins, sizeof(cPins));
            });

            if (FAILED(hr))
            {
                // couldn't get the count of pins. Just move on to the next filter
                continue;
            }

            // process the pins for this filter. Not all will be MIDI pins
            for (UINT pinIndex = 0; pinIndex < cPins; pinIndex++)
            {
            //    bool isMidi1Pin{ false };

                // TODO
                std::wstring customPortName{};

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

                KsHandleWrapper m_PinHandleWrapper1(filterDevice.Id().c_str(), pinIndex, MidiTransport_CyclicUMP, handleDupe.get());
                KsHandleWrapper m_PinHandleWrapper2(
                    filterDevice.Id().c_str(), pinIndex, MidiTransport_StandardByteStream, handleDupe.get());

                if (SUCCEEDED(m_PinHandleWrapper1.Open()))
                {
                    // this is a UMP pin. The KS transport will handle it, so we skip it here.
                    //
                    // TODO: We should probably skip the entire device if we find *any* UMP pins. 
                    // Will there be devices which have simultaneously active UMP and bytestream pins?
                    // I don't think there are. And if we don't skip the device, we're likely to
                    // incorrectly instantiate fallback pins

                    continue;
                }
                else if (SUCCEEDED(m_PinHandleWrapper2.Open()))
                {
                    // this is a MIDI 1.0 byte format pin, so let's process it

                    isCompatibleMidi1Device = true;

                    KsAggregateEndpointMidiPinDefinition pinDefinition{ };

                    //pinDefinition.KSDriverSuppliedName = driverSuppliedName;
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
                    }



                    // get the data flow so we know if this is a MIDI Input or a MIDI Output
                    KSPIN_DATAFLOW dataFlow = (KSPIN_DATAFLOW)0;

                    HRESULT dataFlowHr = deviceHandleWrapper.Execute([&](HANDLE h) -> HRESULT {
                        return GetPinDataFlow(h, pinIndex, dataFlow);
                    });

                    if (SUCCEEDED(dataFlowHr))
                    {
                        if (dataFlow == KSPIN_DATAFLOW_IN)
                        {
                            // MIDI Out (input to device)
                            pinDefinition.PinDataFlow = MidiFlow::MidiFlowIn;
                            pinDefinition.DataFlowFromUserPerspective = MidiFlow::MidiFlowOut;    // opposite

                            pinDefinition.GroupIndex = static_cast<uint8_t>(midiOutputGroupIndexForDevice);

                            pinDefinition.PortIndexWithinThisFilterAndDirection = static_cast<uint8_t>(midiOutputPinIndexForThisFilter);



                            midiOutputPinIndexForThisFilter++;
                            midiOutputGroupIndexForDevice++;
                        }
                        else if (dataFlow == KSPIN_DATAFLOW_OUT)
                        {
                            // MIDI In (output from device)
                            pinDefinition.PinDataFlow = MidiFlow::MidiFlowOut;
                            pinDefinition.DataFlowFromUserPerspective = MidiFlow::MidiFlowIn;     // opposite

                            pinDefinition.GroupIndex = static_cast<uint8_t>(midiInputGroupIndexForDevice);

                            pinDefinition.PortIndexWithinThisFilterAndDirection = static_cast<uint8_t>(midiInputPinIndexForThisFilter);

                            midiInputPinIndexForThisFilter++;
                            midiInputGroupIndexForDevice++;
                        }

                        // This is where we build the proposed names
                        // =================================================

                        std::wstring customName = L"";

                        endpointDefinition.EndpointNameTable.PopulateEntryForMidi1DeviceUsingMidi1Driver(
                            pinDefinition.GroupIndex,
                            pinDefinition.DataFlowFromUserPerspective,
                            customName,
                            driverSuppliedName,
                            pinDefinition.FilterName,
                            pinDefinition.PinName,
                            pinDefinition.PortIndexWithinThisFilterAndDirection
                        );

                        endpointDefinition.MidiPins.push_back(pinDefinition);
                    }
                    else
                    {
                        // this is a failure condition. Move on to next pin
                        LOG_IF_FAILED(dataFlowHr);
                        continue;
                    }


                }

            }

        }

        // now create the device

        if (isCompatibleMidi1Device && endpointDefinition.MidiPins.size() > 0)
        {
            // only some vendor drivers provide an actual manufacturer
            // and all the in-box drivers just provide the Generic USB Audio string
            // TODO: Is "Generic USB Audio" a string that is localized? If so, this
            // code will not have the intended effect outside of en-US
            auto manufacturer = internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(L"System.Devices.DeviceManufacturer", parentDevice, L"");
            if (!manufacturer.empty() && manufacturer != L"(Generic USB Audio)" && manufacturer != L"Microsoft")
            {
                endpointDefinition.ManufacturerName = manufacturer;
            }

            // default hash is the device id. We don't have an iSerial here.
            std::hash<std::wstring> hasher;
            std::wstring hash;
            hash = std::to_wstring(hasher(endpointDefinition.ParentDeviceInstanceId));

            endpointDefinition.EndpointDeviceInstanceId = TRANSPORT_INSTANCE_ID_PREFIX + hash;

            TraceLoggingWrite(
                MidiKSAggregateTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Creating aggregate UMP endpoint.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

            // We've enumerated all the pins on the device. Now create the aggregated UMP endpoint
            RETURN_IF_FAILED(CreateMidiUmpEndpoint(endpointDefinition));
        }
        else
        {
            TraceLoggingWrite(
                MidiKSAggregateTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"No compatible MIDI pins. This is normal in most cases.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );
        }
    }
    else
    {
        // no KS_CATEGORY_AUDIO filters for this device. This is not a failure condition.

        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"No KS_CATEGORY_AUDIO filters for this device. This is fine in most cases.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );
    }

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Filter Enumeration Complete", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(parentDevice.Id().c_str(), "parent device id")
    );

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager::OnDeviceRemoved(DeviceWatcher watcher, DeviceInformationUpdate device)
{
    UNREFERENCED_PARAMETER(watcher);

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(device.Id().c_str(), "device id")
    );

    auto cleanDeviceId = internal::NormalizeDeviceInstanceIdWStringCopy(device.Id().c_str());

    // the interface is no longer active, search through our m_AvailableMidiPins to identify
    // every entry with this filter interface id, and remove the SWD and remove the pin(s) from
    // the m_AvailableMidiPins list.

    auto lock = m_availableEndpointDefinitionsLock.lock();
    do
    {
        auto item = m_availableEndpointDefinitions.find(cleanDeviceId);

        if (item == m_availableEndpointDefinitions.end())
        {
            break;
        }

        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Found device to remove", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(cleanDeviceId.c_str(), "device id")
        );

        // notify the device manager using the InstanceId for this midi device
        LOG_IF_FAILED(m_midiDeviceManager->RemoveEndpoint(cleanDeviceId.c_str()));

        // remove the MIDI_PIN_INFO from the list
        m_availableEndpointDefinitions.erase(item);
    }
    while (TRUE);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager::OnDeviceUpdated(DeviceWatcher, DeviceInformationUpdate update)
{
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Device properties updated", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(update.Id().c_str(), "device id"),
        TraceLoggingUInt32(update.Properties().Size(), "count updated properties")
    );

    //see this function for info on the IDeviceInformationUpdate object: https://learn.microsoft.com/en-us/windows/uwp/devices-sensors/enumerate-devices#enumerate-and-watch-devices

    // NOTE: When you change the assigned driver for the device, instead of sending 
    // separate remove/add events, this SOMETIMES gets a couple of OnDeviceUpdate notifications.

    for (auto const& prop : update.Properties())
    {
        OutputDebugString((std::wstring(L"KSA Updated Property with Key: ") + std::wstring(prop.Key().c_str())).c_str());
    }


    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager::OnDeviceStopped(DeviceWatcher watcher, winrt::Windows::Foundation::IInspectable)
{
    UNREFERENCED_PARAMETER(watcher);

    m_EnumerationCompleted.SetEvent();
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager::OnEnumerationCompleted(DeviceWatcher watcher, winrt::Windows::Foundation::IInspectable)
{
    UNREFERENCED_PARAMETER(watcher);

    m_EnumerationCompleted.SetEvent();
    return S_OK;
}


_Use_decl_annotations_
winrt::hstring CMidi2KSAggregateMidiEndpointManager::FindMatchingInstantiatedEndpoint(WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria& criteria)
{
    criteria.Normalize();

    for (auto const& def : m_availableEndpointDefinitions)
    {
        WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria available{};

        available.DeviceInstanceId = def.second.EndpointDeviceInstanceId;
        available.EndpointDeviceId = def.second.EndpointDeviceId;
        available.UsbVendorId = def.second.VID;
        available.UsbProductId = def.second.PID;
        available.UsbSerialNumber = def.second.SerialNumber;
        available.TransportSuppliedEndpointName = def.second.EndpointName;
        available.DeviceManufacturerName = def.second.ManufacturerName;

        if (available.Matches(criteria))
        {
            return available.EndpointDeviceId;
        }
    }

    return L"";

}

HRESULT
CMidi2KSAggregateMidiEndpointManager::Shutdown()
{
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    if (NewMidiFeatureUpdateKsa2603Enabled())
    {
        m_endpointCreationThread.request_stop();
        m_endpointCreationThreadWakeup.SetEvent();
    }

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