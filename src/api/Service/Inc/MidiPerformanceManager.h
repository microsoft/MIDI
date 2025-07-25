// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class CMidiPerformanceManager
{
public:

    CMidiPerformanceManager() {}
    ~CMidiPerformanceManager() {}
    
    HRESULT Initialize();
    HRESULT Shutdown();

private:

};

