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

    _Success_(return == true)
    bool MidiOutputEndpointConnection::InternalStart()
    {
        throw hresult_not_implemented();
    }


    _Success_(return == true)
    bool MidiOutputEndpointConnection::SendUmpBuffer(
        _In_ internal::MidiTimestamp /* timestamp*/,
        _In_ winrt::Windows::Foundation::IMemoryBuffer const& /* buffer*/,
        _In_ uint32_t /*byteOffset*/ ,
        _In_ uint32_t /*byteLength*/ )
    {
        throw hresult_not_implemented();
    }


    _Success_(return == true)
    bool MidiOutputEndpointConnection::SendUmp(
        _In_ winrt::Windows::Devices::Midi2::IMidiUmp const& /*ump*/)
    {
        throw hresult_not_implemented();
    }

    _Success_(return == true)
    bool MidiOutputEndpointConnection::SendUmpWords(
        _In_ internal::MidiTimestamp /*timestamp*/,
        _In_ uint32_t /*word0*/)
    {
        throw hresult_not_implemented();
    }

    _Success_(return == true)
    bool MidiOutputEndpointConnection::SendUmpWords(
        _In_ internal::MidiTimestamp /*timestamp*/,
        _In_ uint32_t /*word0*/,
        _In_ uint32_t /*word1*/)
    {
        throw hresult_not_implemented();
    }

    _Success_(return == true)
    bool MidiOutputEndpointConnection::SendUmpWords(
        _In_ internal::MidiTimestamp /*timestamp*/,
        _In_ uint32_t /*word0*/,
        _In_ uint32_t /*word1*/,
        _In_ uint32_t /*word2*/)
    {
        throw hresult_not_implemented();
    }

    _Success_(return == true)
    bool MidiOutputEndpointConnection::SendUmpWords(
        _In_ internal::MidiTimestamp /*timestamp*/,
        _In_ uint32_t /*word0*/,
        _In_ uint32_t /*word1*/,
        _In_ uint32_t /*word2*/,
        _In_ uint32_t /*word3*/)
    {
        throw hresult_not_implemented();
    }

    _Success_(return == true)
    bool MidiOutputEndpointConnection::SendUmpWordArray(
        _In_ internal::MidiTimestamp /*timestamp*/,
        _In_ array_view<uint32_t const> /*words*/,
        _In_ uint32_t /*wordCount*/)
    {
        throw hresult_not_implemented();
    }


}
