// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiSequence.h"

#include <sal.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

// Reader for Standard MIDI Files, RP-001, including the RIFF wrapped form used by .rmi files.
//
// !!! EVERYTHING THIS READS IS UNTRUSTED. !!!
// A file arrives by double click, usually from the internet. Every length in the file is treated
// as a claim rather than a fact: nothing is allocated from a declared size without checking it
// against the bytes actually present, and every count has a ceiling. A malformed file must produce
// a message, never a crash.
//
// It is also deliberately forgiving. Real world files are frequently a little wrong - a track
// length that overruns, a missing end of track, a note that never ends - and a player which
// refuses them is less useful than one which plays what is there and says so.
namespace midifile
{
    enum class ReadStatus : int32_t
    {
        Success = 0,
        FileNotFound = 1,
        AccessDenied = 2,
        ReadFailed = 3,
        FileTooLarge = 4,
        NotAMidiFile = 5,
        CorruptHeader = 6,
        NoPlayableData = 7,
        TooMuchData = 8
    };

    struct ReadResult
    {
        ReadStatus Status{ ReadStatus::Success };

        // Where the reader gave up, for a diagnostic message. Not shown to a customer.
        uint64_t ByteOffset{ 0 };

        uint32_t TracksDeclared{ 0 };
        uint32_t TracksRead{ 0 };

        // The file was short, or a chunk overran, and the reader kept what it had.
        bool Truncated{ false };

        bool Succeeded() const noexcept { return Status == ReadStatus::Success; }
    };

    // Ceilings. Generous next to any real musical file, finite next to a hostile one.
    struct ReadLimits
    {
        uint64_t MaximumFileBytes{ 64ull * 1024 * 1024 };
        uint32_t MaximumTracks{ 4096 };
        uint32_t MaximumEvents{ 2000000 };
        uint32_t MaximumEventBytes{ 48u * 1024 * 1024 };
        uint32_t MaximumSingleMessageBytes{ 1024 * 1024 };
        uint32_t MaximumTextBytes{ 16 * 1024 };
        uint32_t MaximumTextEvents{ 200000 };
    };

    ReadResult ParseStandardMidiFile(
        std::span<uint8_t const> fileBytes,
        _Inout_ MidiSequence& sequence,
        _In_ ReadLimits const& limits = {}) noexcept;

    ReadResult ReadStandardMidiFile(
        _In_ std::wstring const& path,
        _Inout_ MidiSequence& sequence,
        _In_ ReadLimits const& limits = {}) noexcept;

    // True when the extension is one this reader handles. Used for the queue and for deciding
    // what a dropped or double clicked file is.
    bool IsStandardMidiFileExtension(_In_ std::wstring const& path) noexcept;
}
