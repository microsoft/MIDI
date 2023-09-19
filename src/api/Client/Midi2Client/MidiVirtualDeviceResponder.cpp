// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiVirtualDeviceResponder.h"
#include "MidiVirtualDeviceResponder.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{

    _Use_decl_annotations_
    bool MidiVirtualDeviceResponder::AddFunctionBlock(midi2::MidiFunctionBlock const& block)
    {
        // add to list if there isn't already a one in that spot

        if (m_functionBlocks.HasKey(block.Number()))
        {
            // we already have a function block with this number
            return false;
        }
        else
        {
            // add to the list

            m_functionBlocks.Insert(block.Number(), block);
        }


        if (m_enabled)
        {

            // send out function block info messages

            // send out function block name messages

            // TODO: Set up a builder for these types of messages. The multi-part ones will create an array of UMPs given the string
            // and the common parameters


        }


        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiVirtualDeviceResponder::UpdateFunctionBlock(midi2::MidiFunctionBlock const& /*block*/)
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiVirtualDeviceResponder::RemoveFunctionBlock(uint8_t /*functionBlockNumber*/)
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    midi2::MidiEndpointInformation MidiVirtualDeviceResponder::EndpointInformation()
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiVirtualDeviceResponder::EndpointInformation(midi2::MidiEndpointInformation const& /*value*/)
    {
        throw hresult_not_implemented();
    }


    _Use_decl_annotations_
    void MidiVirtualDeviceResponder::Initialize()
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiVirtualDeviceResponder::OnEndpointConnectionOpened()
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiVirtualDeviceResponder::Cleanup()
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiVirtualDeviceResponder::ProcessIncomingMessage(
        midi2::MidiMessageReceivedEventArgs const& /*args*/,
        bool& /*skipFurtherListeners*/,
        bool& /*skipMainMessageReceivedEvent*/)
    {
        throw hresult_not_implemented();
    }
}
