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
    void MidiEndpointInformationConfigurator::Initialize()
    {
    }

    void MidiEndpointInformationConfigurator::OnEndpointConnectionOpened()
    {
        // TODO: Send out discovery messages
    }

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

}
