#include "pch.h"
#include "MidiMessageReader.h"
#include "MidiMessageReader.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    winrt::Microsoft::Devices::Midi2::MidiMessageReaderEndOfMessagesBehavior MidiMessageReader::EndOfMessagesBehavior()
    {
        throw hresult_not_implemented();
    }
    void MidiMessageReader::EndOfMessagesBehavior(winrt::Microsoft::Devices::Midi2::MidiMessageReaderEndOfMessagesBehavior const& value)
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageReader::EndOfMessages()
    {
        throw hresult_not_implemented();
    }
    uint64_t MidiMessageReader::PeekNextTimestamp()
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::MidiMessageType MidiMessageReader::PeekNextUmpMessageType()
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::MidiUmpSize MidiMessageReader::PeekNextUmpSize()
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp MidiMessageReader::PeekNextMessage()
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestampList MidiMessageReader::ReadToEnd()
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp MidiMessageReader::ReadNextMessage()
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::MidiUmp32 MidiMessageReader::ReadUmp32()
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::MidiUmp64 MidiMessageReader::ReadUmp64()
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::MidiUmp96 MidiMessageReader::ReadUmp96()
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::MidiUmp128 MidiMessageReader::ReadUmp128()
    {
        throw hresult_not_implemented();
    }
    uint32_t MidiMessageReader::ReadTimestampedUmpStructsIntoBuffer(winrt::Windows::Foundation::IMemoryBufferReference const& buffer, uint32_t byteOffsetinBuffer, uint32_t maxBytesToRead)
    {
        throw hresult_not_implemented();
    }
}
