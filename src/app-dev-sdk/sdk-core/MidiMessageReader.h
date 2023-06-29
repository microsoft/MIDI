#pragma once
#include "MidiMessageReader.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiMessageReader : MidiMessageReaderT<MidiMessageReader>
    {
        MidiMessageReader() = default;

        winrt::Microsoft::Devices::Midi2::MidiMessageReaderNoMoreDataBehavior NoMoreDataBehavior();
        void NoMoreDataBehavior(winrt::Microsoft::Devices::Midi2::MidiMessageReaderNoMoreDataBehavior const& value);
        bool EndOfMessages();
        uint64_t PeekNextTimestamp();
        winrt::Microsoft::Devices::Midi2::MidiMessageType PeekNextMessageType();
        winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp PeekNextMessage();
        winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestampList ReadToEnd();
        winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp ReadNextMessage();
    };
}
