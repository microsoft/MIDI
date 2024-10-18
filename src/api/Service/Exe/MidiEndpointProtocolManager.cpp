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
    std::shared_ptr<CMidiClientManager>& clientManager,
    std::shared_ptr<CMidiDeviceManager>& deviceManager,
    std::shared_ptr<CMidiSessionTracker>& sessionTracker
)
{
    m_clientManager = clientManager;
    m_deviceManager = deviceManager;
    m_sessionTracker = sessionTracker;


    // check to see if we're enabled

    DWORD regValueEnabled{ MIDI_DISCOVERY_ENABLED_REG_DEFAULT_VALUE };
    if (SUCCEEDED(wil::reg::get_value_dword_nothrow(HKEY_LOCAL_MACHINE, MIDI_ROOT_REG_KEY, MIDI_DISCOVERY_ENABLED_REG_VALUE, &regValueEnabled)))
    {
        m_discoveryAndProtocolNegotiationEnabled = (bool)(regValueEnabled > 0);
    }
    else
    {
        m_discoveryAndProtocolNegotiationEnabled = false;
    }

    // we only spin this all up if negotiation is enabled
    if (m_discoveryAndProtocolNegotiationEnabled)
    {

        // use our clsid as the session id. 
        m_sessionId = __uuidof(IMidiEndpointProtocolManagerInterface);

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingGuid(m_sessionId, "session id")
        );


        wil::unique_handle processHandle(GetCurrentProcess());

        RETURN_IF_FAILED(m_sessionTracker->AddClientSession(
            m_sessionId,
            MIDI_PROTOCOL_MANAGER_SESSION_NAME,
            GetCurrentProcessId(),
            processHandle,
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
    }

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
CMidiEndpointProtocolManager::DiscoverAndNegotiate(
    GUID transportId,
    LPCWSTR endpointDeviceInterfaceId,
    ENDPOINTPROTOCOLNEGOTIATIONPARAMS negotiationParams,
    IMidiProtocolNegotiationCompleteCallback* negotiationCompleteCallback
) noexcept
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingBool(m_discoveryAndProtocolNegotiationEnabled, "IsEnabled")
    );

    // clients should check for IsEnabled before calling this function
    if (!m_discoveryAndProtocolNegotiationEnabled)
    {
        return E_ABORT;
    }


    auto cleanEndpointDeviceInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);

    std::shared_ptr<CMidiEndpointProtocolWorker> worker{ nullptr };

    if (m_endpointWorkers.find(cleanEndpointDeviceInterfaceId) != m_endpointWorkers.end())
    {
        // already exists. We can re-request negotiation from the worker
        worker = m_endpointWorkers[cleanEndpointDeviceInterfaceId].Worker;

        // todo: re-request negotiation

    }
    else if (worker == nullptr)
    {
        // allocate a new worker
        worker = std::make_shared<CMidiEndpointProtocolWorker>();
    }

    RETURN_HR_IF_NULL(E_POINTER, worker);

    auto initializeHR = worker->Initialize(
        m_sessionId,
        transportId,
        cleanEndpointDeviceInterfaceId.c_str(),
        m_clientManager,
        m_deviceManager,
        m_sessionTracker
    );

    RETURN_IF_FAILED(initializeHR);

    {
        std::scoped_lock<std::mutex> lock(m_endpointWorkersMapMutex);

        // create the thread

        ProtocolNegotiationWorkerThreadEntry newEntry{};

        newEntry.Worker = worker;

        newEntry.Thread = std::make_shared<std::thread>(&CMidiEndpointProtocolWorker::Start, newEntry.Worker,
            negotiationParams,
            negotiationCompleteCallback);

        m_endpointWorkers[cleanEndpointDeviceInterfaceId] = newEntry;
        m_endpointWorkers[cleanEndpointDeviceInterfaceId].Thread->detach();
    }

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit success", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolManager::RemoveWorkerIfPresent(std::wstring endpointInterfaceId)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit success", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    auto cleanEndpointId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointInterfaceId);

    auto val = m_endpointWorkers.find(cleanEndpointId);

    if (val != m_endpointWorkers.end())
    {
        std::scoped_lock<std::mutex> lock(m_endpointWorkersMapMutex);

        val->second.Worker->EndProcessing();    // this sets an event that tells the thread to quit

        if (val->second.Thread->joinable())
        {
            val->second.Thread->join();             // join until the thread is done
        }

        LOG_IF_FAILED(val->second.Worker->Shutdown());

        val->second.Worker.reset();
        val->second.Thread.reset();

        m_endpointWorkers.erase(cleanEndpointId);

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Worker removed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(cleanEndpointId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );
    }
    else
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"No matching worker present", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(cleanEndpointId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );
    }

    return S_OK;
}


HRESULT
CMidiEndpointProtocolManager::Shutdown()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );


    if (m_discoveryAndProtocolNegotiationEnabled)
    {
        // log a an active session so a user can figure out which
        // processes have any given device open.
        m_sessionTracker->RemoveClientSession(m_sessionId, GetCurrentProcessId());

        for (auto& [key, val] : m_endpointWorkers)
        {
            LOG_IF_FAILED(RemoveWorkerIfPresent(key));
        }

        m_endpointWorkers.clear();
    }

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

