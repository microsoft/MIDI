// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Reads and writes .midisequence files: JSON, so a person or an assistant can read one, and a
// change between two versions reads as a change.
//
// !!! A FILE CAN ARRIVE FROM ANYWHERE. !!!
// Every count is capped by SequenceLimits, every number is range checked, and anything the reader
// doesn't understand is kept rather than trusted. A damaged file produces a status, never a crash.

#include <sal.h>

#include <cstdint>
#include <string>
#include <string_view>

#include "SequenceModel.h"

namespace midisequencer
{
    enum class SequenceReadStatus : int32_t
    {
        Success = 0,
        NotJson = 1,
        NotASequence = 2,
        TooLarge = 3,
    };

    struct SequenceReadResult
    {
        SequenceReadStatus Status{ SequenceReadStatus::Success };

        // Written by a newer version. Open it, but don't save over it, or settings this version
        // doesn't know about would be lost.
        bool FromNewerVersion{ false };

        // Things past a limit, or damaged beyond use, that were left out.
        size_t SkippedItems{ 0 };

        bool Succeeded() const noexcept { return Status == SequenceReadStatus::Success; }
    };

    inline constexpr size_t MaximumSequenceFileCharacters = 256u * 1024 * 1024;

    SequenceReadResult ReadSequenceJson(
        _In_ std::wstring_view text,
        _Out_ Sequence& sequence,
        _In_ SequenceLimits const& limits = {});

    // Fixed key order and two-space indents, one note per line, so two versions of a file can be
    // compared line by line.
    std::wstring WriteSequenceJson(_In_ Sequence const& sequence);

    // A message as text: "40B14A00 80000000". Upper case hex, one group of 8 digits per word.
    std::wstring UmpToText(_In_ ClipEvent const& event);

    // False when the text isn't one to four words, or the word count doesn't match the message type.
    bool UmpFromText(_In_ std::wstring_view text, _Out_ ClipEvent& event) noexcept;
}
