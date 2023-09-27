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
    winrt::Windows::Foundation::Collections::IMapView<uint8_t, midi2::MidiFunctionBlock> MidiVirtualDeviceResponder::FunctionBlocks()
    {
        throw hresult_not_implemented();
    }


    _Use_decl_annotations_
    void MidiVirtualDeviceResponder::AddFunctionBlock(midi2::MidiFunctionBlock const& /*block*/)
    {
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
    bool MidiVirtualDeviceResponder::SuppressHandledMessages()
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiVirtualDeviceResponder::SuppressHandledMessages(bool /*value*/)
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
