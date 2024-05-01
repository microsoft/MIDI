// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "stdafx.h"
#include "ump_helpers.h"
#include "midi_ump_message_defs.h"

// Note: This class only works if these type F messages aren't swallowed up
// by some endpoint transform / processor. We'll need to have code in here
// later to require the processor be added, and if an option is introduced
// later, to ensure that it sets the option to forward messages along.

#define MIDI_PROTOCOL_MANAGER_SESSION_NAME L"MIDI 2.0 Protocol Manager"
#define MIDI_PROTOCOL_MANAGER_PROCESS_NAME L"midisrv.exe"


_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolManager::Initialize(
    std::shared_ptr<CMidiClientManager>& ClientManager,
    std::shared_ptr<CMidiDeviceManager>& DeviceManager,
    std::shared_ptr<CMidiSessionTracker>& SessionTracker
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    // use our clsid as the session id. 
    m_sessionId = __uuidof(IMidiEndpointProtocolManagerInterface);

    m_clientManager = ClientManager;
    m_deviceManager = DeviceManager;
    m_sessionTracker = SessionTracker;

    // log a an active session so a user can figure out which
    // processes have any given device open.
    auto pid = GetCurrentProcessId();

    LOG_IF_FAILED(m_sessionTracker->AddClientSession(
        m_sessionId,
        MIDI_PROTOCOL_MANAGER_SESSION_NAME,
        pid,
        MIDI_PROTOCOL_MANAGER_PROCESS_NAME));
 

    return S_OK;
}



_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolManager::NegotiateAndRequestMetadata(
    LPCWSTR DeviceInterfaceId,
    BOOL PreferToSendJRTimestampsToEndpoint,
    BOOL PreferToReceiveJRTimestampsFromEndpoint,
    BYTE PreferredMidiProtocol,
    WORD TimeoutMS,
    PENDPOINTPROTOCOLNEGOTIATIONRESULTS* NegotiationResults
) noexcept
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(DeviceInterfaceId)
    );

    // TEMP!
    NegotiationResults = nullptr;

    ProtocolManagerWork work;

    //// DEBUG
    ////TimeoutMS = 25000;

    work.EndpointInstanceId = DeviceInterfaceId;
    work.PreferToSendJRTimestampsToEndpoint = PreferToSendJRTimestampsToEndpoint;
    work.PreferToReceiveJRTimestampsFromEndpoint = PreferToReceiveJRTimestampsFromEndpoint;
    work.PreferredMidiProtocol = PreferredMidiProtocol;
    work.TimeoutMS = TimeoutMS;

    //m_queueMutex.lock();
    //m_workQueue.push(std::move(work));
    //m_queueMutex.unlock();

    //// todo: signal event that there's new work
    //m_queueWorkerThreadWakeup.SetEvent();


    return S_OK;
}


HRESULT
CMidiEndpointProtocolManager::Cleanup()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    m_sessionTracker->RemoveClientSession(m_sessionId);

    // TODO terminate any open threads and ensure they close up

    m_clientManager.reset();
    m_deviceManager.reset();
    m_sessionTracker.reset();

    // clear the queue
    while (m_workQueue.size() > 0) m_workQueue.pop();

    m_shutdown = true;
//    m_queueWorkerThreadWakeup.SetEvent();

//    if (m_queueWorkerThread.joinable())
//        m_queueWorkerThread.join();

    return S_OK;
}









