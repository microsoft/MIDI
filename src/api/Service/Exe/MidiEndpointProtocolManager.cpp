// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"
//#include "Midi2MidiSrvAbstraction.h"


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
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
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

    LOG_IF_FAILED(m_sessionTracker->AddClientSessionInternal(
        m_sessionId,
        MIDI_PROTOCOL_MANAGER_SESSION_NAME,
        pid,
        MIDI_PROTOCOL_MANAGER_PROCESS_NAME,
        nullptr));
 

    return S_OK;
}



_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolManager::NegotiateAndRequestMetadata(
    GUID AbstractionGuid,
    LPCWSTR DeviceInterfaceId,
    BOOL PreferToSendJRTimestampsToEndpoint,
    BOOL PreferToReceiveJRTimestampsFromEndpoint,
    BYTE PreferredMidiProtocol,
    WORD TimeoutMilliseconds,
    PENDPOINTPROTOCOLNEGOTIATIONRESULTS* NegotiationResults
) noexcept
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(DeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    auto cleanEndpointDeviceInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(DeviceInterfaceId);

    std::shared_ptr<CMidiEndpointProtocolWorker> worker{ nullptr };

    if (m_endpointWorkers.find(cleanEndpointDeviceInterfaceId) != m_endpointWorkers.end())
    {
        // already exists. We can re-request negotiation from the worker
        worker = m_endpointWorkers[cleanEndpointDeviceInterfaceId];
    }

    if (worker == nullptr)
    {
        // allocate a new worker
        worker = std::make_shared<CMidiEndpointProtocolWorker>();

        RETURN_IF_NULL_ALLOC(worker);
        RETURN_IF_FAILED(worker->Initialize(
            m_sessionId, 
            AbstractionGuid, 
            cleanEndpointDeviceInterfaceId.c_str(),
            m_clientManager, 
            m_deviceManager, 
            m_sessionTracker
            )
        );

        // add it to the map
        m_endpointWorkers[cleanEndpointDeviceInterfaceId] = worker;
    }

    // now that we have the worker created, actually start the work. This function blocks for up to TimeoutMilliseconds.

    RETURN_IF_FAILED(worker->NegotiateAndRequestMetadata(
        PreferToSendJRTimestampsToEndpoint,
        PreferToReceiveJRTimestampsFromEndpoint,
        PreferredMidiProtocol,
        TimeoutMilliseconds,
        NegotiationResults
        )
    );

    return S_OK;
}


HRESULT
CMidiEndpointProtocolManager::Cleanup()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    m_sessionTracker->RemoveClientSession(m_sessionId);

    for (auto& [key, val] : m_endpointWorkers)
    {
        LOG_IF_FAILED(val->Cleanup());
        val.reset();
    }

    m_endpointWorkers.clear();

    m_clientManager.reset();
    m_deviceManager.reset();
    m_sessionTracker.reset();

    return S_OK;
}


//HRESULT
//CMidiEndpointProtocolManager::ProcessCurrentWorkEntry()
//{
//    TraceLoggingWrite(
//        MidiSrvTelemetryProvider::Provider(),
//        MIDI_TRACE_EVENT_INFO,
//        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
//        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
//        TraceLoggingPointer(this, "this")
//    );
//
//
//    // by using the existing abstractions and activation methods just 
//    // like any other client, we will get the transforms, including 
//    // the metadata listener, for free. So we don't need to duplicate
//    // metadata capture code here, and we don't need to make any
//    // explicit call to the metadata capture transform plugin
//
//    RETURN_HR_IF_NULL(E_FAIL, m_serviceAbstraction);
//
//    RETURN_IF_FAILED(m_serviceAbstraction->Activate(__uuidof(IMidiBiDi), (void**)&(m_currentWorkItem.Endpoint)));
//
//    // Create and open a connection to the endpoint, complete with metadata listeners
//
//    DWORD mmcssTaskId{};
//    ABSTRACTIONCREATIONPARAMS abstractionCreationParams{ MidiDataFormat_UMP, nullptr };
//
//    RETURN_IF_FAILED(m_currentWorkItem.Endpoint->Initialize(
//        m_currentWorkItem.EndpointInstanceId.c_str(),
//        &abstractionCreationParams,
//        &mmcssTaskId,
//        (IMidiCallback*)this,
//        MIDI_PROTOCOL_MANAGER_ENDPOINT_CREATION_CONTEXT,
//        m_sessionId
//    ));
//
//
////    if (m_allMessagesReceived.is_signaled()) m_allMessagesReceived.ResetEvent();
//
//    HRESULT hr = S_OK;
//
//    //// Send initial discovery request
//    //// the rest happens in response to messages in the callback
//    //LOG_IF_FAILED(hr = RequestAllEndpointDiscoveryInformation());
//
//    //if (SUCCEEDED(hr))
//    //{
//    //    OutputDebugString(__FUNCTION__ L" - Requested discovery information");
//    //}
//    //else
//    //{
//    //    TraceLoggingWrite(
//    //        MidiSrvTelemetryProvider::Provider(),
//    //        __FUNCTION__,
//    //        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
//    //        TraceLoggingPointer(this, "this"),
//    //        TraceLoggingWideString(L"Failed to request discovery information")
//    //    );
//    //}
//
//    //if (SUCCEEDED(hr))
//    //{
//    //    // Wait until all metadata arrives or we timeout
//    //    if (!m_allMessagesReceived.wait(m_currentWorkItem.TimeoutMS))
//    //    {
//    //        // we didn't receive everything, but that's not a failure condition for this.
//    //    }
//    //}
//
//    //if (m_allMessagesReceived.is_signaled()) m_allMessagesReceived.ResetEvent();
//
//    //m_currentWorkItem.Endpoint->Cleanup();
//    //m_currentWorkItem.Endpoint = nullptr;
//
//    return hr;
//}


//void CMidiEndpointProtocolManager::ThreadWorker()
//{
//    TraceLoggingWrite(
//        MidiSrvTelemetryProvider::Provider(),
//        MIDI_TRACE_EVENT_INFO,
//        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
//        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
//        TraceLoggingPointer(this, "this"),
//        TraceLoggingWideString(L"Thread worker enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
//    );
//
//    while (!m_shutdown)
//    {
//        if (m_workQueue.size() > 0)
//        {
//            m_queueMutex.lock();
//            m_currentWorkItem = std::move(m_workQueue.front());
//            m_workQueue.pop();
//            m_queueMutex.unlock();
//
//            // this will block until completed
//            LOG_IF_FAILED(ProcessCurrentWorkEntry());
//        }
//
//        // todo: how often do we want to process messages?
//        Sleep(300);
//    }
//
//    TraceLoggingWrite(
//        MidiSrvTelemetryProvider::Provider(),
//        MIDI_TRACE_EVENT_INFO,
//        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
//        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
//        TraceLoggingPointer(this, "this"),
//        TraceLoggingWideString(L"Thread worker exit")
//    );
//
//}

