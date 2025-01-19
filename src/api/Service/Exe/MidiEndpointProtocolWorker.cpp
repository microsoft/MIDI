// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"


_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolWorker::Initialize(
    GUID sessionId,
    GUID transportId,
    LPCWSTR endpointDeviceInterfaceId,
    std::shared_ptr<CMidiClientManager>& clientManager,
    std::shared_ptr<CMidiDeviceManager>& deviceManager,
    std::shared_ptr<CMidiSessionTracker>& sessionTracker
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    m_transportId = transportId;
    m_sessionId = sessionId;
    m_endpointDeviceInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);

    m_clientManager = clientManager;
    m_deviceManager = deviceManager;
    m_sessionTracker = sessionTracker;

    m_clientProcessId = GetCurrentProcessId();

    RETURN_IF_FAILED(m_endProcessing.create(wil::EventOptions::ManualReset));

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Initialize complete", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    return S_OK;
}




_Use_decl_annotations_
DEVPROPKEY
CMidiEndpointProtocolWorker::FunctionBlockPropertyKeyFromNumber(
    uint8_t functionBlockNumber
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    DEVPROPKEY key{};
    GUID propertyId{};

    if (functionBlockNumber < MIDI_MAX_FUNCTION_BLOCKS)
    {
        if (SUCCEEDED(::CLSIDFromString(MIDI_STRING_PKEY_GUID, &propertyId)))
        {
            key.fmtid = propertyId;
            key.pid = MIDI_FUNCTION_BLOCK_PROPERTY_INDEX_START + functionBlockNumber;
        }
        else
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"CLSIDFromString failed for Function Block Property Key", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
            );
        }
    }

    return key;
}

_Use_decl_annotations_
DEVPROPKEY
CMidiEndpointProtocolWorker::FunctionBlockNamePropertyKeyFromNumber(
    uint8_t functionBlockNumber
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    DEVPROPKEY key{};
    GUID propertyId{};

    if (functionBlockNumber < MIDI_MAX_FUNCTION_BLOCKS)
    {
        if (SUCCEEDED(::CLSIDFromString(MIDI_STRING_PKEY_GUID, &propertyId)))
        {
            key.fmtid = propertyId;
            key.pid = MIDI_FUNCTION_BLOCK_NAME_PROPERTY_INDEX_START + functionBlockNumber;
        }
    }

    return key;
}




_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolWorker::Start(
    ENDPOINTPROTOCOLNEGOTIATIONPARAMS negotiationParams
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );


    m_inInitialFunctionBlockDiscovery = true;

    // these are defaults, but discovery will tell us what we can really do here.
    m_preferToSendJRTimestampsToEndpoint = negotiationParams.PreferToSendJitterReductionTimestampsToEndpoint;
    m_preferToReceiveJRTimestampsFromEndpoint = negotiationParams.PreferToReceiveJitterReductionTimestampsFromEndpoint;
    m_preferredMidiProtocol = negotiationParams.PreferredMidiProtocol;

    // We continue listening for and updating metadata even after we return.
    // We don't raise any changed events here. Instead, anyone interested in getting
    // changes after the initial negotiation can use a device watcher on that device
    // and react to our stored property changes. This also helps prevent any races.

    try
    {
        // we do this here instead of in initialize so this is created on the worker thread
        if (!m_midiBiDiDevice)
        {
            wil::com_ptr_nothrow<IMidiTransport> serviceTransport{ nullptr };

            // we only support UMP data format for protocol negotiation
            TRANSPORTCREATIONPARAMS transportCreationParams{ };
            transportCreationParams.DataFormat = MidiDataFormats::MidiDataFormats_UMP;
            transportCreationParams.CallingApi = MidiApi_Service;

            DWORD mmcssTaskId{ 0 };
            LONGLONG context{ 0 };

            // this is not a good idea, but we don't have a reference to the COM lib here
            GUID midi2MidiSrvTransportIID = internal::StringToGuid(L"{2BA15E4E-5417-4A66-85B8-2B2260EFBC84}");
            RETURN_IF_FAILED(CoCreateInstance((IID)midi2MidiSrvTransportIID, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&serviceTransport)));
            RETURN_IF_NULL_ALLOC(serviceTransport);

            // create the bidi device
            RETURN_IF_FAILED(serviceTransport->Activate(__uuidof(IMidiBiDi), (void**)&m_midiBiDiDevice));
            RETURN_IF_NULL_ALLOC(m_midiBiDiDevice);

            RETURN_IF_FAILED(m_midiBiDiDevice->Initialize(
                (LPCWSTR)(m_endpointDeviceInterfaceId.c_str()),
                &transportCreationParams,
                &mmcssTaskId,
                (IMidiCallback*)(this),
                context,
                m_sessionId
            ));

            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Protocol negotiation connection created", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
            );
        }

        // add this connection to the session tracker. The manager already logged the overall session
        //LOG_IF_FAILED(m_sessionTracker->AddClientEndpointConnection(
        //    m_sessionId,
        //    m_deviceInterfaceId.c_str(),
        //    (MidiClientHandle)nullptr));

        if (!m_allInitialDiscoveryAndNegotiationMessagesReceived.is_valid())
        {
            RETURN_IF_FAILED(m_allInitialDiscoveryAndNegotiationMessagesReceived.create(wil::EventOptions::ManualReset));
        }


        m_declaredFunctionBlockCount = 0;

        m_functionBlocks.clear();
        m_functionBlockNames.clear();
        m_endpointName.clear();
        m_productInstanceId.clear();

        m_alreadyTriedToNegotiationOnce = false;

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Requesting discovery information", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        // start initial negotiation. Return when timed out or when we have all the requested info.
        LOG_IF_FAILED(RequestAllEndpointDiscoveryInformation());

        // hang out until all info comes in or we time out
        DWORD timeoutMilliseconds{ 15000 };
        m_allInitialDiscoveryAndNegotiationMessagesReceived.wait(timeoutMilliseconds);

        RETURN_IF_FAILED(UpdateAllFunctionBlockPropertiesIfComplete());

        m_inInitialFunctionBlockDiscovery = false;
        RETURN_IF_FAILED(SetDiscoveryCompleteProperty());


        // we just hang out until endProcessing is set.
        m_endProcessing.wait();

        return S_OK;
       
    }
    CATCH_RETURN();
}


