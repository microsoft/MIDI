// Copyright (c) Microsoft Corporation. All rights reserved.
#include "stdafx.h"

_Use_decl_annotations_
HRESULT
CMidiDeviceManager::Initialize(
    std::shared_ptr<CMidiPerformanceManager>& PerformanceManager
)
{
    m_PerformanceManager = PerformanceManager;
    return S_OK;
}

HRESULT
CMidiDeviceManager::Cleanup()
{
    return S_OK;
}

