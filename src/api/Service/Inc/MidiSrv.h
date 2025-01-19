// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class CMidiSrv;
extern std::unique_ptr<CMidiSrv> g_MidiService;

class CMidiSrv
{
public:

    CMidiSrv() {}
    ~CMidiSrv() {}

    HRESULT Initialize();
    HRESULT Shutdown();

    HRESULT GetDeviceManager(std::shared_ptr<CMidiDeviceManager>& manager)
    {
        manager = m_DeviceManager;
        return S_OK;
    }

    HRESULT GetClientManager(std::shared_ptr<CMidiClientManager>& manager)
    {
        manager = m_ClientManager;
        return S_OK;
    }

    HRESULT GetPerformanceManager(std::shared_ptr<CMidiPerformanceManager>& manager)
    {
        manager = m_PerformanceManager;
        return S_OK;
    }

    HRESULT GetProcessManager(std::shared_ptr<CMidiProcessManager>& manager)
    {
        manager = m_ProcessManager;
        return S_OK;
    }

    HRESULT GetConfigurationManager(std::shared_ptr<CMidiConfigurationManager>& manager)
    {
        manager = m_ConfigurationManager;
        return S_OK;
    }

    HRESULT GetEndpointProtocolManager(std::shared_ptr<CMidiEndpointProtocolManager>& manager)
    {
        manager = m_EndpointProtocolManager;
        return S_OK;
    }

    HRESULT GetSessionTracker(std::shared_ptr<CMidiSessionTracker>& tracker)
    {
        tracker = m_SessionTracker;
        return S_OK;
    }

    HRESULT GetTraceLogger(std::shared_ptr<CMidiSrvTraceLogger>& traceLogger)
    {
        traceLogger = m_TraceLogger;
        return S_OK;
    }

private:
    std::shared_ptr<CMidiPerformanceManager> m_PerformanceManager;
    std::shared_ptr<CMidiProcessManager> m_ProcessManager;
    std::shared_ptr<CMidiDeviceManager> m_DeviceManager;
    std::shared_ptr<CMidiClientManager> m_ClientManager;
    std::shared_ptr<CMidiConfigurationManager> m_ConfigurationManager;

    std::shared_ptr<CMidiEndpointProtocolManager> m_EndpointProtocolManager;

    std::shared_ptr<CMidiSessionTracker> m_SessionTracker;

    std::shared_ptr<CMidiSrvTraceLogger> m_TraceLogger;

    bool m_RpcRegistered{false};
    bool m_RpcBound {false};
    wil::unique_rpc_binding_vector  m_RpcBindingVector;

};

