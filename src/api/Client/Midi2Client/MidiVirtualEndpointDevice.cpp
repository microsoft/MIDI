// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiVirtualEndpointDevice.h"
#include "MidiVirtualEndpointDevice.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{


    _Use_decl_annotations_
    void MidiVirtualEndpointDevice::UpdateFunctionBlock(midi2::MidiFunctionBlock const& block) noexcept
    {
        UNREFERENCED_PARAMETER(block);

        //throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiVirtualEndpointDevice::UpdateEndpointName(winrt::hstring const& name) noexcept
    {
        UNREFERENCED_PARAMETER(name);

        //throw hresult_not_implemented();
    }


    _Use_decl_annotations_
    void MidiVirtualEndpointDevice::Initialize(midi2::IMidiEndpointConnectionSource const& endpointConnection) noexcept
    {
        m_endpointConnection = endpointConnection.as<midi2::MidiEndpointConnection>();
    }

    void MidiVirtualEndpointDevice::OnEndpointConnectionOpened() noexcept
    {
        //throw hresult_not_implemented();
    }

    void MidiVirtualEndpointDevice::Cleanup() noexcept
    {
        //throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiVirtualEndpointDevice::ProcessIncomingMessage(
        midi2::MidiMessageReceivedEventArgs const& /*args*/,
        bool& /*skipFurtherListeners*/,
        bool& /*skipMainMessageReceivedEvent*/)  noexcept
    {
        //throw hresult_not_implemented();
    }
}
