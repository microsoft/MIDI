// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class CMidiProcessManager
{
public:

    CMidiProcessManager() {}
    ~CMidiProcessManager() {}
    
    HRESULT Initialize();
    HRESULT Shutdown();

private:

};

