// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiGroupEndpointListener.h"
#include "ClientPlugins.MidiGroupEndpointListener.g.cpp"


namespace winrt::Microsoft::Windows::Devices::Midi2::ClientPlugins::implementation
{

    _Use_decl_annotations_
    void MidiGroupEndpointListener::Initialize(midi2::IMidiEndpointConnectionSource const& endpointConnection)
    {        
        m_endpointConnection = endpointConnection.as<midi2::MidiEndpointConnection>();
    }

    void MidiGroupEndpointListener::OnEndpointConnectionOpened()
    {        
        
    }

    void MidiGroupEndpointListener::Cleanup()
    {        
//       m_endpointConnection = nullptr;
        m_messageReceivedEvent.clear();
    }

    _Use_decl_annotations_
    void MidiGroupEndpointListener::ProcessIncomingMessage(
        midi2::MidiMessageReceivedEventArgs const& args,
        bool& skipFurtherListeners, 
        bool& skipMainMessageReceivedEvent)
    {
        skipFurtherListeners = m_preventCallingFurtherListeners;
        skipMainMessageReceivedEvent = m_preventFiringMainMessageReceivedEvent;

        if (internal::MessageTypeHasGroupField((uint8_t)args.MessageType()))
        {
            uint32_t word0 = args.PeekFirstWord();

            uint8_t messageGroup = internal::GetGroupIndexFromFirstWord(word0);

            // check the channel against our list of channels
            for (auto const& group : m_includedGroups)
            {
                if (group.Index() == messageGroup)
                {
                    // found it. Fire off the event and leave
                    // events are synchronous, so the chain of calls here needs to be short

                    if (m_messageReceivedEvent)
                    {
                        m_messageReceivedEvent((midi2::IMidiMessageReceivedEventSource)*this, args);
                    }

                    break;
                }
            }

        }
    }
}
