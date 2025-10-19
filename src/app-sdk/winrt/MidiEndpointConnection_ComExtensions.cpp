// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiEndpointConnection.h"

#include "ump_iterator.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
    UINT32 MidiEndpointConnection::GetSupportedMaxMidiWordsPerTransmission()
    {
        // the define is for bytes. Convert to MIDI words and return
        return MAXIMUM_LOOPED_UMP_DATASIZE / sizeof(UINT32);
    }

    _Use_decl_annotations_
    BOOL MidiEndpointConnection::ValidateBufferHasOnlyCompleteUmps(
        UINT32 wordCount,
        UINT32* messages
        )
    {
        if (messages == nullptr)
        {
            return FALSE;
        }

        if (wordCount == 0)
        {
            return FALSE;
        }

        return internal::ValidateBufferHasCompleteUmps(messages, wordCount);
    }

    // this just assumes that messages have been validated in some way
    // before they are sent
    _Use_decl_annotations_
    HRESULT
    MidiEndpointConnection::SendMidiMessagesRaw(
        UINT64 timestamp,
        UINT32 wordCount,
        UINT32* completeMessages
    )
    {
        RETURN_HR_IF_NULL(E_FAIL, m_endpointTransport);

        return m_endpointTransport->SendMidiMessage(
            MessageOptionFlags::MessageOptionFlags_None, 
            static_cast<PVOID>(completeMessages), 
            wordCount * sizeof(UINT32),
            timestamp);
    }

    _Use_decl_annotations_
    HRESULT
    MidiEndpointConnection::SetMessagesReceivedCallback(
        IMidiEndpointConnectionMessagesReceivedCallback* messagesReceivedCallback
    )
    {
        TraceLoggingWrite(
            Midi2SdkTelemetryProvider::Provider(),
            MIDI_SDK_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
            TraceLoggingPointer(messagesReceivedCallback, "Callback")
        );

        RETURN_HR_IF_NULL(E_INVALIDARG, messagesReceivedCallback);

        // We now own a reference to this callback
        m_comCallback.copy_from(messagesReceivedCallback);

        return S_OK;
    }


    HRESULT
    MidiEndpointConnection::RemoveMessagesReceivedCallback()
    {
        TraceLoggingWrite(
            Midi2SdkTelemetryProvider::Provider(),
            MIDI_SDK_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD)
        );

        // Clear out the callback
        m_comCallback = nullptr;

        return S_OK;
    }

}
