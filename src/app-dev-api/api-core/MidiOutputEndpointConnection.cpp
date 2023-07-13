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
    bool MidiOutputEndpointConnection::SendUmp(winrt::Windows::Devices::Midi2::IMidiUmp const& ump)
    {
        throw hresult_not_implemented();
    }

    bool MidiOutputEndpointConnection::SendWords(uint64_t timestamp, array_view<uint32_t const> words, uint32_t wordCount)
    {
        //try
        //{
        //    if (_endpoint)
        //    {
        //        auto umpDataSize = (uint32_t)sizeof(internal::PackedUmp32);

        //        _bidiEndpoint->SendMidiMessage((void*)&word0, umpDataSize, timestamp);

        //        return true;
        //    }
        //    else
        //    {
        //        std::cout << __FUNCTION__ << " _bidiEndpoint is nullptr" << std::endl;

        //        return false;
        //    }
        //}
        //catch (winrt::hresult_error const& ex)
        //{
        //    std::cout << __FUNCTION__ << " hresult exception sending message" << std::endl;
        //    std::cout << "HRESULT: 0x" << std::hex << (uint32_t)(ex.code()) << std::endl;
        //    std::cout << "Message: " << winrt::to_string(ex.message()) << std::endl;

        //    return false;
        //}

        return false;
    }
}
