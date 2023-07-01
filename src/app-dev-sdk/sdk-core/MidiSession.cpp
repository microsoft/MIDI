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

#include <midi_timestamp.h>;

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    winrt::Microsoft::Devices::Midi2::MidiSession MidiSession::CreateNewSession(hstring const& sessionName, winrt::Microsoft::Devices::Midi2::MidiSessionSettings const& settings)
    {
        // TODO: Call the service to create the session
        auto session = winrt::make_self<implementation::MidiSession>();

        session->Open();

        return *session;
    }
    void MidiSession::Open()
    {
        _isOpen = true;
    }
    bool MidiSession::IsOpen()
    {
        return _isOpen;
    }
    winrt::Windows::Foundation::Collections::IVectorView<winrt::Microsoft::Devices::Midi2::MidiEndpointConnection> MidiSession::Connections()
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
        return ::Microsoft::Devices::Midi2::Internal::Shared::GetCurrentMidiTimestamp();
    }
    uint64_t MidiSession::GetMidiTimestampFrequency()
    {
        return ::Microsoft::Devices::Midi2::Internal::Shared::GetMidiTimestampFrequency();
    }
    void MidiSession::Close()
    {
        // todo: need to clean up all connnections, disconnect from service, etc.


        _isOpen = false;
    }
}
