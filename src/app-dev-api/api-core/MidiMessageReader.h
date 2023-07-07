// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once
#include "MidiMessageReader.g.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiMessageReader : MidiMessageReaderT<MidiMessageReader>
    {
        MidiMessageReader() = default;

        winrt::Windows::Devices::Midi2::MidiMessageReaderEndOfMessagesBehavior EndOfMessagesBehavior();
        void EndOfMessagesBehavior(winrt::Windows::Devices::Midi2::MidiMessageReaderEndOfMessagesBehavior const& value);
        bool EndOfMessages();
        uint64_t PeekNextTimestamp();
        winrt::Windows::Devices::Midi2::MidiUmpSize PeekNextUmpSize();

        winrt::Windows::Devices::Midi2::MidiUmp32 ReadUmp32();
        winrt::Windows::Devices::Midi2::MidiUmp64 ReadUmp64();
        winrt::Windows::Devices::Midi2::MidiUmp96 ReadUmp96();
        winrt::Windows::Devices::Midi2::MidiUmp128 ReadUmp128();

        uint32_t ReadBuffer(winrt::Windows::Foundation::IMemoryBufferReference const& buffer, uint32_t byteOffsetinBuffer, uint32_t maxBytesToRead);
    };
}
