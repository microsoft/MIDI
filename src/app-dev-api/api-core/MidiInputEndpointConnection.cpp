// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiInputEndpointConnection.h"
#include "MidiInputEndpointConnection.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{

    hstring MidiInputEndpointConnection::GetDeviceSelectorForInput()
    {
        // TODO
        return L"";
    }


    IFACEMETHODIMP MidiInputEndpointConnection::Callback(PVOID Data, UINT Size, LONGLONG Position)
    {
        // TODO: process incoming messages / fire event

        // check each reader. For each reader that returns True when queried, fire off the messagesreceived event


        return S_OK;
    }


    winrt::event_token MidiInputEndpointConnection::MessagesReceived(winrt::Windows::Foundation::EventHandler<winrt::Windows::Devices::Midi2::MidiMessagesReceivedEventArgs> const& handler)
    {
        throw hresult_not_implemented();
    }
    void MidiInputEndpointConnection::MessagesReceived(winrt::event_token const& token) noexcept
    {
        throw hresult_not_implemented();
    }

    void MidiInputEndpointConnection::Start()
    {

    }

}
