// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "bytestreamToUMP.h"
#include "midi2.BS2UMPtransform.h"

_Use_decl_annotations_
HRESULT
CMidi2BS2UMPMidiTransform::Initialize(
    LPCWSTR Device,
    PTRANSFORMCREATIONPARAMS CreationParams,
    DWORD *,
    IMidiCallback * Callback,
    LONGLONG Context,
    IUnknown* /*MidiDeviceManager*/
)
{
    TraceLoggingWrite(
        MidiBS2UMPTransformTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF(ERROR_UNSUPPORTED_TYPE, CreationParams->DataFormatIn != MidiDataFormat_ByteStream);
    RETURN_HR_IF(ERROR_UNSUPPORTED_TYPE, CreationParams->DataFormatOut != MidiDataFormat_UMP);

    m_Device = Device;
    m_Callback = Callback;
    m_Context = Context;

    m_BS2UMP.outputMIDI2 = false;
    m_BS2UMP.defaultGroup = 0;

    return S_OK;
}

HRESULT
CMidi2BS2UMPMidiTransform::Cleanup()
{
    TraceLoggingWrite(
        MidiBS2UMPTransformTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2BS2UMPMidiTransform::SendMidiMessage(
    PVOID Data,
    UINT Length,
    LONGLONG Position
)
{
    // Send the bytestream byte(s) to the parser
    BYTE *data = (BYTE *)Data;
    for (UINT i = 0; i < Length; i++)
    {
        m_BS2UMP.bytestreamParse(data[i]);
    }

    // retrieve the UMP(s) from the parser
    // and sent it on
    while (m_BS2UMP.availableUMP())
    {
        uint32_t umpMessage[MAXIMUM_LOOPED_DATASIZE / 4];
        UINT umpCount;
        for(umpCount = 0; umpCount < _countof(umpMessage) && m_BS2UMP.availableUMP(); umpCount++)
        {
            umpMessage[umpCount] = m_BS2UMP.readUMP();
        }

        // Note from PMB for Gary: Pretty sure "ump" in this context is just a single UMP word. Some messages like
        // SysEx are larger (64 bit) and so would be two words back-to-back, so umpCount would be 2 or greater.
        if (umpCount > 0)
        {
            // there are 4 bytes per each 32 bit UMP returned by the parser.
            RETURN_IF_FAILED(m_Callback->Callback(&(umpMessage[0]), umpCount * 4, Position, m_Context));
        }
    }

    return S_OK;
}
