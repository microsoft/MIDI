// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"
#include "midisrvrpc.h"

#include "Feature_Servicing_MIDI2LegacyControl.h"
#include "Feature_Servicing_MIDI2SynchronizedStart.h"

RPC_STATUS RPC_ENTRY MidiSrvRpcIfCallback(
    RPC_IF_HANDLE,
    void* Context
)
{
    unsigned int type;
    RPC_STATUS status;

    // reject any connection that is not local.
    status = I_RpcBindingInqTransportType(Context, &type);
    if (RPC_S_OK != status)
        return ERROR_ACCESS_DENIED;
    if (TRANSPORT_TYPE_LPC != type)
        return ERROR_ACCESS_DENIED;

    return RPC_S_OK;
}

HRESULT
CMidiSrv::Initialize()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingUInt32(GetCurrentProcessId(), "Process Id")
    );

    auto cleanupOnError = wil::scope_exit([&]()
    {
        Shutdown();
    });

    const PCWSTR RpcSddl =
        L"D:"
        L"(A;;GRGWGX;;;WD)" // World:   Execute | Read | Write
        L"(A;;GRGWGX;;;RC)" // Restricted: Execute | Read | Write
        L"(A;;GA;;;BA)"     // Admin:   All Access
        L"(A;;GA;;;OW)"     // Owner:   All Access
        L"(A;;GR;;;AC)";    // AppContainer:   Read (meaning connect)

    m_PerformanceManager = std::make_shared<CMidiPerformanceManager>();
    RETURN_IF_NULL_ALLOC(m_PerformanceManager);

    m_ProcessManager = std::make_shared<CMidiProcessManager>();
    RETURN_IF_NULL_ALLOC(m_ProcessManager);

    m_DeviceManager = std::make_shared<CMidiDeviceManager>();
    RETURN_IF_NULL_ALLOC(m_DeviceManager);

    m_ClientManager = std::make_shared<CMidiClientManager>();
    RETURN_IF_NULL_ALLOC(m_ClientManager);

    m_ConfigurationManager = std::make_shared<CMidiConfigurationManager>();
    RETURN_IF_NULL_ALLOC(m_ConfigurationManager);

    m_EndpointProtocolManager = std::make_shared<CMidiEndpointProtocolManager>();
    RETURN_IF_NULL_ALLOC(m_EndpointProtocolManager);

    m_SessionTracker = std::make_shared<CMidiSessionTracker>();
    RETURN_IF_NULL_ALLOC(m_SessionTracker);

    m_TraceLogger = std::make_shared<CMidiSrvTraceLogger>();
    RETURN_IF_NULL_ALLOC(m_TraceLogger);

    // NOTE: client manager is not yet initialized when this is called
    RETURN_IF_FAILED(m_SessionTracker->Initialize(m_ClientManager));

    RETURN_IF_FAILED(m_EndpointProtocolManager->Initialize(m_ClientManager, m_DeviceManager, m_SessionTracker));

    RETURN_IF_FAILED(m_PerformanceManager->Initialize());
    RETURN_IF_FAILED(m_ProcessManager->Initialize());
    RETURN_IF_FAILED(m_ConfigurationManager->Initialize());
    RETURN_IF_FAILED(m_ClientManager->Initialize(m_PerformanceManager, m_ProcessManager, m_DeviceManager, m_SessionTracker));

    if (Feature_Servicing_MIDI2SynchronizedStart::IsEnabled())
    {
        // Device manager initialization enumerates endpoints for all transports and can take a while.
        // Run it on a worker thread so the demand-start RPC interface (registered below) becomes
        // available promptly and the triggering RPC call does not block long enough to be torn down as
        // a hang. The worker signals m_DeviceEnumerationCompleteEvent when enumeration finishes, which
        // clients wait on (in their own process) before assuming all midi ports are present.
        //
        // The event is created before RPC registration so that any client which is able to complete the
        // RPC call is guaranteed to find the event already present.
        RETURN_IF_FAILED(CreateDeviceEnumerationCompleteEvent());

        m_DeviceManagerInitializeThread.reset(CreateThread(
            nullptr,
            0,
            &CMidiSrv::DeviceManagerInitializeWorker,
            this,
            0,
            nullptr));
        RETURN_LAST_ERROR_IF_NULL(m_DeviceManagerInitializeThread.get());
    }
    else
    {
        // initialize this last because it starts enumerating endpoints for all the transports
        RETURN_IF_FAILED(m_DeviceManager->Initialize(m_PerformanceManager, m_EndpointProtocolManager, m_ConfigurationManager, m_ClientManager));
    }



    wil::unique_hlocal rpcSecurityDescriptor;

    RETURN_IF_WIN32_BOOL_FALSE(ConvertStringSecurityDescriptorToSecurityDescriptor(
        RpcSddl,
        SDDL_REVISION_1,
        &rpcSecurityDescriptor,
        NULL));

    
    if (rpcSecurityDescriptor.is_valid())
    {
        // this is an ugly set of casts, but the reinterpret_cast error only
        // comes up with C++/20. Risk of a straight c-style cast here going
        // poorly is low. Error comes from the function taking non-const params
        // but our arguments here are const.
        auto rpcStatus = RpcServerUseProtseqEp(
            (RPC_WSTR)MIDISRV_LRPC_PROTOCOL,
            RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
            (RPC_WSTR)MIDISRV_ENDPOINT,
            rpcSecurityDescriptor.get());

        auto rpcHr = HRESULT_FROM_RPCSTATUS(rpcStatus);

        if (FAILED(rpcHr))
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"RpcServerUseProtseqEp failed. It's likely the service is not responding due to a failed startup.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingHResult(rpcHr, MIDI_TRACE_EVENT_HRESULT_FIELD),
                TraceLoggingLong(rpcStatus, "rpc_status")
            );
        }

        RETURN_IF_FAILED(rpcHr);

        RETURN_IF_FAILED(
            HRESULT_FROM_RPCSTATUS(
                RpcServerRegisterIf3(
                    MidiSrvRPC_v1_0_s_ifspec,                       // IfSpec
                    NULL,                                           // MgrTypeUuid
                    NULL,                                           // MgrEpv
                    RPC_IF_AUTOLISTEN | RPC_IF_ALLOW_LOCAL_ONLY,    // Flags
                    RPC_C_LISTEN_MAX_CALLS_DEFAULT,                 // MaxCalls
                    0,                                              // MaxRpcSize (no effect for ncalrpc -- local)
                    MidiSrvRpcIfCallback,                           // IfCallback
                    rpcSecurityDescriptor.get()                     // SecurityDescriptor
                )
            )
        );                 

        m_RpcRegistered = true;

        RETURN_IF_FAILED(
            HRESULT_FROM_RPCSTATUS(
                RpcServerInqBindings(&m_RpcBindingVector)
            )
        );

        RETURN_IF_FAILED(
            HRESULT_FROM_RPCSTATUS(
                RpcEpRegisterW(MidiSrvRPC_v1_0_s_ifspec,
                    m_RpcBindingVector.get(),
                    NULL,
                    NULL
                )
            )
        );

        m_RpcBound = true;
    }
    else
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Returned RPC Security Descriptor is not valid", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );
    }

    cleanupOnError.release();

    return S_OK;
}

