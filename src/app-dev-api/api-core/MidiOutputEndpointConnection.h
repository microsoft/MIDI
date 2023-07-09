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

        uint32_t SendBuffer(winrt::Windows::Devices::Midi2::MidiMessageBuffer const& buffer, uint32_t byteOffsetInBuffer, uint32_t maxBytesToSend);

        void TEMPTEST_SendUmp32(winrt::Windows::Devices::Midi2::MidiUmp32 const& ump);


    
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
