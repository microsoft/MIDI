// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

#include "midi2.UMP2BSTransform.h"

_Use_decl_annotations_
HRESULT
CMidi2UMP2BSMidiTransform::Initialize(
    LPCWSTR device,
    PTRANSFORMCREATIONPARAMS creationParams,
    DWORD *,
    IMidiCallback *callback,
    LONGLONG /*context*/,
    IMidiDeviceManager* /*midiDeviceManager*/
)
{
    TraceLoggingWrite(
        MidiUMP2BSTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), creationParams->DataFormatIn != MidiDataFormats_UMP);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), creationParams->DataFormatOut != MidiDataFormats_ByteStream);

    m_Device = device;
    m_Callback = callback;

    return S_OK;
}

HRESULT
CMidi2UMP2BSMidiTransform::Shutdown()
{
    TraceLoggingWrite(
        MidiUMP2BSTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2UMP2BSMidiTransform::SendMidiMessage(
    MessageOptionFlags optionFlags,
    PVOID inputData,
    UINT length,
    LONGLONG position
)
{
#ifdef _DEBUG
    TraceLoggingWrite(
        MidiUMP2BSTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Translating UMP to MIDI 1.0 bytes", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt32(static_cast<uint32_t>(optionFlags), "optionFlags"),
        TraceLoggingHexUInt32Array(static_cast<uint32_t*>(inputData), static_cast<uint16_t>(length/sizeof(uint32_t)), "data"),
        TraceLoggingUInt32(static_cast<uint32_t>(length), "length bytes"),
        TraceLoggingUInt64(static_cast<uint64_t>(position), MIDI_TRACE_EVENT_MESSAGE_TIMESTAMP_FIELD)
    );
#else
    TraceLoggingWrite(
        MidiUMP2BSTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Translating UMP to MIDI 1.0 bytes", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt32(static_cast<uint32_t>(optionFlags), "optionFlags"),
        TraceLoggingPointer(inputData, "data pointer"),
        TraceLoggingUInt32(static_cast<uint32_t>(length), "length bytes"),
        TraceLoggingUInt64(static_cast<uint64_t>(position), MIDI_TRACE_EVENT_MESSAGE_TIMESTAMP_FIELD)
    );
#endif

    // can only transform 1 set of messages at a time
    auto lock = m_SendLock.lock();


    // we can keep this as a local because of how the callback works
    std::vector<BYTE> translatedBytes{};
    translatedBytes.reserve(length);        // as an approximation of output data size, this is reasonable


    // Send the UMP(s) to the parser
    uint32_t *data = (uint32_t *)inputData;
    for (UINT i = 0; i < (length / sizeof(uint32_t)); i++)
    {
        m_UMP2BS.UMPStreamParse(data[i]);

        // retrieve the bytestream message from the parser and add to our translated data
        while (m_UMP2BS.availableBS())
        {
            translatedBytes.push_back(m_UMP2BS.readBS());
        }
    }

    // send the translated version of everything we've received in this call
    if (translatedBytes.size() > 0)
    {
        // For transforms, by convention the context contains the group index.
        auto hr = m_Callback->Callback(
            (MessageOptionFlags)(optionFlags | MessageOptionFlags_ContextContainsGroupIndex),
            static_cast<PVOID>(translatedBytes.data()),
            static_cast<UINT>(translatedBytes.size()),
            position,
            m_UMP2BS.group);

        if (FAILED(hr))
        {
            m_UMP2BS.resetBuffer();
            RETURN_IF_FAILED(hr);
        }
    }

    return S_OK;
}

