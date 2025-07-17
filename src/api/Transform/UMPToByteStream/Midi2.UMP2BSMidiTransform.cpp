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
    PVOID inputData,
    UINT length,
    LONGLONG position
)
{
    TraceLoggingWrite(
        MidiUMP2BSTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Translating UMP to MIDI 1.0 bytes", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingHexUInt32Array(static_cast<uint32_t*>(inputData), static_cast<uint16_t>(length/sizeof(uint32_t)), "data"),
        TraceLoggingUInt32(static_cast<uint32_t>(length), "length bytes"),
        TraceLoggingUInt64(static_cast<uint64_t>(position), MIDI_TRACE_EVENT_MESSAGE_TIMESTAMP_FIELD)
    );

    // Send the UMP(s) to the parser
    uint32_t *data = (uint32_t *)inputData;
    for (UINT i = 0; i < (length / sizeof(uint32_t)); i++)
    {
        m_UMP2BS.UMPStreamParse(data[i]);

        // retrieve the bytestream message from the parser
        // and send it on
        while (m_UMP2BS.availableBS())
        {
            UINT messageByteCount{ 0 };

            BYTE byteStream[MAXIMUM_LIBMIDI2_BYTESTREAM_DATASIZE];
            UINT byteIndex;
            for(byteIndex = 0; byteIndex < _countof(byteStream) && m_UMP2BS.availableBS(); byteIndex++)
            {
                messageByteCount++;
                byteStream[byteIndex] = m_UMP2BS.readBS();
            }

            if (messageByteCount > 0)
            {
                TraceLoggingWrite(
                    MidiUMP2BSTransformTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_VERBOSE,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Translated to", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingHexUInt8Array(static_cast<uint8_t*>(byteStream), static_cast<uint16_t>(messageByteCount), "translated data"),
                    TraceLoggingUInt32(static_cast<uint32_t>(messageByteCount), "length bytes"),
                    TraceLoggingUInt64(static_cast<uint64_t>(position), MIDI_TRACE_EVENT_MESSAGE_TIMESTAMP_FIELD)
                );

                // For transforms, by convention the context contains the group index.
                auto hr = m_Callback->Callback(byteStream, messageByteCount, position, m_UMP2BS.group);

                if (FAILED(hr))
                {
                    m_UMP2BS.resetBuffer();
                    RETURN_IF_FAILED(hr);
                }

            }
        }
    }

    // we're done for now
    //m_UMP2BS.resetBuffer();

    return S_OK;
}

