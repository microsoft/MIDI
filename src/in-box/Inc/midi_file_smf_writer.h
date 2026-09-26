// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "midi_file_sequence.h"

#include <sal.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

// Writer for Standard MIDI Files, RP-001. The mirror of midi_file_smf_reader.h: anything that
// reader produces, this writer puts back on disk, and reading the result again gives the same
// sequence.
//
// "The same sequence" means the same events at the same ticks, the same tempo and meter maps and
// the same text, not the same bytes. A file is not a canonical form: the reader repairs a missing
// tempo event, merges two tempo events at one tick and decodes Latin-1 text to UTF-8, and those
// repairs stay repaired.
//
// Two things a sequence can hold that this format cannot:
//
//  - A Universal MIDI Packet. MIDI 1.0 has no 16 bit velocity, no 32 bit controller and no per
//    note controller, so each packet is translated where MIDI 1.0 has an equivalent and counted
//    in SkippedEventCount where it does not.
//  - Format 2, where the tracks are independent sequences. The reader lays those out one after
//    another on a single timeline, so the independence is already gone by the time a sequence
//    exists, and the file is written as format 1.
namespace midifile
{
    enum class WriteStatus : int32_t
    {
        Success = 0,
        NothingToWrite = 1,     // no events and no text, so there would be no file worth keeping
        TooMuchData = 2,        // over MaximumFileBytes
        AccessDenied = 3,
        WriteFailed = 4,
        OutOfMemory = 5
    };

    struct WriteResult
    {
        WriteStatus Status{ WriteStatus::Success };

        uint32_t TrackCount{ 0 };
        uint64_t ByteCount{ 0 };

        // Universal MIDI Packets that MIDI 1.0 has no way to express. Zero for every sequence
        // that came from a Standard MIDI File, since the reader cannot produce one.
        uint32_t SkippedEventCount{ 0 };

        bool Succeeded() const noexcept { return Status == WriteStatus::Success; }
    };

    struct WriteOptions
    {
        // Merge every track into one and write format 0. Track membership is lost, which is
        // usually what a capture or a single instrument part wants anyway.
        bool SingleTrack{ false };

        // Leave the status byte off a message that repeats the one before it. Every reader
        // handles this, and it makes a note-heavy file noticeably smaller, but a file written
        // without it survives a damaged byte better.
        bool UseRunningStatus{ false };

        // Ticks per quarter note used when the sequence is absolutely timed, because the format
        // has no way of saying "a tick is a microsecond". At this value and 120 beats per minute
        // one tick is about half a millisecond.
        uint16_t AbsoluteTimingTicksPerQuarterNote{ 960 };

        uint64_t MaximumFileBytes{ 256ull * 1024 * 1024 };
    };

    WriteResult WriteStandardMidiFile(
        _In_ MidiSequence const& sequence,
        _Inout_ std::vector<uint8_t>& fileBytes,
        _In_ WriteOptions const& options = {}) noexcept;

    WriteResult WriteStandardMidiFile(
        _In_ std::wstring const& path,
        _In_ MidiSequence const& sequence,
        _In_ WriteOptions const& options = {}) noexcept;
}
