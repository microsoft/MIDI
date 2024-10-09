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

// if this is defined, then KSMidiEndpointManager will publish a BiDi endpoint
// for pairs of midi in & out endpoints on the same filter.
// Filters which do not have a a single pair of midi in and out,
// separate midi in and out SWD's will always be created.
//#define CREATE_KS_BIDI_SWDS

// If this is defined, we will skip building midi in and midi out
// SWD's for endpoints where BIDI SWD's are created.
// Otherwise, MidiIn, Out, and BiDi will be created. Creating all 3
// is OK for unit testing one at a time, however is not valid for
// normal usage because MidiIn and MidiOut use the same pins as 
// MidiBidi, so only MidiIn and MidiOut or MidiBidi can be used,
// never all 3 at the same time.
//#define BIDI_REPLACES_INOUT_SWDS



_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager::Initialize(
    IMidiDeviceManagerInterface* midiDeviceManager,
    IMidiEndpointProtocolManagerInterface* midiEndpointProtocolManager
)
{
    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"), 
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    RETURN_HR_IF(E_INVALIDARG, nullptr == midiDeviceManager);
    RETURN_HR_IF(E_INVALIDARG, nullptr == midiEndpointProtocolManager);

    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));
    RETURN_IF_FAILED(midiEndpointProtocolManager->QueryInterface(__uuidof(IMidiEndpointProtocolManagerInterface), (void**)&m_MidiProtocolManager));

    winrt::hstring deviceSelector(
        L"System.Devices.InterfaceClassGuid:=\"{6994AD04-93EF-11D0-A3CC-00A0C9223196}\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True");

    m_Watcher = DeviceInformation::CreateWatcher(deviceSelector);

    auto deviceAddedHandler = TypedEventHandler<DeviceWatcher, DeviceInformation>(this, &CMidi2KSAggregateMidiEndpointManager::OnDeviceAdded);
    auto deviceRemovedHandler = TypedEventHandler<DeviceWatcher, DeviceInformationUpdate>(this, &CMidi2KSAggregateMidiEndpointManager::OnDeviceRemoved);
    auto deviceUpdatedHandler = TypedEventHandler<DeviceWatcher, DeviceInformationUpdate>(this, &CMidi2KSAggregateMidiEndpointManager::OnDeviceUpdated);
    auto deviceStoppedHandler = TypedEventHandler<DeviceWatcher, winrt::Windows::Foundation::IInspectable>(this, &CMidi2KSAggregateMidiEndpointManager::OnDeviceStopped);
    auto deviceEnumerationCompletedHandler = TypedEventHandler<DeviceWatcher, winrt::Windows::Foundation::IInspectable>(this, &CMidi2KSAggregateMidiEndpointManager::OnEnumerationCompleted);

    m_DeviceAdded = m_Watcher.Added(winrt::auto_revoke, deviceAddedHandler);
    m_DeviceRemoved = m_Watcher.Removed(winrt::auto_revoke, deviceRemovedHandler);
    m_DeviceUpdated = m_Watcher.Updated(winrt::auto_revoke, deviceUpdatedHandler);
    m_DeviceStopped = m_Watcher.Stopped(winrt::auto_revoke, deviceStoppedHandler);
    m_DeviceEnumerationCompleted = m_Watcher.EnumerationCompleted(winrt::auto_revoke, deviceEnumerationCompletedHandler);

    m_Watcher.Start();

    // Wait for everything to be created so that they're available immediately after service start.
    m_EnumerationCompleted.wait(10000);

    return S_OK;
}




