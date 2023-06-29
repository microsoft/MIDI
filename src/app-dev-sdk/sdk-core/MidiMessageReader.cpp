#include "pch.h"
#include "MidiMessageReader.h"
#include "MidiMessageReader.g.cpp"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    winrt::Microsoft::Devices::Midi2::MidiMessageReaderNoMoreDataBehavior MidiMessageReader::NoMoreDataBehavior()
    {
        throw hresult_not_implemented();
    }
    void MidiMessageReader::NoMoreDataBehavior(winrt::Microsoft::Devices::Midi2::MidiMessageReaderNoMoreDataBehavior const& value)
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
    winrt::Microsoft::Devices::Midi2::MidiMessageType MidiMessageReader::PeekNextMessageType()
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
}
