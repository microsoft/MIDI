// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiMessageTypeEndpointListener.h"
#include "ClientPlugins.MidiMessageTypeEndpointListener.g.cpp"

namespace winrt::Windows::Devices::Midi2::ClientPlugins::implementation
{
    _Use_decl_annotations_
    void MidiMessageTypeEndpointListener::Initialize(midi2::IMidiEndpointConnectionSource const& endpointConnection)
    {       
        m_endpointConnection = endpointConnection.as<midi2::MidiEndpointConnection>();
    }

    void MidiMessageTypeEndpointListener::OnEndpointConnectionOpened()
    {
    }

    void MidiMessageTypeEndpointListener::Cleanup()
    {
        //       m_endpointConnection = nullptr;
        m_messageReceivedEvent.clear();
    }

    _Use_decl_annotations_
    void MidiMessageTypeEndpointListener::ProcessIncomingMessage(
        midi2::MidiMessageReceivedEventArgs const& args,
        bool& skipFurtherListeners, 
        bool& skipMainMessageReceivedEvent)
    {
        try
        {
            auto messageMessageType = args.MessageType();

            for (auto const& messageType : m_includedMessageTypes)
            {
                if (messageMessageType == messageType)
                {
                    // only skip if we actually processed the message

                    skipFurtherListeners = m_preventCallingFurtherListeners;
                    skipMainMessageReceivedEvent = skipMainMessageReceivedEvent || m_preventFiringMainMessageReceivedEvent;

                    if (m_messageReceivedEvent)
                    {
                        m_messageReceivedEvent((midi2::IMidiMessageReceivedEventSource)*this, args);
                    }

                    break;
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
