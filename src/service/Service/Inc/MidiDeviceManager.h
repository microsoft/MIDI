// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

class CMidiDeviceManager
{
public:

    CMidiDeviceManager() {}
    ~CMidiDeviceManager() {}
    
    HRESULT Initialize(_In_ std::shared_ptr<CMidiPerformanceManager>& performanceManager);
    HRESULT Cleanup();

private:

    std::shared_ptr<CMidiPerformanceManager> m_PerformanceManager;
};

