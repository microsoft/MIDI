// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

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
        winrt::Microsoft::Devices::Midi2::UmpWithTimestamp PeekNextMessage();
        winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::UmpWithTimestamp> ReadToEnd();
        winrt::Microsoft::Devices::Midi2::UmpWithTimestamp ReadNextMessage();
    };
}
