// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include <libmidi2/bytestreamToUMP.h>
#include "midi2.BS2UMPtransform.h"

_Use_decl_annotations_
HRESULT
CMidi2BS2UMPMidiTransform::Initialize(
    LPCWSTR device,
    PTRANSFORMCREATIONPARAMS creationParams,
    DWORD *,
    IMidiCallback * callback,
    LONGLONG /*context*/,
    IMidiDeviceManager* /*midiDeviceManager*/
)
{
    TraceLoggingWrite(
        MidiBS2UMPTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), creationParams->DataFormatIn != MidiDataFormats_ByteStream);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), creationParams->DataFormatOut != MidiDataFormats_UMP);

    m_Device = device;
    m_Callback = callback;

    //m_BS2UMP.outputMIDI2 = false;

    if (IS_VALID_GROUP_INDEX(creationParams->UmpGroupIndex))
    {
        m_BS2UMP.defaultGroup = (uint8_t) creationParams->UmpGroupIndex;
    }

    return S_OK;
}

HRESULT
CMidi2BS2UMPMidiTransform::Shutdown()
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
    PVOID inputData,
    UINT length,
    LONGLONG position
)
{
#ifdef _DEBUG
    TraceLoggingWrite(
        MidiBS2UMPTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Translating MIDI 1.0 message to UMP", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingHexUInt8Array(static_cast<uint8_t*>(inputData), static_cast<uint16_t>(length), "data bytes"),
        TraceLoggingUInt32(static_cast<uint32_t>(length), "length bytes"),
        TraceLoggingUInt64(static_cast<uint64_t>(position), MIDI_TRACE_EVENT_MESSAGE_TIMESTAMP_FIELD)
    );
#else
    TraceLoggingWrite(
        MidiBS2UMPTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Translating MIDI 1.0 message to UMP", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingPointer(inputData, "data pointer"),
        TraceLoggingUInt32(static_cast<uint32_t>(length), "length bytes"),
        TraceLoggingUInt64(static_cast<uint64_t>(position), MIDI_TRACE_EVENT_MESSAGE_TIMESTAMP_FIELD)
    );
#endif


    // Note: Group number is set in the initialize function
     
    auto data = static_cast<BYTE *>(inputData);

    for (UINT i = 0; i < length; i++)
    {
        // Send the bytestream byte(s) to the parser. The way the parser works, we need
        // to check for valid UMP words after each byte is parsed.
        m_BS2UMP.bytestreamParse(data[i]);

        if (i == (length - 1))
        {
            // If this is the last data byte in this buffer, ensure any
            // partially processed sysex data is put into a UMP, but leave
            // bs2ump in the sysex processing state because additional sysex
            // data may still come in.
            m_BS2UMP.dumpSysex7State(false);
        }

        // retrieve the UMP(s) from the parser
        // and send it on if we have a full message
        while (m_BS2UMP.availableUMP())
        {
            m_umpMessageCurrentWordCount++;

            // just to ensure we don't go past the end of the array
            // TODO: we should clear the parser's byte buffer on error
            if (m_umpMessageCurrentWordCount > _countof(m_umpMessage))
            {
                // Do this instead of RETURN_HR_IF to keep static analysis happy that
                // we aren't exceeding m_umpMessage.
                RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_INDEX_OUT_OF_BOUNDS));
            }

            // we need to send only a single message at a time. the library 
            // converts to a stream of words which may be multiple UMPs. So we 
            // need to walk the data

            m_umpMessage[m_umpMessageCurrentWordCount-1] = m_BS2UMP.readUMP();

            auto expectedWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(m_umpMessage[0]);

            // if we have a complete UMP, we send it along
            if (expectedWordCount == m_umpMessageCurrentWordCount)
            {
                //TraceLoggingWrite(
                //    MidiBS2UMPTransformTelemetryProvider::Provider(),
                //    MIDI_TRACE_EVENT_VERBOSE,
                //    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                //    TraceLoggingPointer(this, "this"),
                //    TraceLoggingWideString(L"Translated to", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                //    TraceLoggingHexUInt32Array(static_cast<uint32_t*>(m_umpMessage), static_cast<uint16_t>(m_umpMessageCurrentWordCount), "ump data"),
                //    TraceLoggingUInt32(static_cast<uint32_t>(m_umpMessageCurrentWordCount * sizeof(uint32_t)), "length bytes"),
                //    TraceLoggingUInt64(static_cast<uint64_t>(position), MIDI_TRACE_EVENT_MESSAGE_TIMESTAMP_FIELD)
                //);


                // send the message
                // By context, for the conversion transforms the context contains the group index
                LOG_IF_FAILED(m_Callback->Callback(
                    (PVOID)m_umpMessage, 
                    (UINT)m_umpMessageCurrentWordCount * sizeof(uint32_t), 
                    position, 
                    m_BS2UMP.defaultGroup));

                m_umpMessageCurrentWordCount = 0;
                m_umpMessage[0] = m_umpMessage[1] = m_umpMessage[2] = m_umpMessage[3] = 0;
            }

        }
    }


    return S_OK;
}
