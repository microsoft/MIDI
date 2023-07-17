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

    bool MidiOutputEndpointConnection::InternalStart()
    {
        return false;
    }

    bool MidiOutputEndpointConnection::SendUmpBuffer(uint64_t timestamp, winrt::Windows::Foundation::IMemoryBuffer const& buffer)
    {
        // TODO

        return false;
    }

    
    bool MidiOutputEndpointConnection::SendUmp(winrt::Windows::Devices::Midi2::IMidiUmp const& ump)
    {
        throw hresult_not_implemented();
    }

    bool MidiOutputEndpointConnection::SendUmpWords(uint64_t timestamp, array_view<uint32_t const> words, uint32_t wordCount)
    {
        throw hresult_not_implemented();
    }
}
