// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

class CMidiClientManager
{
public:

    CMidiClientManager() {}
    ~CMidiClientManager() {}
    
    HRESULT Initialize(_In_ std::shared_ptr<CMidiPerformanceManager>& performanceManager,
                            _In_ std::shared_ptr<CMidiProcessManager>& processManager,
                            _In_ std::shared_ptr<CMidiDeviceManager>& deviceManager);

    HRESULT Cleanup();

private:
    std::shared_ptr<CMidiPerformanceManager> m_PerformanceManager;
    std::shared_ptr<CMidiProcessManager> m_ProcessManager;
    std::shared_ptr<CMidiDeviceManager> m_DeviceManager;
};

