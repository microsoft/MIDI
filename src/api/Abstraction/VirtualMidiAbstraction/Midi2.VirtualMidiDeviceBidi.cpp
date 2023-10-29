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
CMidi2VirtualMidiDeviceBiDi::Initialize(
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

    m_clientCallback = callback;


    // TODO: Spin up the client endpoint


    return S_OK;
}

HRESULT
CMidi2VirtualMidiDeviceBiDi::Cleanup()
{
    // TODO: Cleanup here needs additional logic to tear down the client endpoint
    // when this endpoint goes away

    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    m_clientCallback = nullptr;
    m_clientBiDi = nullptr;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiDeviceBiDi::SendMidiMessage(
    PVOID message,
    UINT size,
    LONGLONG position
)
{
    // message received from the client

    RETURN_HR_IF_NULL(E_INVALIDARG, message);

    if (size < sizeof(uint32_t))
    {
        // TODO log that data was smaller than minimum UMP size
        return E_FAIL;
    }

    if (m_clientCallback != nullptr)
    {
        return m_clientCallback(message, size, position);
    }

    return S_OK;

}

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiDeviceBiDi::Callback(
    PVOID /*message*/,
    UINT /*size*/,
    LONGLONG /*position*/
)
{
    //if (m_clientCallback != nullptr)
    //{
    //    return m_clientCallback->Callback(message, size, position);
    //}

    return S_OK;
}

