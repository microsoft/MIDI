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


using namespace winrt::Windows::Devices::Enumeration;
using namespace winrt::Windows::Foundation;


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


    winrt::hstring deviceSelector(
        L"System.Devices.InterfaceClassGuid:=\"{E7CCE071-3C03-423f-88D3-F1045D02552B}\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True");

    m_watcher = DeviceInformation::CreateWatcher(deviceSelector);

    auto deviceAddedHandler = TypedEventHandler<DeviceWatcher, DeviceInformation>(this, &CMidiEndpointProtocolManager::OnDeviceAdded);
    auto deviceRemovedHandler = TypedEventHandler<DeviceWatcher, DeviceInformationUpdate>(this, &CMidiEndpointProtocolManager::OnDeviceRemoved);
    auto deviceUpdatedHandler = TypedEventHandler<DeviceWatcher, DeviceInformationUpdate>(this, &CMidiEndpointProtocolManager::OnDeviceUpdated);
    auto deviceStoppedHandler = TypedEventHandler<DeviceWatcher, winrt::Windows::Foundation::IInspectable>(this, &CMidiEndpointProtocolManager::OnDeviceStopped);
    auto deviceEnumerationCompletedHandler = TypedEventHandler<DeviceWatcher, winrt::Windows::Foundation::IInspectable>(this, &CMidiEndpointProtocolManager::OnEnumerationCompleted);

    m_DeviceAdded = m_watcher.Added(winrt::auto_revoke, deviceAddedHandler);
    m_DeviceRemoved = m_watcher.Removed(winrt::auto_revoke, deviceRemovedHandler);
    m_DeviceUpdated = m_watcher.Updated(winrt::auto_revoke, deviceUpdatedHandler);
    m_DeviceStopped = m_watcher.Stopped(winrt::auto_revoke, deviceStoppedHandler);
    m_DeviceEnumerationCompleted = m_watcher.EnumerationCompleted(winrt::auto_revoke, deviceEnumerationCompletedHandler);

    m_watcher.Start();

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolManager::OnDeviceAdded(DeviceWatcher, DeviceInformation)
{
    return S_OK;
}

_Use_decl_annotations_
HRESULT 
CMidiEndpointProtocolManager::OnDeviceRemoved(DeviceWatcher, DeviceInformationUpdate update)
{
    // Check to see if we have an entry. If so, tear it down

    auto cleanId = internal::NormalizeEndpointInterfaceIdWStringCopy(update.Id().c_str());

    RETURN_IF_FAILED(RemoveWorkerIfPresent(cleanId));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolManager::OnDeviceUpdated(DeviceWatcher, DeviceInformationUpdate)
{
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolManager::OnDeviceStopped(DeviceWatcher, winrt::Windows::Foundation::IInspectable)
{
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolManager::OnEnumerationCompleted(DeviceWatcher, winrt::Windows::Foundation::IInspectable)
{
    return S_OK;
}




_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolManager::NegotiateAndRequestMetadata(
    GUID AbstractionGuid,
    LPCWSTR DeviceInterfaceId,
    ENDPOINTPROTOCOLNEGOTIATIONPARAMS NegotiationParams,
    IMidiProtocolNegotiationCompleteCallback* NegotiationCompleteCallback
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
        worker = m_endpointWorkers[cleanEndpointDeviceInterfaceId].Worker;
    }

    if (worker == nullptr)
    {
        // allocate a new worker
        worker = std::make_shared<CMidiEndpointProtocolWorker>();

        if (worker)
        {
            auto initializeHR = worker->Initialize(
                m_sessionId,
                AbstractionGuid,
                cleanEndpointDeviceInterfaceId.c_str(),
                m_clientManager,
                m_deviceManager,
                m_sessionTracker
            );

            if (FAILED(initializeHR))
            {
                // TODO: Log

                return E_FAIL;
            }

            // create the thread

            ProtocolNegotiationWorkerThreadEntry newEntry{};

            newEntry.Worker = worker;

            newEntry.Thread = std::make_shared<std::thread>(&CMidiEndpointProtocolWorker::Start, newEntry.Worker,
                NegotiationParams,
                NegotiationCompleteCallback);

            // TODO: Need to protect this map
            m_endpointWorkers[cleanEndpointDeviceInterfaceId] = newEntry;

            m_endpointWorkers[cleanEndpointDeviceInterfaceId].Thread->detach();
        }
        else
        {
            // TODO: log failed allocation

            return E_FAIL;
        }


    }

    // now that we have the worker created, actually start the work. This function blocks for up to TimeoutMilliseconds.

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolManager::RemoveWorkerIfPresent(std::wstring endpointInterfaceId)
{
    auto val = m_endpointWorkers.find(endpointInterfaceId);

    if (val != m_endpointWorkers.end())
    {
        val->second.Worker->EndProcessing();    // this sets an event that tells the thread to quit

        if (val->second.Thread->joinable())
        {
            val->second.Thread->join();             // join until the thread is done
        }

        LOG_IF_FAILED(val->second.Worker->Cleanup());

        val->second.Worker.reset();
        val->second.Thread.reset();

        m_endpointWorkers.erase(endpointInterfaceId);
    }

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
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    m_sessionTracker->RemoveClientSession(m_sessionId);

    for (auto& [key, val] : m_endpointWorkers)
    {
        RemoveWorkerIfPresent(key);
    }

    m_endpointWorkers.clear();

    m_clientManager.reset();
    m_deviceManager.reset();
    m_sessionTracker.reset();

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

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

