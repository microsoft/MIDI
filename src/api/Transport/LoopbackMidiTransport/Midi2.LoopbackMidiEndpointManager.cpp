// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "midi2.LoopbackMidiTransport.h"

using namespace wil;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

#define MAX_DEVICE_ID_LEN 200 // size in chars

GUID TransportLayerGUID = TRANSPORT_LAYER_GUID;


_Use_decl_annotations_
HRESULT
CMidi2LoopbackMidiEndpointManager::Initialize(
    IMidiDeviceManager* midiDeviceManager, 
    IMidiEndpointProtocolManager* midiEndpointProtocolManager
)
{
    TraceLoggingWrite(
        MidiLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF(E_INVALIDARG, nullptr == midiDeviceManager);
    RETURN_HR_IF(E_INVALIDARG, nullptr == midiEndpointProtocolManager);

    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManager), (void**)&m_MidiDeviceManager));
    RETURN_IF_FAILED(midiEndpointProtocolManager->QueryInterface(__uuidof(IMidiEndpointProtocolManager), (void**)&m_MidiProtocolManager));


    m_TransportTransportId = TransportLayerGUID;    // this is needed so MidiSrv can instantiate the correct transport
    m_ContainerId = m_TransportTransportId;           // we use the transport ID as the container ID for convenience

    RETURN_IF_FAILED(CreateParentDevice());

    m_initialized = true;

    // this must be the last thing we do, as it relies on everything being initialized
    LOG_IF_FAILED(ProcessWorkQueue());

    return S_OK;
}


