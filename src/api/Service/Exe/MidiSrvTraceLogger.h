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

    void LogMidi2CreateClient(HRESULT hr, LPCWSTR DeviceInstanceId, LPCWSTR ProcessName, MidiApi Api, MidiFlow Flow, MidiDataFormats Format, DWORD ClientProcessId);
    void LogMidi2DestroyClient(HRESULT hr);
};
