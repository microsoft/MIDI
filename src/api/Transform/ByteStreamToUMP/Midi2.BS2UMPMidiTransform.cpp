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

    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), CreationParams->DataFormatIn != MidiDataFormat_ByteStream);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), CreationParams->DataFormatOut != MidiDataFormat_UMP);

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


        // TODO: We need to change the approach here because
        // the UMP buffer in BS2UMP is quite small. So we need to feed
        // this one byte at a time and check for available words after 
        // each byte.

        // importantly, the bytes that make up a message could span 
        // calls to this function, so we need to retain a running
        // status. This could get more complex if we're dealing
        // with multiple groups worth of data. That would require
        // keeping buffers for each group.
        // 
        // Unfortunately, this function does not have any context 
        // about the group the data is for. That needs to be 
        // fixed before this works for anything but group 0


    }

    // retrieve the UMP(s) from the parser
    // and sent it on
    while (m_BS2UMP.availableUMP())
    {
        uint32_t umpMessage[MAXIMUM_LOOPED_UMP_DATASIZE / sizeof(uint32_t)];

        // we need to send only a single message at a time. the library 
        // converts to a stream of words which may be multiple UMPs. So we 
        // need to walk the data

        umpMessage[0] = m_BS2UMP.readUMP();
        uint8_t messageWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(umpMessage[0]);
        uint8_t messageWordsRead = 1 ;

        if (m_BS2UMP.availableUMP())
        {
            // this starts at 1 because we already read the first word
            for (uint8_t i = 1; i < messageWordCount && i < _countof(umpMessage) && m_BS2UMP.availableUMP(); i++)
            {
                umpMessage[i] = m_BS2UMP.readUMP();
                messageWordsRead++;
            }
        }


        if (messageWordsRead != messageWordCount)
        {
            // we have a problem here. 

            RETURN_IF_FAILED(E_ABORT);
        }
        else if (messageWordCount > 0)
        {
            // TODO: if this fails, it leaves a bunch of data in the m_BS2UMP cache. Needs
            // to be drained if we'll return. So change to log, clear, and then return.
            // Same with the UMP2BS MIDI Transform

            RETURN_IF_FAILED(m_Callback->Callback(&(umpMessage[0]), messageWordCount * sizeof(uint32_t), Position, m_Context));
        }
        else
        {
            // no data to send
        }

    }

    return S_OK;
}
