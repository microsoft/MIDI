// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiEndpointMetadataEndpointListener.h"
#include "MidiEndpointMetadataEndpointListener.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    void MidiEndpointMetadataEndpointListener::Initialize()
    {
        throw hresult_not_implemented();
    }

    void MidiEndpointMetadataEndpointListener::OnEndpointConnectionOpened()
    {
        throw hresult_not_implemented();
    }

    void MidiEndpointMetadataEndpointListener::Cleanup()
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiEndpointMetadataEndpointListener::ProcessIncomingMessage(
        midi2::MidiMessageReceivedEventArgs const& args,
        bool& skipFurtherListeners,
        bool& skipMainMessageReceivedEvent)
    {
        skipFurtherListeners = false;
        skipMainMessageReceivedEvent = false;

        
        if (args.MessageType() == MidiMessageType::Stream128)
        {
            // Handle Device Identity Notification Message

            // Handle Endpoint Name Notification Message

            // Handle Product Instance Id notification Message
        }

    }

}
