// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiSession.h"
#include "MidiSession.g.cpp"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    winrt::Microsoft::Devices::Midi2::MidiSession MidiSession::CreateNewSession(hstring const& sessionName)
    {
        throw hresult_not_implemented();
    }
    bool MidiSession::IsOpen()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Devices::Midi2::MidiEndpoint> MidiSession::ConnectedEndpoints()
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::MidiEndpoint MidiSession::ConnectToEndpoint(hstring const& midiEndpointId, bool routeIncomingMessagesToSession, winrt::Microsoft::Devices::Midi2::MidiEndpointConnectOptions const& options)
    {
        throw hresult_not_implemented();
    }
    void MidiSession::DisconnectFromEndpoint(hstring const& midiEndpointId)
    {
        throw hresult_not_implemented();
    }
    void MidiSession::Close()
    {
        throw hresult_not_implemented();
    }
}
