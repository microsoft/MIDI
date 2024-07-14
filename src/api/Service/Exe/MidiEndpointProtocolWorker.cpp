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
    GUID SessionId,
    GUID AbstractionGuid,
    LPCWSTR EndpointDeviceInterfaceId,
    std::shared_ptr<CMidiClientManager>& ClientManager,
    std::shared_ptr<CMidiDeviceManager>& DeviceManager,
    std::shared_ptr<CMidiSessionTracker>& SessionTracker
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(EndpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    m_abstractionGuid = AbstractionGuid;
    m_sessionId = SessionId;
    m_deviceInterfaceId = EndpointDeviceInterfaceId;

    m_clientManager = ClientManager;
    m_deviceManager = DeviceManager;
    m_sessionTracker = SessionTracker;

    RETURN_IF_FAILED(m_endProcessing.create(wil::EventOptions::ManualReset));

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Initialize complete", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(EndpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
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
        TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
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
                TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
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
        TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
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
    ENDPOINTPROTOCOLNEGOTIATIONPARAMS NegotiationParams,
    IMidiProtocolNegotiationCompleteCallback* NegotiationCompleteCallback
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );


    m_negotiationCompleteCallback = NegotiationCompleteCallback;

    m_preferToSendJRTimestampsToEndpoint = NegotiationParams.PreferToSendJRTimestampsToEndpoint;
    m_preferToReceiveJRTimestampsFromEndpoint = NegotiationParams.PreferToReceiveJRTimestampsFromEndpoint;
    m_preferredMidiProtocol = NegotiationParams.PreferredMidiProtocol;

    // We continue listening for and updating metadata even after we return.
    // We don't raise any changed events here. Instead, anyone interested in getting
    // changes after the initial negotiation can use a device watcher on that device
    // and react to our stored property changes. This also helps prevent any races.

    try
    {

        // we do this here instead of initialize so this is created on the worker thread
        if (!m_midiBiDiDevice)
        {
            wil::com_ptr_nothrow<IMidiAbstraction> serviceAbstraction;

            // we only support UMP data format for protocol negotiation
            ABSTRACTIONCREATIONPARAMS abstractionCreationParams{ };
            abstractionCreationParams.DataFormat = MidiDataFormat::MidiDataFormat_UMP;
            abstractionCreationParams.InstanceConfigurationJsonData = nullptr;

            DWORD mmcssTaskId{ 0 };
            LONGLONG context{ 0 };


            // this is not a good idea, but we don't have a reference to the lib here
            GUID midi2MidiSrvAbstractionIID = internal::StringToGuid(L"{2BA15E4E-5417-4A66-85B8-2B2260EFBC84}");
            RETURN_IF_FAILED(CoCreateInstance((IID)midi2MidiSrvAbstractionIID, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&serviceAbstraction)));

            // create the bidi device
            RETURN_IF_FAILED(serviceAbstraction->Activate(__uuidof(IMidiBiDi), (void**)&m_midiBiDiDevice));

            RETURN_IF_FAILED(m_midiBiDiDevice->Initialize(
                (LPCWSTR)(m_deviceInterfaceId.c_str()),
                &abstractionCreationParams,
                &mmcssTaskId,
                (IMidiCallback*)(this),
                context,
                m_sessionId
            ));
        }

        // add this connection to the session tracker. The manager already logged the overall session
        //LOG_IF_FAILED(m_sessionTracker->AddClientEndpointConnection(
        //    m_sessionId,
        //    m_deviceInterfaceId.c_str(),
        //    (MidiClientHandle)nullptr));

        if (!m_allNegotiationMessagesReceived)
        {
            RETURN_IF_FAILED(m_allNegotiationMessagesReceived.create(wil::EventOptions::ManualReset));
        }


        // TODO: For now, we're keeping the initial negotiation all in-line, in the same thread.
        // will evaluate a separate worker thread after the implementation is tested and working.

        // The lifetime of this object (CMidiEndpointProtocolWorker instance for the SWD) is controlled by the CMidiEndpointProtocolManager.

        // reset all the data collection fields in case negotiation was
        // called more than once (an allowed scenario)
        m_countFunctionBlocksReceived = 0;
        m_countFunctionBlockNamesReceived = 0;
        m_declaredFunctionBlockCount = 0;

        m_discoveredFunctionBlocks.clear();
        m_functionBlockNames.clear();
        m_endpointName.clear();
        m_productInstanceId.clear();

        m_allNegotiationMessagesReceived.ResetEvent();

        m_alreadyTriedToNegotiationOnce = false;

        m_taskDeviceIdentityReceived = false;
        m_taskEndpointInfoReceived = false;
        m_taskEndpointNameReceived = false;
        m_taskEndpointProductInstanceIdReceived = false;
        m_taskFinalStreamNegotiationResponseReceived = false;

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Requesting discovery information", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        // start initial negotiation. Return when timed out or when we have all the requested info.
        LOG_IF_FAILED(RequestAllEndpointDiscoveryInformation());

        m_allNegotiationMessagesReceived.wait(NegotiationParams.TimeoutMilliseconds);

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Returned from manual reset event wait", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        if (m_allNegotiationMessagesReceived.is_signaled())
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"All discovery/negotiation messages received", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
            );

            // provide all the negotiation results

            m_mostRecentResults.AllEndpointInformationReceived = true;
        }
        else
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Discovery/negotiation messages partially received", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
            );

            // we only received partial results

            m_mostRecentResults.AllEndpointInformationReceived = false;
        }

        // copy the data over into the results structure. We do this because the endpoint
        // deals only with the manager. It doesn't work with this worker at all. Important
        // because in the future, this worker may be on another thread etc. It's an internal
        // implementation detail, not a public contract.


        if (!m_endpointName.empty())
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Endpoint name received", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                TraceLoggingWideString(m_endpointName.c_str(), "endpoint name")
                );

            m_mostRecentResults.EndpointSuppliedName = m_endpointName.c_str();
        }
        else
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"No endpoint name received", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
            );

            m_mostRecentResults.EndpointSuppliedName = nullptr;
        }


        if (!m_productInstanceId.empty())
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Product instance Id received", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                TraceLoggingWideString(m_productInstanceId.c_str(), "product instance id")
            );

            m_mostRecentResults.EndpointSuppliedProductInstanceId = m_productInstanceId.c_str();
        }
        else
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"No product instance id received", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
            );

            m_mostRecentResults.EndpointSuppliedProductInstanceId = nullptr;
        }

        m_mostRecentResults.FunctionBlocksAreStatic = m_functionBlocksAreStatic;
        m_mostRecentResults.CountFunctionBlocksDeclared = m_declaredFunctionBlockCount;

        // Loop through function blocks and copy the name pointers over 
        // into the structure before returning it. Seems extra, but we need a friendly
        // place to work on names before they are finished, and the structure only
        // knows about the LPCWSTR, not std::wstring
        
        if (m_discoveredFunctionBlocks.size() > 0)
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Function blocks received", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
            );

            for (auto& fb : m_discoveredFunctionBlocks)
            {
                if (m_functionBlockNames.find(fb.Number) != m_functionBlockNames.end())
                {
                    fb.Name = m_functionBlockNames[fb.Number].c_str();
                }
                else
                {
                    fb.Name = nullptr;
                }
            }

            // add the function blocks now they are fully valid
            m_mostRecentResults.DiscoveredFunctionBlocks = m_discoveredFunctionBlocks.data();
            m_mostRecentResults.CountFunctionBlocksReceived = (BYTE)m_discoveredFunctionBlocks.size();
        }
        else
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"No function blocks received", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
            );

            m_mostRecentResults.DiscoveredFunctionBlocks = nullptr;
            m_mostRecentResults.CountFunctionBlocksReceived = 0;
        }
       
        // Call callback

        if (m_negotiationCompleteCallback != nullptr)
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Initial protocol negotiation complete, calling callback function", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
            );

            LOG_IF_FAILED(m_negotiationCompleteCallback->ProtocolNegotiationCompleteCallback(
                m_abstractionGuid, 
                m_deviceInterfaceId.c_str(), 
                &m_mostRecentResults
                )
            );
        }
        else
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Initial protocol negotiation complete, but no callback provided", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
            );

        }


        // we just hang out until endProcessing is set
        // TODO: This won't allow calling negotiation a second time, so need to think about that

        m_endProcessing.wait();



        return S_OK;
    }
    CATCH_RETURN();
}