_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolManager::Callback(PVOID Data, UINT Size, LONGLONG Position, LONGLONG Context)
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
                        __FUNCTION__,
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Received Endpoint Info Notification", "message")
                    );

                    m_currentWorkItem.TaskEndpointInfoReceived = true;
                    m_currentWorkItem.DeclaredFunctionBlockCount = internal::GetEndpointInfoNotificationNumberOfFunctionBlocksFromSecondWord(ump.word1);

                    RequestAllFunctionBlocks();
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_DEVICE_IDENTITY_NOTIFICATION:
                    TraceLoggingWrite(
                        MidiSrvTelemetryProvider::Provider(),
                        __FUNCTION__,
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Received Device Identity Notification", "message")
                    );

                    m_currentWorkItem.TaskDeviceIdentityReceived = true;
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_NOTIFICATION:
                    TraceLoggingWrite(
                        MidiSrvTelemetryProvider::Provider(),
                        __FUNCTION__,
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Received Stream Configuration Notification", "message")
                    );

                    ProcessStreamConfigurationRequest(ump);
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_INFO_NOTIFICATION:
                    TraceLoggingWrite(
                        MidiSrvTelemetryProvider::Provider(),
                        __FUNCTION__,
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Received Function Block Info Notification", "message")
                    );

                    m_currentWorkItem.CountFunctionBlocksReceived += 1;
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION:
                    TraceLoggingWrite(
                        MidiSrvTelemetryProvider::Provider(),
                        __FUNCTION__,
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Received Function Block Name Notification", "message")
                    );

                    if (internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE ||
                        internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_END)
                    {
                        m_currentWorkItem.CountFunctionBlockNamesReceived += 1;
                    }
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_PRODUCT_INSTANCE_ID_NOTIFICATION:
                    TraceLoggingWrite(
                        MidiSrvTelemetryProvider::Provider(),
                        __FUNCTION__,
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Received Product Instance Id Notification", "message")
                    );

                    if (internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE ||
                        internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_END)
                    {
                        m_currentWorkItem.TaskEndpointProductInstanceIdReceived = true;
                    }
                    break;

                case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION:
                    TraceLoggingWrite(
                        MidiSrvTelemetryProvider::Provider(),
                        __FUNCTION__,
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Received Endpoint Name Notification", "message")
                    );

                    if (internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE ||
                        internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_MESSAGE_MULTI_FORM_END)
                    {
                        m_currentWorkItem.TaskEndpointNameReceived = true;
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
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Couldn't fill the UMP")
            );

            return E_FAIL;
        }
    }
    else
    {
        // Either null (hopefully not) or not a UMP128 so can't be a stream message. Fall out quickly
    }


    // check flags. If we've received everything, signal

    if (m_currentWorkItem.TaskEndpointInfoReceived &&
        m_currentWorkItem.TaskEndpointNameReceived &&
        m_currentWorkItem.TaskEndpointProductInstanceIdReceived &&
        m_currentWorkItem.TaskDeviceIdentityReceived &&
        m_currentWorkItem.CountFunctionBlockNamesReceived == m_currentWorkItem.DeclaredFunctionBlockCount &&
        m_currentWorkItem.CountFunctionBlocksReceived == m_currentWorkItem.DeclaredFunctionBlockCount &&
        m_currentWorkItem.TaskFinalStreamNegotiationResponseReceived)
    {
//        m_allMessagesReceived.SetEvent();
    }

    return S_OK;
}

HRESULT
CMidiEndpointProtocolManager::RequestAllFunctionBlocks()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    internal::PackedUmp128 ump{};

    if (m_currentWorkItem.Endpoint)
    {
        // first word
        ump.word0 = internal::BuildFunctionBlockDiscoveryRequestFirstWord(
            MIDI_STREAM_MESSAGE_FUNCTION_BLOCK_REQUEST_ALL_FUNCTION_BLOCKS,
            MIDI_STREAM_MESSAGE_FUNCTION_BLOCK_REQUEST_MESSAGE_ALL_FILTER_FLAGS);

        // send it immediately
        return m_currentWorkItem.Endpoint->SendMidiMessage((byte*)&ump, (UINT)sizeof(ump), 0);
    }

    return E_FAIL;
}


HRESULT
CMidiEndpointProtocolManager::RequestAllEndpointDiscoveryInformation()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    internal::PackedUmp128 ump{};

    if (m_currentWorkItem.Endpoint)
    {
        // first word
        ump.word0 = internal::BuildEndpointDiscoveryRequestFirstWord(MIDI_PREFERRED_UMP_VERSION_MAJOR, MIDI_PREFERRED_UMP_VERSION_MINOR);

        // second word
        uint8_t filterBitmap = MIDI_ENDPOINT_DISCOVERY_MESSAGE_ALL_FILTER_FLAGS;
        internal::SetMidiWordMostSignificantByte4(ump.word1, filterBitmap);

        // send it immediately
        return m_currentWorkItem.Endpoint->SendMidiMessage((byte*)&ump, (UINT)sizeof(ump), 0);
    }
    else
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Endpoint is null")
        );
    }

    return E_FAIL;
}

