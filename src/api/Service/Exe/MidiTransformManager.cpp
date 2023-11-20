// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "stdafx.h"

_Use_decl_annotations_
HRESULT CMidiTransformManager::Initialize(
    std::shared_ptr<CMidiPerformanceManager>& performanceManager,
    std::shared_ptr<CMidiClientManager>& clientManager,
    std::shared_ptr<CMidiDeviceManager>& deviceManager)
{
    UNREFERENCED_PARAMETER(performanceManager);
    UNREFERENCED_PARAMETER(clientManager);
    UNREFERENCED_PARAMETER(deviceManager);

    // TODO

    return S_OK;
}


HRESULT CMidiTransformManager::Cleanup()
{
    return S_OK;
}