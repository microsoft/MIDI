// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Generators that listen (design section 29). Learn, Life, Evolve and change ringing can take
// another clip or a track as their source. Learned once, the notes stay in the settings. Kept
// listening, pass n hears what the source played during pass n - 1: a clip side by side, as if
// both started together, and a track where the listener plays. Everything heard comes through
// the one render path, so a listener hears exactly what plays.
//
// Free of pch.h and XAML like the rest of the model, so the unit tests use the same code.

#include <sal.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "GeneratorKinds.h"
#include "SequenceModel.h"

namespace midisequencer
{
    // What a track played during [fromTick, toTick), as notes at absolute ticks. Playback hears
    // what the track actually played there, from its timeline or a clip launched on it. Without
    // one, a track is heard from its timeline, as an export plays it.
    using TrackHearing = std::function<void(Track const& track, int64_t fromTick, int64_t toTick, std::vector<Note>& heard)>;

    struct ListenContext
    {
        Sequence const* Document{ nullptr };
        TrackHearing const* HearTrack{ nullptr };

        // What playback has launched and where its loop has got to, as a number, so what's heard
        // live is kept only while they stay the same. 0 without playback.
        uint64_t Epoch{ 0 };
    };

    // What a listener heard before a pass: the notes, on its own steps counted from the start of
    // what it heard, and the same notes learned as notes of a scale. Each is empty when there are
    // fewer than two: notes heard together on one step learn as one.
    struct Hearing
    {
        std::vector<StepNote> Notes{};
        std::vector<LearnedNote> Learned{};
    };

    // Learn, Life, Life drums, Evolve and change ringing.
    bool CanListen(_In_ GeneratorType type) noexcept;

    // Life, Life drums and Evolve, which carry what they heard from pass to pass rather than
    // learning each pass afresh.
    bool CarriesHearing(_In_ GeneratorType type) noexcept;

    // A source picked, and learning again each pass.
    bool ListensEachPass(_In_ GeneratorSettings const& generator) noexcept;

    // The notes that start in [fromTick, toTick): a track's, or a clip's placed at tick 0 and
    // looping (side by side). Absolute ticks, sorted. A note is cut where playback cuts it.
    std::vector<Note> NotesHeard(_In_ ListenContext const& context, _In_ ListenKind kind, _In_ std::wstring_view id, _In_ int64_t fromTick, _In_ int64_t toTick);

    // The notes any track plays that start in [fromTick, toTick), including one made up to play a
    // launched clip, through the one render path. Its own listeners hear through the context.
    std::vector<Note> NotesPlayed(_In_ ListenContext const& context, _In_ Track const& track, _In_ int64_t fromTick, _In_ int64_t toTick);

    // What a generator learns once from a source: a clip's first pass, or its first four when it
    // changes each pass, side by side; or a track's timeline from its start. At most 64 bars.
    // Fewer than two notes learns nothing.
    std::vector<LearnedNote> LearnOnce(_In_ Sequence const& sequence, _In_ int64_t stepTicks, _In_ ListenKind kind, _In_ std::wstring_view id);

    // What a generator that keeps listening heard before a pass that starts at passStart: what its
    // source played during the pass before, as notes and as notes of the scale. Empty for the
    // first pass, for fewer than two notes, and without a sequence; the pass then plays from what
    // the clip learned last.
    Hearing HeardBefore(_In_ ListenContext const& context, _In_ Clip const& clip, _In_ int64_t passStart, _In_ uint64_t pass);

    // What Life and Evolve hear before each pass when they keep listening, as the pass that starts
    // at passStart sees it. Before is empty when the clip doesn't keep listening.
    PassHearing MakePassHearing(_In_ ListenContext const& context, _In_ Clip const& clip, _In_ int64_t passStart, _In_ uint64_t pass);

    // One pass of a source, on a listener's steps, for Life to drop onto its board when it learns
    // once: a clip's first pass, side by side, or a track's first pass that plays anything.
    // Fewer than two notes hears nothing.
    std::vector<StepNote> HearOnce(_In_ Sequence const& sequence, _In_ Clip const& listener, _In_ ListenKind kind, _In_ std::wstring_view id);

    // The source's name, for showing what was learned from: the clip's or the track's.
    std::wstring ListenSourceName(_In_ Sequence const& sequence, _In_ ListenKind kind, _In_ std::wstring_view id);
}
