// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "midi2.VirtualPatchBayabstraction.h"

_Use_decl_annotations_
HRESULT
CMidi2VirtualPatchBayBiDi::Initialize(
    LPCWSTR,
    DWORD *,
    IMidiCallback * callback
)
{

    TraceLoggingWrite(
        MidiVirtualPatchBayAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    m_callback = callback;


    // TODO: Using the device Id parameter, associate with the endpoint in the endpoint device table

    // may need to store a this pointer in that table as the IMidiBiDi property

    return S_OK;
}

HRESULT
CMidi2VirtualPatchBayBiDi::Cleanup()
{
    TraceLoggingWrite(
        MidiVirtualPatchBayAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    // TODO: Remove the BiDi pointer from the endpoint device table

    m_callback = nullptr;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2VirtualPatchBayBiDi::SendMidiMessage(
    PVOID message,
    UINT size,
    LONGLONG /*position*/
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, message);

    if (size < sizeof(uint32_t))
    {
        // TODO log that data was smaller than minimum UMP size
        return E_FAIL;
    }

    //m_callback->Callback(message, size, position);

    return S_OK;

}

_Use_decl_annotations_
HRESULT
CMidi2VirtualPatchBayBiDi::Callback(
    PVOID /*message*/,
    UINT /*size*/,
    LONGLONG /*position*/
)
{
    //return E_NOTIMPL;

    return S_OK;
}

