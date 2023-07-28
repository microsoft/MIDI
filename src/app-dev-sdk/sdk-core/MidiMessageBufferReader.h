// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiMessageBufferReader.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiMessageBufferReader
    {
        MidiMessageBufferReader() = default;

        static bool ReadWordArray(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t& timestamp, array_view<uint32_t> words, uint32_t arrayOffset, uint32_t wordCount);
        static bool ReadUmp32Words(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t& timestamp, uint32_t& word0);
        static bool ReadUmp64Words(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t& timestamp, uint32_t& word0, uint32_t& word1);
        static bool ReadUmp96Words(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t& timestamp, uint32_t& word0, uint32_t& word1, uint32_t& word2);
        static bool ReadUmp128Words(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t& timestamp, uint32_t& word0, uint32_t& word1, uint32_t& word2, uint32_t& word3);
        static bool ReadIMidiUmp(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t offset);
        static bool ReadUmp32(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, winrt::Windows::Devices::Midi2::MidiUmp32& ump);
        static bool ReadUmp64(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, winrt::Windows::Devices::Midi2::MidiUmp64& ump);
        static bool ReadUmp96(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, winrt::Windows::Devices::Midi2::MidiUmp96& ump);
        static bool ReadUmp128(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, winrt::Windows::Devices::Midi2::MidiUmp128& ump);
        static bool ReadUmp32WordsLL(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t& timestamp, uint64_t& word0);
        static bool ReadUmp64WordsLL(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t& timestamp, uint64_t& word0and1);
        static bool ReadUmp96WordsLL(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t& timestamp, uint64_t& word0and1, uint64_t& word2);
        static bool ReadUmp128WordsLL(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t& timestamp, uint32_t& word0and1, uint32_t& word2and3);
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiMessageBufferReader : MidiMessageBufferReaderT<MidiMessageBufferReader, implementation::MidiMessageBufferReader>
    {
    };
}
