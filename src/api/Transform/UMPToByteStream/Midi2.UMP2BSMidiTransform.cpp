// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

#include "midi2.UMP2BSTransform.h"

_Use_decl_annotations_
HRESULT
CMidi2UMP2BSMidiTransform::Initialize(
    LPCWSTR Device,
    PTRANSFORMCREATIONPARAMS CreationParams,
    DWORD *,
    IMidiCallback *Callback,
    LONGLONG Context,
    IUnknown* /*MidiDeviceManager*/
)
{
    TraceLoggingWrite(
        MidiUMP2BSTransformTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    RETURN_HR_IF(ERROR_UNSUPPORTED_TYPE, CreationParams->DataFormatIn != MidiDataFormat_UMP);
    RETURN_HR_IF(ERROR_UNSUPPORTED_TYPE, CreationParams->DataFormatOut != MidiDataFormat_ByteStream);

    m_Device = Device;
    m_Callback = Callback;
    m_Context = Context;

    return S_OK;
}

HRESULT
CMidi2UMP2BSMidiTransform::Cleanup()
{
    TraceLoggingWrite(
        MidiUMP2BSTransformTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2UMP2BSMidiTransform::SendMidiMessage(
    PVOID Data,
    UINT Length,
    LONGLONG Position
)
{
    // Send the UMP(s) to the parser
    uint32_t *data = (uint32_t *)Data;
    for (UINT i = 0; i < (Length / 4); i++)
    {
        m_UMP2BS.UMPStreamParse(data[i]);
    }

    // retrieve the bytestream message from the parser
    // and send it on
    while (m_UMP2BS.availableBS())
    {
        BYTE byteStream[MAXIMUM_LOOPED_DATASIZE];
        UINT byteCount;
        for(byteCount = 0; byteCount < _countof(byteStream) && m_UMP2BS.availableBS(); byteCount++)
        {
            byteStream[byteCount] = m_UMP2BS.readBS();
        }

        if (byteCount > 0)
        {
            RETURN_IF_FAILED(m_Callback->Callback(&(byteStream[0]), byteCount, Position, m_Context));
        }
    }

    return S_OK;
}

