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
    winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi2::MidiUmpMessageType> MidiMessageTypeEndpointListener::IncludeMessageTypes()
    {
        throw hresult_not_implemented();
    }

    void MidiMessageTypeEndpointListener::Initialize()
    {
        throw hresult_not_implemented();
    }

    void MidiMessageTypeEndpointListener::OnEndpointConnectionOpened()
    {
        throw hresult_not_implemented();
    }

    void MidiMessageTypeEndpointListener::Cleanup()
    {
        throw hresult_not_implemented();
    }

    void MidiMessageTypeEndpointListener::ProcessIncomingMessage(
        _In_ winrt::Windows::Devices::Midi2::MidiMessageReceivedEventArgs const& /*args*/,
        _Out_ bool& skipFurtherListeners, 
        _Out_ bool& skipMainMessageReceivedEvent)
    {
        skipFurtherListeners = false;
        skipMainMessageReceivedEvent = false;

        throw hresult_not_implemented();
    }

}
