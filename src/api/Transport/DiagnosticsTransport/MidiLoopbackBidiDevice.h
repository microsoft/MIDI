// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiLoopbackBidiDevice
{
public:
    void SetCallbackA(_In_ IMidiCallback*, _In_ LONGLONG);
    void SetCallbackB(_In_ IMidiCallback*, _In_ LONGLONG);

    HRESULT SendMidiMessageFromAToB(
        _In_ void*,
        _In_ UINT32,
        _In_ LONGLONG);

    HRESULT SendMidiMessageFromBToA(
        _In_ void*,
        _In_ UINT32,
        _In_ LONGLONG);

    void Shutdown();
    void ShutdownA();
    void ShutdownB();

    MidiLoopbackBidiDevice();
    ~MidiLoopbackBidiDevice();

private:
    IMidiCallback* m_CallbackA;
    LONGLONG m_ContextA;

    IMidiCallback* m_CallbackB;
    LONGLONG m_ContextB;

};