HRESULT 
CMidiEndpointProtocolWorker::SetDiscoveryCompleteProperty()
{
    // this function is called when discovery has completed or timed out
    
    DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;

    DEVPROPERTY props[] =
    {
        {{ PKEY_MIDI_EndpointDiscoveryProcessComplete, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<DEVPROP_BOOLEAN>(sizeof(DEVPROP_BOOLEAN)), (PVOID)(&devPropTrue) },
    };

    RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_endpointDeviceInterfaceId.c_str(), ARRAYSIZE(props), props));

    return S_OK;
}

HRESULT
CMidiEndpointProtocolWorker::CheckIfDiscoveryComplete()
{
    if (m_taskEndpointInfoReceived &&
        m_taskFinalStreamNegotiationResponseReceived &&
        m_taskEndpointNameReceived &&
        m_taskEndpointProductInstanceIdReceived &&
        m_taskDeviceIdentityReceived &&
        m_functionBlocks.size() == m_declaredFunctionBlockCount &&
        m_functionBlockNames.size() == m_declaredFunctionBlockCount)
    {
        if (!m_allInitialDiscoveryAndNegotiationMessagesReceived.is_signaled())
        {
            m_allInitialDiscoveryAndNegotiationMessagesReceived.SetEvent();
        }
    }

    return S_OK;
}



