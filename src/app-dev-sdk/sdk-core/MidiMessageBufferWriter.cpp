// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiMessageBufferWriter.h"
#include "MidiMessageBufferWriter.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    bool MidiMessageBufferWriter::WriteWordArray(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t timestamp, array_view<uint32_t const> words, uint32_t wordCount)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferWriter::WriteUmp32Words(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t timestamp, uint32_t word0)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferWriter::WriteUmp64Words(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t timestamp, uint32_t word0, uint32_t word1)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferWriter::WriteUmp96Words(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t timestamp, uint32_t word0, uint32_t word1, uint32_t word2)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferWriter::WriteUmp128Words(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t timestamp, uint32_t word0, uint32_t word1, uint32_t word2, uint32_t word3)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferWriter::WriteIMidiUmp(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t offset)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferWriter::WriteUmp32(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, winrt::Windows::Devices::Midi2::MidiUmp32 const& ump)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferWriter::WriteUmp64(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, winrt::Windows::Devices::Midi2::MidiUmp64 const& ump)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferWriter::WriteUmp96(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, winrt::Windows::Devices::Midi2::MidiUmp96 const& ump)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferWriter::WriteUmp128(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, winrt::Windows::Devices::Midi2::MidiUmp128 const& ump)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferWriter::WriteUmp32WordsLL(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t timestamp, uint64_t word0)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferWriter::WriteUmp64WordsLL(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t timestamp, uint64_t word0and1)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferWriter::WriteUmp96WordsLL(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t timestamp, uint64_t word0and1, uint64_t word2)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferWriter::WriteUmp128WordsLL(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t timestamp, uint32_t word0and1, uint32_t word2and3)
    {
        throw hresult_not_implemented();
    }
}
