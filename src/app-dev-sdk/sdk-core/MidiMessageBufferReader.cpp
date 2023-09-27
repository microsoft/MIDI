// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiMessageBufferReader.h"
#include "MidiMessageBufferReader.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    bool MidiMessageBufferReader::ReadWordArray(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t& timestamp, array_view<uint32_t> words, uint32_t arrayOffset, uint32_t wordCount)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferReader::ReadUmp32Words(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t& timestamp, uint32_t& word0)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferReader::ReadUmp64Words(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t& timestamp, uint32_t& word0, uint32_t& word1)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferReader::ReadUmp96Words(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t& timestamp, uint32_t& word0, uint32_t& word1, uint32_t& word2)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferReader::ReadUmp128Words(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t& timestamp, uint32_t& word0, uint32_t& word1, uint32_t& word2, uint32_t& word3)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferReader::ReadIMidiUmp(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t offset)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferReader::ReadUmp32(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, winrt::Windows::Devices::Midi2::MidiUmp32& ump)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferReader::ReadUmp64(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, winrt::Windows::Devices::Midi2::MidiUmp64& ump)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferReader::ReadUmp96(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, winrt::Windows::Devices::Midi2::MidiUmp96& ump)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferReader::ReadUmp128(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, winrt::Windows::Devices::Midi2::MidiUmp128& ump)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferReader::ReadUmp32WordsLL(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t& timestamp, uint64_t& word0)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferReader::ReadUmp64WordsLL(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t& timestamp, uint64_t& word0and1)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferReader::ReadUmp96WordsLL(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t& timestamp, uint64_t& word0and1, uint64_t& word2)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBufferReader::ReadUmp128WordsLL(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t bufferByteOffset, uint64_t& timestamp, uint32_t& word0and1, uint32_t& word2and3)
    {
        throw hresult_not_implemented();
    }
}