_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolWorker::Callback(
    PVOID data,
    UINT size,
    LONGLONG position,
    LONGLONG context)
{

    //TraceLoggingWrite(
    //    MidiSrvTelemetryProvider::Provider(),
    //    MIDI_TRACE_EVENT_WARNING,
    //    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
    //    TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
    //    TraceLoggingPointer(this, "this"),
    //    TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
    //    TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    //);


    if (m_endProcessing.is_signaled()) return S_OK;

    UNREFERENCED_PARAMETER(position);
    UNREFERENCED_PARAMETER(context);

    RETURN_HR_IF_NULL(E_INVALIDARG, data);

    if (size == UMP128_BYTE_COUNT)
    {
        internal::PackedUmp128 ump;

        if (internal::FillPackedUmp128FromBytePointer((byte*)data, (uint8_t)size, ump))
        {
            // if type F, process it.

            if (internal::GetUmpMessageTypeFromFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_UMP_MESSAGE_TYPE)
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Stream message received.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                );

                auto messageStatus = internal::GetStatusFromStreamMessageFirstWord(ump.word0);

                switch (messageStatus)
                {
                case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_INFO_NOTIFICATION:
                    RETURN_IF_FAILED(ProcessEndpointInfoNotificationMessage(ump));
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_DEVICE_IDENTITY_NOTIFICATION:
                    RETURN_IF_FAILED(ProcessDeviceIdentityNotificationMessage(ump));
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_NOTIFICATION:
                    RETURN_IF_FAILED(ProcessStreamConfigurationRequest(ump));
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_INFO_NOTIFICATION:
                    RETURN_IF_FAILED(ProcessFunctionBlockInfoNotificationMessage(ump));
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION:
                    RETURN_IF_FAILED(ProcessFunctionBlockNameNotificationMessage(ump));
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_PRODUCT_INSTANCE_ID_NOTIFICATION:
                    RETURN_IF_FAILED(ProcessProductInstanceIdNotificationMessage(ump));
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION:
                    RETURN_IF_FAILED(ProcessEndpointNameNotificationMessage(ump));
                    break;

                default:
                    // some other type F we don't understand. Just ignore
                    break;

                }

                LOG_IF_FAILED(CheckIfDiscoveryComplete());

            }
            else
            {
                // not a stream message. Ignore and move on
            }
        }
        else
        {
            // couldn't fill the UMP. Shouldn't happen since we pre-validate
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Couldn't fill the UMP", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
            );

            RETURN_IF_FAILED(E_FAIL);
        }
    }
    else
    {
        // Not a UMP128 so can't be a stream message. Ignore and fall out quickly
    }


    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolWorker::ProcessEndpointInfoNotificationMessage(internal::PackedUmp128& ump)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Received Endpoint Info Notification", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    // check protocol
    auto supportsMidi2 = internal::GetEndpointInfoNotificationMidi2ProtocolCapabilityFromSecondWord(ump.word1);
    auto supportsMidi1 = internal::GetEndpointInfoNotificationMidi1ProtocolCapabilityFromSecondWord(ump.word1);

    auto endpointTransmitsJR = internal::GetEndpointInfoNotificationTransmitJRTimestampCapabilityFromSecondWord(ump.word1);
    auto endpointReceivesJR = internal::GetEndpointInfoNotificationReceiveJRTimestampCapabilityFromSecondWord(ump.word1);

    bool functionBlocksAreStatic = internal::GetEndpointInfoNotificationStaticFunctionBlocksFlagFromSecondWord(ump.word1);
    uint8_t functionBlocksDeclaredCount = internal::GetEndpointInfoNotificationNumberOfFunctionBlocksFromSecondWord(ump.word1);

    if (!supportsMidi1 && m_preferredMidiProtocol == MIDI_PROP_CONFIGURED_PROTOCOL_MIDI1)
    {
        // endpoint doesn't support MIDI 1 protocol, so we will only negotiate MIDI 2
        m_preferredMidiProtocol = MIDI_PROP_CONFIGURED_PROTOCOL_MIDI2;
    }

    // this check is second so we fall back to MIDI 1 protocol if MIDI 2 isn't supported
    if (!supportsMidi2 && m_preferredMidiProtocol == MIDI_PROP_CONFIGURED_PROTOCOL_MIDI2)
    {
        // endpoint doesn't support MIDI 2 protocol, so we will only negotiate MIDI 1
        m_preferredMidiProtocol = MIDI_PROP_CONFIGURED_PROTOCOL_MIDI1;
    }

    // JR timestamp preferences
    if (!endpointTransmitsJR && m_preferToReceiveJRTimestampsFromEndpoint)
    {
        // endpoint doesn't transmit JR, so we won't request it
        m_preferToReceiveJRTimestampsFromEndpoint = false;
    }

    if (!endpointReceivesJR && m_preferToSendJRTimestampsToEndpoint)
    {
        // endpoint doesn't receive JR, so we won't request it
        m_preferToSendJRTimestampsToEndpoint = false;
    }


    if (m_inInitialFunctionBlockDiscovery)
    {
        // per-spec, these values are not allowed to change after initial discovery
        m_functionBlocksAreStatic = functionBlocksAreStatic;
        m_declaredFunctionBlockCount = functionBlocksDeclaredCount;
    }

    if (!m_inInitialFunctionBlockDiscovery && (m_declaredFunctionBlockCount != functionBlocksDeclaredCount))
    {
        // illegal to change count of function blocks after initial discovery

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Illegal attempt to change function block count after initial discovery", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        return S_OK;
    }

    if (!m_inInitialFunctionBlockDiscovery && (m_functionBlocksAreStatic != functionBlocksAreStatic))
    {
        // illegal to change static/not-static property after initial discovery

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Illegal attempt to change the static function block declaration after initial discovery", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );


        return S_OK;
    }

    RETURN_IF_FAILED(UpdateEndpointInfoProperties(ump));

    m_taskEndpointInfoReceived = true;

    // when we get the info notification during initial discovery, we request function blocks, too
    if (m_inInitialFunctionBlockDiscovery)
    {
        RETURN_IF_FAILED(RequestAllFunctionBlocks());
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolWorker::ProcessDeviceIdentityNotificationMessage(internal::PackedUmp128& ump)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Received Device Identity Notification", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    LOG_IF_FAILED(UpdateDeviceIdentityProperty(ump));

    m_taskDeviceIdentityReceived = true;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolWorker::ProcessFunctionBlockInfoNotificationMessage(internal::PackedUmp128& ump)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Received Function Block Info Notification", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    auto blockNumber = internal::GetFunctionBlockNumberFromInfoNotificationFirstWord(ump.word0);

    // after initial discovery, the function block count can't change, no matter
    // if static or not. Numbers must be 0 to CountFunctionBlocksDeclared-1

    if (!m_inInitialFunctionBlockDiscovery && m_functionBlocksAreStatic)
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Endpoint illegally tried change static function blocks", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        return S_OK;
    }

    if (m_inInitialFunctionBlockDiscovery && blockNumber < m_declaredFunctionBlockCount)
    {
        MidiFunctionBlockProperty prop = BuildFunctionBlockPropertyFromInfoNotificationMessage(ump);

        m_functionBlocks.insert_or_assign(prop.BlockNumber, prop);
    }
    else if (!m_inInitialFunctionBlockDiscovery && blockNumber < m_declaredFunctionBlockCount)
    {
        // normal update after initial discovery
        RETURN_IF_FAILED(UpdateFunctionBlockProperty(ump));
    }
    else if (!m_inInitialFunctionBlockDiscovery && blockNumber >= m_declaredFunctionBlockCount)
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Endpoint illegally tried to add more function blocks after initial discovery", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolWorker::ProcessFunctionBlockNameNotificationMessage(internal::PackedUmp128& ump)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Received Function Block Name Notification", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    if (!m_inInitialFunctionBlockDiscovery && m_functionBlocksAreStatic)
    {
        // we reject this function block because the initial ones were declared static,
        // meaning no changes are allowed
        return S_OK;
    }

    uint8_t functionBlockNumber = MIDIWORDBYTE3(ump.word0);

    switch (internal::GetFormFromStreamMessageFirstWord(ump.word0))
    {
    case MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE: // complete name in single message. Just update property
    {
        MidiFunctionBlockName name{ };

        name.Name = internal::TrimmedWStringCopy(ParseStreamTextMessage(ump));
        name.IsComplete = true;

        m_functionBlockNames.insert_or_assign(functionBlockNumber, name);

        RETURN_IF_FAILED(UpdateFunctionBlockNameProperty(functionBlockNumber, name.Name));
    }
    break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_START: // start of multi-part name message. Overwrite any other name in the map
    {
        MidiFunctionBlockName name{ };

        name.Name = internal::TrimmedWStringCopy(ParseStreamTextMessage(ump));
        name.IsComplete = false;

        m_functionBlockNames.insert_or_assign(functionBlockNumber, name);
    }
    break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_CONTINUE: //continuation of multi-part name message. Append to name in map
        if (m_functionBlockNames.find(functionBlockNumber) != m_functionBlockNames.end())
        {
            auto& name = m_functionBlockNames.find(functionBlockNumber)->second;
            name.Name += ParseStreamTextMessage(ump);

            m_functionBlockNames.insert_or_assign(functionBlockNumber, name);
        }
        else
        {
            // name isn't already in the map, so a start message was skipped. Don't append anything at all.
        }
        break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_END: // end of multi-part name message. Finish name and update property
        if (m_functionBlockNames.find(functionBlockNumber) != m_functionBlockNames.end())
        {
            auto& name = m_functionBlockNames.find(functionBlockNumber)->second;
            name.Name = internal::TrimmedWStringCopy(name.Name + ParseStreamTextMessage(ump));
            name.IsComplete = true;

            RETURN_IF_FAILED(UpdateFunctionBlockNameProperty(functionBlockNumber, name.Name));
        }
        else
        {
            // name isn't already in the map, so at least start message was skipped. Don't append anything at all.
        }
        break;

    default:
        // won't actually happen because the Form field is only 2 bits
        break;
    }

    return S_OK;
}



