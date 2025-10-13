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
    MessageOptionFlags optionFlags,
    PVOID inputData,
    UINT length,
    LONGLONG position
)
{
    // can only transform 1 message at a time
    auto lock = m_SendLock.lock();

#ifdef _DEBUG
    TraceLoggingWrite(
        MidiBS2UMPTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Translating MIDI 1.0 message to UMP", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt32(static_cast<uint32_t>(optionFlags), "optionFlags"),
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
        TraceLoggingUInt32(static_cast<uint32_t>(optionFlags), "optionFlags"),
        TraceLoggingPointer(inputData, "data pointer"),
        TraceLoggingUInt32(static_cast<uint32_t>(length), "length bytes"),
        TraceLoggingUInt64(static_cast<uint64_t>(position), MIDI_TRACE_EVENT_MESSAGE_TIMESTAMP_FIELD)
    );
#endif

    // Note: Group number is set in the initialize function
     
    auto data = static_cast<BYTE *>(inputData);


    // we can keep this as a local because of how the callback works
    std::vector<UINT32> translatedWords{};
    translatedWords.reserve(length / 3 + 1);        // as an approximation of output data size, this is reasonable


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

        // retrieve the UMP(s) from the parser and add to the outgoing message
        while (m_BS2UMP.availableUMP())
        {
            translatedWords.push_back(m_BS2UMP.readUMP());
        }
    }

    if (translatedWords.size() > 0)
    {
        // send the message. The context contains the group index
        auto hr = m_Callback->Callback(
            (MessageOptionFlags)(optionFlags | MessageOptionFlags_ContextContainsGroupIndex),
            static_cast<PVOID>(translatedWords.data()),
            static_cast<UINT>(translatedWords.size() * sizeof(UINT32)),
            position,
            m_BS2UMP.defaultGroup);

        if (FAILED(hr))
        {
            m_BS2UMP.resetBuffer();
            RETURN_IF_FAILED(hr);
        }
    }

    return S_OK;
}
