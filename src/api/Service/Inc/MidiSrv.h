// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

class CMidiSrv;
extern std::unique_ptr<CMidiSrv> g_MidiService;

class CMidiSrv
{
public:

    CMidiSrv() {}
    ~CMidiSrv() {}

    HRESULT Initialize();
    HRESULT Cleanup();

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



private:
    std::shared_ptr<CMidiPerformanceManager> m_PerformanceManager;
    std::shared_ptr<CMidiProcessManager> m_ProcessManager;
    std::shared_ptr<CMidiDeviceManager> m_DeviceManager;
    std::shared_ptr<CMidiClientManager> m_ClientManager;
    std::shared_ptr<CMidiConfigurationManager> m_ConfigurationManager;


};

