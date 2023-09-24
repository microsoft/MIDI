// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiEndpointInformationConfigurator.h"
#include "MidiEndpointInformationConfigurator.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    void MidiEndpointInformationConfigurator::Initialize()
    {
    }

    _Use_decl_annotations_
    void MidiEndpointInformationConfigurator::OnEndpointConnectionOpened()
    {
        RestartDiscoveryAndNegotiation();




    }

    _Use_decl_annotations_
    void MidiEndpointInformationConfigurator::Cleanup()
    {
    }



    _Use_decl_annotations_
    void MidiEndpointInformationConfigurator::ProcessIncomingMessage(
        midi2::MidiMessageReceivedEventArgs const& /*args*/,
        bool& skipFurtherListeners,
        bool& skipMainMessageReceivedEvent)
    {
        skipFurtherListeners = false;
        skipMainMessageReceivedEvent = false;

        throw hresult_not_implemented();
    }



    _Use_decl_annotations_
    void MidiEndpointInformationConfigurator::RestartDiscoveryAndNegotiation()
    {
        throw hresult_not_implemented();


        // see UMP spec page 37 for this process for protocol negotiation
        // the OS here is Device 1

        // send out endpoint discovery asking for endpoint info and stream configuration

        // process incoming 

        // Request configuration based on the conneciton open options
        // 

    }



}
