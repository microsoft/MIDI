// Copyright (c) Microsoft Corporation. All rights reserved.
#include "stdafx.h"

_Use_decl_annotations_
HRESULT
CMidiClientManager::Initialize(
    std::shared_ptr<CMidiPerformanceManager>& performanceManager,
    std::shared_ptr<CMidiProcessManager>& processManager,
    std::shared_ptr<CMidiDeviceManager>& deviceManager
)
{
    m_PerformanceManager = performanceManager;
    m_ProcessManager = processManager;
    m_DeviceManager = deviceManager;

    return S_OK;
}

HRESULT
CMidiClientManager::Cleanup()
{
    m_PerformanceManager.reset();
    m_ProcessManager.reset();
    m_DeviceManager.reset();

    return S_OK;
}