_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolWorker::ProcessProductInstanceIdNotificationMessage(internal::PackedUmp128& ump)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Received Product Instance Id Notification", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    switch (internal::GetFormFromStreamMessageFirstWord(ump.word0))
    {
    case MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE: // complete name in single message. Just update property
        m_productInstanceId = internal::TrimmedWStringCopy(ParseStreamTextMessage(ump));
        RETURN_IF_FAILED(UpdateEndpointProductInstanceIdProperty());

        m_taskEndpointProductInstanceIdReceived = true;
        break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_START: // start of multi-part name message. Overwrite any other name in the map
    {
        m_productInstanceId = ParseStreamTextMessage(ump);
    }
    break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_CONTINUE: //continuation of multi-part name message. Append to name in map
        if (!m_productInstanceId.empty())
        {
            m_productInstanceId += ParseStreamTextMessage(ump);
        }
        else
        {
            // name isn't already started, so a start message was skipped. Don't append anything at all.
        }
        break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_END: // end of multi-part name message. Finish name and update property
        if (!m_productInstanceId.empty())
        {
            m_productInstanceId = internal::TrimmedWStringCopy(m_productInstanceId + ParseStreamTextMessage(ump));
            RETURN_IF_FAILED(UpdateEndpointProductInstanceIdProperty());
            m_taskEndpointProductInstanceIdReceived = true;
        }
        else
        {
            // name isn't already started, so a start message was skipped. Don't append anything at all.
        }

        break;

    default:
        // won't actually happen because the Form field is only 2 bits
        break;
    }

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolWorker::ProcessEndpointNameNotificationMessage(internal::PackedUmp128& ump)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Received Endpoint Name Notification", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    switch (internal::GetFormFromStreamMessageFirstWord(ump.word0))
    {
    case MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE: // complete name in single message. Just update property
        m_endpointName = internal::TrimmedWStringCopy(ParseStreamTextMessage(ump));
        RETURN_IF_FAILED(UpdateEndpointNameProperty());
        m_taskEndpointNameReceived = true;
        break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_START: // start of multi-part name message. Overwrite any other name in the map
    {
        m_endpointName = ParseStreamTextMessage(ump);
    }
    break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_CONTINUE: //continuation of multi-part name message. Append to name in map
        if (!m_endpointName.empty())
        {
            m_endpointName += ParseStreamTextMessage(ump);
        }
        else
        {
            // name isn't already started, so a start message was skipped. Don't append anything at all.
        }
        break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_END: // end of multi-part name message. Finish name and update property
        if (!m_endpointName.empty())
        {
            m_endpointName = internal::TrimmedWStringCopy(m_endpointName + ParseStreamTextMessage(ump));

            RETURN_IF_FAILED(UpdateEndpointNameProperty());
            m_taskEndpointNameReceived = true;
        }
        else
        {
            // name isn't already started, so a start message was skipped. Don't append anything at all.
        }

        break;

    default:
        // won't actually happen because the Form field is only 2 bits
        break;
    }

    return S_OK;
}







HRESULT
CMidiEndpointProtocolWorker::RequestAllFunctionBlocks()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    RETURN_HR_IF_NULL(E_POINTER, m_midiBiDiDevice);

    // if we're not in initial discovery and the function blocks are static, we don't issue another request
    if (!m_inInitialFunctionBlockDiscovery && m_functionBlocksAreStatic)
    {
        return S_OK;
    }

    internal::PackedUmp128 ump{};

    // first word
    ump.word0 = internal::BuildFunctionBlockDiscoveryRequestFirstWord(
        MIDI_STREAM_MESSAGE_FUNCTION_BLOCK_REQUEST_ALL_FUNCTION_BLOCKS,
        MIDI_STREAM_MESSAGE_FUNCTION_BLOCK_REQUEST_MESSAGE_ALL_FILTER_FLAGS);

    // send it immediately
    RETURN_IF_FAILED(m_midiBiDiDevice->SendMidiMessage((byte*)&ump, (UINT)sizeof(ump), 0));

    return S_OK;
}

