// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiMessageWriter.g.h"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiMessageWriter : MidiMessageWriterT<MidiMessageWriter>
    {
        MidiMessageWriter() = default;

        bool IsFull();
        void WriteUmpWords32(uint64_t midiTimestamp, uint32_t umpWord1);
        void WriteUmpWords64(uint64_t midiTimestamp, uint32_t umpWord1, uint32_t umpWord2);
        void WriteUmpWords96(uint64_t midiTimestamp, uint32_t umpWord1, uint32_t umpWord2, uint32_t umpWord3);
        void WriteUmpWords128(uint64_t midiTimestamp, uint32_t umpWord1, uint32_t umpWord2, uint32_t umpWord3, uint32_t umpWord4);
        void WriteUmpWords(uint64_t midiTimeStamp, array_view<uint32_t const> words, uint8_t wordCount);
        void WriteUmpWithTimestamp(winrt::Microsoft::Devices::Midi2::UmpWithTimestamp const& ump);
        void WriteUmpWithTimestamp(uint64_t midiTimestamp, winrt::Microsoft::Devices::Midi2::Ump const& ump);
        void WriteMultipleUmpsWithTimestamps(winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::UmpWithTimestamp> const& umpList);
    };
}
