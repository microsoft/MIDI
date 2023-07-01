// Copyright(c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiSession.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiSession : MidiSessionT<MidiSession>
    {
        MidiSession() = default;

        static winrt::Microsoft::Devices::Midi2::MidiSession CreateNewSession(hstring const& sessionName, winrt::Microsoft::Devices::Midi2::MidiSessionSettings const& settings);
        bool IsOpen();
        winrt::Windows::Foundation::Collections::IVectorView<winrt::Microsoft::Devices::Midi2::MidiEndpointConnection> Connections();
        winrt::Microsoft::Devices::Midi2::MidiEndpointConnection ConnectToEndpoint(hstring const& midiEndpointId, bool routeIncomingMessagesToSession, winrt::Microsoft::Devices::Midi2::MidiEndpointConnectOptions const& options);
        void DisconnectFromEndpoint(hstring const& midiEndpointId);
        uint64_t GetMidiTimestamp();
        uint64_t GetMidiTimestampFrequency();
        void Close();
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiSession : MidiSessionT<MidiSession, implementation::MidiSession>
    {
    };
}