HRESULT
CMidiEndpointProtocolWorker::RequestAllEndpointDiscoveryInformation()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    RETURN_HR_IF_NULL(E_POINTER, m_midiBiDiDevice);

    internal::PackedUmp128 ump{};

    // first word
    ump.word0 = internal::BuildEndpointDiscoveryRequestFirstWord(MIDI_PREFERRED_UMP_VERSION_MAJOR, MIDI_PREFERRED_UMP_VERSION_MINOR);

    // second word
    uint8_t filterBitmap = MIDI_ENDPOINT_DISCOVERY_MESSAGE_ALL_FILTER_FLAGS;
    internal::SetMidiWordMostSignificantByte4(ump.word1, filterBitmap);

    // send it immediately
    RETURN_IF_FAILED(m_midiBiDiDevice->SendMidiMessage((byte*)&ump, (UINT)sizeof(ump), 0));

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit success", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolWorker::ProcessStreamConfigurationRequest(internal::PackedUmp128& ump)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    RETURN_HR_IF_NULL(E_POINTER, m_midiBiDiDevice);

    // see if all is what we want. If not, we'll send a request to change configuration.

    auto protocol = internal::GetStreamConfigurationNotificationProtocolFromFirstWord(ump.word0);
    auto endpointRxJR = internal::GetStreamConfigurationNotificationReceiveJRFromFirstWord(ump.word0);
    auto endpointTxJR = internal::GetStreamConfigurationNotificationTransmitJRFromFirstWord(ump.word0);

    // TODO: See bugs GH 279/280. We should only negotiate protocol if the device supports both m1 and m2.
    // if it only supports m1, we should stick with that. If only m2, we should stick with that.
    // but JR timestamps can still be negotiated

    if (protocol != m_preferredMidiProtocol ||
        endpointRxJR != m_preferToSendJRTimestampsToEndpoint ||
        endpointTxJR != m_preferToReceiveJRTimestampsFromEndpoint)
    {
        if (!m_alreadyTriedToNegotiationOnce)
        {
            internal::PackedUmp128 configurationRequestUmp{};

            ump.word0 = internal::BuildStreamConfigurationRequestFirstWord(
                m_preferredMidiProtocol,
                m_preferToSendJRTimestampsToEndpoint,
                m_preferToReceiveJRTimestampsFromEndpoint);

            m_alreadyTriedToNegotiationOnce = true;

            // send it immediately
            RETURN_IF_FAILED(m_midiBiDiDevice->SendMidiMessage((byte*)&configurationRequestUmp, (UINT)sizeof(configurationRequestUmp), 0));

            return S_OK;
        }
        else
        {
            //// we've already tried negotiating once. Don't do it again
            //m_taskFinalStreamNegotiationResponseReceived = true;

            internal::PackedUmp128 configurationNotificationUmp{};

            ump.word0 = internal::BuildStreamConfigurationNotificationFirstWord(
                m_preferredMidiProtocol,
                m_preferToReceiveJRTimestampsFromEndpoint,  // reversed from the request message. See spec
                m_preferToSendJRTimestampsToEndpoint        // reversed from the request message. See spec
            );

            LOG_IF_FAILED(UpdateStreamConfigurationProperties(ump));

            RETURN_IF_FAILED(m_midiBiDiDevice->SendMidiMessage((byte*)&configurationNotificationUmp, (UINT)sizeof(configurationNotificationUmp), 0));
            
            return S_OK;
        }
    }
    else
    {
        internal::PackedUmp128 configurationNotificationUmp{};

        ump.word0 = internal::BuildStreamConfigurationNotificationFirstWord(
            m_preferredMidiProtocol,
            m_preferToReceiveJRTimestampsFromEndpoint,  // reversed from the request message. See spec
            m_preferToSendJRTimestampsToEndpoint        // reversed from the request message. See spec
            );

        LOG_IF_FAILED(UpdateStreamConfigurationProperties(ump));

        RETURN_IF_FAILED(m_midiBiDiDevice->SendMidiMessage((byte*)&configurationNotificationUmp, (UINT)sizeof(configurationNotificationUmp), 0));

        return S_OK;
    }
}


HRESULT
CMidiEndpointProtocolWorker::Shutdown()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    // signal to stop worker thread
    EndProcessing();

    if (m_midiBiDiDevice)
    {
        m_midiBiDiDevice->Shutdown();
        m_midiBiDiDevice.reset();
    }

    //if (m_sessionTracker)
    //{
    //    LOG_IF_FAILED(m_sessionTracker->RemoveClientEndpointConnection(m_sessionId, m_clientProcessId, m_deviceInterfaceId.c_str(), (MidiClientHandle)nullptr));
    //}

    return S_OK;
}




// TODO: An optimization that will help prevent so many update notifications (which trigger device manager
// refresh, as well as client-side events) would be to build a complete update list when in the middle of
// initial discovery/negotiation and committing them only after that completes or times out.


HRESULT
CMidiEndpointProtocolWorker::UpdateEndpointNameProperty()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    auto cleanedValue{ internal::TrimmedWStringCopy(m_endpointName) + L"\0" };

    if (!cleanedValue.empty())
    {
        FILETIME currentTime;
        GetSystemTimePreciseAsFileTime(&currentTime);

        DEVPROPERTY props[] =
        {
            {{ PKEY_MIDI_EndpointProvidedName, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_STRING, static_cast<ULONG>((cleanedValue.length() + 1) * sizeof(WCHAR)), (PVOID)(cleanedValue.c_str()) },
            {{ PKEY_MIDI_EndpointProvidedNameLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_FILETIME, static_cast<ULONG>(sizeof(FILETIME)), (PVOID)(&currentTime) },
        };

        RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_endpointDeviceInterfaceId.c_str(), ARRAYSIZE(props), props));

        // clear out any old value that's in there
        //m_endpointName.clear();

        return S_OK;
    }
    else
    {
        // TODO: Need to remove the property

        return S_OK;
    }
}

HRESULT
CMidiEndpointProtocolWorker::UpdateEndpointProductInstanceIdProperty()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );


    FILETIME currentTime;
    GetSystemTimePreciseAsFileTime(&currentTime);


    std::wstring cleanedValue{ internal::TrimmedWStringCopy(m_productInstanceId) + L"\0" };

    if (!cleanedValue.empty())
    {
        DEVPROPERTY props[] =
        {
            {{ PKEY_MIDI_EndpointProvidedProductInstanceId, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_STRING, static_cast<ULONG>((cleanedValue.length() + 1) * sizeof(WCHAR)), (PVOID)(cleanedValue.c_str()) },
            {{ PKEY_MIDI_EndpointProvidedProductInstanceIdLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_FILETIME, static_cast<ULONG>(sizeof(FILETIME)), (PVOID)(&currentTime) },
        };

        RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_endpointDeviceInterfaceId.c_str(), ARRAYSIZE(props), props));

        // clear out any old value that's in there
        //m_productInstanceId.clear();

        return S_OK;
    }
    else
    {
        // Need to remove the property
        DEVPROPERTY props[] =
        {
            {{ PKEY_MIDI_EndpointProvidedProductInstanceId, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_EMPTY, 0, nullptr },
            {{ PKEY_MIDI_EndpointProvidedProductInstanceIdLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_FILETIME, static_cast<ULONG>(sizeof(FILETIME)), (PVOID)(&currentTime) },
        };

        RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_endpointDeviceInterfaceId.c_str(), ARRAYSIZE(props), props));

        return S_OK;
    }
}