_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolWorker::Callback(
    PVOID Data,
    UINT Size,
    LONGLONG Position,
    LONGLONG Context)
{
    UNREFERENCED_PARAMETER(Position);
    UNREFERENCED_PARAMETER(Context);

    if (Data != nullptr && Size == UMP128_BYTE_COUNT)
    {
        internal::PackedUmp128 ump;

        if (internal::FillPackedUmp128FromBytePointer((byte*)Data, (uint8_t)Size, ump))
        {
            // if type F, process it.

            if (internal::GetUmpMessageTypeFromFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_UMP_MESSAGE_TYPE)
            {
                LOG_IF_FAILED(ProcessStreamMessage(ump));

                // check flags. If we've received everything, signal

                if (m_taskEndpointInfoReceived &&
                    m_taskEndpointNameReceived &&
                    m_taskEndpointProductInstanceIdReceived &&
                    m_taskDeviceIdentityReceived &&
                    m_countFunctionBlockNamesReceived == m_declaredFunctionBlockCount &&
                    m_countFunctionBlocksReceived == m_declaredFunctionBlockCount &&
                    m_taskFinalStreamNegotiationResponseReceived)
                {
                    if (m_allNegotiationMessagesReceived.is_valid() && !m_allNegotiationMessagesReceived.is_signaled())
                    {
                        // we're done with negotiation, and can return from the initial function. Code will continue to
                        // capture new metadata when messages signal change, but the initial steps have completed.
                        m_allNegotiationMessagesReceived.SetEvent();
                    }
                }
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
                TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
            );

            return E_FAIL;
        }
    }
    else
    {
        // Either null (hopefully not) or not a UMP128 so can't be a stream message. Fall out quickly
    }


    return S_OK;
}



