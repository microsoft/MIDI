// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiGroupEndpointListener.h"
#include "MidiGroupEndpointListener.g.cpp"


namespace MIDI_CPP_NAMESPACE::implementation
{

    _Use_decl_annotations_
    void MidiGroupEndpointListener::Initialize(midi2::IMidiEndpointConnectionSource const& endpointConnection)
    {        
        internal::LogInfo(__FUNCTION__, L"Enter");

        m_endpointConnection = endpointConnection.as<midi2::MidiEndpointConnection>();
    }

    void MidiGroupEndpointListener::OnEndpointConnectionOpened()
    {        
        internal::LogInfo(__FUNCTION__, L"Enter");
    }

    void MidiGroupEndpointListener::Cleanup()
    {        
        internal::LogInfo(__FUNCTION__, L"Enter");
        
//       m_endpointConnection = nullptr;
    }

    _Use_decl_annotations_
    void MidiGroupEndpointListener::ProcessIncomingMessage(
        midi2::MidiMessageReceivedEventArgs const& args,
        bool& skipFurtherListeners, 
        bool& skipMainMessageReceivedEvent)
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

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
                        m_messageReceivedEvent((midi2::IMidiMessageReceivedEventSource)m_endpointConnection, args);
                    }

                    break;
                }
            }

        }
    }
}
