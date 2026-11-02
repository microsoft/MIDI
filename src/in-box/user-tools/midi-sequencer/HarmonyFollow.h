// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Following the Key and chords row (proposal revision 2, section 3): generators that play in the
// song's key, Chord tones, clips that follow the key, and the arpeggiator's Chords source. No XAML
// here, so the tests compile it unchanged.

#include <sal.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "SequenceModel.h"

namespace midisequencer
{
    // A note moved by scale steps from one key to another: its place in the first key's scale
    // keeps its place in the second's, so E in C major is F♯ in D major and E♭ in C minor. A
    // note between two scale notes keeps its distance from the one below. The root moves by the
    // smaller way round (C to B goes down a semitone, not up 11). Scales with fewer notes map
    // each step to the nearest one below. Stays inside 0 to 127.
    uint8_t MoveByScaleSteps(_In_ uint8_t note, _In_ KeyChange const& from, _In_ KeyChange const& to) noexcept;

    // The note of a chord nearest a note, the lower one when two are as near.
    uint8_t NearestChordTone(_In_ uint8_t note, _In_ ChordEvent const& chord);

    // Whether a tick starts a beat of the meter there.
    bool StartsBeat(_In_ std::vector<MeterChange> const& meter, _In_ int64_t tick) noexcept;

    // The song's key at a tick, when the sequence has a Key and chords row with a key.
    std::optional<KeyChange> SongKeyAt(_In_ Sequence const& sequence, _In_ int64_t tick) noexcept;

    // The generator as it plays at a tick: in the song's key, when it follows it and the song has
    // one there. Nothing when it plays as written, so the caller can skip the copy.
    std::optional<GeneratorSettings> GeneratorInSongKey(_In_ Sequence const& sequence, _In_ GeneratorSettings const& generator, _In_ int64_t tick);

    // Whether a clip's notes move with the song's key: notes clips and melodic patterns with
    // Follow key on. Drum patterns never follow.
    bool ClipFollowsKey(_In_ Clip const& clip) noexcept;

    // The key the notes of a clip that follows the key move into at a tick. Nothing when they
    // play as written.
    std::optional<std::pair<KeyChange, KeyChange>> FollowKeyMove(_In_ Sequence const& sequence, _In_ Clip const& clip, _In_ int64_t tick) noexcept;

    // Whether a generator type plays notes from a key, so Song key and Chord tones apply to it.
    // Euclidean rhythms, Life drums and the arpeggiator don't.
    bool GeneratorUsesKey(_In_ GeneratorType type) noexcept;

    // Whether a clip follows the song's key: Follow key, or a generator with Song key. The
    // timeline marks these.
    bool FollowsSongKey(_In_ Clip const& clip) noexcept;

    // Everything that plays a clip uses these two, so playback, an arpeggiator following a track
    // and a generator listening to one all hear the same notes.
    //
    // The clip to make a generator's pass from: a copy in the song's key at songTick, when it
    // follows the song's key and that's different from its own. Nothing to play the clip as it is.
    std::optional<Clip> ClipInSongKey(_In_ Sequence const& sequence, _In_ Clip const& clip, _In_ int64_t songTick);

    // A note as it plays at songTick: moved into the song's key, for a clip that follows it, and
    // onto the chord there, for a generator with Chord tones when the note starts a beat.
    Note AsPlayed(_In_ Sequence const& sequence, _In_ Clip const& clip, _In_ Note note, _In_ int64_t songTick) noexcept;

    // A new clip in a sequence with a key (Pete's decision 2 of November 1 2026): a generator that
    // plays from a key uses Song key, and a melodic pattern follows the key, written in the song's
    // key at the tick. Anything else, or a sequence with no key there, is left as made.
    void StartInSongKey(_Inout_ Clip& clip, _In_ Sequence const& sequence, _In_ int64_t tick);
}
