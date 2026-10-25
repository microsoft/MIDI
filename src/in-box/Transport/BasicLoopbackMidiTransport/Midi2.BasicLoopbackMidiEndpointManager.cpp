// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "midi2.BasicLoopbackMidiTransport.h"

#include "MidiEndpointNameTable.h"
#include "Feature_Servicing_MIDI2PortNamingRework.h"

using namespace wil;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

GUID TransportLayerGUID = TRANSPORT_LAYER_GUID;


_Use_decl_annotations_
HRESULT
CMidi2BasicLoopbackMidiEndpointManager::Initialize(
    IMidiDeviceManager* midiDeviceManager, 
    IMidiEndpointProtocolManager* midiEndpointProtocolManager
)
try
{
    TraceLoggingWrite(
        MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF(E_INVALIDARG, nullptr == midiDeviceManager);
    RETURN_HR_IF(E_INVALIDARG, nullptr == midiEndpointProtocolManager);

    wil::com_ptr_nothrow<IMidiDeviceManager> deviceManager;
    wil::com_ptr_nothrow<IMidiEndpointProtocolManager> protocolManager;

    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManager), (void**)&deviceManager));
    RETURN_IF_FAILED(midiEndpointProtocolManager->QueryInterface(__uuidof(IMidiEndpointProtocolManager), (void**)&protocolManager));

    {
        auto lock = m_serviceLock.lock_exclusive();
        m_MidiDeviceManager = deviceManager;
        m_MidiProtocolManager = protocolManager;
    }

    // The service never shuts down an endpoint manager whose Initialize failed, so a failure here
    // has to let go of the service's objects itself, or they are released at DLL unload, after
    // the service has destroyed them.
    auto releaseOnFailure = wil::scope_exit([&]() noexcept
        {
            wil::com_ptr_nothrow<IMidiDeviceManager> releasedDeviceManager;
            wil::com_ptr_nothrow<IMidiEndpointProtocolManager> releasedProtocolManager;
            {
                auto lock = m_serviceLock.lock_exclusive();
                releasedDeviceManager = std::move(m_MidiDeviceManager);
                releasedProtocolManager = std::move(m_MidiProtocolManager);
            }
        });

    m_TransportTransportId = TransportLayerGUID;    // this is needed so MidiSrv can instantiate the correct transport

    RETURN_IF_FAILED(CreateParentDevice());

    releaseOnFailure.release();

    // Set before the queue is drained. A loopback queued by a create that saw this still false is
    // either drained below or, once the create sees it true, drained by the create itself.
    m_initialized = true;

    // this must be the last thing we do, as it relies on everything being initialized
    LOG_IF_FAILED(ProcessWorkQueue());

    return S_OK;
}
CATCH_RETURN()