_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolWorker::ProcessStreamMessage(internal::PackedUmp128 ump)
{
#ifdef _DEBUG 
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingUInt32(ump.word0, MIDI_TRACE_EVENT_MIDI_WORD0_FIELD)
    );
#endif

    auto messageStatus = internal::GetStatusFromStreamMessageFirstWord(ump.word0);

    switch (messageStatus)
    {
    case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_INFO_NOTIFICATION:
    {
#ifdef _DEBUG 
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Received Endpoint Info Notification", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );
#endif

        LOG_IF_FAILED(UpdateEndpointInfoProperties(ump));

        m_taskEndpointInfoReceived = true;

        m_declaredFunctionBlockCount = internal::GetEndpointInfoNotificationNumberOfFunctionBlocksFromSecondWord(ump.word1);
        m_functionBlocksAreStatic = internal::GetEndpointInfoNotificationStaticFunctionBlocksFlagFromSecondWord(ump.word1);

        // TODO: Update other internal values as needed

        LOG_IF_FAILED(RequestAllFunctionBlocks());
    }
    break;

    case MIDI_STREAM_MESSAGE_STATUS_DEVICE_IDENTITY_NOTIFICATION:
    {
#ifdef _DEBUG 
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Received Device Identity Notification", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );
#endif

        LOG_IF_FAILED(UpdateDeviceIdentityProperty(ump));

        m_taskDeviceIdentityReceived = true;
    }
    break;

    case MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_NOTIFICATION:
    {
#ifdef _DEBUG 
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Received Stream Configuration Notification", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );
#endif

        LOG_IF_FAILED(UpdateStreamConfigurationProperties(ump));
        LOG_IF_FAILED(ProcessStreamConfigurationRequest(ump));
    }
    break;

    case MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_INFO_NOTIFICATION:
    {
#ifdef _DEBUG 
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Received Function Block Info Notification", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );
#endif

        m_countFunctionBlocksReceived += 1;

        DISCOVEREDFUNCTIONBLOCK block{};

        auto direction = MIDIWORDBYTE4LOWCRUMB1(ump.word0);

        if (direction == MIDI_FUNCTION_BLOCK_DIRECTION_BLOCK_INPUT || direction == MIDI_FUNCTION_BLOCK_DIRECTION_BLOCK_BIDIRECTIONAL)
        {
            // input to block == is a MIDI Output from PC
            block.IsMIDIMessageDestination = true;
        }

        if (direction == MIDI_FUNCTION_BLOCK_DIRECTION_BLOCK_OUTPUT || direction == MIDI_FUNCTION_BLOCK_DIRECTION_BLOCK_BIDIRECTIONAL)
        {
            // output from block == is a MIDI input to PC
            block.IsMIDIMessageSource = true;
        }

        block.IsActive = (BOOL)(MIDIWORDBYTE3HIGHBIT(ump.word0));
        block.Number = internal::CleanupByte7(MIDIWORDBYTE3(ump.word0));
        block.FirstGroup = MIDIWORDBYTE1(ump.word1);
        block.NumberOfGroupsSpanned = MIDIWORDBYTE2(ump.word1);

        m_discoveredFunctionBlocks.push_back(block);

        LOG_IF_FAILED(UpdateFunctionBlockProperty(ump));
    }
    break;

    case MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION:
    {
#ifdef _DEBUG 
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Received Function Block Name Notification", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );
#endif

        if (internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE ||
            internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_END)
        {
            m_countFunctionBlockNamesReceived += 1;
        }

        LOG_IF_FAILED(HandleFunctionBlockNameMessage(ump));
    }
    break;

    case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_PRODUCT_INSTANCE_ID_NOTIFICATION:
    {
#ifdef _DEBUG 
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Received Product Instance Id Notification", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );
#endif

        if (internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE ||
            internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_END)
        {
            m_taskEndpointProductInstanceIdReceived = true;
        }

        LOG_IF_FAILED(HandleProductInstanceIdMessage(ump));
    }
    break;

    case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION:
    {
#ifdef _DEBUG 
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Received Endpoint Name Notification", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );
#endif

        if (internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE ||
            internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_END)
        {
            m_taskEndpointNameReceived = true;
        }
        LOG_IF_FAILED(HandleEndpointNameMessage(ump));
    }
    break;

    default:
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Message is unidentified stream message", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );
        // some other type F we don't understand. Just ignore
    }
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
        TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    internal::PackedUmp128 ump{};

    if (m_midiBiDiDevice)
    {
        // first word
        ump.word0 = internal::BuildFunctionBlockDiscoveryRequestFirstWord(
            MIDI_STREAM_MESSAGE_FUNCTION_BLOCK_REQUEST_ALL_FUNCTION_BLOCKS,
            MIDI_STREAM_MESSAGE_FUNCTION_BLOCK_REQUEST_MESSAGE_ALL_FILTER_FLAGS);

        // send it immediately
        return m_midiBiDiDevice->SendMidiMessage((byte*)&ump, (UINT)sizeof(ump), 0);
    }

    return E_FAIL;
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
        TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    internal::PackedUmp128 ump{};

    if (m_midiBiDiDevice)
    {
        // first word
        ump.word0 = internal::BuildEndpointDiscoveryRequestFirstWord(MIDI_PREFERRED_UMP_VERSION_MAJOR, MIDI_PREFERRED_UMP_VERSION_MINOR);

        // second word
        uint8_t filterBitmap = MIDI_ENDPOINT_DISCOVERY_MESSAGE_ALL_FILTER_FLAGS;
        internal::SetMidiWordMostSignificantByte4(ump.word1, filterBitmap);

        // send it immediately
        return m_midiBiDiDevice->SendMidiMessage((byte*)&ump, (UINT)sizeof(ump), 0);
    }
    else
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Endpoint is null", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );
    }

    return E_FAIL;
}

