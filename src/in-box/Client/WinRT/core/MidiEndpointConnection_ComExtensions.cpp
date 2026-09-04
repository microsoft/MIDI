// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiEndpointConnection.h"

#include "ump_iterator.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    // This is also shared with the WinRT interface
    UINT32 MidiEndpointConnection::GetSupportedMaxMidiWordsPerTransmission()
    {
        // the define is for bytes. Convert to MIDI words and return
        return MAXIMUM_LOOPED_UMP_DATASIZE / sizeof(UINT32);
    }

    _Use_decl_annotations_
    BOOL MidiEndpointConnection::ValidateBufferHasOnlyCompleteUmps(
        UINT32 const wordCount,
        UINT32 const* messages
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

        // The iterator wants a mutable pointer but only reads, so the caller's buffer is safe.
        return internal::ValidateBufferHasCompleteUmps(const_cast<UINT32*>(messages), wordCount);
    }

    // this just assumes that messages have been validated in some way
    // before they are sent
    _Use_decl_annotations_
    HRESULT
    MidiEndpointConnection::SendMidiMessagesRaw(
        UINT64 const timestamp,
        UINT32 const wordCount,
        UINT32 const* completeMessages
    )
    {
        RETURN_HR_IF_NULL(E_FAIL, m_endpointTransport);
        RETURN_HR_IF(E_INVALIDARG, wordCount > GetSupportedMaxMidiWordsPerTransmission());

        MessageOptionFlags flags;

        if (m_connectionSettings.WaitForEndpointReceiptOnSend())
        {
            flags = MessageOptionFlags::MessageOptionFlags_WaitForSendComplete;
        }
        else
        {
            flags = MessageOptionFlags::MessageOptionFlags_None;
        }

        // send it

        // The transport takes PVOID but copies the buffer out, so it does not write to it.
        return m_endpointTransport->SendMidiMessage(
            flags,
            static_cast<PVOID>(const_cast<UINT32*>(completeMessages)),
            wordCount * sizeof(UINT32),
            timestamp);

    }

    _Use_decl_annotations_
    HRESULT
    MidiEndpointConnection::SetMessagesReceivedCallback(
        IMidiEndpointConnectionMessagesReceivedCallback* const messagesReceivedCallback
    )
    {
        std::lock_guard<std::mutex> guard(m_comCallbackLock);

        TraceLoggingWrite(
            Midi2SdkTelemetryProvider::Provider(),
            MIDI_SDK_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
            TraceLoggingPointer(messagesReceivedCallback, "Callback")
        );

        RETURN_HR_IF_NULL(E_INVALIDARG, messagesReceivedCallback);

        // Attach does not increment the ref count. 
        //m_comCallback.copy_from(messagesReceivedCallback);
        m_comCallback.attach(messagesReceivedCallback);

        return S_OK;
    }


    HRESULT
    MidiEndpointConnection::RemoveMessagesReceivedCallback()
    {
        std::lock_guard<std::mutex> guard(m_comCallbackLock);

        TraceLoggingWrite(
            Midi2SdkTelemetryProvider::Provider(),
            MIDI_SDK_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD)
        );

        // Clear out the callback
        
        if (m_comCallback)
        {
            m_comCallback.detach();
            m_comCallback = nullptr;
        }


        return S_OK;
    }

}
