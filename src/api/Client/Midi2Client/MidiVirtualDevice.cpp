// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiVirtualDevice.h"
#include "MidiVirtualDevice.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{

    _Use_decl_annotations_
    bool MidiVirtualDevice::AddFunctionBlock(midi2::MidiFunctionBlock const& block)
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
    void MidiVirtualDevice::UpdateFunctionBlock(midi2::MidiFunctionBlock const& /*block*/)
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiVirtualDevice::RemoveFunctionBlock(uint8_t /*functionBlockNumber*/)
    {
        throw hresult_not_implemented();
    }

    //midi2::MidiEndpointInformation MidiVirtualDeviceResponder::EndpointInformation()
    //{
    //    throw hresult_not_implemented();
    //}

    //_Use_decl_annotations_
    //void MidiVirtualDeviceResponder::EndpointInformation(midi2::MidiEndpointInformation const& /*value*/)
    //{
    //    throw hresult_not_implemented();
    //}


    _Use_decl_annotations_
    void MidiVirtualDevice::Initialize(midi2::IMidiEndpointConnectionSource const& endpointConnection)
    {
        m_endpointConnection = endpointConnection.as<midi2::MidiEndpointConnection>();
    }

    void MidiVirtualDevice::OnEndpointConnectionOpened()
    {
        throw hresult_not_implemented();
    }

    void MidiVirtualDevice::Cleanup()
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiVirtualDevice::ProcessIncomingMessage(
        midi2::MidiMessageReceivedEventArgs const& /*args*/,
        bool& /*skipFurtherListeners*/,
        bool& /*skipMainMessageReceivedEvent*/)
    {
        throw hresult_not_implemented();
    }
}