_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolWorker::ProcessStreamConfigurationRequest(internal::PackedUmp128 ump)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    // see if all is what we want. If not, we'll send a request to change configuration.

    if (m_midiBiDiDevice)
    {
        auto protocol = internal::GetStreamConfigurationNotificationProtocolFromFirstWord(ump.word0);
        auto endpointRxJR = internal::GetStreamConfigurationNotificationReceiveJRFromFirstWord(ump.word0);
        auto endpointTxJR = internal::GetStreamConfigurationNotificationTransmitJRFromFirstWord(ump.word0);

        if (protocol !=m_preferredMidiProtocol ||
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
                return m_midiBiDiDevice->SendMidiMessage((byte*)&configurationRequestUmp, (UINT)sizeof(configurationRequestUmp), 0);
            }
            else
            {
                // we've already tried negotiating once. Don't do it again
                m_taskFinalStreamNegotiationResponseReceived = true;
                return S_OK;
            }
        }
        else
        {
            // all good on this try
            m_taskFinalStreamNegotiationResponseReceived = true;
            return S_OK;
        }

    }

    return E_FAIL;
}


HRESULT
CMidiEndpointProtocolWorker::Cleanup()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );


    LOG_IF_FAILED(m_sessionTracker->RemoveClientEndpointConnection(m_sessionId, m_deviceInterfaceId.c_str(), (MidiClientHandle)nullptr));

    // stop worker thread

    m_shutdown = true;
    //    m_queueWorkerThreadWakeup.SetEvent();

    //    if (m_queueWorkerThread.joinable())
    //        m_queueWorkerThread.join();


    m_allNegotiationMessagesReceived.reset();


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
        TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
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

        RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_deviceInterfaceId.c_str(), ARRAYSIZE(props), (PVOID)props));

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
        TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
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

        RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_deviceInterfaceId.c_str(), ARRAYSIZE(props), (PVOID)props));

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

        RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_deviceInterfaceId.c_str(), ARRAYSIZE(props), (PVOID)props));

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
        TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
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

        RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_deviceInterfaceId.c_str(), ARRAYSIZE(props), (PVOID)props));

        // clear out any old name that's in there
      //  m_functionBlockNames.erase(functionBlockNumber);
        return S_OK;
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

        RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_deviceInterfaceId.c_str(), ARRAYSIZE(props), (PVOID)props));

        return S_OK;
    }
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
        TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
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

    RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_deviceInterfaceId.c_str(), ARRAYSIZE(props), (PVOID)props));

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
        TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
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
            DEVPROP_TYPE_BINARY, static_cast<ULONG>(sizeof(prop)), &prop },

        {{ PKEY_MIDI_DeviceIdentityLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_FILETIME, static_cast<ULONG>(sizeof(FILETIME)), (PVOID)(&currentTime) },

    };

    RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_deviceInterfaceId.c_str(), ARRAYSIZE(props), (PVOID)props));

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
        TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    BYTE umpVersionMajor = MIDIWORDBYTE3(endpointInfoNotificationMessage.word0);
    BYTE umpVersionMinor = MIDIWORDBYTE4(endpointInfoNotificationMessage.word0);
    BYTE functionBlockCount = internal::CleanupByte7(MIDIWORDBYTE1(endpointInfoNotificationMessage.word1));

    DEVPROP_BOOLEAN functionBlocksAreStatic = MIDIWORDHIGHBIT(endpointInfoNotificationMessage.word1) ? DEVPROP_TRUE : DEVPROP_FALSE;

    DEVPROP_BOOLEAN supportsMidi1Protocol = MIDIWORDBYTE3LOWBIT1(endpointInfoNotificationMessage.word1) ? DEVPROP_TRUE : DEVPROP_FALSE;
    DEVPROP_BOOLEAN supportsMidi2Protocol = MIDIWORDBYTE3LOWBIT2(endpointInfoNotificationMessage.word1) ? DEVPROP_TRUE : DEVPROP_FALSE;

    DEVPROP_BOOLEAN supportsSendingJR = MIDIWORDBYTE4LOWBIT1(endpointInfoNotificationMessage.word1) ? DEVPROP_TRUE : DEVPROP_FALSE;
    DEVPROP_BOOLEAN supportsReceivingJR = MIDIWORDBYTE4LOWBIT2(endpointInfoNotificationMessage.word1) ? DEVPROP_TRUE : DEVPROP_FALSE;

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
            DEVPROP_TYPE_BYTE, static_cast<ULONG>(sizeof(umpVersionMajor)), &umpVersionMinor },

        {{ PKEY_MIDI_FunctionBlockCount, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BYTE, static_cast<ULONG>(sizeof(functionBlockCount)), &functionBlockCount },

        {{ PKEY_MIDI_FunctionBlocksAreStatic, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(functionBlocksAreStatic)), &functionBlocksAreStatic },

        {{ PKEY_MIDI_EndpointInformationLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_FILETIME, static_cast<ULONG>(sizeof(FILETIME)), (PVOID)(&currentTime) },

    };

    RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_deviceInterfaceId.c_str(), ARRAYSIZE(props), (PVOID)props));

    return S_OK;
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
        TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    MidiFunctionBlockProperty prop;

    prop.IsActive = MIDIWORDBYTE3HIGHBIT(functionBlockInfoNotificationMessage.word0);
    prop.BlockNumber = internal::CleanupByte7(MIDIWORDBYTE3(functionBlockInfoNotificationMessage.word0));
    prop.Direction = MIDIWORDBYTE4LOWCRUMB1(functionBlockInfoNotificationMessage.word0);
    prop.Midi1 = MIDIWORDBYTE4LOWCRUMB2(functionBlockInfoNotificationMessage.word0);
    prop.UIHint = MIDIWORDBYTE4LOWCRUMB3(functionBlockInfoNotificationMessage.word0);
    prop.Reserved0 = MIDIWORDBYTE4LOWCRUMB4(functionBlockInfoNotificationMessage.word0);

    prop.FirstGroup = MIDIWORDBYTE1(functionBlockInfoNotificationMessage.word1);
    prop.NumberOfGroupsSpanned = MIDIWORDBYTE2(functionBlockInfoNotificationMessage.word1);
    prop.MidiCIMessageVersionFormat = MIDIWORDBYTE3(functionBlockInfoNotificationMessage.word1);
    prop.MaxSysEx8Streams = MIDIWORDBYTE4(functionBlockInfoNotificationMessage.word1);

    prop.Reserved1 = functionBlockInfoNotificationMessage.word2;
    prop.Reserved2 = functionBlockInfoNotificationMessage.word3;

    FILETIME currentTime;
    GetSystemTimePreciseAsFileTime(&currentTime);

    DEVPROPKEY propKey = FunctionBlockPropertyKeyFromNumber(prop.BlockNumber);

    DEVPROPERTY props[] =
    {
        {{ propKey, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BINARY, static_cast<ULONG>(sizeof(prop)), &prop },

        {{ PKEY_MIDI_FunctionBlocksLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_FILETIME, static_cast<ULONG>(sizeof(FILETIME)), (PVOID)(&currentTime) },

    };

    RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_deviceInterfaceId.c_str(), ARRAYSIZE(props), (PVOID)props));

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
        TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
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


