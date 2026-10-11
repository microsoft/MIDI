// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Arpeggiator clips (design section 4): one note a step from a set of notes, in an order, over
// some octaves. The notes are the keys held on the track's source, as they're played, or the
// notes another track's timeline plays at each step. The second is worked out from the sequence
// like any other clip, so exports have it. The first can only be played live.
// Free of pch.h and XAML like the rest of the model, so the unit tests use the same code.

#include <sal.h>

#include <cstdint>
#include <functional>
#include <vector>

#include "SequenceModel.h"

namespace midisequencer
{
    inline constexpr uint8_t MaximumArpeggiatorOctaves = 4;
    inline constexpr size_t MaximumHeldKeys = 32;

    struct HeldNote
    {
        uint8_t Number{ 60 };
        uint16_t Velocity{ 0xC000 };

        bool operator==(HeldNote const&) const = default;
    };

    // The keys held on a track's source, which the engine keeps as they're played.
    struct ArpeggiatorInput
    {
        // Down now, in the order they went down.
        std::vector<HeldNote> Held{};

        // The keys held last time, kept after they go up, for an arpeggiator that keeps playing.
        std::vector<HeldNote> Latched{};

        // Steps played since the first of these keys went down, so a new chord starts at the
        // start of its order.
        uint64_t StepsPlayed{ 0 };

        // Set by a render that had an arpeggiator playing these keys, even when none were held.
        bool Used{ false };

        void Press(_In_ uint8_t number, _In_ uint16_t velocity);
        void Release(_In_ uint8_t number);

        // Playback stopped: the next keys start a new chord, and nothing latched plays on.
        void Restart() noexcept;
    };

    // Starts with 1/16 steps over two octaves, like comp 4.
    GeneratorSettings MakeArpeggiatorGenerator();

    // The notes an arpeggiator steps through for a chord: in its order, over its octaves, none
    // above 127. Random gives them lowest first, and each step picks one. The chord comes in the
    // order its keys went down or its notes started, which As held keeps.
    std::vector<HeldNote> ArpeggiatorSequence(_In_ std::vector<HeldNote> const& chord, _In_ ArpeggiatorOrder order, _In_ uint8_t octaves);

    // The notes sounding on a track's timeline at a tick.
    struct SoundingChord
    {
        // In the order they started, one per note number.
        std::vector<HeldNote> Notes{};

        // Where the newest of them started: the chord begins there.
        int64_t StartTick{ 0 };
    };

    // An arpeggiator on that track is left out, so two can't follow each other round.
    SoundingChord NotesSoundingAt(_In_ Sequence const& sequence, _In_ Track const& track, _In_ int64_t tick);

    // Each note one pass of an arpeggiator plays that starts inside [fromTick, toTick), in step
    // order. keys are the keys held on the track's source; without them, an arpeggiator that
    // plays held keys plays nothing, which is what an export gets.
    void ForEachArpeggiatorNote(
        _In_ Sequence const& sequence,
        _In_ Clip const& clip,
        _In_ int64_t passStart,
        _In_ int64_t passEnd,
        _In_ uint64_t pass,
        _In_ int64_t fromTick,
        _In_ int64_t toTick,
        _Inout_opt_ ArpeggiatorInput* keys,
        _In_ std::function<void(Note const&)> const& add);

    bool IsArpeggiator(_In_ Clip const& clip) noexcept;

    // An arpeggiator that plays the keys held on its track's source.
    bool PlaysHeldKeys(_In_ Clip const& clip) noexcept;

    // Whether a track has one on its timeline or in a launcher slot, so its source has to be
    // open whether the track is armed or not.
    bool TrackHasHeldKeysArpeggiator(_In_ Sequence const& sequence, _In_ Track const& track);

    // An ordinary notes clip holding what an arpeggiator that plays another track's notes plays
    // in one placement, from the placement's start. Notes are only there for that kind.
    Clip MakeNotesFromArpeggiator(_In_ Sequence const& sequence, _In_ Clip const& arpeggiator, _In_ Placement const& placement);
}
