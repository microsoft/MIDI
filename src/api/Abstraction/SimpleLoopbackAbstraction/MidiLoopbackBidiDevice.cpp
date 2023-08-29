// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include "MidiLoopbackBidiDevice.h"

HRESULT MidiLoopbackBidiDevice::SendMidiMessageFromAToB(
    _In_ void* message,
    _In_ UINT32 size,
    _In_ LONGLONG timestamp)
{
    RETURN_HR_IF_NULL(E_POINTER, m_callbackB);
    RETURN_HR_IF_NULL(E_INVALIDARG, message);
    RETURN_HR_IF(E_INVALIDARG, size < sizeof(uint32_t));

    // Sending message from A output to B input

    return m_callbackB->Callback(message, size, timestamp);

}

HRESULT MidiLoopbackBidiDevice::SendMidiMessageFromBToA(
    _In_ void* message,
    _In_ UINT32 size,
    _In_ LONGLONG timestamp)
{
    RETURN_HR_IF_NULL(E_POINTER, m_callbackA);
    RETURN_HR_IF_NULL(E_INVALIDARG, message);
    RETURN_HR_IF(E_INVALIDARG, size < sizeof(uint32_t));

    // Sending message from B output to A input

    return m_callbackA->Callback(message, size, timestamp);
}



void MidiLoopbackBidiDevice::SetCallbackA(_In_ IMidiCallback* callback)
{
    m_callbackA = callback;
}

void MidiLoopbackBidiDevice::SetCallbackB(_In_ IMidiCallback* callback)
{
    m_callbackB = callback;
}


MidiLoopbackBidiDevice::MidiLoopbackBidiDevice() = default;

MidiLoopbackBidiDevice::~MidiLoopbackBidiDevice()
{
    m_callbackA = nullptr;
    m_callbackB = nullptr;
}