_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolWorker::UpdateFunctionBlockNameProperty(uint8_t functionBlockNumber, std::wstring name)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    FILETIME currentTime;
    GetSystemTimePreciseAsFileTime(&currentTime);

    std::wstring cleanedValue{ internal::TrimmedWStringCopy(name) + L"\0" };

    if (!cleanedValue.empty())
    {
        DEVPROPERTY props[] =
        {
            {{ FunctionBlockNamePropertyKeyFromNumber(functionBlockNumber), DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_STRING, static_cast<ULONG>((cleanedValue.length() + 1) * sizeof(WCHAR)), (PVOID)(cleanedValue.c_str()) },
            {{ PKEY_MIDI_FunctionBlocksLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_FILETIME, static_cast<ULONG>(sizeof(FILETIME)), (PVOID)(&currentTime) },
        };

        RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_endpointDeviceInterfaceId.c_str(), ARRAYSIZE(props), props));
    }
    else
    {
        // Need to remove the property
        DEVPROPERTY props[] =
        {
            {{ FunctionBlockNamePropertyKeyFromNumber(functionBlockNumber), DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_EMPTY, 0, nullptr },
            {{ PKEY_MIDI_FunctionBlocksLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_FILETIME, static_cast<ULONG>(sizeof(FILETIME)), (PVOID)(&currentTime) },
        };

        RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_endpointDeviceInterfaceId.c_str(), ARRAYSIZE(props), props));
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolWorker::UpdateStreamConfigurationProperties(internal::PackedUmp128& endpointStreamConfigurationNotificationMessage)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    FILETIME currentTime;
    GetSystemTimePreciseAsFileTime(&currentTime);

    BYTE configuredProtocol = MIDIWORDBYTE3(endpointStreamConfigurationNotificationMessage.word0);

    DEVPROP_BOOLEAN configuredToSendJR = MIDIWORDBYTE4LOWBIT1(endpointStreamConfigurationNotificationMessage.word0) ? DEVPROP_TRUE : DEVPROP_FALSE;
    DEVPROP_BOOLEAN configuredToReceiveJR = MIDIWORDBYTE4LOWBIT2(endpointStreamConfigurationNotificationMessage.word0) ? DEVPROP_TRUE : DEVPROP_FALSE;

    DEVPROPERTY props[] =
    {
        {{ PKEY_MIDI_EndpointConfiguredToSendJRTimestamps, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(configuredToSendJR)), &configuredToSendJR },

        {{ PKEY_MIDI_EndpointConfiguredToReceiveJRTimestamps, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(configuredToReceiveJR)), &configuredToReceiveJR },

        {{ PKEY_MIDI_EndpointConfiguredProtocol, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BYTE, static_cast<ULONG>(sizeof(configuredProtocol)), &configuredProtocol },

        {{ PKEY_MIDI_EndpointConfigurationLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_FILETIME, static_cast<ULONG>(sizeof(FILETIME)), (PVOID)(&currentTime) },

    };

    RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_endpointDeviceInterfaceId.c_str(), ARRAYSIZE(props), props));

    return S_OK;
}


