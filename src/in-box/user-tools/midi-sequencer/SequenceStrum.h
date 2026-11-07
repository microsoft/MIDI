// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Strum generator clips (strum proposal; Pete, November 5 2026). A Strum plays the chord the Key
// and chords row has at each moment, or the chord another track is playing, the way a player
// would: voiced as a real shape on a guitar, a ukulele or a piano, and played on a pattern of
// strokes. Each stroke spreads the chord's notes along a curve, a few milliseconds apart, down
// from the lowest string or up from the highest. Like the arpeggiator, it's worked out from the
// sequence, so jumping back replays what you heard, Make notes bakes it into notes, and an
// export has it.
//
// The pattern is a character a step:
//   D  a down stroke, every string          d  a softer one
//   U  an up stroke, the top strings        u  a softer one
//   x  a muted stroke: short and quiet, or a key switch
//   R  a rasgueado: fingers flicked down one after another inside the step, faster or slower
//   T  tremolo: fast down and up strokes filling the step
//   B  the thumb on the bass, between the two lowest strings by turns, as in Travis picking
//   1 to 4  a finger on a string, counted from the top
//   P  a pinch: the thumb's bass and the top string together
//   G  a glissando up every string, filling the step       g  one down
//   -  nothing new; what's sounding rings on (any other character does the same)
//
// The same curves shape the piano roll's Strum tool (NoteTools.h).
// Free of pch.h and XAML like the rest of the model, so the unit tests use the same code.

#include <sal.h>

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "SequenceModel.h"

namespace midisequencer
{
    struct ArpeggiatorInput;

    inline constexpr size_t MaximumStrumPattern = 64;
    inline constexpr uint16_t MaximumStrumSpreadMs = 500;
    inline constexpr uint16_t MaximumStrumSpreadTicks = TicksPerQuarterNote;

    // Where in a strum a note of it lands, from 0 for the first to 1 for the last, for the shape
    // and how strongly it bends (0 to 1). Every shape starts at 0 and ends at 1.
    double StrumCurve(_In_ StrumShape shape, _In_ double x, _In_ double bend) noexcept;

    // A note of a voiced chord, with the string it's on. Strings count from the one a down stroke
    // hits first.
    struct StrungNote
    {
        uint8_t String{ 0 };
        uint8_t Number{ 60 };

        bool operator==(StrungNote const&) const = default;
    };

    // A chord as an instrument plays it: the strings that sound, in the order a down stroke hits
    // them. bass is the pitch class that has to be lowest (a guitar's or a piano's bass); notes
    // are used as they are for As written. A guitar picks the shape nearest position on the neck
    // (0 for as low as it goes) that a hand can hold, with any strings it can't use muted below
    // the bass. A ukulele uses all four strings. capo raises both by that many frets. A harp
    // plays every note of the chord from G2 to G5. Nothing when there's nothing to voice.
    std::vector<StrungNote> VoiceChord(
        _In_ StrumInstrument instrument,
        _In_ std::vector<uint8_t> const& pitchClasses,
        _In_ uint8_t bass,
        _In_ std::vector<uint8_t> const& notes,
        _In_ uint8_t capo = 0,
        _In_ uint8_t position = 0);

    // How many strings the instrument has, and how many from the top an up stroke hits.
    uint8_t InstrumentStrings(_In_ StrumInstrument instrument) noexcept;
    bool UpStrokeHits(_In_ StrumInstrument instrument, _In_ uint8_t string, _In_ size_t voicedStrings) noexcept;

    // Starts on the Key and chords row, with a folk pattern on 1/8 steps for a guitar.
    GeneratorSettings MakeStrumGenerator();

    // The strum patterns the panel offers.
    struct StrumStyle
    {
        wchar_t const* NameKey{ nullptr };      // resource
        wchar_t const* Pattern{ nullptr };
        int64_t StepTicks{ TicksPerQuarterNote / 2 };
        bool Ring{ true };
        uint16_t NoteLength{ 50 };
        uint8_t Burst{ 4 };
        StrumShape BurstCurve{ StrumShape::SpeedingUp };
    };

    std::vector<StrumStyle> const& StrumStyles();
    void UseStrumStyle(_Inout_ GeneratorSettings& generator, _In_ StrumStyle const& style);

    // Only the characters a pattern knows, at most MaximumStrumPattern of them.
    std::wstring CleanStrumPattern(_In_ std::wstring_view pattern);

    // Each note one pass of a Strum plays that starts inside [fromTick, toTick). songTick maps a
    // tick to where it is in the song, for the Key and chords row (RenderOptions::SongTick); left
    // out, a tick is where it is. keys are the keys held on the track's source, for a Strum that
    // plays them; without them it plays nothing, which is what an export gets.
    void ForEachStrumNote(
        _In_ Sequence const& sequence,
        _In_ Clip const& clip,
        _In_ int64_t passStart,
        _In_ int64_t passEnd,
        _In_ uint64_t pass,
        _In_ int64_t fromTick,
        _In_ int64_t toTick,
        _In_ std::function<void(Note const&)> const& add,
        _In_opt_ std::function<int64_t(int64_t)> const* songTick = nullptr,
        _Inout_opt_ ArpeggiatorInput* keys = nullptr);

    bool IsStrum(_In_ Clip const& clip) noexcept;

    // Generators that play notes they're given, from the Key and chords row or another track: the
    // arpeggiator and the Strum. A track they follow leaves them out, so two can't follow each
    // other round.
    bool PlaysAlong(_In_ Clip const& clip) noexcept;
}
