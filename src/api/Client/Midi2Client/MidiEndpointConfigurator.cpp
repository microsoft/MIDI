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

        throw hresult_not_implemented();
    }



    _Use_decl_annotations_
    bool MidiEndpointConfigurator::Negotiate()
    {
        throw hresult_not_implemented();


        // see UMP spec page 37 for this process for protocol negotiation
        // the OS here is Device 1

        // send out endpoint discovery asking for endpoint info and stream configuration

        // process incoming endpoint info notification

        // Request configuration based on the connection open options
        // 

    }

    _Use_decl_annotations_
    bool MidiEndpointConfigurator::RequestFunctionBlocks()
    {
        throw hresult_not_implemented();
    }


}
