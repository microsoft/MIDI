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
    LONGLONG /*Context*/,
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

    m_BS2UMP.outputMIDI2 = false;

    if (IS_VALID_GROUP_INDEX(CreationParams->UmpGroupIndex))
    {
        m_BS2UMP.defaultGroup = (uint8_t) CreationParams->UmpGroupIndex;
    }

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
    // Note: Group number is set in the initialize function
     
    auto data = static_cast<BYTE *>(Data);

    for (UINT i = 0; i < Length; i++)
    {
        // Send the bytestream byte(s) to the parser. The way the parser works, we need
        // to check for valid UMP words after each byte is parsed.
        m_BS2UMP.bytestreamParse(data[i]);

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
                // send the message
                // By context, for the conversion transforms the context contains the group index
                LOG_IF_FAILED(m_Callback->Callback(
                    (PVOID)m_umpMessage, 
                    (UINT)m_umpMessageCurrentWordCount * sizeof(uint32_t), 
                    Position, 
                    m_BS2UMP.defaultGroup));

                m_umpMessageCurrentWordCount = 0;
                m_umpMessage[0] = m_umpMessage[1] = m_umpMessage[2] = m_umpMessage[3] = { 0 };
            }

        }


    }


    return S_OK;
}
