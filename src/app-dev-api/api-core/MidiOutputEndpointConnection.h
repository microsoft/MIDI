// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiOutputEndpointConnection.g.h"
#include "MidiEndpointConnection.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiOutputEndpointConnection : MidiOutputEndpointConnectionT<MidiOutputEndpointConnection, Windows::Devices::Midi2::implementation::MidiEndpointConnection>
    {
        MidiOutputEndpointConnection() = default;

        static hstring GetDeviceSelectorForOutput();

        winrt::Windows::Devices::Midi2::MidiMessageWriter MessageWriter() { return _messageWriter; }

        void Start();

    private:
        winrt::Windows::Devices::Midi2::MidiMessageWriter _messageWriter = nullptr;

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiOutputEndpointConnection : MidiOutputEndpointConnectionT<MidiOutputEndpointConnection, implementation::MidiOutputEndpointConnection>
    {
    };
}
