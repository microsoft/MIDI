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
    uint32_t MidiOutputEndpointConnection::SendBuffer(winrt::Windows::Devices::Midi2::MidiMessageBuffer const& buffer, uint32_t byteOffsetInBuffer, uint32_t maxBytesToSend)
    {
        throw hresult_not_implemented();
    }

    bool MidiOutputEndpointConnection::Start(::Windows::Devices::Midi2::Internal::InternalMidiDeviceConnection* connection)
    {
        return false;
    }


    void MidiOutputEndpointConnection::TEMPTEST_SendUmp32(winrt::Windows::Devices::Midi2::MidiUmp32 const& ump)
    {
        throw hresult_not_implemented();
    }
}
