// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
//#include "MidiLoopbackBidiDevice.h"

HRESULT MidiLoopbackBidiDevice::SendMidiMessageFromAToB(
    _In_ void* Message,
    _In_ UINT32 Size,
    _In_ LONGLONG Timestamp)
{
    // we don't want a fail hresult here as it just means the callback went away
    if (m_CallbackB == nullptr) return S_OK;

    RETURN_HR_IF_NULL(E_INVALIDARG, Message);
    RETURN_HR_IF(E_INVALIDARG, Size < sizeof(uint32_t));

    // Sending message from A output to B input

    return m_CallbackB->Callback(Message, Size, Timestamp, m_ContextB);

}

HRESULT MidiLoopbackBidiDevice::SendMidiMessageFromBToA(
    _In_ void* Message,
    _In_ UINT32 Size,
    _In_ LONGLONG Timestamp)
{
    // we don't want a fail hresult here as it just means the callback went away
    if (m_CallbackA == nullptr) return S_OK;

    RETURN_HR_IF_NULL(E_INVALIDARG, Message);
    RETURN_HR_IF(E_INVALIDARG, Size < sizeof(uint32_t));

    // Sending message from B output to A input

    return m_CallbackA->Callback(Message, Size, Timestamp, m_ContextB);
}


void MidiLoopbackBidiDevice::Cleanup()
{
    CleanupA();
    CleanupB();
}

void MidiLoopbackBidiDevice::CleanupA()
{
    m_CallbackA = nullptr;
    m_ContextA = 0;
}

void MidiLoopbackBidiDevice::CleanupB()
{
    m_CallbackB = nullptr;
    m_ContextB = 0;
}

_Use_decl_annotations_
void
MidiLoopbackBidiDevice::SetCallbackA
(
    IMidiCallback* Callback,
    LONGLONG Context
)
{
    m_CallbackA = Callback;
    m_ContextA = Context;
}

_Use_decl_annotations_
void
MidiLoopbackBidiDevice::SetCallbackB
(
    IMidiCallback* Callback,
    LONGLONG Context
)
{
    m_CallbackB = Callback;
    m_ContextB = Context;
}


MidiLoopbackBidiDevice::MidiLoopbackBidiDevice() = default;

MidiLoopbackBidiDevice::~MidiLoopbackBidiDevice()
{
    Cleanup();
}
