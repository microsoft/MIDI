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
                // TODO: If this fails, it leaves a bunch of stuff in the m_UMP2BS that will get sent next time
                // around. Should likely drain that before moving on

                // For transforms, by convention the context contains the group index.
                RETURN_IF_FAILED(m_Callback->Callback(&(byteStream[0]), messageByteCount, position, m_UMP2BS.group));
            }
        }
    }

    return S_OK;
}

