// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiChannelEndpointListener.h"
#include "MidiChannelEndpointListener.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    void MidiChannelEndpointListener::Initialize()
    {
        // Nothing to do to initialize, so all good
    }

    _Use_decl_annotations_
    void MidiChannelEndpointListener::OnEndpointConnectionOpened()
    {
        // Nothing special to do when connection is opened, so all good
    }

    _Use_decl_annotations_
    void MidiChannelEndpointListener::Cleanup()
    {
        // No cleanup required.
    }

    _Use_decl_annotations_
    void MidiChannelEndpointListener::ProcessIncomingMessage(
        midi2::MidiMessageReceivedEventArgs const& args,
        bool& skipFurtherListeners, 
        bool& skipMainMessageReceivedEvent)
    {
        skipFurtherListeners = m_preventCallingFurtherListeners;
        skipMainMessageReceivedEvent = m_preventFiringMainMessageReceivedEvent;

        if (internal::MessageTypeHasChannelField((uint8_t)args.MessageType()))
        {
            uint32_t word0 = args.InspectFirstWord();

            // check the group. If the group is not specified, we listen to all groups, but for a specific channel
            if (m_includedGroup == nullptr ||
                internal::GetGroupIndexFromFirstWord(word0) == m_includedGroup.Index())
            {
                uint8_t messageChannel = internal::GetChannelIndexFromFirstWord(word0);

                // check the channel against our list of channels
                for (auto const& channel: m_includedChannels)
                {
                    if (channel.Index() == messageChannel)
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
            else
            {
                // we're not interested in this group
            }

        }

    }
}
