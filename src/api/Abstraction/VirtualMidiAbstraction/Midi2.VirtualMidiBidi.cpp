// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "midi2.VirtualMidiabstraction.h"

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiBiDi::Initialize(
    LPCWSTR,
    DWORD *,
    IMidiCallback * callback
)
{

    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    m_callback = callback;

    return S_OK;
}

HRESULT
CMidi2VirtualMidiBiDi::Cleanup()
{
    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    m_callback = nullptr;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiBiDi::SendMidiMessage(
    PVOID message,
    UINT size,
    LONGLONG position
)
{
    if (m_callback == nullptr)
    {
        // TODO log that callback is null
        return E_FAIL;
    }

    if (message == nullptr)
    {
        // TODO log that message was null
        return E_FAIL;
    }

    if (size < sizeof(uint32_t))
    {
        // TODO log that data was smaller than minimum UMP size
        return E_FAIL;
    }

    //// copy the data

    //auto data = std::make_unique<byte[]>(size);

    //std::memcpy((void*)(data.get()), message, size);


    //uint32_t firstWord = *(uint32_t*)message;

    //std::wstringstream ss;

    //ss << "CMidi2SampleMidiBiDi::SendMidiMessage. Received timestamp: " << std::hex << position << ", Size: " << size << ", First word: " << firstWord;
    //OutputDebugString(ss.str().c_str());

    //// resend it. "position" is the timestamp

    //_callback->Callback(data.get(), size, position);

    //OutputDebugString(L"CMidi2SampleMidiBiDi::SendMidiMessage Callback message sent");


    m_callback->Callback(message, size, position);

    return S_OK;

}

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiBiDi::Callback(
    PVOID,
    UINT,
    LONGLONG
)
{
    //return E_NOTIMPL;

    // just eat it for this simple loopback
    return S_OK;
}

