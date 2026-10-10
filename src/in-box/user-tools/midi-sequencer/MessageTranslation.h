// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// MIDI 2.0 channel voice to MIDI 1.0 and back, for one destination at a time. Windows only scales
// down for some endpoints and never because of a function block, and it doesn't scale MIDI 1.0 up
// at all, so the sequencer sends each destination what it speaks itself.
//
// Values follow M2-115 min-center-max scaling, the same as ScaleUp and ScaleDown.

#include <sal.h>

#include <array>
#include <cstdint>

namespace midisequencer
{
    struct TranslatedMessages
    {
        // Up to four one-word messages, or one message of up to four words.
        std::array<std::array<uint32_t, 4>, 4> Messages{};
        std::array<uint8_t, 4> WordCounts{};
        uint8_t Count{ 0 };

        // MIDI 1.0 has no way to say it: per-note controllers, per-note pitch bend, per-note
        // management, relative controllers.
        bool Dropped{ false };
    };

    // A MIDI 2.0 channel voice message (type 4) as MIDI 1.0 channel voice messages (type 2).
    // Anything else passes through unchanged.
    TranslatedMessages TranslateToMidi1(_In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) noexcept;

    // A MIDI 1.0 channel voice message (type 2) as a MIDI 2.0 one (type 4). A note on with velocity
    // 0 becomes a note off, because in MIDI 2.0 velocity 0 is a real velocity. Anything else passes
    // through unchanged.
    TranslatedMessages TranslateToMidi2(_In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) noexcept;
}
