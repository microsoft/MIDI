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
        throw hresult_not_implemented();
    }


    bool MidiOutputEndpointConnection::SendUmpBuffer(uint64_t timestamp, winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t byteOffset, uint32_t byteLength)
    {
        throw hresult_not_implemented();
    }


    
    bool MidiOutputEndpointConnection::SendUmp(winrt::Windows::Devices::Midi2::IMidiUmp const& ump)
    {
        throw hresult_not_implemented();
    }

    bool MidiOutputEndpointConnection::SendUmp32Words(uint64_t timestamp, uint32_t word0)
    {
        throw hresult_not_implemented();
    }
    bool MidiOutputEndpointConnection::SendUmp64Words(uint64_t timestamp, uint32_t word0, uint32_t word1)
    {
        throw hresult_not_implemented();
    }
    bool MidiOutputEndpointConnection::SendUmp96Words(uint64_t timestamp, uint32_t word0, uint32_t word1, uint32_t word2)
    {
        throw hresult_not_implemented();
    }
    bool MidiOutputEndpointConnection::SendUmp128Words(uint64_t timestamp, uint32_t word0, uint32_t word1, uint32_t word2, uint32_t word3)
    {
        throw hresult_not_implemented();
    }

    bool MidiOutputEndpointConnection::SendUmpWords(uint64_t timestamp, array_view<uint32_t const> words, uint32_t wordCount)
    {
        throw hresult_not_implemented();
    }


}
