// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



#include "pch.h"

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


    // calling winrt::init_apartment more than once on the same thread is ok. We need to 
    // guarantee it is called before we use the watcher.
    winrt::init_apartment(winrt::apartment_type::multi_threaded);


    winrt::hstring parentDeviceSelector(
        L"System.Devices.ClassGuid:=\"{4d36e96c-e325-11ce-bfc1-08002be10318}\" AND " \
        L"System.Devices.Present:=System.StructuredQueryType.Boolean#True");

    // :=System.StructuredQueryType.Boolean#True

    auto additionalProps = winrt::single_threaded_vector<winrt::hstring>();

    additionalProps.Append(L"System.Devices.DeviceManufacturer");
    additionalProps.Append(L"System.Devices.Manufacturer");

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

    m_watcher.Start();

    // Wait for everything to be created so that they're available immediately after service start.
    m_EnumerationCompleted.wait(INITIAL_ENUMERATION_TIMEOUT_MS);

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
    commonProperties.UniqueIdentifier = nullptr;
    commonProperties.ManufacturerName = masterEndpointDefinition.ManufacturerName.empty() ? nullptr : masterEndpointDefinition.ManufacturerName.c_str();
    commonProperties.SupportedDataFormats = MidiDataFormats::MidiDataFormats_UMP;
    commonProperties.NativeDataFormat = MidiDataFormats_ByteStream;
    
    UINT32 capabilities { 0 };
    capabilities |= MidiEndpointCapabilities_SupportsMultiClient;
    capabilities |= MidiEndpointCapabilities_GenerateIncomingTimestamps;
    capabilities |= MidiEndpointCapabilities_SupportsMidi1Protocol;
    commonProperties.Capabilities = (MidiEndpointCapabilities) capabilities;

    // this property does not apply for aggregate endpoints
    //interfaceDevProperties.push_back({ {DEVPKEY_KsMidiPort_KsFilterInterfaceId, DEVPROP_STORE_SYSTEM, nullptr},
    //    DEVPROP_TYPE_STRING, static_cast<ULONG>((masterEndpointDefinition.FilterDeviceId.length() + 1) * sizeof(WCHAR)), (PVOID)masterEndpointDefinition.FilterDeviceId.c_str() });

    //MidiTransport transportCapability{ MidiTransport::MidiTransport_StandardByteStream };
    //interfaceDevProperties.push_back({ {DEVPKEY_KsTransport, DEVPROP_STORE_SYSTEM, nullptr },
    //    DEVPROP_TYPE_UINT32, static_cast<ULONG>(sizeof(UINT32)), (PVOID)&transportCapability });

    // create group terminal blocks and the pin map


    //TraceLoggingWrite(
    //    MidiKSAggregateTransportTelemetryProvider::Provider(),
    //    MIDI_TRACE_EVENT_INFO,
    //    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
    //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
    //    TraceLoggingPointer(this, "this"),
    //    TraceLoggingWideString(L"Building group terminal blocks and pin map", MIDI_TRACE_EVENT_MESSAGE_FIELD),
    //    TraceLoggingWideString(masterEndpointDefinition.EndpointName.c_str(), "name")
    //);

    //uint8_t currentGtbInputGroupIndex{ 0 };
    //uint8_t currentGtbOutputGroupIndex{ 0 };
    uint8_t currentBlockNumber{ 0 };

    std::vector<internal::GroupTerminalBlockInternal> blocks{ };
    std::vector<PinMapEntryStagingEntry> pinMapEntries{ };

    std::vector<std::wstring> portNamesUserFlowOut{ };
    std::vector<std::wstring> portNamesUserFlowIn{ };

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

        MidiFlow flowFromUserPerspective;

        pinMapEntry.GroupIndex = pin.GroupIndex;
        gtb.FirstGroupIndex = pin.GroupIndex;

        if (pin.PinDataFlow == MidiFlow::MidiFlowIn)
        {
            flowFromUserPerspective = MidiFlow::MidiFlowOut;

            gtb.Direction = MIDI_GROUP_TERMINAL_BLOCK_INPUT;   // from the pin/gtb's perspective
            gtb.Name = pin.PortNames.BlockName;

            // this is kept just so we can add ordinal differentiators if we need to
            portNamesUserFlowOut.push_back(gtb.Name);
        }
        else if (pin.PinDataFlow == MidiFlow::MidiFlowOut)
        {
            flowFromUserPerspective = MidiFlow::MidiFlowIn;

            gtb.Direction = MIDI_GROUP_TERMINAL_BLOCK_OUTPUT;   // from the pin/gtb's perspective
            gtb.Name = pin.PortNames.BlockName;

            // this is kept just so we can add ordinal differentiators if we need to
            portNamesUserFlowIn.push_back(gtb.Name);
        }
        else
        {
            RETURN_IF_FAILED(E_INVALIDARG);
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


    // Write Name table property
    // =====================================================

    std::vector<internal::Midi1PortNaming::Midi1PortNameEntry> portNameEntries{};

    for (auto const& pinEntry : masterEndpointDefinition.MidiPins)
    {
        portNameEntries.push_back(pinEntry.PortNames);
    }

    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingUInt32(static_cast<uint32_t>(portNameEntries.size()), "port name entries count"),
        TraceLoggingWideString(masterEndpointDefinition.EndpointName.c_str(), "name")
    );

    std::vector<std::byte> nameTablePropertyData{ };

    if (internal::Midi1PortNaming::WriteMidi1PortNameTableToPropertyDataPointer(portNameEntries, nameTablePropertyData))
    {
        interfaceDevProperties.push_back({ { PKEY_MIDI_Midi1PortNameTable, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BINARY, (ULONG)nameTablePropertyData.size(), (PVOID)nameTablePropertyData.data() });
    }
    else
    {
        // write empty data
        interfaceDevProperties.push_back({ { PKEY_MIDI_Midi1PortNameTable, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_EMPTY, 0, nullptr });

        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Unable to write MIDI 1 port name table to property data", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(masterEndpointDefinition.EndpointName.c_str(), "name")
        );
    }

    //
    // TODO: This needs to be re-done to use user-specified properties from config file
    //
    auto naming = Midi1PortNameSelectionProperty::PortName_UseGlobalDefault;

    interfaceDevProperties.push_back({ { PKEY_MIDI_Midi1PortNamingSelection, DEVPROP_STORE_SYSTEM, nullptr },
        DEVPROP_TYPE_UINT32, (ULONG)sizeof(Midi1PortNameSelectionProperty), (PVOID)&naming });

    // ==============================================


    // We present as a UMP endpoint, so we need to set this property so the service can create the MIDI 1 ports
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

        // todo: return new device interface id

        auto lock = m_availableEndpointDefinitionsLock.lock();

        // Add to internal endpoint manager
        m_availableEndpointDefinitions.insert_or_assign(masterEndpointDefinition.ParentDeviceInstanceId, masterEndpointDefinition);

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


winrt::hstring
GetStringProperty(_In_ DeviceInformation di, _In_ winrt::hstring propertyName, _In_ winrt::hstring defaultValue)
{
    auto prop = di.Properties().Lookup(propertyName);

    if (prop == nullptr)
    {
        return defaultValue;
    }

    auto value = winrt::unbox_value<winrt::hstring>(prop);

    if (value.empty())
    {
        return defaultValue;
    }

    return value;
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

    RETURN_IF_FAILED(hrComponent);

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






_Use_decl_annotations_
HRESULT 
CMidi2KSAggregateMidiEndpointManager::OnDeviceAdded(
    DeviceWatcher watcher, 
    DeviceInformation parentDevice
)
{
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

    auto deviceInstanceId = GetStringProperty(parentDevice, L"System.Devices.DeviceInstanceId", L"");
    RETURN_HR_IF(E_FAIL, deviceInstanceId.empty());


    endpointDefinition.ParentDeviceName = parentDevice.Name();
    endpointDefinition.EndpointName = parentDevice.Name();
    endpointDefinition.ParentDeviceInstanceId = parentDevice.Id();


    // we set this if we find any compatible MIDI 1.0 byte format pins
    bool isCompatibleMidi1Device{ false };

    // enumerate all KS_CATEGORY_AUDIO filters for this parent media device
    winrt::hstring filterDeviceSelector(
        L"System.Devices.InterfaceClassGuid:=\"{6994AD04-93EF-11D0-A3CC-00A0C9223196}\""\
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

            wil::unique_handle hFilter;
            if (FAILED(FilterInstantiate(filterDevice.Id().c_str(), &hFilter)))
            {
                // couldn't instantiate this filter for some reason.
                continue;
            }

            std::wstring driverSuppliedName{};

            LOG_IF_FAILED(GetKSDriverSuppliedName(hFilter.get(), driverSuppliedName));

            // enumerate all the pins for this filter
            ULONG cPins{ 0 };
            if (FAILED(PinPropertySimple(hFilter.get(), 0, KSPROPSETID_Pin, KSPROPERTY_PIN_CTYPES, &cPins, sizeof(cPins))))
            {
                // couldn't get the count of pins. Just move on to the next filter
                continue;
            }



            // process the pins for this filter. Not all will be MIDI pins
            for (UINT pinIndex = 0; pinIndex < cPins; pinIndex++)
            {
            //    bool isMidi1Pin{ false };
                wil::unique_handle hPin;


                // TODO
                std::wstring customPortName{};

                // Check the communication capabilities of the pin so we can fail fast
                KSPIN_COMMUNICATION communication = (KSPIN_COMMUNICATION)0;
                RETURN_IF_FAILED(
                    PinPropertySimple(
                        hFilter.get(),
                        pinIndex,
                        KSPROPSETID_Pin,
                        KSPROPERTY_PIN_COMMUNICATION,
                        &communication,
                        sizeof(KSPIN_COMMUNICATION)
                    )
                );

                // The external connector pin representing the physical connection
                // has KSPIN_COMMUNICATION_NONE. We can only create on software IO pins which
                // have a valid communication. Skip connector pins.
                if (communication == KSPIN_COMMUNICATION_NONE)
                {
                    continue;
                }


                if (SUCCEEDED(InstantiateMidiPin(hFilter.get(), pinIndex, MidiTransport_CyclicUMP, &hPin)))
                {
                    // this is a UMP pin. The KS transport will handle it, so we skip it here.
                    //
                    // TODO: We should probably skip the entire device if we find *any* UMP pins. 
                    // Will there be devices which have simultaneously active UMP and bytestream pins?
                    // I don't think there are. And if we don't skip the device, we're likely to
                    // incorrectly instantiate fallback pins

                    continue;
                }
                else if (SUCCEEDED(InstantiateMidiPin(hFilter.get(), pinIndex, MidiTransport_StandardByteStream, &hPin)))
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
                    if (SUCCEEDED(GetPinName(hFilter.get(), pinIndex, pinName)))
                    {
                        pinDefinition.PinName = pinName;
                    }


                    // get the data flow so we know if this is a MIDI Input or a MIDI Output
                    KSPIN_DATAFLOW dataFlow = (KSPIN_DATAFLOW)0;
                    auto dataFlowHr = GetPinDataFlow(hFilter.get(), pinIndex, dataFlow);
                    if (SUCCEEDED(dataFlowHr))
                    {
                        if (dataFlow == KSPIN_DATAFLOW_IN)
                        {
                            // MIDI Out (input to device)
                            pinDefinition.PinDataFlow = MidiFlow::MidiFlowIn;
                            pinDefinition.PortNames.DataFlowFromUserPerspective = MidiFlow::MidiFlowOut;    // opposite

                            pinDefinition.GroupIndex = static_cast<uint8_t>(midiOutputGroupIndexForDevice);
                            pinDefinition.PortNames.GroupIndex = pinDefinition.GroupIndex;

                            pinDefinition.PortIndexWithinThisFilterAndDirection = static_cast<uint8_t>(midiOutputPinIndexForThisFilter);

                            midiOutputPinIndexForThisFilter++;
                            midiOutputGroupIndexForDevice++;
                        }
                        else if (dataFlow == KSPIN_DATAFLOW_OUT)
                        {
                            // MIDI In (output from device)
                            pinDefinition.PinDataFlow = MidiFlow::MidiFlowOut;
                            pinDefinition.PortNames.DataFlowFromUserPerspective = MidiFlow::MidiFlowIn;     // opposite

                            pinDefinition.GroupIndex = static_cast<uint8_t>(midiInputGroupIndexForDevice);
                            pinDefinition.PortNames.GroupIndex = pinDefinition.GroupIndex;

                            pinDefinition.PortIndexWithinThisFilterAndDirection = static_cast<uint8_t>(midiInputPinIndexForThisFilter);

                            midiInputPinIndexForThisFilter++;
                            midiInputGroupIndexForDevice++;
                        }

                        // This is where we build the proposed names
                        // =================================================

                        internal::Midi1PortNaming::PopulateMidi1PortNameEntryNames(
                            pinDefinition.PortNames,
                            driverSuppliedName,
                            endpointDefinition.ParentDeviceName,
                            pinDefinition.FilterName,
                            pinDefinition.PinName,
                            customPortName,
                            pinDefinition.PortNames.DataFlowFromUserPerspective,
                            pinDefinition.PortIndexWithinThisFilterAndDirection,
                            pinDefinition.GroupIndex
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

                hPin.reset();

            }

        }

        // now create the device

        if (isCompatibleMidi1Device && endpointDefinition.MidiPins.size() > 0)
        {
            // only some vendor drivers provide an actual manufacturer
            // and all the in-box drivers just provide the Generic USB Audio string
            // TODO: Is "Generic USB Audio" a string that is localized? If so, this
            // code will not have the intended effect outside of en-US
            auto manufacturer = GetStringProperty(parentDevice, L"System.Devices.DeviceManufacturer", L"");
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
CMidi2KSAggregateMidiEndpointManager::OnDeviceRemoved(DeviceWatcher, DeviceInformationUpdate device)
{
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
        auto item = m_availableEndpointDefinitions.find((std::wstring)device.Id());

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
            TraceLoggingWideString(device.Id().c_str(), "device id")
        );

        // notify the device manager using the InstanceId for this midi device
        LOG_IF_FAILED(m_midiDeviceManager->RemoveEndpoint(device.Id().c_str()));

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
CMidi2KSAggregateMidiEndpointManager::OnDeviceStopped(DeviceWatcher, winrt::Windows::Foundation::IInspectable)
{
    m_EnumerationCompleted.SetEvent();
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager::OnEnumerationCompleted(DeviceWatcher, winrt::Windows::Foundation::IInspectable)
{
    m_EnumerationCompleted.SetEvent();
    return S_OK;
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
