// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiChannelEndpointListener.h"
#include "ClientPlugins.MidiChannelEndpointListener.g.cpp"


namespace winrt::Windows::Devices::Midi2::ClientPlugins::implementation
{
    _Use_decl_annotations_
    void MidiChannelEndpointListener::Initialize(midi2::IMidiEndpointConnectionSource const& endpointConnection)
    {
        m_endpointConnection = endpointConnection.as<midi2::MidiEndpointConnection>();
    }

    void MidiChannelEndpointListener::OnEndpointConnectionOpened()
    {
        // Nothing special to do when connection is opened, so all good
    }

    void MidiChannelEndpointListener::Cleanup()
    {
 //       m_endpointConnection = nullptr;
        m_messageReceivedEvent.clear();
    }

    _Use_decl_annotations_
    void MidiChannelEndpointListener::ProcessIncomingMessage(
        midi2::MidiMessageReceivedEventArgs const& args,
        bool& skipFurtherListeners, 
        bool& skipMainMessageReceivedEvent)
    {
        try
        {
            uint32_t word0 = args.PeekFirstWord();

            // check the group. If the group is not specified, we listen to all groups, but for a specific channel
            // if the message type doesn't have a group field, we're not interested in it at all
            if (internal::MessageTypeHasGroupField((uint8_t)args.MessageType()) && 
                (m_includedGroup == nullptr || internal::GetGroupIndexFromFirstWord(word0) == m_includedGroup.Index()))
            {
                if (args.MessageType() == MidiMessageType::SystemCommon32 && m_includeSystemCommonAndRealTimeMessages)
                {
                    skipFurtherListeners = m_preventCallingFurtherListeners;
                    skipMainMessageReceivedEvent = skipMainMessageReceivedEvent || m_preventFiringMainMessageReceivedEvent;

                    // Fire off the event and leave
                    // events are synchronous, so the chain of calls here needs to be short

                    if (m_messageReceivedEvent)
                    {
                        m_messageReceivedEvent((midi2::IMidiMessageReceivedEventSource)*this, args);
                    }
                }
                else if (internal::MessageTypeHasChannelField((uint8_t)args.MessageType()))
                {
                    uint8_t messageChannel = internal::GetChannelIndexFromFirstWord(word0);

                    // check the channel against our list of channels
                    for (auto const& channel : m_includedChannels)
                    {
                        if (channel.Index() == messageChannel)
                        {
                            // only skip if we actually processed the message

                            skipFurtherListeners = m_preventCallingFurtherListeners;
                            skipMainMessageReceivedEvent = skipMainMessageReceivedEvent || m_preventFiringMainMessageReceivedEvent;

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
            else
            {
                // not interested in this group, or the message is a stream message or other type without any group
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error processing incoming message in client plugin.");
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception processing incoming message in client plugin.");
        }
    }
}
