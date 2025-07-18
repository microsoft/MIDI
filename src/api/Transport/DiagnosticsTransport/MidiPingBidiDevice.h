// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiPingBidiDevice
{
public:
    void SetCallback(
        _In_ IMidiCallback*,
        _In_ LONGLONG);

    HRESULT SendMidiMessage(
        _In_ void*,
        _In_ UINT32,
        _In_ LONGLONG);


    void Shutdown();

    MidiPingBidiDevice();
    ~MidiPingBidiDevice();

private:
    IMidiCallback* m_Callback;
    LONGLONG m_Context;
};
