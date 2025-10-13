// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiEndpointConnection.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
    //UINT MidiEndpointConnection::GetSupportedMaxWordsPerTransmission()
    //{
    //    // TODO: Return whatever constant we're using to gate message count

    //    return MAXIMUM_LOOPED_BUFFER_SIZE / sizeof(UINT32);
    //}

    _Use_decl_annotations_
    HRESULT
    MidiEndpointConnection::SendMidiMessagesRaw(
        UINT32* completeMessages,
        UINT32 wordCount,
        UINT64 timestamp
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
        RETURN_HR_IF_NULL(E_INVALIDARG, messagesReceivedCallback);

        // We now own a reference to this callback
        m_comCallback.copy_from(messagesReceivedCallback);

        return S_OK;
    }


    HRESULT
    MidiEndpointConnection::RemoveMessagesReceivedCallback()
    {
        // Clear out the callback
        m_comCallback = nullptr;

        return S_OK;
    }

}