// The device identity is a single message, so we don't keep a copy. Instead, we just do
// the parsing here and then update the property. We assume this has already been identified
// as a type=F, form=0, status=0x02 stream message
_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolWorker::UpdateDeviceIdentityProperty(internal::PackedUmp128& identityMessage)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    MidiDeviceIdentityProperty prop;

    prop.Reserved0 = MIDIWORDSHORT2(identityMessage.word0);

    prop.Reserved1 = internal::CleanupByte7(MIDIWORDBYTE1(identityMessage.word1));
    prop.ManufacturerSysExIdByte1 = internal::CleanupByte7(MIDIWORDBYTE2(identityMessage.word1));
    prop.ManufacturerSysExIdByte2 = internal::CleanupByte7(MIDIWORDBYTE3(identityMessage.word1));
    prop.ManufacturerSysExIdByte3 = internal::CleanupByte7(MIDIWORDBYTE4(identityMessage.word1));

    prop.DeviceFamilyLsb = internal::CleanupByte7(MIDIWORDBYTE1(identityMessage.word2));
    prop.DeviceFamilyMsb = internal::CleanupByte7(MIDIWORDBYTE2(identityMessage.word2));
    prop.DeviceFamilyModelNumberLsb = internal::CleanupByte7(MIDIWORDBYTE3(identityMessage.word2));
    prop.DeviceFamilyModelNumberMsb = internal::CleanupByte7(MIDIWORDBYTE4(identityMessage.word2));

    prop.SoftwareRevisionLevelByte1 = internal::CleanupByte7(MIDIWORDBYTE1(identityMessage.word3));
    prop.SoftwareRevisionLevelByte2 = internal::CleanupByte7(MIDIWORDBYTE2(identityMessage.word3));
    prop.SoftwareRevisionLevelByte3 = internal::CleanupByte7(MIDIWORDBYTE3(identityMessage.word3));
    prop.SoftwareRevisionLevelByte4 = internal::CleanupByte7(MIDIWORDBYTE4(identityMessage.word3));

    FILETIME currentTime;
    GetSystemTimePreciseAsFileTime(&currentTime);

    DEVPROPERTY props[] =
    {
        {{ PKEY_MIDI_DeviceIdentity, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BINARY, static_cast<ULONG>(sizeof(prop)), (PVOID)(&prop) },

        {{ PKEY_MIDI_DeviceIdentityLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_FILETIME, static_cast<ULONG>(sizeof(FILETIME)), (PVOID)(&currentTime) },

    };

    RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_endpointDeviceInterfaceId.c_str(), ARRAYSIZE(props), props));

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolWorker::UpdateEndpointInfoProperties(internal::PackedUmp128& endpointInfoNotificationMessage)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    BYTE umpVersionMajor = internal::GetEndpointInfoNotificationUmpVersionMajorFirstWord(endpointInfoNotificationMessage.word0);
    BYTE umpVersionMinor = internal::GetEndpointInfoNotificationUmpVersionMinorFirstWord(endpointInfoNotificationMessage.word0);
    BYTE functionBlockCount = internal::GetEndpointInfoNotificationNumberOfFunctionBlocksFromSecondWord(endpointInfoNotificationMessage.word1);

    DEVPROP_BOOLEAN functionBlocksAreStatic = internal::GetEndpointInfoNotificationStaticFunctionBlocksFlagFromSecondWord(endpointInfoNotificationMessage.word1) ? DEVPROP_TRUE : DEVPROP_FALSE;

    DEVPROP_BOOLEAN supportsMidi1Protocol = internal::GetEndpointInfoNotificationMidi1ProtocolCapabilityFromSecondWord(endpointInfoNotificationMessage.word1) ? DEVPROP_TRUE : DEVPROP_FALSE;
    DEVPROP_BOOLEAN supportsMidi2Protocol = internal::GetEndpointInfoNotificationMidi2ProtocolCapabilityFromSecondWord(endpointInfoNotificationMessage.word1) ? DEVPROP_TRUE : DEVPROP_FALSE;

    DEVPROP_BOOLEAN supportsSendingJR = internal::GetEndpointInfoNotificationTransmitJRTimestampCapabilityFromSecondWord(endpointInfoNotificationMessage.word1) ? DEVPROP_TRUE : DEVPROP_FALSE;
    DEVPROP_BOOLEAN supportsReceivingJR = internal::GetEndpointInfoNotificationReceiveJRTimestampCapabilityFromSecondWord(endpointInfoNotificationMessage.word1) ? DEVPROP_TRUE : DEVPROP_FALSE;

    FILETIME currentTime;
    GetSystemTimePreciseAsFileTime(&currentTime);


    DEVPROPERTY props[] =
    {
        {{ PKEY_MIDI_EndpointSupportsMidi2Protocol, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(supportsMidi2Protocol)), &supportsMidi2Protocol },

        {{ PKEY_MIDI_EndpointSupportsMidi1Protocol, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(supportsMidi1Protocol)), &supportsMidi1Protocol },

        {{ PKEY_MIDI_EndpointSupportsReceivingJRTimestamps, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(supportsReceivingJR)), &supportsReceivingJR },

        {{ PKEY_MIDI_EndpointSupportsSendingJRTimestamps, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(supportsSendingJR)), &supportsSendingJR },

        {{ PKEY_MIDI_EndpointUmpVersionMajor, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BYTE, static_cast<ULONG>(sizeof(umpVersionMajor)), &umpVersionMajor },

        {{ PKEY_MIDI_EndpointUmpVersionMinor, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BYTE, static_cast<ULONG>(sizeof(umpVersionMinor)), &umpVersionMinor },

        {{ PKEY_MIDI_FunctionBlockDeclaredCount, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BYTE, static_cast<ULONG>(sizeof(functionBlockCount)), &functionBlockCount },

        {{ PKEY_MIDI_FunctionBlocksAreStatic, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(functionBlocksAreStatic)), &functionBlocksAreStatic },

        {{ PKEY_MIDI_EndpointInformationLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_FILETIME, static_cast<ULONG>(sizeof(FILETIME)), (PVOID)(&currentTime) },

    };

    RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_endpointDeviceInterfaceId.c_str(), ARRAYSIZE(props), props));

    return S_OK;
}



HRESULT
CMidiEndpointProtocolWorker::UpdateAllFunctionBlockPropertiesIfComplete()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    // we only check blocks, not names, because names are optional
    // this may lead to a bit of a race, but it's the best we can do
    if (m_functionBlocks.size() != m_declaredFunctionBlockCount)
    {
        return S_OK;
    }

    // this is for efficiency during initial discovery. This function is called when all 
    // function blocks have been received. It's possible not all names have been received
    // which is a bit of a pain, as that is an allowed outcome.

    std::vector<DEVPROPERTY> props{ };

    // add all function blocks
    for (auto const& fb : m_functionBlocks)
    {
        DEVPROPKEY propKey = FunctionBlockPropertyKeyFromNumber(fb.first);

        props.push_back({{ propKey, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BINARY, static_cast<ULONG>(sizeof(fb.second)), (PVOID)(&fb.second) });
    }

    // add all function block names

    for (auto& fbname : m_functionBlockNames)
    {
        if (fbname.second.IsComplete)
        {
            fbname.second.Name = internal::TrimmedWStringCopy(fbname.second.Name) + L"\0" ;

            if (!fbname.second.Name.empty())
            {
                props.push_back({{ FunctionBlockNamePropertyKeyFromNumber(fbname.first), DEVPROP_STORE_SYSTEM, nullptr},
                    DEVPROP_TYPE_STRING, static_cast<ULONG>((fbname.second.Name.length() + 1) * sizeof(WCHAR)), (PVOID)(fbname.second.Name.c_str()) });
            }
            else
            {
                props.push_back({{ FunctionBlockNamePropertyKeyFromNumber(fbname.first), DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_EMPTY, 0, nullptr });
            }
        }
    }

    // add the update timestamp

    FILETIME currentTime;
    GetSystemTimePreciseAsFileTime(&currentTime);

    props.push_back({ { PKEY_MIDI_FunctionBlocksLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_FILETIME, static_cast<ULONG>(sizeof(FILETIME)), (PVOID)(&currentTime) });


    // update all the properties
    RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_endpointDeviceInterfaceId.c_str(), (ULONG)props.size(), (DEVPROPERTY*)props.data()));

    // we have all the initial function blocks, so we're out of the FB discovery phase
    m_inInitialFunctionBlockDiscovery = false;

    return S_OK;
}

_Use_decl_annotations_
MidiFunctionBlockProperty 
CMidiEndpointProtocolWorker::BuildFunctionBlockPropertyFromInfoNotificationMessage(internal::PackedUmp128& ump)
{
    MidiFunctionBlockProperty prop;

    prop.IsActive = internal::GetFunctionBlockActiveFlagFromInfoNotificationFirstWord(ump.word0);
    prop.BlockNumber = internal::GetFunctionBlockNumberFromInfoNotificationFirstWord(ump.word0);
    prop.Reserved0 = MIDIWORDBYTE4LOWCRUMB4(ump.word0);
    prop.UIHint = internal::GetFunctionBlockUIHintFromInfoNotificationFirstWord(ump.word0);
    prop.Midi1 = internal::GetFunctionBlockMidi10FromInfoNotificationFirstWord(ump.word0);
    prop.Direction = internal::GetFunctionBlockDirectionFromInfoNotificationFirstWord(ump.word0);

    prop.FirstGroup = internal::GetFunctionBlockFirstGroupFromInfoNotificationSecondWord(ump.word1);
    prop.NumberOfGroupsSpanned = internal::GetFunctionBlockNumberOfGroupsFromInfoNotificationSecondWord(ump.word1);
    prop.MidiCIMessageVersionFormat = internal::GetFunctionBlockMidiCIVersionFromInfoNotificationSecondWord(ump.word1);
    prop.MaxSysEx8Streams = internal::GetFunctionBlockMaxSysex8StreamsFromInfoNotificationSecondWord(ump.word1);

    prop.Reserved1 = ump.word2;
    prop.Reserved2 = ump.word3;

    return prop;
}

_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolWorker::UpdateFunctionBlockProperty(internal::PackedUmp128& functionBlockInfoNotificationMessage)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    MidiFunctionBlockProperty prop = BuildFunctionBlockPropertyFromInfoNotificationMessage(functionBlockInfoNotificationMessage);


    FILETIME currentTime;
    GetSystemTimePreciseAsFileTime(&currentTime);

    DEVPROPKEY propKey = FunctionBlockPropertyKeyFromNumber(prop.BlockNumber);

    DEVPROPERTY props[] =
    {
        {{ propKey, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BINARY, static_cast<ULONG>(sizeof(prop)), (PVOID)(&prop)},

        {{ PKEY_MIDI_FunctionBlocksLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_FILETIME, static_cast<ULONG>(sizeof(FILETIME)), (PVOID)(&currentTime) },
    };

    RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_endpointDeviceInterfaceId.c_str(), ARRAYSIZE(props), props));

    m_functionBlocks.insert_or_assign(prop.BlockNumber, prop);

    return S_OK;
}




