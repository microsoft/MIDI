// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiMessageWriter.h"
#include "MidiMessageWriter.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    bool MidiMessageWriter::IsFull()
    {
        throw hresult_not_implemented();
    }
    void MidiMessageWriter::WriteUmpWords32(uint64_t midiTimestamp, uint32_t umpWord1)
    {
        throw hresult_not_implemented();
    }
    void MidiMessageWriter::WriteUmpWords64(uint64_t midiTimestamp, uint32_t umpWord1, uint32_t umpWord2)
    {
        throw hresult_not_implemented();
    }
    void MidiMessageWriter::WriteUmpWords96(uint64_t midiTimestamp, uint32_t umpWord1, uint32_t umpWord2, uint32_t umpWord3)
    {
        throw hresult_not_implemented();
    }
    void MidiMessageWriter::WriteUmpWords128(uint64_t midiTimestamp, uint32_t umpWord1, uint32_t umpWord2, uint32_t umpWord3, uint32_t umpWord4)
    {
        throw hresult_not_implemented();
    }
    void MidiMessageWriter::WriteUmpWords(uint64_t midiTimeStamp, array_view<uint32_t const> words, uint8_t wordCount)
    {
        throw hresult_not_implemented();
    }
    uint32_t MidiMessageWriter::WriteBuffer(winrt::Windows::Foundation::IMemoryBufferReference const& buffer, uint32_t byteOffsetInBuffer, uint32_t maxBytesToWrite)
    {
        throw hresult_not_implemented();
    }
    void MidiMessageWriter::WriteUmp32Struct(winrt::Windows::Devices::Midi2::MidiUmp32 const& ump)
    {
        throw hresult_not_implemented();
    }
    void MidiMessageWriter::WriteUmp64Struct(winrt::Windows::Devices::Midi2::MidiUmp64 const& ump)
    {
        throw hresult_not_implemented();
    }
    void MidiMessageWriter::WriteUmp96Struct(winrt::Windows::Devices::Midi2::MidiUmp96 const& ump)
    {
        throw hresult_not_implemented();
    }
    void MidiMessageWriter::WriteUmp128Struct(winrt::Windows::Devices::Midi2::MidiUmp128 const& ump)
    {
        throw hresult_not_implemented();
    }

    void MidiMessageWriter::WriteUmp32(winrt::Windows::Devices::Midi2::IMidiUmp32 const& ump)
    {
        throw hresult_not_implemented();
    }
    void MidiMessageWriter::WriteUmp64(winrt::Windows::Devices::Midi2::IMidiUmp64 const& ump)
    {
        throw hresult_not_implemented();
    }
    void MidiMessageWriter::WriteUmp96(winrt::Windows::Devices::Midi2::IMidiUmp96 const& ump)
    {
        throw hresult_not_implemented();
    }
    void MidiMessageWriter::WriteUmp128(winrt::Windows::Devices::Midi2::IMidiUmp128 const& ump)
    {
        throw hresult_not_implemented();
    }


}
