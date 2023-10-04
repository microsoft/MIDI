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


namespace winrt::Windows::Devices::Midi2::implementation
{

    void MidiGroupEndpointListener::Initialize()
    {        
    }

    void MidiGroupEndpointListener::OnEndpointConnectionOpened()
    {        
    }

    void MidiGroupEndpointListener::Cleanup()
    {        
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
                        m_messageReceivedEvent(m_inputConnection, args);
                    }

                    break;
                }
            }

        }
    }
}
