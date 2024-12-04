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
    ENDPOINTPROTOCOLNEGOTIATIONPARAMS negotiationParams,
    IMidiProtocolNegotiationCompleteCallback* negotiationCompleteCallback
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


    m_inInitialDiscoveryAndNegotiation = true;

    m_negotiationCompleteCallback = negotiationCompleteCallback;

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

        if (!m_allNegotiationMessagesReceived.is_valid())
        {
            RETURN_IF_FAILED(m_allNegotiationMessagesReceived.create(wil::EventOptions::ManualReset));
        }


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


        // Partner device-compatibility requirement. May be able to remove this in the future
        // but needed for now for their USB device to work properly. Given when this is called, 
        // and that it is on a worker thread, it doesn't really negatively impact any other 
        // devices or initial enumeration.
        // Search for "#midisrv#midiu_ks_" is to restrict this to just USB devices using the 
        // USB MIDI 2.0 driver, so network and other MIDI 2.0 transports are unaffected
        if (auto index = internal::NormalizeEndpointInterfaceIdWStringCopy(m_endpointDeviceInterfaceId).find(L"#midisrv#midiu_ks_");
            index != std::wstring::npos)
        {
            Sleep(2500);
        }
        // End partner compatibility mitigation added 2024-10-08


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

        // We need to wait on endProcessing as well as negotiation timeout, in case device is disconnected
        // while we are in the middle of discovery and protocol negotiation
        std::vector<HANDLE> waitHandles;
        waitHandles.push_back(m_allNegotiationMessagesReceived.get());
        waitHandles.push_back(m_endProcessing.get());

        WaitForMultipleObjects(static_cast<DWORD>(waitHandles.size()), waitHandles.data(), false, negotiationParams.TimeoutMilliseconds);
        //m_allNegotiationMessagesReceived.wait(negotiationParams.TimeoutMilliseconds);

        if (m_endProcessing.is_signaled())
        {
            // bail
            return S_OK;
        }
        else
        {
            if (m_allNegotiationMessagesReceived.is_signaled())
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"All discovery/negotiation messages received within timeout period", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
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
                    TraceLoggingWideString(L"Incomplete or no discovery/negotiation messages received (event timed out)", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
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
                    MIDI_TRACE_EVENT_VERBOSE,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Endpoint name received", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
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
                    TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                );

                m_mostRecentResults.EndpointSuppliedName = nullptr;
            }


            if (!m_productInstanceId.empty())
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_VERBOSE,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Product instance Id received", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
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
                    TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
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
                    MIDI_TRACE_EVENT_VERBOSE,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Function blocks received", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
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
                m_mostRecentResults.CountFunctionBlocksReceived = static_cast<BYTE>(m_discoveredFunctionBlocks.size());

                // warning in case we get some inconsistent results
                if (m_mostRecentResults.CountFunctionBlocksDeclared != m_mostRecentResults.CountFunctionBlocksReceived)
                {
                    // We did not receive the correct number of function blocks. This is a warning
                    // condition.

                    TraceLoggingWrite(
                        MidiSrvTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_WARNING,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Incorrect function block count received", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                        TraceLoggingUInt8(m_mostRecentResults.CountFunctionBlocksDeclared, "Declared FB Count"),
                        TraceLoggingUInt8(m_mostRecentResults.CountFunctionBlocksReceived, "Received FB Count")
                    );

                    m_mostRecentResults.AllEndpointInformationReceived = false;
                }
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
                    TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                );

                m_mostRecentResults.DiscoveredFunctionBlocks = nullptr;
                m_mostRecentResults.CountFunctionBlocksReceived = 0;
                m_mostRecentResults.AllEndpointInformationReceived = false;
            }

            // Call callback

            if (m_negotiationCompleteCallback != nullptr)
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_VERBOSE,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Initial protocol negotiation complete, calling callback function", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                );

                LOG_IF_FAILED(m_negotiationCompleteCallback->ProtocolNegotiationCompleteCallback(
                    m_transportId,
                    m_endpointDeviceInterfaceId.c_str(),
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
                    TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                );
            }

            // clear a couple flags used in capture functions so that additional processing is still allowed
            m_inInitialDiscoveryAndNegotiation = false;
            m_alreadyTriedToNegotiationOnce = false;

            // we just hang out until endProcessing is set. There's no timeout here.
            m_endProcessing.wait();

            return S_OK;
        }
    }
    CATCH_RETURN();
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



                // check flags. If we've received everything, signal

                if (m_inInitialDiscoveryAndNegotiation)
                {
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
                            // we're done with discovery and negotiation, and can return from the initial function. Code will continue to
                            // capture new metadata when messages signal change, but the initial steps have completed.
                            m_allNegotiationMessagesReceived.SetEvent();

                            m_inInitialDiscoveryAndNegotiation = false;
                        }
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

    m_taskEndpointInfoReceived = true;

    // check protocol
    auto supportsMidi2 = internal::GetEndpointInfoNotificationMidi2ProtocolCapabilityFromSecondWord(ump.word1);
    auto supportsMidi1 = internal::GetEndpointInfoNotificationMidi1ProtocolCapabilityFromSecondWord(ump.word1);

    auto endpointTransmitsJR = internal::GetEndpointInfoNotificationTransmitJRTimestampCapabilityFromSecondWord(ump.word1);
    auto endpointReceivesJR = internal::GetEndpointInfoNotificationReceiveJRTimestampCapabilityFromSecondWord(ump.word1);

    bool functionBlocksAreStatic = internal::GetEndpointInfoNotificationStaticFunctionBlocksFlagFromSecondWord(ump.word1);
    uint8_t functionBlocksDeclaredCount = internal::GetEndpointInfoNotificationNumberOfFunctionBlocksFromSecondWord(ump.word1);

    if (m_inInitialDiscoveryAndNegotiation)
    {
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

        m_functionBlocksAreStatic = functionBlocksAreStatic;
        m_declaredFunctionBlockCount = functionBlocksDeclaredCount;
    }

    if (!m_inInitialDiscoveryAndNegotiation && (m_declaredFunctionBlockCount != functionBlocksDeclaredCount))
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

    if (!m_inInitialDiscoveryAndNegotiation && (m_functionBlocksAreStatic != functionBlocksAreStatic))
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

    // we only request function blocks if in initial discovery
    if (m_inInitialDiscoveryAndNegotiation)
    {
        m_functionBlockNames.clear();
        m_discoveredFunctionBlocks.clear();
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

    if (m_inInitialDiscoveryAndNegotiation)
    {
        m_taskDeviceIdentityReceived = true;
    }

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

    if (!m_inInitialDiscoveryAndNegotiation && m_mostRecentResults.FunctionBlocksAreStatic)
    {
        // we reject this function block because the initial ones were declared static,
        // meaning no changes are allowed
        return S_OK;
    }

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

    // we don't use a map here because we need contiguous memory
    auto existingBlock = std::find_if(
        m_discoveredFunctionBlocks.begin(),
        m_discoveredFunctionBlocks.end(),
        [&block](const DISCOVEREDFUNCTIONBLOCK& existing) { return existing.Number == block.Number; });

    if (existingBlock == m_discoveredFunctionBlocks.end())
    {
        // we found an existing block with the id, so replace it
        // This really shouldn't happen except on a subsequent
        // function block change after initial discovery
        m_discoveredFunctionBlocks.emplace(existingBlock, block);

        RETURN_IF_FAILED(UpdateFunctionBlockProperty(ump));
    }
    else
    {
        // after initial discovery, the function block count can't change, no matter
        // if static or not. Numbers must be 0 to CountFunctionBlocksDeclared-1

        if (m_inInitialDiscoveryAndNegotiation || block.Number < m_mostRecentResults.CountFunctionBlocksDeclared)
        {
            m_countFunctionBlocksReceived += 1;
            m_discoveredFunctionBlocks.push_back(block);

            RETURN_IF_FAILED(UpdateFunctionBlockProperty(ump));
        }
        else
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

    if (!m_inInitialDiscoveryAndNegotiation && m_mostRecentResults.FunctionBlocksAreStatic)
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
        std::wstring completeName = internal::TrimmedWStringCopy(ParseStreamTextMessage(ump));

        m_functionBlockNames.insert_or_assign(functionBlockNumber, completeName);

        RETURN_IF_FAILED(UpdateFunctionBlockNameProperty(functionBlockNumber, completeName));
    }
    break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_START: // start of multi-part name message. Overwrite any other name in the map
    {
        std::wstring name = ParseStreamTextMessage(ump);

        m_functionBlockNames.insert_or_assign(functionBlockNumber, name);
    }
    break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_CONTINUE: //continuation of multi-part name message. Append to name in map
        if (m_functionBlockNames.find(functionBlockNumber) != m_functionBlockNames.end())
        {
            std::wstring name = m_functionBlockNames.find(functionBlockNumber)->second;
            name += ParseStreamTextMessage(ump);

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
            name = internal::TrimmedWStringCopy(name + ParseStreamTextMessage(ump));

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

    if (m_inInitialDiscoveryAndNegotiation)
    {
        if (internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE ||
            internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_END)
        {
            m_countFunctionBlockNamesReceived += 1;
        }
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

    if (m_inInitialDiscoveryAndNegotiation)
    {
        if (internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE ||
            internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_END)
        {
            m_taskEndpointProductInstanceIdReceived = true;
        }
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

    if (m_inInitialDiscoveryAndNegotiation)
    {
        if (internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE ||
            internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_END)
        {
            m_taskEndpointNameReceived = true;
        }
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
            // we've already tried negotiating once. Don't do it again
            m_taskFinalStreamNegotiationResponseReceived = true;

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
        // all good on this try
        m_taskFinalStreamNegotiationResponseReceived = true;

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


    m_allNegotiationMessagesReceived.reset();
    m_negotiationCompleteCallback = nullptr;

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

        {{ PKEY_MIDI_FunctionBlockCount, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BYTE, static_cast<ULONG>(sizeof(functionBlockCount)), &functionBlockCount },

        {{ PKEY_MIDI_FunctionBlocksAreStatic, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(functionBlocksAreStatic)), &functionBlocksAreStatic },

        {{ PKEY_MIDI_EndpointInformationLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_FILETIME, static_cast<ULONG>(sizeof(FILETIME)), (PVOID)(&currentTime) },

    };

    RETURN_IF_FAILED(m_deviceManager->UpdateEndpointProperties(m_endpointDeviceInterfaceId.c_str(), ARRAYSIZE(props), props));

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
        TraceLoggingWideString(m_endpointDeviceInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    MidiFunctionBlockProperty prop;

    prop.IsActive = internal::GetFunctionBlockActiveFlagFromInfoNotificationFirstWord(functionBlockInfoNotificationMessage.word0);
    prop.BlockNumber = internal::GetFunctionBlockNumberFromInfoNotificationFirstWord(functionBlockInfoNotificationMessage.word0);
    prop.Direction = internal::GetFunctionBlockDirectionFromInfoNotificationFirstWord(functionBlockInfoNotificationMessage.word0);
    prop.Midi1 = internal::GetFunctionBlockMidi10FromInfoNotificationFirstWord(functionBlockInfoNotificationMessage.word0);
    prop.UIHint = internal::GetFunctionBlockUIHintFromInfoNotificationFirstWord(functionBlockInfoNotificationMessage.word0);
    prop.Reserved0 = MIDIWORDBYTE4LOWCRUMB4(functionBlockInfoNotificationMessage.word0);

    prop.FirstGroup = internal::GetFunctionBlockFirstGroupFromInfoNotificationSecondWord(functionBlockInfoNotificationMessage.word1);
    prop.NumberOfGroupsSpanned = internal::GetFunctionBlockNumberOfGroupsFromInfoNotificationSecondWord(functionBlockInfoNotificationMessage.word1);
    prop.MidiCIMessageVersionFormat = internal::GetFunctionBlockMidiCIVersionFromInfoNotificationSecondWord(functionBlockInfoNotificationMessage.word1);
    prop.MaxSysEx8Streams = internal::GetFunctionBlockMaxSysex8StreamsFromInfoNotificationSecondWord(functionBlockInfoNotificationMessage.word1);

    prop.Reserved1 = functionBlockInfoNotificationMessage.word2;
    prop.Reserved2 = functionBlockInfoNotificationMessage.word3;

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




