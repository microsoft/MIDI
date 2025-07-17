// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "stdafx.h"

class CMidiSrvTraceLogger
{
public:
    CMidiSrvTraceLogger()
    {
    }

    void LogMidi2CreateClient(
        _In_ HRESULT hr,
        _In_ LPCWSTR deviceInstanceId,
        _In_ LPCWSTR processName,
        _In_ GUID api,
        _In_ MidiFlow flow,
        _In_ MidiDataFormats format,
        _In_ DWORD clientProcessId
    );
    void LogMidi2DestroyClient(_In_ HRESULT hr);
};
