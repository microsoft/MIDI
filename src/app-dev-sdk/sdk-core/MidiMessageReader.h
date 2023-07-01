// Copyright(c) Microsoft Corporation.
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

        winrt::Microsoft::Devices::Midi2::MidiMessageReaderEndOfMessagesBehavior EndOfMessagesBehavior();
        void EndOfMessagesBehavior(winrt::Microsoft::Devices::Midi2::MidiMessageReaderEndOfMessagesBehavior const& value);
        bool EndOfMessages();
        uint64_t PeekNextTimestamp();
        winrt::Microsoft::Devices::Midi2::MidiUmpMessageType PeekNextUmpMessageType();
        winrt::Microsoft::Devices::Midi2::MidiUmpSize PeekNextUmpSize();
        winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp PeekNextMessage();
        winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestampList ReadToEnd();
        winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp ReadNextMessage();
        winrt::Microsoft::Devices::Midi2::MidiUmp32 ReadUmp32();
        winrt::Microsoft::Devices::Midi2::MidiUmp64 ReadUmp64();
        winrt::Microsoft::Devices::Midi2::MidiUmp96 ReadUmp96();
        winrt::Microsoft::Devices::Midi2::MidiUmp128 ReadUmp128();
        uint32_t ReadTimestampedUmpStructsIntoBuffer(winrt::Windows::Foundation::IMemoryBufferReference const& buffer, uint32_t byteOffsetinBuffer, uint32_t maxBytesToRead);
    };
}
