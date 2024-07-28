// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include <libmidi2/bytestreamToUMP.h>
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
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF(ERROR_UNSUPPORTED_TYPE, CreationParams->DataFormatIn != MidiDataFormat_ByteStream);
    RETURN_HR_IF(ERROR_UNSUPPORTED_TYPE, CreationParams->DataFormatOut != MidiDataFormat_UMP);

    m_Device = Device;
    m_Callback = Callback;
    m_Context = Context;

    m_BS2UMP.outputMIDI2 = false;

    // TODO: This group needs to come from a property, not be set to 0. See GH bug #377
    // This won't impact the aggregated MIDI endpoints but it *will* impact the client-side
    // MIDI 1.0 APIs

    m_BS2UMP.defaultGroup = 0;

    return S_OK;
}

HRESULT
CMidi2BS2UMPMidiTransform::Cleanup()
{
    TraceLoggingWrite(
        MidiBS2UMPTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
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
        uint32_t umpMessage[MAXIMUM_LOOPED_DATASIZE / sizeof(uint32_t)];
        UINT umpCount;
        for(umpCount = 0; umpCount < _countof(umpMessage) && m_BS2UMP.availableUMP(); umpCount++)
        {
            umpMessage[umpCount] = m_BS2UMP.readUMP();
        }

        if (umpCount > 0)
        {
            // TODO: if this fails, it leaves a bunch of data in the m_BS2UMP cache. Needs
            // to be drained if we'll return. So change to log, clear, and then return.
            // Same with the UMP2BS MIDI Transform
            RETURN_IF_FAILED(m_Callback->Callback(&(umpMessage[0]), umpCount * sizeof(uint32_t), Position, m_Context));
        }
    }

    return S_OK;
}
