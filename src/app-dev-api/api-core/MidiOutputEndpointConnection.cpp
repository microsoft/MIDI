// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiOutputEndpointConnection.h"
#include "MidiOutputEndpointConnection.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{

    bool MidiOutputEndpointConnection::Start(::Windows::Devices::Midi2::Internal::InternalMidiDeviceConnection* connection)
    {
        return false;
    }

    uint32_t MidiOutputEndpointConnection::SendBuffer(internal::MidiTimestamp, winrt::Windows::Foundation::IMemoryBuffer const& midiData, uint32_t byteOffset, uint32_t length)
    {
        throw hresult_not_implemented();
    }
    void MidiOutputEndpointConnection::SendUmp(winrt::Windows::Devices::Midi2::IMidiUmp const& ump)
    {
        throw hresult_not_implemented();
    }
}