HRESULT
CMidiSrv::CreateDeviceEnumerationCompleteEvent()
{
    Feature_Servicing_MIDI2SynchronizedStart::AssertEnabled();

    // The event is set/reset only by midisrv. Every caller of the midi APIs, including AppContainer
    // applications, may open it to synchronize (wait) and query its state, but may not modify it.
    // Note that GENERIC_READ does not include SYNCHRONIZE, so the wait/query access is granted with
    // an explicit access mask (EVENT_QUERY_STATE | SYNCHRONIZE == 0x00100001).
    //
    // Only a DACL is specified (no owner). Forcing an explicit owner (e.g. O:SY) fails with
    // ERROR_INVALID_OWNER (1307) unless the service token can assign that SID; instead the owner
    // defaults to the creating token, which is correct and matches the RPC endpoint SDDL above.
    // midisrv runs as LocalService (LS), so full access is granted to LS (the account that
    // sets/resets the event), not LocalSystem.
    const PCWSTR eventSddl =
        L"D:"
        L"(A;;0x1F0003;;;LS)"   // LocalService (midisrv): EVENT_ALL_ACCESS
        L"(A;;0x00100001;;;WD)" // World:          EVENT_QUERY_STATE | SYNCHRONIZE
        L"(A;;0x00100001;;;AC)";// AppContainers:  EVENT_QUERY_STATE | SYNCHRONIZE

    wil::unique_hlocal eventSecurityDescriptor;
    RETURN_IF_WIN32_BOOL_FALSE(ConvertStringSecurityDescriptorToSecurityDescriptor(
        eventSddl,
        SDDL_REVISION_1,
        &eventSecurityDescriptor,
        nullptr));

    SECURITY_ATTRIBUTES securityAttributes{};
    securityAttributes.nLength = sizeof(securityAttributes);
    securityAttributes.lpSecurityDescriptor = eventSecurityDescriptor.get();
    securityAttributes.bInheritHandle = FALSE;

    // Manual-reset, initially non-signaled. Once enumeration completes it remains signaled for the
    // lifetime of the service so that later clients return from their wait immediately.
    auto deviceEnumerationEventHandle = CreateEventW(
        &securityAttributes,
        TRUE,   // manual reset
        FALSE,  // initially non-signaled
        MIDISRV_DEVICE_ENUMERATION_COMPLETE_EVENT_NAME);
    RETURN_LAST_ERROR_IF_NULL(deviceEnumerationEventHandle);

    const DWORD createEventLastError = GetLastError();

    m_DeviceEnumerationCompleteEvent.reset(deviceEnumerationEventHandle);

    // If the named event already exists (e.g., a previous midisrv instance signaled it and a client
    // still holds a handle), bInitialState is ignored and the event may be left signaled. Reset it so
    // clients will wait until the new instance finishes enumerating devices.
    if (createEventLastError == ERROR_ALREADY_EXISTS)
    {
        RETURN_IF_WIN32_BOOL_FALSE(ResetEvent(m_DeviceEnumerationCompleteEvent.get()));
    }

    return S_OK;
}

