// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiMessageTypeEndpointListener.h"
#include "MidiMessageTypeEndpointListener.g.cpp"

namespace MIDI_CPP_NAMESPACE::implementation
{

    _Use_decl_annotations_
    void MidiMessageTypeEndpointListener::Initialize(midi2::IMidiEndpointConnectionSource const& endpointConnection)
    {       
        internal::LogInfo(__FUNCTION__, L"Enter");

        m_endpointConnection = endpointConnection.as<midi2::MidiEndpointConnection>();
    }

    void MidiMessageTypeEndpointListener::OnEndpointConnectionOpened()
    {
        internal::LogInfo(__FUNCTION__, L"Enter");
    }

    void MidiMessageTypeEndpointListener::Cleanup()
    {
        internal::LogInfo(__FUNCTION__, L"Enter");
        //       m_endpointConnection = nullptr;
    }

    _Use_decl_annotations_
    void MidiMessageTypeEndpointListener::ProcessIncomingMessage(
        midi2::MidiMessageReceivedEventArgs const& args,
        bool& skipFurtherListeners, 
        bool& skipMainMessageReceivedEvent)
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        skipFurtherListeners = m_preventCallingFurtherListeners;
        skipMainMessageReceivedEvent = m_preventFiringMainMessageReceivedEvent;

        auto messageMessageType = args.MessageType();

        for (auto const& messageType : m_includedMessageTypes)
        {
            if (messageMessageType == messageType)
            {
                if (m_messageReceivedEvent)
                {
                    m_messageReceivedEvent((midi2::IMidiMessageReceivedEventSource)m_endpointConnection, args);
                }

                break;
            }
        }
    }

}
