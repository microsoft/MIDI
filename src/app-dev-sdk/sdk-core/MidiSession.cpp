// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"

#include "MidiSession.h"
#include "MidiSession.g.cpp"

namespace Shared = Microsoft::Devices::Midi2::Internal::Shared;

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    winrt::Microsoft::Devices::Midi2::MidiSession MidiSession::CreateNewSession(hstring const& sessionName, winrt::Microsoft::Devices::Midi2::MidiSessionSettings const& settings)
    {
        winrt::com_ptr<MidiSession> sessionImpl = winrt::make_self<MidiSession>();

        // TODO: Capture pid, app title, etc. to send to service

        // TODO: Call APIs to create session, and handle any other construction/settings/etc.

        sessionImpl->_connectedEndpoints = winrt::single_threaded_observable_vector<winrt::Microsoft::Devices::Midi2::MidiEndpointConnection>();

        // API calls succeeded so call it good and return it
        sessionImpl->_isOpen = true;

        // return the projected instance
        return (winrt::Microsoft::Devices::Midi2::MidiSession)(*sessionImpl);
    }

    bool MidiSession::IsOpen()
    {
        return _isOpen;
    }


    winrt::Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Devices::Midi2::MidiEndpointConnection> MidiSession::Connections()
    {
        return _connectedEndpoints;
    }

    winrt::Microsoft::Devices::Midi2::MidiEndpointConnection MidiSession::ConnectToEndpoint(hstring const& midiEndpointId, bool routeIncomingMessagesToSession, winrt::Microsoft::Devices::Midi2::MidiEndpointConnectOptions const& options)
    {
        throw hresult_not_implemented();
    }


    void MidiSession::DisconnectFromEndpoint(hstring const& midiEndpointId)
    {
        throw hresult_not_implemented();
    }
    void MidiSession::Close()
    {
        // TODO: Call API to close the session and disconnect from the service and disconnect all endpoints

        _isOpen = false;
    }




    uint64_t MidiSession::GetMidiTimestamp()
    {
        return Shared::GetCurrentMidiTimestamp();
    }
    uint64_t MidiSession::GetMidiTimestampFrequency()
    {
        return Shared::GetMidiTimestampFrequency();
    }


    winrt::event_token MidiSession::MessagesReceived(winrt::Windows::Foundation::EventHandler<winrt::Microsoft::Devices::Midi2::MidiMessagesReceivedEventArgs> const& handler)
    {
        throw hresult_not_implemented();
    }
    void MidiSession::MessagesReceived(winrt::event_token const& token) noexcept
    {
        throw hresult_not_implemented();
    }

}