_Use_decl_annotations_
std::wstring 
CMidiEndpointProtocolWorker::ParseStreamTextMessage(internal::PackedUmp128& message)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );


    // check the status to know which byte is first to be grabbed

    // preallocate a wstring to the maximum length for a single message to avoid reallocation. Fill with zero

    uint8_t maxCharsThisMessage{ };

    uint16_t messageStatus = internal::GetStatusFromStreamMessageFirstWord(message.word0);

    if (messageStatus == MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION)
    {
        // function block name messages have 13 character bytes instead of 14 due to having the FB number in the message
        maxCharsThisMessage = 13;
    }
    else if ((messageStatus == MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION) ||
             (messageStatus == MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_PRODUCT_INSTANCE_ID_NOTIFICATION))
    {
        maxCharsThisMessage = 14;
    }
    else
    {
        // we don't recognize this type of text message. Bail

        return std::wstring{};
    }

    std::wstring text;
    text.reserve(maxCharsThisMessage + 1);  // try to avoid reallocations


    if (maxCharsThisMessage == 14)
    {
        if (MIDIWORDBYTE3(message.word0) != 0) text.push_back((wchar_t)MIDIWORDBYTE3(message.word0));
    }

    // this unroll is easier than looping, honestly.
    // Also, the property set completely fails if there are any embedded nuls, so need to
    // ignore any in the source data
    if (MIDIWORDBYTE4(message.word0) != 0) text.push_back((wchar_t)MIDIWORDBYTE4(message.word0));

    if (MIDIWORDBYTE1(message.word1) != 0) text.push_back((wchar_t)MIDIWORDBYTE1(message.word1));
    if (MIDIWORDBYTE2(message.word1) != 0) text.push_back((wchar_t)MIDIWORDBYTE2(message.word1));
    if (MIDIWORDBYTE3(message.word1) != 0) text.push_back((wchar_t)MIDIWORDBYTE3(message.word1));
    if (MIDIWORDBYTE4(message.word1) != 0) text.push_back((wchar_t)MIDIWORDBYTE4(message.word1));

    if (MIDIWORDBYTE1(message.word2) != 0) text.push_back((wchar_t)MIDIWORDBYTE1(message.word2));
    if (MIDIWORDBYTE2(message.word2) != 0) text.push_back((wchar_t)MIDIWORDBYTE2(message.word2));
    if (MIDIWORDBYTE3(message.word2) != 0) text.push_back((wchar_t)MIDIWORDBYTE3(message.word2));
    if (MIDIWORDBYTE4(message.word2) != 0) text.push_back((wchar_t)MIDIWORDBYTE4(message.word2));

    if (MIDIWORDBYTE1(message.word3) != 0) text.push_back((wchar_t)MIDIWORDBYTE1(message.word3));
    if (MIDIWORDBYTE2(message.word3) != 0) text.push_back((wchar_t)MIDIWORDBYTE2(message.word3));
    if (MIDIWORDBYTE3(message.word3) != 0) text.push_back((wchar_t)MIDIWORDBYTE3(message.word3));
    if (MIDIWORDBYTE4(message.word3) != 0) text.push_back((wchar_t)MIDIWORDBYTE4(message.word3));

    text.shrink_to_fit();

    return text;
}




