// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Turns a track's timeline into the messages it plays, at absolute ticks. Playback and every
// export use this one path, so a file always holds what you heard.

#include <sal.h>

#include <array>
#include <cstdint>
#include <vector>

#include "SequenceModel.h"

namespace midisequencer
{
    enum class RenderedKind : uint8_t
    {
        // At one tick: note offs go first, so a note repeated on the same tick restarts instead
        // of being cut off by its own end. Then other messages, then note ons.
        NoteOff = 0,
        Other = 1,
        NoteOn = 2,
    };

    struct RenderedMessage
    {
        int64_t Tick{ 0 };
        std::array<uint32_t, 4> Words{};
        uint8_t WordCount{ 0 };
        RenderedKind Kind{ RenderedKind::Other };
    };

    // A file can place a one-tick looping clip for a billion ticks. One render stops here rather
    // than running the PC out of memory; real sequences are far below it.
    inline constexpr size_t MaximumRenderedMessages = 4000000;

    // Every message from the track's placements with a time in [fromTick, toTick), sorted. Notes
    // come out as MIDI 2.0 note on and note off messages. Groups and channels are as stored; the
    // destination is applied by ApplyDestination.
    //
    // A note that would run past the end of its placement, or past the loop point of a looping
    // clip, ends there.
    void RenderTrack(
        _In_ Sequence const& sequence,
        _In_ Track const& track,
        _In_ int64_t fromTick,
        _In_ int64_t toTick,
        _Inout_ std::vector<RenderedMessage>& messages,
        _In_ size_t maximumMessages = MaximumRenderedMessages);

    // Where the track's last placement ends. 0 for a track with nothing on its timeline.
    int64_t TrackEndTick(_In_ Sequence const& sequence, _In_ Track const& track) noexcept;

    // Where the last placement of any track ends.
    int64_t SequenceEndTick(_In_ Sequence const& sequence) noexcept;

    // Rewrites a message for the track's destination: its group, and its channel when the
    // destination sets one. Messages without a group or channel pass through unchanged.
    void ApplyDestination(
        _In_ TrackDestination const& destination,
        _Inout_ std::array<uint32_t, 4>& words,
        _In_ uint8_t wordCount) noexcept;

    // A MIDI 2.0 note message, message type 4. Two words.
    void BuildNoteOn(_In_ Note const& note, _In_ uint8_t group, _Out_writes_(2) uint32_t* words) noexcept;
    void BuildNoteOff(_In_ Note const& note, _In_ uint8_t group, _Out_writes_(2) uint32_t* words) noexcept;
}
