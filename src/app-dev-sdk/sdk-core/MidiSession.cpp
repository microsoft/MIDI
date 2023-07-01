// Copyright(c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiSession.h"
#include "MidiSession.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    winrt::Microsoft::Devices::Midi2::MidiSession MidiSession::CreateNewSession(hstring const& sessionName, winrt::Microsoft::Devices::Midi2::MidiSessionSettings const& settings)
    {
        // TODO: Call the service to create the session

        return winrt::make<MidiSession>();
    }
    bool MidiSession::IsOpen()
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::MidiEndpointConnectionList MidiSession::Connections()
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::MidiEndpointConnection MidiSession::ConnectToEndpoint(hstring const& midiEndpointId, bool routeIncomingMessagesToSession, winrt::Microsoft::Devices::Midi2::MidiEndpointConnectOptions const& options)
    {
        throw hresult_not_implemented();
    }
    void MidiSession::DisconnectFromEndpoint(hstring const& midiEndpointId)
    {
        throw hresult_not_implemented();
    }
    uint64_t MidiSession::GetMidiTimestamp()
    {
        throw hresult_not_implemented();
    }
    uint64_t MidiSession::GetMidiTimestampFrequency()
    {
        throw hresult_not_implemented();
    }
    void MidiSession::Close()
    {
        throw hresult_not_implemented();
    }
}