DWORD WINAPI
CMidiSrv::DeviceManagerInitializeWorker(
    _In_ LPVOID context
)
{
    Feature_Servicing_MIDI2SynchronizedStart::AssertEnabled();

    auto self = static_cast<CMidiSrv*>(context);

    // The device manager creates COM transport objects, so an apartment is required on this thread.
    auto coUninitialize = wil::CoInitializeEx(COINIT_MULTITHREADED);

    // CMidiDeviceManager::Initialize logs (but does not fail startup on) individual transport failures.
    // Log any unexpected failure here for diagnosability.
    LOG_IF_FAILED(self->m_DeviceManager->Initialize(
        self->m_PerformanceManager,
        self->m_EndpointProtocolManager,
        self->m_ConfigurationManager,
        self->m_ClientManager));

    // Signal completion regardless of result so waiting clients are released rather than timing out.
    SetEvent(self->m_DeviceEnumerationCompleteEvent.get());

    return 0;
}

HRESULT
CMidiSrv::Shutdown()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    if (Feature_Servicing_MIDI2LegacyControl::IsEnabled())
    {
        // Shut down the protocol manager before the RPC unregister,
        // as the protocol manager has a client which uses the RPC.
        if (m_EndpointProtocolManager)
        {
            // Shut down the protocol manager, but don't reset/release
            // the memory yet, as other components may still have references
            // to it.
            RETURN_IF_FAILED(m_EndpointProtocolManager->Shutdown());
        }
    }

    if (m_RpcBound)
    {
        LOG_IF_WIN32_ERROR(::RpcEpUnregister(MidiSrvRPC_v1_0_s_ifspec, m_RpcBindingVector.get(), nullptr));
        m_RpcBound = false;
    }
    
    if (m_RpcRegistered)
    {
        LOG_IF_WIN32_ERROR(::RpcServerUnregisterIfEx(MidiSrvRPC_v1_0_s_ifspec, nullptr, 1));
        m_RpcRegistered = false;
    }
    
    if (m_RpcBindingVector)
    {
        m_RpcBindingVector.reset();
    }

    if (m_ClientManager)
    {
        RETURN_IF_FAILED(m_ClientManager->Shutdown());
        m_ClientManager.reset();
    }

    if (Feature_Servicing_MIDI2SynchronizedStart::IsEnabled())
    {
        // Wait for the device manager initialization worker to finish before tearing down the device
        // manager it is using. Signal the completion event first so that any client currently waiting on
        // enumeration is released as the service shuts down. These members are only set when the feature
        // is enabled, so this is a no-op on the legacy (synchronous) path.
        if (m_DeviceManagerInitializeThread)
        {
            if (m_DeviceEnumerationCompleteEvent)
            {
                SetEvent(m_DeviceEnumerationCompleteEvent.get());
            }

            WaitForSingleObject(m_DeviceManagerInitializeThread.get(), INFINITE);
            m_DeviceManagerInitializeThread.reset();
        }
        m_DeviceEnumerationCompleteEvent.reset();
    }

    if (m_DeviceManager)
    {
        RETURN_IF_FAILED(m_DeviceManager->Shutdown());
        m_DeviceManager.reset();
    }

    if (m_PerformanceManager)
    {
        RETURN_IF_FAILED(m_PerformanceManager->Shutdown());
        m_PerformanceManager.reset();
    }

    if (m_ProcessManager)
    {
        RETURN_IF_FAILED(m_ProcessManager->Shutdown());
        m_ProcessManager.reset();
    }

    if (m_ConfigurationManager)
    {
        RETURN_IF_FAILED(m_ConfigurationManager->Shutdown());
        m_ConfigurationManager.reset();
    }

    if (Feature_Servicing_MIDI2LegacyControl::IsEnabled())
    {
        // Release the memory now that no other components are using the
        // protocol manager.
        m_EndpointProtocolManager.reset();
    }
    else
    {
        if (m_EndpointProtocolManager)
        {
            RETURN_IF_FAILED(m_EndpointProtocolManager->Shutdown());
            m_EndpointProtocolManager.reset();
        }
    }

    if (m_SessionTracker)
    {
        RETURN_IF_FAILED(m_SessionTracker->Shutdown());
        m_SessionTracker.reset();
    }


    return S_OK;
}


