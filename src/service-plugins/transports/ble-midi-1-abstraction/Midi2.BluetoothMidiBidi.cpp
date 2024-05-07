// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "midi2.BluetoothMidiabstraction.h"

_Use_decl_annotations_
HRESULT
CMidi2BluetoothMidiBiDi::Initialize(
    LPCWSTR,
    PABSTRACTIONCREATIONPARAMS,
    DWORD *,
    IMidiCallback * Callback,
    LONGLONG Context,
    GUID /* SessionId */

)
{

    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    m_Callback = Callback;
    m_Context = Context;


    // TODO: Using the device Id parameter, associate with the endpoint in the endpoint device table

    // may need to store a this pointer in that table as the IMidiBiDi property

    return S_OK;
}

HRESULT
CMidi2BluetoothMidiBiDi::Cleanup()
{
    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    // TODO: Remove the BiDi pointer from the endpoint device table

    m_Callback = nullptr;
    m_Context = 0;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2BluetoothMidiBiDi::SendMidiMessage(
    PVOID Message,
    UINT Size,
    LONGLONG /*Position*/
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, Message);

    if (Size < sizeof(uint32_t))
    {
        // TODO log that data was smaller than minimum UMP size
        return E_FAIL;
    }

    //m_Callback->Callback(Message, Size, Position, m_Context);

    return S_OK;

}

_Use_decl_annotations_
HRESULT
CMidi2BluetoothMidiBiDi::Callback(
    PVOID /*Message*/,
    UINT /*Size*/,
    LONGLONG /*Position*/,
    LONGLONG /*Context*/
)
{
    //return E_NOTIMPL;

    return S_OK;
}

