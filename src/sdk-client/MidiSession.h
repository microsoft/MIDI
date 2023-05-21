// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiSession.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiSession : MidiSessionT<MidiSession>
    {
        MidiSession() = default;

        static winrt::Microsoft::Devices::Midi2::MidiSession CreateNewSession(hstring const& sessionName);
        bool IsOpen();
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Devices::Midi2::MidiEndpoint> ConnectedEndpoints();
        winrt::Microsoft::Devices::Midi2::MidiEndpoint ConnectToEndpoint(hstring const& midiEndpointId, bool routeIncomingMessagesToSession, winrt::Microsoft::Devices::Midi2::MidiEndpointConnectOptions const& options);
        void DisconnectFromEndpoint(hstring const& midiEndpointId);
        void Close();
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiSession : MidiSessionT<MidiSession, implementation::MidiSession>
    {
    };
}
