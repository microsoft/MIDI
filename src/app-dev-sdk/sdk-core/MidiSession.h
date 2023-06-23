// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

// this is a shared file in the repo. It defines what we're using for MIDI timestamps
#include <midi_timestamp.h>

#include "MidiSession.g.h"
#include "MidiSessionSettings.h"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiSession : MidiSessionT<MidiSession>
    {
        MidiSession() = default;

        static winrt::Microsoft::Devices::Midi2::MidiSession CreateNewSession(hstring const& sessionName, winrt::Microsoft::Devices::Midi2::MidiSessionSettings const& settings);
        bool IsOpen();
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Devices::Midi2::MidiEndpointConnection> Connections();
        winrt::Microsoft::Devices::Midi2::MidiEndpointConnection ConnectToEndpoint(hstring const& midiEndpointId, bool routeIncomingMessagesToSession, winrt::Microsoft::Devices::Midi2::MidiEndpointConnectOptions const& options);
        void DisconnectFromEndpoint(hstring const& midiEndpointId);
        void Close();

        uint64_t GetMidiTimestamp();
        uint64_t GetMidiTimestampFrequency();

        winrt::event_token MessagesReceived(winrt::Windows::Foundation::EventHandler<winrt::Microsoft::Devices::Midi2::MidiMessagesReceivedEventArgs> const& handler);
        void MessagesReceived(winrt::event_token const& token) noexcept;

    private:
        bool _isOpen = false;
        Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Devices::Midi2::MidiEndpointConnection> _connectedEndpoints;
        

    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiSession : MidiSessionT<MidiSession, implementation::MidiSession>
    {
    };
}
