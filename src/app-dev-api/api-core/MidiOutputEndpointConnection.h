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
#include "midi_service_interface.h"
#include "InternalMidiDeviceConnection.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiOutputEndpointConnection : MidiOutputEndpointConnectionT<MidiOutputEndpointConnection, Windows::Devices::Midi2::implementation::MidiEndpointConnection>
    {
        MidiOutputEndpointConnection() = default;
        static hstring GetDeviceSelectorForOutput() { return L""; /* TODO */ }

        uint32_t SendBuffer(internal::MidiTimestamp timestamp, winrt::Windows::Foundation::IMemoryBuffer const& midiData, uint32_t byteOffset, uint32_t length);
        bool SendUmp(winrt::Windows::Devices::Midi2::IMidiUmp const& ump);
        bool SendWords(uint64_t timestamp, array_view<uint32_t const> words, uint32_t wordCount);

    
        bool Start(::Windows::Devices::Midi2::Internal::InternalMidiDeviceConnection* connection);

    private:

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiOutputEndpointConnection : MidiOutputEndpointConnectionT<MidiOutputEndpointConnection, implementation::MidiOutputEndpointConnection>
    {
    };
}
