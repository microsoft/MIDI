// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"
#include "midisrvrpc.h"

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

    // initialize this last because it starts enumerating endpoints for all the transports
    RETURN_IF_FAILED(m_DeviceManager->Initialize(m_PerformanceManager, m_EndpointProtocolManager, m_ConfigurationManager));



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
CMidiSrv::Shutdown()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

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

    if (m_SessionTracker)
    {
        RETURN_IF_FAILED(m_SessionTracker->Shutdown());
        m_SessionTracker.reset();
    }


    return S_OK;
}


