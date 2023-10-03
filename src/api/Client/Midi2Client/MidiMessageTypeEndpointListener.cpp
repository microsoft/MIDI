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

namespace winrt::Windows::Devices::Midi2::implementation
{

    void MidiMessageTypeEndpointListener::Initialize()
    {       
    }

    void MidiMessageTypeEndpointListener::OnEndpointConnectionOpened()
    {
    }

    void MidiMessageTypeEndpointListener::Cleanup()
    {
    }

    _Use_decl_annotations_
    void MidiMessageTypeEndpointListener::ProcessIncomingMessage(
        winrt::Windows::Devices::Midi2::MidiMessageReceivedEventArgs const& args,
        bool& skipFurtherListeners, 
        bool& skipMainMessageReceivedEvent)
    {
        skipFurtherListeners = m_preventCallingFurtherListeners;
        skipMainMessageReceivedEvent = m_preventFiringMainMessageReceivedEvent;

        auto messageMessageType = args.UmpMessageType();

        for (auto const& messageType : m_includedMessageTypes)
        {
            if (messageMessageType == messageType)
            {
                if (m_messageReceivedEvent)
                {
                    m_messageReceivedEvent(m_inputConnection, args);
                }

                break;
            }
        }
    }

}
