// Copyright (c) Microsoft Corporation. All rights reserved.
#include "stdafx.h"
#include "midisrvrpc.h"

RPC_STATUS MidiSrvRpcIfCallback(
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
    auto coInit = wil::CoInitializeEx(COINIT_MULTITHREADED);

    auto cleanupOnError = wil::scope_exit([&]()
    {
        Cleanup();
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

    RETURN_IF_FAILED(m_PerformanceManager->Initialize());
    RETURN_IF_FAILED(m_ProcessManager->Initialize());
    RETURN_IF_FAILED(m_DeviceManager->Initialize(m_PerformanceManager));
    RETURN_IF_FAILED(m_ClientManager->Initialize(m_PerformanceManager, m_ProcessManager, m_DeviceManager));

    wil::unique_hlocal rpcSecurityDescriptor;

    RETURN_IF_WIN32_BOOL_FALSE(ConvertStringSecurityDescriptorToSecurityDescriptor(
        RpcSddl,
        SDDL_REVISION_1,
        &rpcSecurityDescriptor,
        NULL));

    RETURN_IF_FAILED(HRESULT_FROM_RPCSTATUS(RpcServerUseProtseqEp(
        reinterpret_cast<RPC_WSTR>(MIDISRV_LRPC_PROTOCOL),
        RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
        reinterpret_cast<RPC_WSTR>(MIDISRV_ENDPOINT),
        rpcSecurityDescriptor.get())));

    RETURN_IF_FAILED(HRESULT_FROM_RPCSTATUS(RpcServerRegisterIf3(
        MidiSrvRPC_v1_0_s_ifspec,
        NULL, NULL,
        RPC_IF_AUTOLISTEN | RPC_IF_ALLOW_LOCAL_ONLY,
        RPC_C_LISTEN_MAX_CALLS_DEFAULT, 0,
        MidiSrvRpcIfCallback, rpcSecurityDescriptor.get())));

    cleanupOnError.release();

    return S_OK;
}

HRESULT
CMidiSrv::Cleanup()
{
    auto coInit = wil::CoInitializeEx(COINIT_MULTITHREADED);

    if (m_ClientManager)
    {
        RETURN_IF_FAILED(m_ClientManager->Cleanup());
        m_ClientManager.reset();
    }

    if (m_DeviceManager)
    {
        RETURN_IF_FAILED(m_DeviceManager->Cleanup());
        m_DeviceManager.reset();
    }

    if (m_PerformanceManager)
    {
        RETURN_IF_FAILED(m_PerformanceManager->Cleanup());
        m_PerformanceManager.reset();
    }

    if (m_ProcessManager)
    {
        RETURN_IF_FAILED(m_ProcessManager->Cleanup());
        m_ProcessManager.reset();
    }

    return S_OK;
}

