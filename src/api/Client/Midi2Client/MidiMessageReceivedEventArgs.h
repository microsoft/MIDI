// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

#include <pch.h>

#include "MidiMessageReceivedEventArgs.g.h"

#include "MidiUmp32.h"
#include "MidiUmp64.h"
#include "MidiUmp96.h"
#include "MidiUmp128.h"

// TODO: Consider making this deferrable
// https://learn.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/author-events


namespace winrt::Windows::Devices::Midi2::implementation
{
    // Getxxx methods return something new
    // Fill methods put data into existing data structures

    struct MidiMessageReceivedEventArgs : MidiMessageReceivedEventArgsT<MidiMessageReceivedEventArgs>
    {
        MidiMessageReceivedEventArgs() = default;

        // internal implementation constructor
        MidiMessageReceivedEventArgs(_In_ PVOID data, _In_ UINT sizeInBytes, _In_ internal::MidiTimestamp);

        ::winrt::Windows::Devices::Midi2::MidiUmpPacketType UmpType() const noexcept;

        ::winrt::Windows::Devices::Midi2::MidiUmpMessageType UmpMessageType() const noexcept;

        uint32_t InspectFirstWord() const noexcept { return m_data.Word0; }

        internal::MidiTimestamp Timestamp() const noexcept { return m_timestamp; }

        ::winrt::Windows::Devices::Midi2::IMidiUmp GetUmp();

        uint8_t FillWords(
            _Inout_ uint32_t& word0, 
            _Inout_ uint32_t& word1, 
            _Inout_ uint32_t& word2, 
            _Inout_ uint32_t& word3);

        bool FillUmp32( _In_ ::winrt::Windows::Devices::Midi2::MidiUmp32 const& ump);
        bool FillUmp64( _In_ ::winrt::Windows::Devices::Midi2::MidiUmp64 const& ump);
        bool FillUmp96( _In_ ::winrt::Windows::Devices::Midi2::MidiUmp96 const& ump);
        bool FillUmp128(_In_ ::winrt::Windows::Devices::Midi2::MidiUmp128 const& ump);
        
        uint8_t FillWordArray(
            _In_ array_view<uint32_t> words, 
            _In_ uint32_t const startIndex);

        uint8_t FillByteArray(
            _In_ array_view<uint8_t> bytes, 
            _In_ uint32_t const startIndex);
        
        uint8_t FillBuffer(
            _In_ foundation::IMemoryBuffer const& buffer, 
            _In_ uint32_t const byteOffset);


    private:
        uint8_t GetValidMessageWordCount() { return internal::GetUmpLengthInMidiWordsFromFirstWord(m_data.Word0); }

        internal::MidiTimestamp m_timestamp{ 0 };

        // this does mean each event argument is up to 12 bytes larger than it needs to be, but this
        // is more efficient speed-wise, and avoids additional heap allocations.


        internal::PackedMaxInternalUmpStorage m_data{};

    };
}