HRESULT
CMidi2BasicLoopbackMidiEndpointManager::ProcessWorkQueue()
{
    TraceLoggingWrite(
        MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

    uint32_t countItemsProcessed{ 0 };

    // GetNextWorkItem is now atomic (locked), so drive the loop off its
    // return value rather than a separate IsEmpty() check, which would be
    // a TOCTOU against a concurrent enqueue/drain.
    TransportWorkItem item{ };
    while (TransportState::Current().GetEndpointWorkQueue()->GetNextWorkItem(item))
    {
        // CreateEndpoint releases the reservation itself when it fails
        LOG_IF_FAILED(CreateEndpoint(item.Definition));

        countItemsProcessed++;
    }

    TraceLoggingWrite(
        MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
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
CMidi2BasicLoopbackMidiEndpointManager::CreateParentDevice()
{
    TraceLoggingWrite(
        MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    // this happens before initialization is complete, so don't gate on m_initialized here

    auto deviceManager = GetDeviceManager();
    RETURN_HR_IF_NULL(E_POINTER, deviceManager);

    // the parent device parameters are set by the transport (this)
    std::wstring parentDeviceName{ TRANSPORT_PARENT_DEVICE_NAME };
    std::wstring parentDeviceId{ internal::NormalizeDeviceInstanceIdWStringCopy(TRANSPORT_PARENT_ID) };

    SW_DEVICE_CREATE_INFO createInfo = {};
    createInfo.cbSize = sizeof(createInfo);
    createInfo.pszInstanceId = parentDeviceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = parentDeviceName.c_str();

    wil::unique_cotaskmem_string newDeviceId;

    RETURN_IF_FAILED(deviceManager->ActivateVirtualParentDevice(
        0,
        nullptr,
        &createInfo,
        &newDeviceId
    ));

    m_parentDeviceId = internal::NormalizeDeviceInstanceIdWStringCopy(newDeviceId.get());


    TraceLoggingWrite(
        MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
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
CMidi2BasicLoopbackMidiEndpointManager::UpdateEndpointMutedStateProperty(
    std::shared_ptr<MidiBasicLoopbackDeviceDefinition const> definition)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, definition);

    // a feedback trip calls this on its own thread, and Shutdown may have let go already
    auto deviceManager = GetDeviceManager();
    RETURN_HR_IF_NULL(E_UNEXPECTED, deviceManager);

    TraceLoggingWrite(
        MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(definition->CreatedEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingBool(definition->IsMuted, "is muted")
    );

    DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;
    DEVPROP_BOOLEAN devPropFalse = DEVPROP_FALSE;

    std::vector<DEVPROPERTY> interfaceDevProperties{};

    // see if this is going to start off as muted
    if (definition->IsMuted)
    {
        interfaceDevProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_IsMuted, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, (ULONG)(sizeof(DEVPROP_BOOLEAN)), &devPropTrue });
    }
    else
    {
        interfaceDevProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_IsMuted, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, (ULONG)(sizeof(DEVPROP_BOOLEAN)), &devPropFalse });
    }

    RETURN_IF_FAILED(deviceManager->UpdateEndpointProperties(
        definition->CreatedEndpointInterfaceId.c_str(),
        static_cast<ULONG>(interfaceDevProperties.size()),
        interfaceDevProperties.data()
        ));

    TraceLoggingWrite(
        MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(definition->CreatedEndpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingBool(definition->IsMuted, "is muted")
    );

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2BasicLoopbackMidiEndpointManager::DeleteEndpoint(
    std::shared_ptr<MidiBasicLoopbackDeviceDefinition const> definition)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, definition);

    TraceLoggingWrite(
        MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    auto deviceManager = GetDeviceManager();
    RETURN_HR_IF_NULL(E_UNEXPECTED, deviceManager);

    RETURN_IF_FAILED(deviceManager->RemoveEndpoint(definition->CreatedShortClientInstanceId.c_str()));

    // Also need to remove from internal list

    TransportState::Current().GetEndpointTable()->RemoveDevice(definition->AssociationId);

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2BasicLoopbackMidiEndpointManager::CreateEndpoint(
    std::shared_ptr<MidiBasicLoopbackDeviceDefinition> definition
    )
try
{
    RETURN_HR_IF_NULL(E_INVALIDARG, definition);

    auto const table = TransportState::Current().GetEndpointTable();
    RETURN_HR_IF_NULL(E_UNEXPECTED, table);

    auto const associationId = definition->AssociationId;

    // Whatever goes wrong below, the association id and unique id become free again
    auto releaseReservation = wil::scope_exit([&]() noexcept
        {
            table->ReleaseReservation(associationId);
        });

    RETURN_HR_IF(E_UNEXPECTED, !m_initialized);

    auto deviceManager = GetDeviceManager();
    RETURN_HR_IF_NULL(E_POINTER, deviceManager);

    RETURN_HR_IF_MSG(E_INVALIDARG, definition->EndpointName.empty(), "Empty endpoint name");
    RETURN_HR_IF_MSG(E_INVALIDARG, definition->InstanceIdPrefix.empty(), "Empty endpoint prefix");
    RETURN_HR_IF_MSG(E_INVALIDARG, definition->EndpointUniqueIdentifier.empty(), "Empty endpoint unique id");


    TraceLoggingWrite(
        MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingGuid(definition->AssociationId, "association id"),
        TraceLoggingWideString(definition->InstanceIdPrefix.c_str(), "prefix"),
        TraceLoggingWideString(definition->EndpointUniqueIdentifier.c_str(), "unique identifier")
        );

    DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;
    DEVPROP_BOOLEAN devPropFalse = DEVPROP_FALSE;


    std::wstring transportCode(TRANSPORT_CODE);

    std::wstring endpointName = definition->EndpointName;
    std::wstring endpointDescription = definition->EndpointDescription;

    // The specification states the name limit as a UTF-8 byte count, not a character count
    if (Feature_Servicing_MIDI2EndpointNameUtf8ByteLimit::IsEnabled())
    {
        endpointName = internal::TruncateToUtf8ByteCount(endpointName, MIDI_STREAM_MESSAGE_ENDPOINT_NAME_MAX_LENGTH);
    }

    std::vector<DEVPROPERTY> interfaceDevProperties{};

    // no user or in-protocol data in this case
    std::wstring friendlyName = internal::CalculateEndpointDevicePrimaryName(endpointName, L"", L"");


    TraceLoggingWrite(
        MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Adding endpoint properties", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingGuid(definition->AssociationId, "association id"),
        TraceLoggingWideString(definition->EndpointUniqueIdentifier.c_str(), "unique identifier"),
        TraceLoggingWideString(transportCode.c_str(), "transport code")
    );

    std::wstring associationIdString = internal::GuidToString(definition->AssociationId);

    // we keep this to keep the logical structure the same as the MIDI 2.0 loopback endpoints
    interfaceDevProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_VirtualMidiEndpointAssociator, DEVPROP_STORE_SYSTEM, nullptr},
        DEVPROP_TYPE_STRING, (ULONG)(sizeof(wchar_t) * (associationIdString.length() + 1)), (PVOID)associationIdString.c_str() });


    // see if this is going to start off as muted
    if (definition->IsMuted)
    {
        interfaceDevProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_IsMuted, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, (ULONG)(sizeof(DEVPROP_BOOLEAN)), &devPropTrue });
    }
    else
    {
        interfaceDevProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_IsMuted, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, (ULONG)(sizeof(DEVPROP_BOOLEAN)), &devPropFalse });
    }

    if (!definition->ImageFileName.empty())
    {
        interfaceDevProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_CustomImagePath, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, (ULONG)(sizeof(wchar_t) * (definition->ImageFileName.length() + 1)), (PVOID)definition->ImageFileName.c_str() });
    }
   
    
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
        MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Activating endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingGuid(definition->AssociationId, "association id"),
        TraceLoggingWideString(definition->EndpointUniqueIdentifier.c_str(), "unique identifier"),
        TraceLoggingWideString(instanceId.c_str(), "instance id")
    );

    MIDIENDPOINTCOMMONPROPERTIES commonProperties{};
    commonProperties.TransportId = m_TransportTransportId;
    commonProperties.EndpointDeviceType = MidiEndpointDeviceType::MidiEndpointDeviceType_Normal;
    commonProperties.FriendlyName = friendlyName.c_str();
    commonProperties.TransportCode = transportCode.c_str();
    commonProperties.EndpointName = endpointName.c_str();
    commonProperties.EndpointDescription = nullptr; // TODO: Should generate a default description here
    commonProperties.CustomEndpointName = friendlyName.c_str(); // technically, the user supplied this so we put it in both spots
    commonProperties.CustomEndpointDescription = endpointDescription.size() > 0 ? endpointDescription.c_str() : nullptr;
    commonProperties.UniqueIdentifier = definition->EndpointUniqueIdentifier.c_str();
    commonProperties.SupportedDataFormats = MidiDataFormats::MidiDataFormats_UMP;
    commonProperties.NativeDataFormat = MidiDataFormats::MidiDataFormats_UMP;

    UINT32 capabilities {0};
    capabilities |= MidiEndpointCapabilities_SupportsMidi1Protocol;     // MIDI 1.0 only, so no SupportsMidi2Protocol
    capabilities |= MidiEndpointCapabilities_SupportsMultiClient;
    capabilities |= MidiEndpointCapabilities_GenerateIncomingTimestamps;
    commonProperties.Capabilities = (MidiEndpointCapabilities) capabilities;


    // add a single group terminal block in each direction to support MIDI 1.0 port creation without creating 16 ins and 16 outs.

    // TODO: This should be set via the configuration sent up

    std::vector<internal::GroupTerminalBlockInternal> blocks{ };

    internal::GroupTerminalBlockInternal gtb1;
    gtb1.Number = 1;             // gtb indexes start at 1
    gtb1.GroupCount = 1;         // todo: we could get this from properties
    gtb1.FirstGroupIndex = 0;    // group indexes start at 0
    gtb1.Protocol = 0x01;        // 0x01 = MIDI 1.0
    gtb1.MaxInputBandwidth = 0x0000;    // 0x0000 = not rate-limited. 0x0001 would be 31.25 kb/s
    gtb1.MaxOutputBandwidth = 0x0000;   // 0x0000 = not rate-limited. 0x0001 would be 31.25 kb/s
    gtb1.Direction = MIDI_GROUP_TERMINAL_BLOCK_INPUT;   // MIDI Out from user's perspective
    gtb1.Name = friendlyName; //+ L" Out";           // todo: get this from properties so folks can control the port name
    blocks.push_back(gtb1);

    internal::GroupTerminalBlockInternal gtb2;
    gtb2.Number = 1;             // gtb indexes start at 1
    gtb2.GroupCount = 1;         // todo: we could get this from properties
    gtb2.FirstGroupIndex = 0;    // group indexes start at 0
    gtb2.Protocol = 0x01;        // 0x01 = MIDI 1.0
    gtb2.MaxInputBandwidth = 0x0000;    // 0x0000 = not rate-limited. 0x0001 would be 31.25 kb/s
    gtb2.MaxOutputBandwidth = 0x0000;   // 0x0000 = not rate-limited. 0x0001 would be 31.25 kb/s
    gtb2.Direction = MIDI_GROUP_TERMINAL_BLOCK_OUTPUT;  // MIDI In from user's perspective
    gtb2.Name = friendlyName; // + L" In";           // todo: get this from properties so folks can control the port name
    blocks.push_back(gtb2);


    std::vector<std::byte> groupTerminalBlockData;
    if (internal::WriteGroupTerminalBlocksToPropertyDataPointer(blocks, groupTerminalBlockData))
    {
        interfaceDevProperties.push_back({ { PKEY_MIDI_GroupTerminalBlocks, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BINARY, (ULONG)groupTerminalBlockData.size(), (PVOID)groupTerminalBlockData.data() });

    }

    // Sort out MIDI 1.0 endpoint names
    WindowsMidiServicesNamingLib::MidiEndpointNameTable nameTable{};

    LOG_IF_FAILED(nameTable.PopulateAllEntriesForNativeUmpDevice(L"", blocks));

    if (Feature_Servicing_MIDI2PortNamingRework::IsEnabled())
    {
        nameTable.SetPortNamesHaveLegacyEquivalent(false);

        LOG_IF_FAILED(nameTable.RebuildNewStyleNames(friendlyName, false));
    }

    LOG_IF_FAILED(nameTable.WriteProperties(interfaceDevProperties));

    
    auto const activateHR = deviceManager->ActivateEndpoint(
        (PCWSTR)m_parentDeviceId.c_str(),                       // parent instance Id
        false,                                                  // UMP-only. When set to false, WinMM MIDI 1.0 ports are created
        MidiFlow::MidiFlowBidirectional,                        // MIDI Flow
        &commonProperties,
        (ULONG)interfaceDevProperties.size(),
        (ULONG)0,
        interfaceDevProperties.data(),
        nullptr,
        &createInfo,
        &newDeviceInterfaceId);

    RETURN_IF_FAILED(activateHR);

    // S_FALSE means the service already has an endpoint with this instance id, and no interface id
    // came back. That endpoint isn't this one, so it's left alone.
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_DEVICE_ALREADY_ATTACHED), activateHR == S_FALSE || newDeviceInterfaceId.get() == nullptr);

    TraceLoggingWrite(
        MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Endpoint activated", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingGuid(definition->AssociationId, "association id"),
        TraceLoggingWideString(definition->EndpointUniqueIdentifier.c_str(), "unique identifier"),
        TraceLoggingWideString(newDeviceInterfaceId.get(), "new device interface id")
    );

    // From here on, a failure must not leave this endpoint in the service with nothing in the
    // table to remove it by.
    auto removeOnFailure = wil::scope_exit([&]() noexcept
        {
            LOG_IF_FAILED(deviceManager->RemoveEndpoint(instanceId.c_str()));
        });

    // we need this for removal later
    definition->CreatedShortClientInstanceId = instanceId;
    definition->CreatedEndpointInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(newDeviceInterfaceId.get());

    // The device gets its own copy. The caller keeps reading this one for its response, and a
    // published definition is never changed in place.
    auto device = std::make_shared<MidiBasicLoopbackDevice>(
        std::make_shared<MidiBasicLoopbackDeviceDefinition const>(*definition),
        MidiBasicLoopbackFeedback::Create(definition->AssociationId, definition->FeedbackProtectionEnabled));

    // store for tracking. This also takes over the reservation.
    RETURN_IF_FAILED(table->AddDevice(definition->AssociationId, device));

    removeOnFailure.release();
    releaseReservation.release();

    // Written after activation because activation clears it, and nothing negotiates it for this endpoint
    BYTE configuredProtocol{ MIDI_PROP_CONFIGURED_PROTOCOL_MIDI1 };

    DEVPROPERTY configuredProtocolProperty{ { PKEY_MIDI_EndpointConfiguredProtocol, DEVPROP_STORE_SYSTEM, nullptr },
        DEVPROP_TYPE_BYTE, static_cast<ULONG>(sizeof(configuredProtocol)), &configuredProtocol };

    LOG_IF_FAILED(deviceManager->UpdateEndpointProperties(
        definition->CreatedEndpointInterfaceId.c_str(),
        1,
        &configuredProtocolProperty));

    TraceLoggingWrite(
        MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Done", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingGuid(definition->AssociationId, "association id"),
        TraceLoggingWideString(definition->EndpointUniqueIdentifier.c_str(), "unique identifier")
    );

    return S_OK;
}
CATCH_RETURN()



HRESULT
CMidi2BasicLoopbackMidiEndpointManager::Shutdown()
try
{
    TraceLoggingWrite(
        MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    // TransportState::Shutdown releases the static's reference to this object
    wil::com_ptr<CMidi2BasicLoopbackMidiEndpointManager> keepAlive{ this };

    m_initialized = false;

    // destroy and release all the devices we have created
    LOG_IF_FAILED(TransportState::Current().Shutdown());

    // Swapped out under the lock and released outside it
    wil::com_ptr_nothrow<IMidiDeviceManager> deviceManager;
    wil::com_ptr_nothrow<IMidiEndpointProtocolManager> protocolManager;
    {
        auto lock = m_serviceLock.lock_exclusive();
        deviceManager = std::move(m_MidiDeviceManager);
        protocolManager = std::move(m_MidiProtocolManager);
    }

    return S_OK;
}
CATCH_RETURN()