_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolManager::ProcessStreamConfigurationRequest(internal::PackedUmp128 ump)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    // see if all is what we want. If not, we'll send a request to change configuration.

    if (m_currentWorkItem.Endpoint)
    {
        auto protocol = internal::GetStreamConfigurationNotificationProtocolFromFirstWord(ump.word0);
        auto endpointRxJR = internal::GetStreamConfigurationNotificationReceiveJRFromFirstWord(ump.word0);
        auto endpointTxJR = internal::GetStreamConfigurationNotificationTransmitJRFromFirstWord(ump.word0);

        if (protocol != m_currentWorkItem.PreferredMidiProtocol ||
            endpointRxJR != m_currentWorkItem.PreferToSendJRTimestampsToEndpoint ||
            endpointTxJR != m_currentWorkItem.PreferToReceiveJRTimestampsFromEndpoint)
        {
            if (!m_currentWorkItem.AlreadyTriedToNegotiationOnce)
            {
                internal::PackedUmp128 configurationRequestUmp{};

                ump.word0 = internal::BuildStreamConfigurationRequestFirstWord(
                    m_currentWorkItem.PreferredMidiProtocol,
                    m_currentWorkItem.PreferToSendJRTimestampsToEndpoint,
                    m_currentWorkItem.PreferToReceiveJRTimestampsFromEndpoint);

                m_currentWorkItem.AlreadyTriedToNegotiationOnce = true;

                // send it immediately
                return m_currentWorkItem.Endpoint->SendMidiMessage((byte*)&configurationRequestUmp, (UINT)sizeof(configurationRequestUmp), 0);
            }
            else
            {
                // we've already tried negotiating once. Don't do it again
                m_currentWorkItem.TaskFinalStreamNegotiationResponseReceived = true;
                return S_OK;
            }
        }
        else
        {
            // all good on this try
            m_currentWorkItem.TaskFinalStreamNegotiationResponseReceived = true;
            return S_OK;
        }

    }

    return E_FAIL;
}



HRESULT
CMidiEndpointProtocolManager::ProcessCurrentWorkEntry()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    // by using the existing abstractions and activation methods just 
    // like any other client, we will get the transforms, including 
    // the metadata listener, for free. So we don't need to duplicate
    // metadata capture code here, and we don't need to make any
    // explicit call to the metadata capture transform plugin

    RETURN_HR_IF_NULL(E_FAIL, m_serviceAbstraction);

    RETURN_IF_FAILED(m_serviceAbstraction->Activate(__uuidof(IMidiBiDi), (void**)&(m_currentWorkItem.Endpoint)));

    // Create and open a connection to the endpoint, complete with metadata listeners

    DWORD mmcssTaskId{};
    ABSTRACTIONCREATIONPARAMS abstractionCreationParams{ MidiDataFormat_UMP, nullptr };

    RETURN_IF_FAILED(m_currentWorkItem.Endpoint->Initialize(
        m_currentWorkItem.EndpointInstanceId.c_str(),
        &abstractionCreationParams,
        &mmcssTaskId,
        (IMidiCallback*)this,
        MIDI_PROTOCOL_MANAGER_ENDPOINT_CREATION_CONTEXT,
        m_sessionId
    ));

//    OutputDebugString(__FUNCTION__ L" - Resetting messages received event");

//    if (m_allMessagesReceived.is_signaled()) m_allMessagesReceived.ResetEvent();

    HRESULT hr = S_OK;

    //// Send initial discovery request
    //// the rest happens in response to messages in the callback
    //LOG_IF_FAILED(hr = RequestAllEndpointDiscoveryInformation());

    //if (SUCCEEDED(hr))
    //{
    //    OutputDebugString(__FUNCTION__ L" - Requested discovery information");
    //}
    //else
    //{
    //    TraceLoggingWrite(
    //        MidiSrvTelemetryProvider::Provider(),
    //        __FUNCTION__,
    //        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
    //        TraceLoggingPointer(this, "this"),
    //        TraceLoggingWideString(L"Failed to request discovery information")
    //    );
    //}

    //if (SUCCEEDED(hr))
    //{
    //    // Wait until all metadata arrives or we timeout
    //    if (!m_allMessagesReceived.wait(m_currentWorkItem.TimeoutMS))
    //    {
    //        // we didn't receive everything, but that's not a failure condition for this.
    //    }
    //}

    //if (m_allMessagesReceived.is_signaled()) m_allMessagesReceived.ResetEvent();

    //m_currentWorkItem.Endpoint->Cleanup();
    //m_currentWorkItem.Endpoint = nullptr;

    return hr;
}


void CMidiEndpointProtocolManager::ThreadWorker()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Thread worker enter")
    );

    while (!m_shutdown)
    {
        if (m_workQueue.size() > 0)
        {
            m_queueMutex.lock();
            m_currentWorkItem = std::move(m_workQueue.front());
            m_workQueue.pop();
            m_queueMutex.unlock();

            // this will block until completed
            LOG_IF_FAILED(ProcessCurrentWorkEntry());
        }

        // todo: how often do we want to process messages?
        Sleep(300);
    }

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Thread worker exit")
    );

}

