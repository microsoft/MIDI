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

private:
    std::shared_ptr<CMidiPerformanceManager> m_PerformanceManager;
    std::shared_ptr<CMidiProcessManager> m_ProcessManager;
    std::shared_ptr<CMidiDeviceManager> m_DeviceManager;
    std::shared_ptr<CMidiClientManager> m_ClientManager;
};

