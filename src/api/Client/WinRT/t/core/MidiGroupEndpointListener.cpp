// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiGroupEndpointListener.h"
#include "ClientPlugins.MidiGroupEndpointListener.g.cpp"


namespace winrt::Windows::Devices::Midi2::ClientPlugins::implementation
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
        try
        {
            if (internal::MessageTypeHasGroupField((uint8_t)args.MessageType()))
            {
                uint32_t word0 = args.PeekFirstWord();

                uint8_t messageGroup = internal::GetGroupIndexFromFirstWord(word0);

                // check the channel against our list of channels
                for (auto const& group : m_includedGroups)
                {
                    if (group.Index() == messageGroup)
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
