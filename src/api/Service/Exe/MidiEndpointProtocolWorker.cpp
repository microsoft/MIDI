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
    LPCWSTR DeviceInterfaceId,
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
        TraceLoggingPointer(this, "this")
    );

    m_abstractionGuid = AbstractionGuid;
    m_sessionId = SessionId;
    m_deviceInterfaceId = DeviceInterfaceId;

    m_clientManager = ClientManager;
    m_deviceManager = DeviceManager;
    m_sessionTracker = SessionTracker;

    wil::com_ptr_nothrow<IMidiAbstraction> midiAbstraction;

    // we only support UMP data format for protocol negotiation
    ABSTRACTIONCREATIONPARAMS abstractionCreationParams;
    abstractionCreationParams.DataFormat = MidiDataFormat::MidiDataFormat_UMP;

    DWORD mmcssTaskId{ 0 };
    LONGLONG context{ 0 };


    // This approach ONLY works because all UMP endpoints are required to be multi-client
    RETURN_IF_FAILED(CoCreateInstance(m_abstractionGuid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiAbstraction)));
    RETURN_IF_FAILED(midiAbstraction->Activate(__uuidof(IMidiBiDi), (void**)&m_midiBiDiDevice));
    RETURN_IF_FAILED(m_midiBiDiDevice->Initialize(m_deviceInterfaceId.c_str(), &abstractionCreationParams, &mmcssTaskId, this, context, m_sessionId));

    // todo: add this connection to the session tracker




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
        TraceLoggingPointer(this, "this")
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
                TraceLoggingWideString(L"CLSIDFromString failed for Function Block Property Key", MIDI_TRACE_EVENT_MESSAGE_FIELD)
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
        TraceLoggingPointer(this, "this")
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
CMidiEndpointProtocolWorker::NegotiateAndRequestMetadata(
    BOOL PreferToSendJRTimestampsToEndpoint,
    BOOL PreferToReceiveJRTimestampsFromEndpoint,
    BYTE PreferredMidiProtocol,
    WORD TimeoutMilliseconds,
    PENDPOINTPROTOCOLNEGOTIATIONRESULTS* NegotiationResults
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    m_preferToSendJRTimestampsToEndpoint = PreferToSendJRTimestampsToEndpoint;
    m_preferToReceiveJRTimestampsFromEndpoint = PreferToReceiveJRTimestampsFromEndpoint;
    m_preferredMidiProtocol = PreferredMidiProtocol;

    // We continue listening for and updating metadata even after we return.
    // We don't raise any changed events here. Instead, anyone interested in getting
    // changes after the initial negotiation can use a device watcher on that device
    // and react to our stored property changes. This also helps prevent any races.

    try
    {
        // TODO: For now, we're keeping the initial negotiation all in-line, in the same thread.
        // will evaluate a background thread after the implementation is tested and working.

        // The lifetime of this object (CMidiEndpointProtocolWorker instance for the SWD) is controlled by the CMidiEndpointProtocolManager.

        auto results = std::make_shared<ENDPOINTPROTOCOLNEGOTIATIONRESULTS>();

        // start initial negotiation. Return when timed out or when we have all the info.
        LOG_IF_FAILED(RequestAllEndpointDiscoveryInformation());

        m_allNegotiationMessagesReceived.wait(TimeoutMilliseconds);
        
        if (m_allNegotiationMessagesReceived.is_signaled())
        {
            // provide the negotiation results

            results->AllEndpointInformationReceived = true;

        }
        else
        {
            // partial results

            results->AllEndpointInformationReceived = false;
        }

        // TODO: Fill the rest of the results fields

        results->EndpointSuppliedName = m_endpointName.c_str();
        results->EndpointSuppliedProductInstanceId = m_productInstanceId.c_str();

        //results->FunctionBlocksAreStatic =
        results->NumberOfFunctionBlocksReceived = m_countFunctionBlocksReceived;
        results->NumberOfFunctionBlocksDeclared = m_declaredFunctionBlockCount;





        *NegotiationResults = results.get();

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
                auto messageStatus = internal::GetStatusFromStreamMessageFirstWord(ump.word0);

                switch (messageStatus)
                {
                case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_INFO_NOTIFICATION:
                    TraceLoggingWrite(
                        MidiSrvTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_INFO,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Received Endpoint Info Notification", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                    );

                    m_taskEndpointInfoReceived = true;
                    m_declaredFunctionBlockCount = internal::GetEndpointInfoNotificationNumberOfFunctionBlocksFromSecondWord(ump.word1);

                    RequestAllFunctionBlocks();
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_DEVICE_IDENTITY_NOTIFICATION:
                    TraceLoggingWrite(
                        MidiSrvTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_INFO,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Received Device Identity Notification", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                    );

                    m_taskDeviceIdentityReceived = true;
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_NOTIFICATION:
                    TraceLoggingWrite(
                        MidiSrvTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_INFO,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Received Stream Configuration Notification", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                    );

                    ProcessStreamConfigurationRequest(ump);
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_INFO_NOTIFICATION:
                    TraceLoggingWrite(
                        MidiSrvTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_INFO,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Received Function Block Info Notification", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                    );

                    m_countFunctionBlocksReceived += 1;
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION:
                    TraceLoggingWrite(
                        MidiSrvTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_INFO,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Received Function Block Name Notification", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                    );

                    if (internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE ||
                        internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_END)
                    {
                        m_countFunctionBlockNamesReceived += 1;
                    }
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_PRODUCT_INSTANCE_ID_NOTIFICATION:
                    TraceLoggingWrite(
                        MidiSrvTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_INFO,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Received Product Instance Id Notification", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                    );

                    if (internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE ||
                        internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_END)
                    {
                        m_taskEndpointProductInstanceIdReceived = true;
                    }
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION:
                    TraceLoggingWrite(
                        MidiSrvTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_INFO,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Received Endpoint Name Notification", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                    );

                    if (internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE ||
                        internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_END)
                    {
                        m_taskEndpointNameReceived = true;
                    }
                    break;

                default:
                    break;
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
                TraceLoggingWideString(L"Couldn't fill the UMP", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

            return E_FAIL;
        }
    }
    else
    {
        // Either null (hopefully not) or not a UMP128 so can't be a stream message. Fall out quickly
    }


    // check flags. If we've received everything, signal

    if (m_taskEndpointInfoReceived &&
        m_taskEndpointNameReceived &&
        m_taskEndpointProductInstanceIdReceived &&
        m_taskDeviceIdentityReceived &&
        m_countFunctionBlockNamesReceived == m_declaredFunctionBlockCount &&
        m_countFunctionBlocksReceived == m_declaredFunctionBlockCount &&
        m_taskFinalStreamNegotiationResponseReceived)
    {
        //        m_allMessagesReceived.SetEvent();
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
        TraceLoggingPointer(this, "this")
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
        TraceLoggingPointer(this, "this")
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
            TraceLoggingWideString(L"Endpoint is null", MIDI_TRACE_EVENT_MESSAGE_FIELD)
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
        TraceLoggingPointer(this, "this")
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
        TraceLoggingPointer(this, "this")
    );


    // stop worker thread

    m_shutdown = true;
    //    m_queueWorkerThreadWakeup.SetEvent();

    //    if (m_queueWorkerThread.joinable())
    //        m_queueWorkerThread.join();


    return S_OK;
}




HRESULT
CMidiEndpointProtocolWorker::UpdateEndpointNameProperty()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    auto cleanedValue{ internal::TrimmedWStringCopy(m_endpointName) + L"\0" };

    FILETIME currentTime;
    GetSystemTimePreciseAsFileTime(&currentTime);


    DEVPROPERTY props[] =
    {
        {{ PKEY_MIDI_EndpointProvidedName, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((cleanedValue.length() + 1) * sizeof(WCHAR)), (PVOID)(cleanedValue.c_str()) },
        {{ PKEY_MIDI_EndpointProvidedNameLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_FILETIME, static_cast<ULONG>(sizeof(FILETIME)), (PVOID)(&currentTime) },
    };

    RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_deviceInstanceId.c_str(), ARRAYSIZE(props), (PVOID)props));

    // clear out any old value that's in there
    //m_endpointName.clear();

    return S_OK;
}

HRESULT
CMidiEndpointProtocolWorker::UpdateEndpointProductInstanceIdProperty()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    FILETIME currentTime;
    GetSystemTimePreciseAsFileTime(&currentTime);


    std::wstring cleanedValue{ internal::TrimmedWStringCopy(m_productInstanceId) + L"\0" };

    DEVPROPERTY props[] =
    {
        {{ PKEY_MIDI_EndpointProvidedProductInstanceId, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((cleanedValue.length() + 1) * sizeof(WCHAR)), (PVOID)(cleanedValue.c_str()) },
        {{ PKEY_MIDI_EndpointProvidedProductInstanceIdLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_FILETIME, static_cast<ULONG>(sizeof(FILETIME)), (PVOID)(&currentTime) },
    };

    RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_deviceInstanceId.c_str(), ARRAYSIZE(props), (PVOID)props));

    // clear out any old value that's in there
    //m_productInstanceId.clear();

    return S_OK;
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
        TraceLoggingPointer(this, "this")
    );

    FILETIME currentTime;
    GetSystemTimePreciseAsFileTime(&currentTime);


    std::wstring cleanedValue{ internal::TrimmedWStringCopy(name) + L"\0" };

    DEVPROPERTY props[] =
    {
        {{ FunctionBlockNamePropertyKeyFromNumber(functionBlockNumber), DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((cleanedValue.length() + 1) * sizeof(WCHAR)), (PVOID)(cleanedValue.c_str()) },
        {{ PKEY_MIDI_FunctionBlocksLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_FILETIME, static_cast<ULONG>(sizeof(FILETIME)), (PVOID)(&currentTime) },
    };

    RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_deviceInstanceId.c_str(), ARRAYSIZE(props), (PVOID)props));

    // clear out any old name that's in there
  //  m_functionBlockNames.erase(functionBlockNumber);

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
        TraceLoggingPointer(this, "this")
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

    RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_deviceInstanceId.c_str(), ARRAYSIZE(props), (PVOID)props));

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
        TraceLoggingPointer(this, "this")
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

    RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_deviceInstanceId.c_str(), ARRAYSIZE(props), (PVOID)props));

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
        TraceLoggingPointer(this, "this")
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

    RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_deviceInstanceId.c_str(), ARRAYSIZE(props), (PVOID)props));

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
        TraceLoggingPointer(this, "this")
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


    RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_deviceInstanceId.c_str(), ARRAYSIZE(props), (PVOID)props));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolWorker::ProcessStreamMessage(internal::PackedUmp128 ump)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    auto messageStatus = internal::GetStatusFromStreamMessageFirstWord(ump.word0);

    switch (messageStatus)
    {
    case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_INFO_NOTIFICATION:
        UpdateEndpointInfoProperties(ump);
        //m_currentWorkItem.TaskEndpointInfoReceived = true;
        //m_currentWorkItem.DeclaredFunctionBlockCount = internal::GetEndpointInfoNotificationNumberOfFunctionBlocksFromSecondWord(ump.word1);

        //RequestAllFunctionBlocks();
        break;

    case MIDI_STREAM_MESSAGE_STATUS_DEVICE_IDENTITY_NOTIFICATION:
        UpdateDeviceIdentityProperty(ump);
        //m_currentWorkItem.TaskDeviceIdentityReceived = true;
        break;

    case MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_NOTIFICATION:
        UpdateStreamConfigurationProperties(ump);
        break;

    case MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_INFO_NOTIFICATION:
        UpdateFunctionBlockProperty(ump);
        break;

    case MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION:
        HandleFunctionBlockNameMessage(ump);
        break;

    case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_PRODUCT_INSTANCE_ID_NOTIFICATION:
        HandleProductInstanceIdMessage(ump);
        break;

    case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION:
        HandleEndpointNameMessage(ump);
        break;

    default:
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Message is unidentified stream message", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );
        break;
    }

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
        TraceLoggingPointer(this, "this")
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
        TraceLoggingPointer(this, "this")
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
        TraceLoggingPointer(this, "this")
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
        TraceLoggingPointer(this, "this")
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


