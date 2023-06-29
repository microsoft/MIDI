#include "pch.h"
#include "MidiMessageWriter.h"
#include "MidiMessageWriter.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
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
    void MidiMessageWriter::WriteUmpWithTimestamp(winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp const& ump)
    {
        throw hresult_not_implemented();
    }
    void MidiMessageWriter::WriteMultipleUmpsWithTimestamps(winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestampList const& umpList)
    {
        throw hresult_not_implemented();
    }
    uint32_t MidiMessageWriter::WriteTimestampedUmpStructsFromBuffer(winrt::Windows::Foundation::IMemoryBufferReference const& buffer, uint32_t byteOffsetinBuffer, uint32_t maxBytesToWrite)
    {
        throw hresult_not_implemented();
    }
    void MidiMessageWriter::WriteTimestampedUmp32Struct(winrt::Microsoft::Devices::Midi2::MidiUmp32 const& ump)
    {
        throw hresult_not_implemented();
    }
    void MidiMessageWriter::WriteTimestampedUmp64Struct(winrt::Microsoft::Devices::Midi2::MidiUmp64 const& ump)
    {
        throw hresult_not_implemented();
    }
    void MidiMessageWriter::WriteTimestampedUmp96Struct(winrt::Microsoft::Devices::Midi2::MidiUmp96 const& ump)
    {
        throw hresult_not_implemented();
    }
    void MidiMessageWriter::WriteTimestampedUmp128Struct(winrt::Microsoft::Devices::Midi2::MidiUmp128 const& ump)
    {
        throw hresult_not_implemented();
    }
}