HRESULT
CMidi2LoopbackMidiEndpointManager::ProcessWorkQueue()
{
    TraceLoggingWrite(
        MidiLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

    uint32_t countItemsProcessed{ 0 };

    while (!TransportState::Current().GetEndpointWorkQueue()->IsEmpty())
    {
        TransportWorkItem item{ };

        if (TransportState::Current().GetEndpointWorkQueue()->GetNextWorkItem(item))
        {
            if (item.Type == TransportWorkItemWorkType::Create)
            {
                LOG_IF_FAILED(CreateEndpointPair(item.DefinitionA, item.DefinitionB));

                countItemsProcessed++;
            }

            // TODO: Process other types of work items
        }
    }

    TraceLoggingWrite(
        MidiLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt32(countItemsProcessed, "count items processed")
    );

    return S_OK;
}


HRESULT
CMidi2LoopbackMidiEndpointManager::CreateParentDevice()
{
    TraceLoggingWrite(
        MidiLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    // this happens before initialization is complete, so don't gate on m_initialized here

    RETURN_HR_IF_NULL(E_POINTER, m_MidiDeviceManager);

    // the parent device parameters are set by the transport (this)
    std::wstring parentDeviceName{ TRANSPORT_PARENT_DEVICE_NAME };
    std::wstring parentDeviceId{ internal::NormalizeDeviceInstanceIdWStringCopy(TRANSPORT_PARENT_ID) };

    SW_DEVICE_CREATE_INFO createInfo = {};
    createInfo.cbSize = sizeof(createInfo);
    createInfo.pszInstanceId = parentDeviceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = parentDeviceName.c_str();
    createInfo.pContainerId = &m_ContainerId;

    wil::unique_cotaskmem_string newDeviceId;

    RETURN_IF_FAILED(m_MidiDeviceManager->ActivateVirtualParentDevice(
        0,
        nullptr,
        &createInfo,
        &newDeviceId
    ));

    m_parentDeviceId = internal::NormalizeDeviceInstanceIdWStringCopy(newDeviceId.get());


    TraceLoggingWrite(
        MidiLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(newDeviceId.get(), "New parent device instance id")
    );

    return S_OK;
}



_Use_decl_annotations_
HRESULT
CMidi2LoopbackMidiEndpointManager::DeleteEndpointPair(
    _In_ MidiLoopbackDeviceDefinition const& definitionA,
    _In_ MidiLoopbackDeviceDefinition const& definitionB)
{
    TraceLoggingWrite(
        MidiLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    // we can't really do much with the return values here other than log them.

    LOG_IF_FAILED(DeleteSingleEndpoint(definitionA));
    LOG_IF_FAILED(DeleteSingleEndpoint(definitionB));

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2LoopbackMidiEndpointManager::DeleteSingleEndpoint(
    MidiLoopbackDeviceDefinition const& definition
)
{
    RETURN_HR_IF(E_UNEXPECTED, !m_initialized);
    RETURN_HR_IF_NULL(E_POINTER, m_MidiDeviceManager);

    TraceLoggingWrite(
        MidiLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(definition.AssociationId.c_str(), "association id"),
        TraceLoggingWideString(definition.EndpointUniqueIdentifier.c_str(), "unique identifier"),
        TraceLoggingWideString(definition.InstanceIdPrefix.c_str(), "prefix"),
        TraceLoggingWideString(definition.EndpointName.c_str(), "name"),
        TraceLoggingWideString(definition.EndpointDescription.c_str(), "description")
    );

    return m_MidiDeviceManager->RemoveEndpoint(definition.CreatedShortClientInstanceId.c_str());
}



_Use_decl_annotations_
HRESULT
CMidi2LoopbackMidiEndpointManager::CreateSingleEndpoint(
    std::shared_ptr<MidiLoopbackDeviceDefinition> definition
    )
{
    RETURN_HR_IF(E_UNEXPECTED, !m_initialized);
    RETURN_HR_IF_NULL(E_POINTER, m_MidiDeviceManager);

    RETURN_HR_IF_NULL(E_INVALIDARG, definition);

    RETURN_HR_IF_MSG(E_INVALIDARG, definition->EndpointName.empty(), "Empty endpoint name");
    RETURN_HR_IF_MSG(E_INVALIDARG, definition->InstanceIdPrefix.empty(), "Empty endpoint prefix");
    RETURN_HR_IF_MSG(E_INVALIDARG, definition->EndpointUniqueIdentifier.empty(), "Empty endpoint unique id");


    TraceLoggingWrite(
        MidiLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(definition->AssociationId.c_str(), "association id"),
        TraceLoggingWideString(definition->InstanceIdPrefix.c_str(), "prefix"),
        TraceLoggingWideString(definition->EndpointUniqueIdentifier.c_str(), "unique identifier"),
        TraceLoggingWideString(definition->EndpointName.c_str(), "name"),
        TraceLoggingWideString(definition->EndpointDescription.c_str(), "description")
        );



    std::wstring transportCode(TRANSPORT_CODE);

    //DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;
    //   DEVPROP_BOOLEAN devPropFalse = DEVPROP_FALSE;

    std::wstring endpointName = definition->EndpointName;
    std::wstring endpointDescription = definition->EndpointDescription;

    std::vector<DEVPROPERTY> interfaceDeviceProperties{};

    // no user or in-protocol data in this case
    std::wstring friendlyName = internal::CalculateEndpointDevicePrimaryName(endpointName, L"", L"");


    TraceLoggingWrite(
        MidiLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Adding endpoint properties", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(definition->AssociationId.c_str(), "association id"),
        TraceLoggingWideString(definition->EndpointUniqueIdentifier.c_str(), "unique identifier"),
        TraceLoggingWideString(friendlyName.c_str(), "friendlyName"),
        TraceLoggingWideString(transportCode.c_str(), "transport code"),
        TraceLoggingWideString(endpointName.c_str(), "endpointName"),
        TraceLoggingWideString(endpointDescription.c_str(), "endpointDescription")
    );

    // this is needed for the loopback endpoints to have a relationship with each other
    interfaceDeviceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_VirtualMidiEndpointAssociator, DEVPROP_STORE_SYSTEM, nullptr},
        DEVPROP_TYPE_STRING, (ULONG)(sizeof(wchar_t) * (definition->AssociationId.length() + 1)), (PVOID)definition->AssociationId.c_str() });

    // Device properties


    SW_DEVICE_CREATE_INFO createInfo = {};
    createInfo.cbSize = sizeof(createInfo);

    // build the instance id, which becomes the middle of the SWD id
    std::wstring instanceId = internal::NormalizeDeviceInstanceIdWStringCopy(
        definition->InstanceIdPrefix + definition->EndpointUniqueIdentifier);

    createInfo.pszInstanceId = instanceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = friendlyName.c_str();

    wil::unique_cotaskmem_string newDeviceInterfaceId;

    TraceLoggingWrite(
        MidiLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Activating endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(definition->AssociationId.c_str(), "association id"),
        TraceLoggingWideString(definition->EndpointUniqueIdentifier.c_str(), "unique identifier"),
        TraceLoggingWideString(instanceId.c_str(), "instance id")
    );

    MIDIENDPOINTCOMMONPROPERTIES commonProperties{};
    commonProperties.TransportId = m_TransportTransportId;
    commonProperties.EndpointDeviceType = MidiEndpointDeviceType::MidiEndpointDeviceType_Normal;
    commonProperties.FriendlyName = friendlyName.c_str();
    commonProperties.TransportCode = transportCode.c_str();
    commonProperties.EndpointName = endpointName.c_str();
    commonProperties.EndpointDescription = endpointDescription.size() > 0 ? endpointDescription.c_str() : nullptr;
    commonProperties.CustomEndpointName = nullptr;
    commonProperties.CustomEndpointDescription = nullptr;
    commonProperties.UniqueIdentifier = definition->EndpointUniqueIdentifier.c_str();
    commonProperties.SupportedDataFormats = MidiDataFormats::MidiDataFormats_UMP;
    commonProperties.NativeDataFormat = MidiDataFormats::MidiDataFormats_UMP;

    UINT32 capabilities {0};
    capabilities |= MidiEndpointCapabilities_SupportsMidi1Protocol;
    capabilities |= MidiEndpointCapabilities_SupportsMidi2Protocol;
    capabilities |= MidiEndpointCapabilities_SupportsMultiClient;
    capabilities |= MidiEndpointCapabilities_GenerateIncomingTimestamps;
    commonProperties.Capabilities = (MidiEndpointCapabilities) capabilities;


    // add a single group terminal block in each direction to support MIDI 1.0 port creation without creating 16 ins and 16 outs.

    std::vector<internal::GroupTerminalBlockInternal> blocks{ };

    internal::GroupTerminalBlockInternal gtb1;
    gtb1.Number = 1;             // gtb indexes start at 1
    gtb1.GroupCount = 1;         // todo: we could get this from properties
    gtb1.FirstGroupIndex = 0;    // group indexes start at 0
    gtb1.Protocol = 0x11;        // 0x11 = MIDI 2.0
    gtb1.Direction = MIDI_GROUP_TERMINAL_BLOCK_INPUT;   // MIDI Out from user's perspective
    gtb1.Name = friendlyName + L" Out";           // todo: get this from properties so folks can control the port name
    blocks.push_back(gtb1);

    internal::GroupTerminalBlockInternal gtb2;
    gtb2.Number = 1;             // gtb indexes start at 1
    gtb2.GroupCount = 1;         // todo: we could get this from properties
    gtb2.FirstGroupIndex = 0;    // group indexes start at 0
    gtb2.Protocol = 0x11;        // 0x11 = MIDI 2.0
    gtb2.Direction = MIDI_GROUP_TERMINAL_BLOCK_OUTPUT;  // MIDI In from user's perspective
    gtb2.Name = friendlyName + L" In";           // todo: get this from properties so folks can control the port name
    blocks.push_back(gtb2);


    std::vector<std::byte> groupTerminalBlockData;
    if (internal::WriteGroupTerminalBlocksToPropertyDataPointer(blocks, groupTerminalBlockData))
    {
        interfaceDeviceProperties.push_back({ { PKEY_MIDI_GroupTerminalBlocks, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BINARY, (ULONG)groupTerminalBlockData.size(), (PVOID)groupTerminalBlockData.data() });

        auto naming = Midi1PortNameSelectionProperty::PortName_UseGroupTerminalBlocksExactly;

        interfaceDeviceProperties.push_back({ { PKEY_MIDI_Midi1PortNamingSelection, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BOOLEAN, (ULONG)sizeof(Midi1PortNameSelectionProperty), (PVOID)&naming });

    }

    
    RETURN_IF_FAILED(m_MidiDeviceManager->ActivateEndpoint(
        (PCWSTR)m_parentDeviceId.c_str(),                       // parent instance Id
        definition->UMPOnly,                                    // UMP-only. When set to false, WinMM MIDI 1.0 ports are created
        MidiFlow::MidiFlowBidirectional,                        // MIDI Flow
        &commonProperties,
        (ULONG)interfaceDeviceProperties.size(),
        (ULONG)0,
        interfaceDeviceProperties.data(),
        nullptr,
        &createInfo,
        &newDeviceInterfaceId));


    TraceLoggingWrite(
        MidiLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Endpoint activated", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(definition->AssociationId.c_str(), "association id"),
        TraceLoggingWideString(definition->EndpointUniqueIdentifier.c_str(), "unique identifier"),
        TraceLoggingWideString(newDeviceInterfaceId.get(), "new device interface id")
    );


    // we need this for removal later
    definition->CreatedShortClientInstanceId = instanceId;
    definition->CreatedEndpointInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(newDeviceInterfaceId.get());

    //MidiEndpointTable::Current().AddCreatedEndpointDevice(entry);
    //MidiEndpointTable::Current().AddCreatedClient(entry.VirtualEndpointAssociationId, entry.CreatedClientEndpointId);


    TraceLoggingWrite(
        MidiLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Done", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(definition->AssociationId.c_str(), "association id"),
        TraceLoggingWideString(definition->EndpointUniqueIdentifier.c_str(), "unique identifier")
    );

    return S_OK;
}





_Use_decl_annotations_
HRESULT 
CMidi2LoopbackMidiEndpointManager::CreateEndpointPair(
    std::shared_ptr<MidiLoopbackDeviceDefinition> definitionA,
    std::shared_ptr<MidiLoopbackDeviceDefinition> definitionB
)
{
    TraceLoggingWrite(
        MidiLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    if (SUCCEEDED(CreateSingleEndpoint(definitionA)))
    {
        if (SUCCEEDED(CreateSingleEndpoint(definitionB)))
        {
            // all good now. Create the device table entry.

            auto associationId = definitionA->AssociationId;

            auto device = MidiLoopbackDevice{};

         //   device.IsFromConfigurationFile = isFromConfigurationFile;
            device.DefinitionA = *definitionA;
            device.DefinitionB = *definitionB;

            TransportState::Current().GetEndpointTable()->SetDevice(associationId, device);

        }
        else
        {
            // failed to create B. We need to remove A now

            TraceLoggingWrite(
                MidiLoopbackMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Failed to create loopback endpoint B. Removing A now.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

            // we can't do anything with the return value here
            DeleteSingleEndpoint(*definitionA);

            return E_FAIL;
        }
    }
    else
    {
        // failed to create A
        TraceLoggingWrite(
            MidiLoopbackMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Failed to create loopback endpoint A", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        return E_FAIL;
    }

    return S_OK;
}



HRESULT
CMidi2LoopbackMidiEndpointManager::Shutdown()
{
    TraceLoggingWrite(
        MidiLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    // destroy and release all the devices we have created

//    LOG_IF_FAILED(TransportState::Current().GetEndpointTable()->Shutdown());

    TransportState::Current().Shutdown();

    m_MidiDeviceManager.reset();
    m_MidiProtocolManager.reset();

    m_initialized = false;

    return S_OK;
}

