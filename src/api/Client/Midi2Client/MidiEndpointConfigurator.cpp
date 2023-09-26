// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiEndpointConfigurator.h"
#include "MidiEndpointConfigurator.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    void MidiEndpointConfigurator::Initialize()
    {
    }

    _Use_decl_annotations_
    void MidiEndpointConfigurator::OnEndpointConnectionOpened()
    {
        bool infoFoundInCache = false;

        // TODO: do we already have cached info? 

        if (!infoFoundInCache)
        {
            // No cached info. Send out the negotiation requests and go through the full process
            Negotiate();
        }

    }

    _Use_decl_annotations_
    void MidiEndpointConfigurator::Cleanup()
    {
    }



    _Use_decl_annotations_
    void MidiEndpointConfigurator::ProcessIncomingMessage(
        midi2::MidiMessageReceivedEventArgs const& /*args*/,
        bool& skipFurtherListeners,
        bool& skipMainMessageReceivedEvent)
    {
        skipFurtherListeners = false;
        skipMainMessageReceivedEvent = false;






        
    }



    _Use_decl_annotations_
    bool MidiEndpointConfigurator::Negotiate()
    {
        return false;


        // see UMP spec page 37 for this process for protocol negotiation
        // the OS here is Device 1

        // send out endpoint discovery asking for endpoint info and stream configuration

        // process incoming endpoint info notification

        // Request configuration based on the connection open options
        // 

    }

    _Use_decl_annotations_
    bool MidiEndpointConfigurator::RequestAllFunctionBlocks()
    {
        return RequestSingleFunctionBlock(0xFF);       // 0xFF is flag for "all"
    }

    _Use_decl_annotations_
    bool MidiEndpointConfigurator::RequestSingleFunctionBlock(uint8_t functionBlockNumber)
    {
        auto request = MidiStreamMessageBuilder::BuildFunctionBlockDiscoveryMessage(
            MidiClock::GetMidiTimestamp(),
            functionBlockNumber,
            midi2::MidiFunctionBlockDiscoveryFilterFlags::RequestFunctionBlockInformation | midi2::MidiFunctionBlockDiscoveryFilterFlags::RequestFunctionBlockName
        );

        // eating the status here probably isn't all that cool a thing to do
        return m_outputConnection.SendMessagePacket(request) == MidiSendMessageResult::Success;
    }

}