_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolWorker::HandleFunctionBlockNameMessage(internal::PackedUmp128& functionBlockNameMessage)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );


    uint8_t functionBlockNumber = MIDIWORDBYTE3(functionBlockNameMessage.word0);

    switch (internal::GetFormFromStreamMessageFirstWord(functionBlockNameMessage.word0))
    {
    case MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE: // complete name in single message. Just update property
        RETURN_IF_FAILED(UpdateFunctionBlockNameProperty(functionBlockNumber, ParseStreamTextMessage(functionBlockNameMessage)));
        break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_START: // start of multi-part name message. Overwrite any other name in the map
    {
        std::wstring name = ParseStreamTextMessage(functionBlockNameMessage);

        m_functionBlockNames.insert_or_assign(functionBlockNumber, name);
    }
    break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_CONTINUE: //continuation of multi-part name message. Append to name in map
        if (m_functionBlockNames.find(functionBlockNumber) != m_functionBlockNames.end())
        {
            std::wstring name = m_functionBlockNames.find(functionBlockNumber)->second;
            name += ParseStreamTextMessage(functionBlockNameMessage);

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
            std::wstring name = m_functionBlockNames.find(functionBlockNumber)->second;
            name += ParseStreamTextMessage(functionBlockNameMessage);

            RETURN_IF_FAILED(UpdateFunctionBlockNameProperty(functionBlockNumber, name));
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
CMidiEndpointProtocolWorker::HandleEndpointNameMessage(internal::PackedUmp128& endpointNameMessage)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    switch (internal::GetFormFromStreamMessageFirstWord(endpointNameMessage.word0))
    {
    case MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE: // complete name in single message. Just update property
        m_endpointName = ParseStreamTextMessage(endpointNameMessage);
        RETURN_IF_FAILED(UpdateEndpointNameProperty());
        break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_START: // start of multi-part name message. Overwrite any other name in the map
    {
        m_endpointName = ParseStreamTextMessage(endpointNameMessage);
    }
    break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_CONTINUE: //continuation of multi-part name message. Append to name in map
        if (!m_endpointName.empty())
        {
            m_endpointName += ParseStreamTextMessage(endpointNameMessage);
        }
        else
        {
            // name isn't already started, so a start message was skipped. Don't append anything at all.
        }
        break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_END: // end of multi-part name message. Finish name and update property
        if (!m_endpointName.empty())
        {
            m_endpointName += ParseStreamTextMessage(endpointNameMessage);
            RETURN_IF_FAILED(UpdateEndpointNameProperty());
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
CMidiEndpointProtocolWorker::HandleProductInstanceIdMessage(internal::PackedUmp128& productInstanceIdMessage)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_deviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    switch (internal::GetFormFromStreamMessageFirstWord(productInstanceIdMessage.word0))
    {
    case MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE: // complete name in single message. Just update property
        m_productInstanceId = ParseStreamTextMessage(productInstanceIdMessage);
        RETURN_IF_FAILED(UpdateEndpointProductInstanceIdProperty());
        break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_START: // start of multi-part name message. Overwrite any other name in the map
    {
        m_productInstanceId = ParseStreamTextMessage(productInstanceIdMessage);
    }
    break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_CONTINUE: //continuation of multi-part name message. Append to name in map
        if (!m_productInstanceId.empty())
        {
            m_productInstanceId += ParseStreamTextMessage(productInstanceIdMessage);
        }
        else
        {
            // name isn't already started, so a start message was skipped. Don't append anything at all.
        }
        break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_END: // end of multi-part name message. Finish name and update property
        if (!m_productInstanceId.empty())
        {
            m_productInstanceId += ParseStreamTextMessage(productInstanceIdMessage);
            RETURN_IF_FAILED(UpdateEndpointProductInstanceIdProperty());
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


