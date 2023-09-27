// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiMessageBufferWriter.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiMessageBufferWriter
    {
        MidiMessageBufferWriter() = default;

        static bool WriteWordArray(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t timestamp, array_view<uint32_t const> words, uint32_t wordCount);
        static bool WriteUmp32Words(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t timestamp, uint32_t word0);
        static bool WriteUmp64Words(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t timestamp, uint32_t word0, uint32_t word1);
        static bool WriteUmp96Words(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t timestamp, uint32_t word0, uint32_t word1, uint32_t word2);
        static bool WriteUmp128Words(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t timestamp, uint32_t word0, uint32_t word1, uint32_t word2, uint32_t word3);
        static bool WriteIMidiUmp(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t offset);
        static bool WriteUmp32(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, winrt::Windows::Devices::Midi2::MidiUmp32 const& ump);
        static bool WriteUmp64(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, winrt::Windows::Devices::Midi2::MidiUmp64 const& ump);
        static bool WriteUmp96(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, winrt::Windows::Devices::Midi2::MidiUmp96 const& ump);
        static bool WriteUmp128(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, winrt::Windows::Devices::Midi2::MidiUmp128 const& ump);
        static bool WriteUmp32WordsLL(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t timestamp, uint64_t word0);
        static bool WriteUmp64WordsLL(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t timestamp, uint64_t word0and1);
        static bool WriteUmp96WordsLL(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t timestamp, uint64_t word0and1, uint64_t word2);
        static bool WriteUmp128WordsLL(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t timestamp, uint32_t word0and1, uint32_t word2and3);
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiMessageBufferWriter : MidiMessageBufferWriterT<MidiMessageBufferWriter, implementation::MidiMessageBufferWriter>
    {
    };
}
