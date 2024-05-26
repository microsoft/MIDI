// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiMessageReceivedEventArgs.g.h"


// TODO: Consider making this deferrable
// https://learn.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/author-events


namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
    // Getxxx methods return something new
    // Fill methods put data into existing data structures

    struct MidiMessageReceivedEventArgs : MidiMessageReceivedEventArgsT<MidiMessageReceivedEventArgs>
    {
        MidiMessageReceivedEventArgs() = default;

        // internal implementation constructor
        MidiMessageReceivedEventArgs(_In_ PVOID data, _In_ UINT sizeInBytes, _In_ internal::MidiTimestamp);

        midi2::MidiPacketType PacketType() const noexcept;
        midi2::MidiMessageType MessageType() const noexcept;

        uint32_t PeekFirstWord() const noexcept { return m_data.Word0; }

        internal::MidiTimestamp Timestamp() const noexcept { return m_timestamp; }

        midi2::IMidiUniversalPacket GetMessagePacket();

        uint8_t FillWords(
            _Inout_ uint32_t& word0, 
            _Inout_ uint32_t& word1, 
            _Inout_ uint32_t& word2, 
            _Inout_ uint32_t& word3);

        bool FillMessage32( _In_ midi2::MidiMessage32 const& message);
        bool FillMessage64( _In_ midi2::MidiMessage64 const& message);
        bool FillMessage96( _In_ midi2::MidiMessage96 const& message);
        bool FillMessage128(_In_ midi2::MidiMessage128 const& message);
        
        uint8_t FillMessageStruct(_Inout_ midi2::MidiMessageStruct& message);

        // these array_view parameters are quite specific. Reference this:
        // https://devblogs.microsoft.com/oldnewthing/20200205-00/?p=103398/
        
        _Success_(return > 0)
        uint8_t FillWordArray(
            _In_ uint32_t const startIndex,
            _In_ array_view<uint32_t> words
        );

        _Success_(return > 0)
            uint8_t FillByteArray(
            _In_ uint32_t const startIndex,
            _In_ array_view<uint8_t> bytes
        );
        
        _Success_(return > 0)
        uint8_t FillBuffer(
            _In_ uint32_t const byteOffset,
            _In_ foundation::IMemoryBuffer const& buffer
        );

        _Success_(return > 0)
        uint8_t AppendWordsToList(
            _In_ collections::IVector<uint32_t> wordList
        );

    private:
        _Success_(return > 0)
        uint8_t GetValidMessageWordCount() { return internal::GetUmpLengthInMidiWordsFromFirstWord(m_data.Word0); }

        internal::MidiTimestamp m_timestamp{ 0 };

        // this does mean each event argument is up to 12 bytes larger than it needs to be, but this
        // is more efficient speed-wise, and avoids additional heap allocations.

        internal::PackedMaxInternalUmpStorage m_data{};

    };
}

