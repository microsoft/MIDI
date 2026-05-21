// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
//#include "MidiLoopbackBidiDevice.h"
#include <Feature_Servicing_MIDI2LoopbackBidi.h>

HRESULT MidiLoopbackBidiDevice::SendMidiMessageFromAToB(
    _In_ void* message,
    _In_ UINT32 size,
    _In_ LONGLONG timestamp)
{
    if (Feature_Servicing_MIDI2LoopbackBidi::IsEnabled())
    {
        wil::com_ptr_nothrow<IMidiCallback> callback;
        LONGLONG context;
        {
            auto callbackLock = m_CallbackLock.lock();
            callback = m_CallbackB;
            context = m_ContextB;
        }

        // we don't want a fail hresult here as it just means the callback went away
        if (callback == nullptr) return S_OK;

        RETURN_HR_IF_NULL(E_INVALIDARG, message);
        RETURN_HR_IF(E_INVALIDARG, size < sizeof(uint32_t));

        // Sending message from A output to B input

        return callback->Callback(MessageOptionFlags_None, message, size, timestamp, context);
    }
    else
    {
        // we don't want a fail hresult here as it just means the callback went away
        if (m_CallbackB == nullptr) return S_OK;

        RETURN_HR_IF_NULL(E_INVALIDARG, message);
        RETURN_HR_IF(E_INVALIDARG, size < sizeof(uint32_t));

        // Sending message from A output to B input

        return m_CallbackB->Callback(MessageOptionFlags_None, message, size, timestamp, m_ContextB);
    }
}

HRESULT MidiLoopbackBidiDevice::SendMidiMessageFromBToA(
    _In_ void* message,
    _In_ UINT32 size,
    _In_ LONGLONG timestamp)
{
    if (Feature_Servicing_MIDI2LoopbackBidi::IsEnabled())
    {
        wil::com_ptr_nothrow<IMidiCallback> callback;
        LONGLONG context;
        {
            auto callbackLock = m_CallbackLock.lock();
            callback = m_CallbackA;
            context = m_ContextB;
        }

        // we don't want a fail hresult here as it just means the callback went away
        if (callback == nullptr) return S_OK;

        RETURN_HR_IF_NULL(E_INVALIDARG, message);
        RETURN_HR_IF(E_INVALIDARG, size < sizeof(uint32_t));

        // Sending message from B output to A input

        return callback->Callback(MessageOptionFlags_None, message, size, timestamp, context);
    }
    else
    {
        // we don't want a fail hresult here as it just means the callback went away
        if (m_CallbackA == nullptr) return S_OK;

        RETURN_HR_IF_NULL(E_INVALIDARG, message);
        RETURN_HR_IF(E_INVALIDARG, size < sizeof(uint32_t));

        // Sending message from B output to A input

        return m_CallbackA->Callback(MessageOptionFlags_None, message, size, timestamp, m_ContextB);
    }
}


void MidiLoopbackBidiDevice::Shutdown()
{
    ShutdownA();
    ShutdownB();
}

void MidiLoopbackBidiDevice::ShutdownA()
{
    if (Feature_Servicing_MIDI2LoopbackBidi::IsEnabled())
    {
        auto callbackLock = m_CallbackLock.lock();
        m_CallbackA = nullptr;
        m_ContextA = 0;
    }
    else
    {
        m_CallbackA = nullptr;
        m_ContextA = 0;
    }
}

void MidiLoopbackBidiDevice::ShutdownB()
{
    if (Feature_Servicing_MIDI2LoopbackBidi::IsEnabled())
    {
        auto callbackLock = m_CallbackLock.lock();
        m_CallbackB = nullptr;
        m_ContextB = 0;
    }
    else
    {
        m_CallbackB = nullptr;
        m_ContextB = 0;
    }
}

_Use_decl_annotations_
void
MidiLoopbackBidiDevice::SetCallbackA
(
    IMidiCallback* callback,
    LONGLONG context
)
{
    if (Feature_Servicing_MIDI2LoopbackBidi::IsEnabled())
    {
        auto callbackLock = m_CallbackLock.lock();
        m_CallbackA = callback;
        m_ContextA = context;
    }
    else
    {
        m_CallbackA = callback;
        m_ContextA = context;
    }
}

_Use_decl_annotations_
void
MidiLoopbackBidiDevice::SetCallbackB
(
    IMidiCallback* callback,
    LONGLONG context
)
{
    if (Feature_Servicing_MIDI2LoopbackBidi::IsEnabled())
    {
        auto callbackLock = m_CallbackLock.lock();
        m_CallbackB = callback;
        m_ContextB = context;
    }
    else
    {
        m_CallbackB = callback;
        m_ContextB = context;
    }
}


MidiLoopbackBidiDevice::MidiLoopbackBidiDevice() = default;

MidiLoopbackBidiDevice::~MidiLoopbackBidiDevice()
{
    Shutdown();
}