_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager::CreateMidiUmpEndpoint(
    KsAggregateEndpointDefinition& masterEndpointDefinition
)
{
    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Creating aggregate UMP endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(masterEndpointDefinition.EndpointName.c_str(), "name")
        );


    // we require at least one valid pin
    RETURN_HR_IF(E_INVALIDARG, masterEndpointDefinition.Pins.size() < 1);

    std::vector<DEVPROPERTY> interfaceDevProperties;

    MIDIENDPOINTCOMMONPROPERTIES commonProperties{};
    commonProperties.AbstractionLayerGuid = ABSTRACTION_LAYER_GUID;
    commonProperties.EndpointDeviceType = MidiEndpointDeviceType_Normal;
    commonProperties.FriendlyName = masterEndpointDefinition.EndpointName.c_str();
    commonProperties.TransportCode = TRANSPORT_CODE;
    commonProperties.EndpointName = masterEndpointDefinition.FilterName.c_str();
    commonProperties.EndpointDescription = nullptr;
    commonProperties.CustomEndpointName = nullptr;
    commonProperties.CustomEndpointDescription = nullptr;
    commonProperties.UniqueIdentifier = nullptr;
    commonProperties.ManufacturerName = nullptr;
    commonProperties.SupportedDataFormats = MidiDataFormats::MidiDataFormats_UMP;
    commonProperties.NativeDataFormat = MidiDataFormats_ByteStream;
    
    UINT32 capabilities {0};
    capabilities |= MidiEndpointCapabilities_SupportsMultiClient;
    capabilities |= MidiEndpointCapabilities_GenerateIncomingTimestamps;
    capabilities |= MidiEndpointCapabilities_SupportsMidi1Protocol;
    commonProperties.Capabilities = (MidiEndpointCapabilities) capabilities;

    interfaceDevProperties.push_back({ {DEVPKEY_KsMidiPort_KsFilterInterfaceId, DEVPROP_STORE_SYSTEM, nullptr},
        DEVPROP_TYPE_STRING, static_cast<ULONG>((masterEndpointDefinition.FilterDeviceId.length() + 1) * sizeof(WCHAR)), (PVOID)masterEndpointDefinition.FilterDeviceId.c_str() });

    interfaceDevProperties.push_back({ {DEVPKEY_KsTransport, DEVPROP_STORE_SYSTEM, nullptr },
        DEVPROP_TYPE_UINT32, static_cast<ULONG>(sizeof(UINT32)), (PVOID)&masterEndpointDefinition.TransportCapability });

    // create group terminal blocks and the pin map

    uint8_t currentGtbInputGroupIndex{ 0 };
    uint8_t currentGtbOutputGroupIndex{ 0 };
    uint8_t currentBlockNumber{ 0 };

    KSMIDI_PIN_MAP pinMap{ };
    std::vector<internal::GroupTerminalBlockInternal> blocks;

    for (auto const& pin : masterEndpointDefinition.Pins)
    {
        internal::GroupTerminalBlockInternal gtb;

        gtb.Number = ++currentBlockNumber;
        gtb.GroupCount = 1; // always a single group for aggregate devices

        if (pin.DataFlowFromClientPerspective == MidiFlow::MidiFlowIn)
        {
            if (currentGtbOutputGroupIndex >= KSMIDI_PIN_MAP_ENTRY_COUNT)
                continue;

            pinMap.InputEntries[currentGtbOutputGroupIndex].IsValid = true;
            pinMap.InputEntries[currentGtbOutputGroupIndex].PinId = pin.PinNumber;

            gtb.Direction = MIDI_GROUP_TERMINAL_BLOCK_OUTPUT;   // from the pin/gtb's perspective
            gtb.FirstGroupIndex = currentGtbOutputGroupIndex;

            currentGtbOutputGroupIndex++;
        }
        else
        {
            if (currentGtbInputGroupIndex >= KSMIDI_PIN_MAP_ENTRY_COUNT)
                continue;

            pinMap.OutputEntries[currentGtbInputGroupIndex].IsValid = true;
            pinMap.OutputEntries[currentGtbInputGroupIndex].PinId = pin.PinNumber;

            gtb.Direction = MIDI_GROUP_TERMINAL_BLOCK_INPUT;   // from the pin/gtb's perspective
            gtb.FirstGroupIndex = currentGtbInputGroupIndex;

            currentGtbInputGroupIndex++;
        }

        // sort out the name for this group terminal
        if (pin.Name.empty())
        {
            std::wstring dir = pin.DataFlowFromClientPerspective == MidiFlow::MidiFlowIn ? L"In" : L"Out";

            gtb.Name = std::wstring(dir) + L" " + std::to_wstring(gtb.FirstGroupIndex + 1);
        }
        else
        {
            gtb.Name = pin.Name;
        }

        // default values as defined in the MIDI 2.0 USB spec
        gtb.Protocol = 0x01;                // midi 1.0
        gtb.MaxInputBandwidth = 0x0001;     // 31.25 kbps
        gtb.MaxOutputBandwidth = 0x0001;    // 31.25 kbps

        blocks.push_back(gtb);
    }


    std::vector<std::byte> groupTerminalBlockData;

    if (internal::WriteGroupTerminalBlocksToPropertyDataPointer(blocks, groupTerminalBlockData))
    {
        interfaceDevProperties.push_back({ { PKEY_MIDI_OUT_GroupTerminalBlocks, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BINARY, (ULONG)groupTerminalBlockData.size(), (PVOID)groupTerminalBlockData.data()});

        interfaceDevProperties.push_back({ { PKEY_MIDI_IN_GroupTerminalBlocks, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BINARY, (ULONG)groupTerminalBlockData.size(), (PVOID)groupTerminalBlockData.data() });
    }
    else
    {
        // write empty data
    }


    interfaceDevProperties.push_back({ { DEVPKEY_KsMidiGroupPinMap, DEVPROP_STORE_SYSTEM, nullptr },
        DEVPROP_TYPE_BINARY, sizeof(KSMIDI_PIN_MAP), &(pinMap) });


    SW_DEVICE_CREATE_INFO createInfo{ };

    createInfo.cbSize = sizeof(createInfo);
    createInfo.pszInstanceId = masterEndpointDefinition.EndpointDeviceInstanceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = masterEndpointDefinition.EndpointName.c_str();

    // Call the device manager and finish the creation

    HRESULT swdCreationResult;
    wil::unique_cotaskmem_string newDeviceInterfaceId;


    LOG_IF_FAILED(
        swdCreationResult = m_MidiDeviceManager->ActivateEndpoint(
            masterEndpointDefinition.ParentDeviceInstanceId.c_str(),
            false,                                  // TODO: create UMP only, handle the MIDI 1.0 compat ports
            MidiFlow::MidiFlowBidirectional,        // bidi only for the UMP endpoint
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
            MidiKSAggregateAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Aggregate UMP endpoint created", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(masterEndpointDefinition.EndpointName.c_str(), "name"),
            TraceLoggingWideString(newDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        // todo: return new device interface id



        // Add to internal endpoint manager
        m_availableEndpointDefinitions.push_back(std::move(masterEndpointDefinition));

        return swdCreationResult; 
    }
    else
    {
        TraceLoggingWrite(
            MidiKSAggregateAbstractionTelemetryProvider::Provider(),
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

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidiEndpointManager::CreateMidiBytestreamEndpoints(
    KsAggregateEndpointDefinition& /*masterEndpointDefinition*/
)
{

    // TODO

    return S_OK;
}




// TODO: Currently, this process is set up to get alerted of new devices at the
// filter level, which is normal. However, some MIDI devices, especially those
// with custom drivers, present different filters solely for naming reasons. 
// Those filters end up as different aggregate endpoints here, which is very 
// messy. For example, the MOTU MIDI Express 128 presents one filter for each
// numbered port pair, resulting 8 created MIDI BiDi Endpoint devices.
// Others, like some Roland devices, end up as two different devices for a
// management port and a user port, adding to confusion from users when they
// go to select them. 
//
// So the logic needs to change to consider the whole set of filters at a 
// parent device or container level. This could mean a ton of additional
// alerts as the parent device can show up in any number of ways (composite
// device, audio device etc.). Another option will be to capture the info
// like it is now, but not create the aggregate device until we're sure
// we have all the filters and all their pins accounted for first. This will
// require some additional logic, but will result in fewer unnecessary 
// device notifications being handled by this abstraction.
//
// Note that devices can dynamically create filters using KsCreateFilterFactory
// which may complicate this, as there may be no defined upper bound to the
// number of filters a device may expose.
// https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ks/ns-ks-_ksdevice_descriptor
// https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ks/ns-ks-_ksdevice
// https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ks/nf-ks-ksfiltergetdevice
//
// The ugly but functional approach may be to create the devices per-filter, but to 
// keep track of previously created devices, and then update those devices with new
// filters and pins in their pin maps as they are discovered.

_Use_decl_annotations_
HRESULT 
CMidi2KSAggregateMidiEndpointManager::OnDeviceAdded(
    DeviceWatcher watcher, 
    DeviceInformation device
)
{
    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(device.Id().c_str(), "added device")
    );

    wil::unique_handle hFilter;
    std::hash<std::wstring> hasher;
    std::wstring hash;
    ULONG cPins{ 0 };

    std::wstring transportCode(TRANSPORT_CODE);

    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();

    auto properties = device.Properties();

    KsAggregateEndpointDefinition endpointDefinition{ };


    // retrieve the device instance id from the DeviceInformation property store
    auto prop = properties.Lookup(winrt::to_hstring(L"System.Devices.DeviceInstanceId"));
    RETURN_HR_IF_NULL(E_INVALIDARG, prop);
    endpointDefinition.ParentDeviceInstanceId = winrt::unbox_value<winrt::hstring>(prop).c_str();
    endpointDefinition.FilterDeviceId = internal::NormalizeDeviceInstanceIdWStringCopy(device.Id().c_str());

    // get the parent device
    auto parentDeviceInfo = DeviceInformation::CreateFromIdAsync(endpointDefinition.ParentDeviceInstanceId,
        additionalProperties,winrt::Windows::Devices::Enumeration::DeviceInformationKind::Device).get();

    // the default name is the filter name, not the parent device name
    endpointDefinition.ParentDeviceName = parentDeviceInfo.Name();
    endpointDefinition.FilterName = device.Name();
    endpointDefinition.EndpointName = device.Name();
    endpointDefinition.TransportCapability = MidiTransport::MidiTransport_CyclicUMP;

    // default hash is the device id. We don't have an iSerial here.
    hash = std::to_wstring(hasher(endpointDefinition.FilterDeviceId));

 //   std::vector<std::unique_ptr<MIDI_PIN_INFO>> newMidiPins;

    // instantiate the interface
    RETURN_IF_FAILED(FilterInstantiate(endpointDefinition.FilterDeviceId.c_str(), &hFilter));
    RETURN_IF_FAILED(PinPropertySimple(hFilter.get(), 0, KSPROPSETID_Pin, KSPROPERTY_PIN_CTYPES, &cPins, sizeof(cPins)));

    for (UINT i = 0; i < cPins; i++)
    {
        wil::unique_handle hPin;
        KSPIN_DATAFLOW dataFlow = (KSPIN_DATAFLOW)0;
        //MidiTransport transportCapability { MidiTransport_Invalid };
        //MidiDataFormats dataFormatCapability { MidiDataFormats_Invalid };
        KSPIN_COMMUNICATION communication = (KSPIN_COMMUNICATION)0;
        GUID nativeDataFormat{ 0 };

        std::unique_ptr<WCHAR> pinNameData;
        ULONG pinNameDataSize{ 0 };

        KsAggregateEndpointPinDefinition pinDef;
        pinDef.PinNumber = i;


        RETURN_IF_FAILED(
            PinPropertySimple(
                hFilter.get(), 
                i, 
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

        //
        // ================== Standard Byte / MIDI 1.0 Interfaces ===============================================
        // The pin is byte stream, but the endpoint definition will be cyclic ump
        if (SUCCEEDED(InstantiateMidiPin(hFilter.get(), i, MidiTransport_StandardByteStream, &hPin)))
        {
            TraceLoggingWrite(
                MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_VERBOSE,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Opened pin", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(device.Id().c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                TraceLoggingUInt32(i, "pin id")
                );


            // get the name from the Pin. This is how many developers would prefer we name MIDI ports

            LOG_IF_FAILED_WITH_EXPECTED(
                PinPropertyAllocate(
                    hFilter.get(), 
                    i, 
                    KSPROPSETID_Pin, 
                    KSPROPERTY_PIN_NAME, 
                    (PVOID*)&pinNameData,
                    &pinNameDataSize),
                HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));

            // Check to see if the pin has an iJack name
            if (pinNameDataSize > 0)
            {
                TraceLoggingWrite(
                    MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Retrieved pin name", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(device.Id().c_str(), "device id"),
                    TraceLoggingUInt32(i, "pin id"),
                    TraceLoggingWideString(pinNameData.get(), "pin name")
                );

                if (IsAutogeneratedPinName(endpointDefinition.FilterName.c_str(), i, pinNameData.get()))
                {
                    pinDef.Name= L"";
                }
                else
                {
                    // use the name from the pin. This is the most preferred name for most devices
                    pinDef.Name = std::wstring(pinNameData.get());
                }
            }
            else
            {
                // no pin name provided
                pinDef.Name = L"";
            }

            hPin.reset();

            RETURN_IF_FAILED(
                PinPropertySimple(
                    hFilter.get(), 
                    i, 
                    KSPROPSETID_Pin, 
                    KSPROPERTY_PIN_DATAFLOW, 
                    &dataFlow, 
                    sizeof(KSPIN_DATAFLOW)
                )
            );

            if (dataFlow == KSPIN_DATAFLOW_IN)
            {
                // for render (MIDIOut) we need KSPIN_DATAFLOW_IN
                pinDef.DataFlowFromClientPerspective = MidiFlowOut;

                TraceLoggingWrite(
                    MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_VERBOSE,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Pin is KSPIN_DATAFLOW_IN (MidiOut pin)", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(device.Id().c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                    TraceLoggingUInt32(i, "pin id"),
                    TraceLoggingWideString(pinDef.Name.c_str(), "pin def name")
                    );
            }
            else if (dataFlow == KSPIN_DATAFLOW_OUT)
            {
                pinDef.DataFlowFromClientPerspective = MidiFlowIn;

                TraceLoggingWrite(
                    MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_VERBOSE,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Pin is KSPIN_DATAFLOW_OUT (MidiIn pin)", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(device.Id().c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                    TraceLoggingUInt32(i, "pin id"),
                    TraceLoggingWideString(pinDef.Name.c_str(), "pin def name")
                    );
            }
            else
            {
                TraceLoggingWrite(
                    MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Pin dataflow is unexpected value. Skipping pin.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(device.Id().c_str(), "device id"),
                    TraceLoggingUInt32(i, "pin id"),
                    TraceLoggingWideString(pinDef.Name.c_str(), "pin def name")
                    );

                continue;
            }

            // add the pin to our endpoint
            endpointDefinition.Pins.push_back(std::move(pinDef));
        }

        endpointDefinition.EndpointDeviceInstanceId = TRANSPORT_INSTANCE_ID_PREFIX;
        endpointDefinition.EndpointDeviceInstanceId += hash;
    }


    if (endpointDefinition.Pins.size() > 0)
    {
        // We've enumerated all the pins on the device. Now create the aggregated UMP endpoint
        RETURN_IF_FAILED(CreateMidiUmpEndpoint(endpointDefinition));

        // create the individual MIDI 1.0 compat endpoints
        RETURN_IF_FAILED(CreateMidiBytestreamEndpoints(endpointDefinition));
    }
    else
    {
        TraceLoggingWrite(
            MidiKSAggregateAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Device has no compatible MIDI 1.0 pins. This is normal for many devices.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(device.Id().c_str(), "device id")
            );
    }

    return S_OK;
}





_Use_decl_annotations_
HRESULT CMidi2KSAggregateMidiEndpointManager::OnDeviceRemoved(DeviceWatcher, DeviceInformationUpdate device)
{
    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
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
    do
    {
        auto item = std::find_if(m_availableEndpointDefinitions.begin(), 
            m_availableEndpointDefinitions.end(), [&](const KsAggregateEndpointDefinition& endpointDefinition)
        {
            if (endpointDefinition.FilterDeviceId == cleanDeviceId)
            {
                return true;
            }

            return false;
        });


        if (item == m_availableEndpointDefinitions.end())
        {
            break;
        }

        TraceLoggingWrite(
            MidiKSAggregateAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Found device to remove", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(device.Id().c_str(), "device id")
        );

        // notify the device manager using the InstanceId for this midi device
        LOG_IF_FAILED(m_MidiDeviceManager->RemoveEndpoint(device.Id().c_str()));

        // remove the MIDI_PIN_INFO from the list
        m_availableEndpointDefinitions.erase(item);
    }
    while (TRUE);

    return S_OK;
}

_Use_decl_annotations_
HRESULT CMidi2KSAggregateMidiEndpointManager::OnDeviceUpdated(DeviceWatcher, DeviceInformationUpdate)
{
    //see this function for info on the IDeviceInformationUpdate object: https://learn.microsoft.com/en-us/windows/uwp/devices-sensors/enumerate-devices#enumerate-and-watch-devices
    return S_OK;
}

_Use_decl_annotations_
HRESULT CMidi2KSAggregateMidiEndpointManager::OnDeviceStopped(DeviceWatcher, winrt::Windows::Foundation::IInspectable)
{
    m_EnumerationCompleted.SetEvent();
    return S_OK;
}

_Use_decl_annotations_
HRESULT CMidi2KSAggregateMidiEndpointManager::OnEnumerationCompleted(DeviceWatcher, winrt::Windows::Foundation::IInspectable)
{
    m_EnumerationCompleted.SetEvent();
    return S_OK;
}



HRESULT
CMidi2KSAggregateMidiEndpointManager::Shutdown()
{
    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    AbstractionState::Current().Shutdown();

    m_Watcher.Stop();
    m_EnumerationCompleted.wait(500);
    m_DeviceAdded.revoke();
    m_DeviceRemoved.revoke();
    m_DeviceUpdated.revoke();
    m_DeviceStopped.revoke();
    m_DeviceEnumerationCompleted.revoke();

    m_MidiDeviceManager.reset();
    m_MidiProtocolManager.reset();

    return S_OK;
}
