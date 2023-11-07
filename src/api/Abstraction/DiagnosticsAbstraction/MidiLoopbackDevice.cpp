// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
//#include "MidiLoopbackDevice.h"

HRESULT MidiLoopbackDevice::SendMidiMessage(
    _In_ void* message,
    _In_ UINT32 size,
    _In_ LONGLONG timestamp)
{
    RETURN_HR_IF_NULL(E_POINTER, m_callback);
    RETURN_HR_IF_NULL(E_INVALIDARG, message);
    RETURN_HR_IF(E_INVALIDARG, size < sizeof(uint32_t));

    // this is a loopback, so all we do is call the callback

    return m_callback->Callback(message, size, timestamp);
}


void MidiLoopbackDevice::Cleanup()
{
    m_callback = nullptr;
}

void MidiLoopbackDevice::SetCallback(_In_ IMidiCallback* callback)
{
    m_callback = callback;
}

MidiLoopbackDevice::MidiLoopbackDevice() = default;

MidiLoopbackDevice::~MidiLoopbackDevice()
{
    Cleanup();
}