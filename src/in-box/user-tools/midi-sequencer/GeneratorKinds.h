// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The generators of design section 28: Life, Learn, Words, Evolve, the infinity series,
// L-systems and change ringing. Like the Euclidean rhythm and the chance melody, each works out a
// pass from the clip's settings, its seed and the pass number, so jumping back replays what you
// heard and an export writes it. None of them uses a trained model: each is a short set of rules.
//
// Free of pch.h and XAML like the rest of the model, so the unit tests use the same code. The
// window draws each generator from the functions here too.

#include <sal.h>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "SequenceModel.h"

namespace midisequencer
{
    // A note one of these generators plays, counted in its steps.
    struct StepNote
    {
        int64_t Step{ 0 };
        int64_t Steps{ 1 };             // how many steps it lasts, at least 1
        uint8_t Number{ 60 };
        uint16_t Velocity{ 49151 };

        bool operator==(StepNote const&) const = default;
    };

    inline constexpr uint16_t SoftVelocity = 39321;     // 60%
    inline constexpr uint16_t NormalVelocity = 49151;   // 75%
    inline constexpr uint16_t AccentVelocity = 62258;   // 95%

    // What a generator that keeps listening heard before each of its passes, for the ones that
    // carry what they heard from pass to pass: Life drops it onto its board, and Evolve scores
    // its copies against it (design section 29). GeneratorListen.h makes these.
    struct PassHearing
    {
        // The same while everything it can hear stays the same, so a pass is kept by it.
        uint64_t Key{ 0 };

        // The same, apart from what playback has launched and where its loop has got to, so a
        // board or a melody carries on through those rather than starting again.
        uint64_t RunKey{ 0 };

        // Playback hears what plays now. A pass far past what was worked out hears only the few
        // passes before it, so starting part way through a song doesn't stall playback.
        bool Live{ false };

        // What it heard before a pass, 1 or later: notes on its own steps, counted from the start
        // of what it heard. Empty when it heard fewer than two.
        std::function<std::vector<StepNote>(uint64_t pass)> Before{};
    };

    // The generators work out how loud each note is, from 0 to 1, and write it as a velocity in
    // the default range, 50% to 95%. GeneratedPass moves it into the generator's own range, or
    // sets it from one of the shapes every generator offers, so the default range plays them
    // just as they're written.
    inline constexpr uint16_t DefaultVelocityLow = 32768;
    inline constexpr uint16_t DefaultVelocityHigh = 62258;

    inline uint16_t VelocityForLoudness(_In_ double loudness) noexcept
    {
        auto const clamped = loudness < 0.0 ? 0.0 : loudness > 1.0 ? 1.0 : loudness;
        return static_cast<uint16_t>(DefaultVelocityLow + static_cast<uint16_t>(clamped * (DefaultVelocityHigh - DefaultVelocityLow) + 0.5));
    }

    // The shapes a type offers, its own first, then the four every generator has. The first is
    // its default.
    std::vector<VelocityShape> VelocityShapesFor(_In_ GeneratorType type);

    VelocityShape DefaultVelocityShape(_In_ GeneratorType type) noexcept;

    inline constexpr size_t MaximumLifeRows = 48;
    inline constexpr size_t MaximumDrumRows = 16;
    inline constexpr size_t MaximumLearnedNotes = 512;
    inline constexpr size_t MaximumGeneratorText = 400;
    inline constexpr size_t MaximumGrammarRules = 6;
    inline constexpr size_t MaximumGrammarLength = 64;
    inline constexpr uint8_t MinimumBells = 4;
    inline constexpr uint8_t MaximumBells = 8;

    // Passes these generators work out, at most. Past this, a pass plays what the last one did.
    inline constexpr uint64_t MaximumSteppedPasses = 20000;

    // Life, Learn, Words, Evolve, the infinity series, L-systems and change ringing: the
    // generators whose passes come from GeneratedPass.
    bool IsSectionTwentyEightGenerator(_In_ GeneratorType type) noexcept;

    // Whether the clip's seed changes what a generator plays, so the header shows it.
    bool GeneratorUsesSeed(_In_ GeneratorSettings const& generator) noexcept;

    // A new generator of one of these types, ready to play: its key, its range and the rest.
    GeneratorSettings MakeSectionTwentyEightGenerator(_In_ GeneratorType type);

    // Fills in what a generator of its type needs and doesn't have yet, such as a Life drum
    // board's drums, the words to spell or an L-system's rules. Leaves everything else alone, so
    // switching types back and forth keeps each one's settings.
    void PrepareGenerator(_Inout_ GeneratorSettings& generator);

    // Counts, lengths and choices in bounds, for the settings below.
    void NormalizeSectionTwentyEight(_Inout_ GeneratorSettings& generator);

    // Every setting the same, other than keys this version doesn't know.
    bool SameGeneratorSettings(_In_ GeneratorSettings const& a, _In_ GeneratorSettings const& b) noexcept;

    // What one pass plays, sorted by step. Results are kept for a while, so playback, which asks
    // for the same pass in small pieces, works each pass out once. Life and Evolve take what they
    // heard, when they keep listening.
    std::vector<StepNote> GeneratedPass(_In_ GeneratorSettings const& generator, _In_ uint32_t seed, _In_ int64_t steps, _In_ uint64_t pass, _In_opt_ PassHearing const* hearing = nullptr);

    // ---- the key and range (GeneratorKinds.cpp) ----

    // The root nearest the middle of the range, as an index into ScaleNotes. 0 when the range has
    // no root in it.
    size_t MiddleRootIndex(_In_ GeneratorSettings const& generator, _In_ std::vector<uint8_t> const& ladder);

    // Reflects an index that wandered off either end of a ladder of `size` notes back into it.
    size_t ReflectIndex(_In_ int64_t index, _In_ size_t size) noexcept;

    // Keeps every jump to LargestJump, and starts and ends the pass on the root, when the
    // settings ask. Notes stay in the key and the range.
    void ApplyKeyRules(_In_ GeneratorSettings const& generator, _Inout_ std::vector<StepNote>& notes);

    // ---- Life (GeneratorLife.cpp) ----

    enum class LifeShape : uint8_t
    {
        Glider = 0,
        Blinker = 1,
        RPentomino = 2,
    };

    struct LifeBoard
    {
        int32_t Columns{ 0 };
        int32_t Rows{ 0 };

        // Row by row from the bottom: 0 empty, 1 alive, 2 born this pass.
        std::vector<uint8_t> Cells{};

        // How many passes each cell has lived, the same way round: 1 the pass it's born, up to
        // 255, and 0 for an empty cell.
        std::vector<uint8_t> Ages{};

        // It stopped changing, and was started again, given cells or held.
        bool Settled{ false };

        uint8_t At(_In_ int32_t column, _In_ int32_t row) const noexcept
        {
            return column >= 0 && row >= 0 && column < Columns && row < Rows ? Cells[static_cast<size_t>(row) * static_cast<size_t>(Columns) + static_cast<size_t>(column)] : uint8_t{ 0 };
        }
    };

    // The note each row plays, from the bottom up: the key's notes in the range, or the drums.
    std::vector<uint8_t> LifeRowNotes(_In_ GeneratorSettings const& generator);

    // The board on a pass. Columns is the clip's steps.
    LifeBoard LifeBoardAt(_In_ GeneratorSettings const& generator, _In_ uint32_t seed, _In_ int64_t steps, _In_ uint64_t pass, _In_opt_ PassHearing const* hearing = nullptr);

    // Where notes heard land on a board: each note's step is its column, and its row is the row
    // whose note it is, or the nearest, once it's moved an octave at a time into the rows' range.
    // Sorted, without repeats.
    std::vector<LifeCell> HeardCells(_In_ GeneratorSettings const& generator, _In_ std::vector<StepNote> const& heard, _In_ int32_t columns);

    // The cells a scatter starts with, so drawing can start from them.
    std::vector<LifeCell> ScatteredCells(_In_ GeneratorSettings const& generator, _In_ uint32_t seed, _In_ int32_t columns, _In_ int32_t rows);

    // A named shape in the middle of a board.
    std::vector<LifeCell> LifeShapeCells(_In_ LifeShape shape, _In_ int32_t columns, _In_ int32_t rows);

    // Kick, snare, clap, closed and open hi-hats, two toms and a ride: General MIDI drums.
    std::vector<uint8_t> DefaultDrumRows();

    // ---- Learn and Words (GeneratorLearn.cpp) ----

    inline constexpr size_t TuneCount = 3;

    // A built-in tune: 0 Ode to Joy, 1 Frère Jacques, 2 Twinkle, Twinkle, Little Star.
    std::vector<LearnedNote> TuneNotes(_In_ uint8_t tune);

    // The tune, clip or words a Learn generator learned from.
    std::vector<LearnedNote> LearnedSource(_In_ GeneratorSettings const& generator);

    // A clip's melody as steps of the key it's in, which this works out from its notes. Chords
    // keep their highest note. stepTicks is the generator's step.
    std::vector<LearnedNote> LearnFromNotes(_In_ std::vector<Note> const& notes, _In_ int64_t stepTicks);

    // How often each degree of the generator's scale goes to each other one in what it learned,
    // for drawing. [from][to].
    std::vector<std::vector<uint32_t>> LearnedMoves(_In_ GeneratorSettings const& generator);

    // The degree of the generator's scale a note is, or -1 when it's not in the scale.
    int32_t ScaleDegree(_In_ GeneratorSettings const& generator, _In_ uint8_t number) noexcept;

    // The note each learned note plays in a generator's key and range, as Learn plays it: the
    // learned notes' middle octave on the root nearest the middle of the range.
    std::vector<uint8_t> LearnedPitches(_In_ GeneratorSettings const& generator, _In_ std::vector<LearnedNote> const& learned);

    // A seed from words, the same on every PC: "Midnight drive" always gives the same passes.
    uint32_t SeedFromWords(_In_ std::wstring_view words) noexcept;

    // Up to eight hex digits, the way the seed is shown, or nothing.
    std::optional<uint32_t> SeedFromHex(_In_ std::wstring_view text) noexcept;

    // Words spelled as notes, one character a step, for playing and for drawing.
    struct SpelledStep
    {
        int64_t Step{ 0 };
        int64_t Steps{ 1 };
        uint8_t Number{ 0 };            // 0 for a rest
        bool Accent{ false };
        wchar_t Letter{ 0 };            // the character, or 0 for an ending's note
        uint8_t Stress{ 50 };           // 0 to 100, how hard it's spoken: VelocityShape::Speech
    };

    std::vector<SpelledStep> SpellWords(_In_ GeneratorSettings const& generator);

    // How many steps the spelled words take.
    int64_t SpelledLength(_In_ std::vector<SpelledStep> const& spelled) noexcept;

    // ---- Evolve (GeneratorEvolve.cpp) ----

    inline constexpr size_t EvolveChildren = 8;

    // The five rules in GeneratorSettings::Weights, then Sounds like, which counts only when the
    // clip has a source to sound like.
    inline constexpr size_t EvolveRuleCount = 6;

    // A pass, how it came from the pass before, and how each copy scored, for drawing.
    struct EvolveFamily
    {
        std::vector<StepNote> Parent{};
        std::array<std::vector<StepNote>, EvolveChildren> Children{};
        std::array<double, EvolveChildren> Scores{};
        size_t Chosen{ 0 };

        // Each rule's score for the pass, 0 to 1: Weights' order, then Sounds like.
        std::array<double, EvolveRuleCount> Rules{};

        // What the copies were scored against for sounding like the source, on the clip's steps.
        // Empty without a source.
        std::vector<StepNote> Target{};
    };

    EvolveFamily EvolveFamilyAt(_In_ GeneratorSettings const& generator, _In_ uint32_t seed, _In_ int64_t steps, _In_ uint64_t pass, _In_opt_ PassHearing const* hearing = nullptr);

    // The score of each pass from the first, for drawing how it climbs.
    std::vector<double> EvolveScores(_In_ GeneratorSettings const& generator, _In_ uint32_t seed, _In_ int64_t steps, _In_ uint64_t passes, _In_opt_ PassHearing const* hearing = nullptr);

    // ---- the infinity series, L-systems and change ringing (GeneratorStreams.cpp) ----

    // Per Nørgård's series: 0, 1, -1, 2, 1, 0, -2, 3, ... with a(2n) = -a(n) and a(2n + 1) = a(n) + 1.
    int32_t InfinitySeries(_In_ uint64_t index) noexcept;

    // Where in the series a clip starts, from its seed.
    uint64_t InfinityStart(_In_ uint32_t seed) noexcept;

    struct LSystemPreset
    {
        wchar_t const* NameKey{ nullptr };     // the resource with its name
        wchar_t const* Axiom{ nullptr };
        std::array<std::pair<wchar_t, wchar_t const*>, 2> Rules{};
        uint8_t Angle{ 90 };
    };

    inline constexpr size_t LSystemPresetCount = 6;

    std::array<LSystemPreset, LSystemPresetCount> const& LSystemPresets() noexcept;

    // Sets the axiom, the rules and the angle to a preset's.
    void UseLSystemPreset(_Inout_ GeneratorSettings& generator, _In_ size_t preset);

    // The preset the settings match, or SIZE_MAX.
    size_t MatchingLSystemPreset(_In_ GeneratorSettings const& generator) noexcept;

    // The path an L-system's turtle draws, a line for each step it takes. Step is the step that
    // line plays; lines that don't play (a lowercase letter) are rests.
    struct TurtleLine
    {
        float X0{ 0 };
        float Y0{ 0 };
        float X1{ 0 };
        float Y1{ 0 };
        int64_t Step{ 0 };
        bool Plays{ true };
    };

    struct TurtleDrawing
    {
        std::vector<TurtleLine> Lines{};
        int32_t Generations{ 0 };
        int64_t Steps{ 0 };             // how long the melody it grew is, in steps
    };

    TurtleDrawing LSystemTurtle(_In_ GeneratorSettings const& generator);

    // The rows of a method's plain course, from rounds, bell 1 (the treble) first. Every row is
    // the bells' order, 1 to Bells.
    std::vector<std::vector<uint8_t>> RingingCourse(_In_ GeneratorSettings const& generator);

    // Where a course's row starts, in steps, with the handstroke gaps.
    int64_t RingingRowStep(_In_ GeneratorSettings const& generator, _In_ size_t row) noexcept;

    // The note each bell rings, bell 1 first.
    std::vector<uint8_t> BellNotes(_In_ GeneratorSettings const& generator);

    // ---- the passes, for GeneratedPass (in each generator's file) ----

    std::vector<StepNote> LifePass(_In_ GeneratorSettings const& generator, _In_ uint32_t seed, _In_ int64_t steps, _In_ uint64_t pass, _In_opt_ PassHearing const* hearing = nullptr);
    std::vector<StepNote> LearnPass(_In_ GeneratorSettings const& generator, _In_ uint32_t seed, _In_ int64_t steps, _In_ uint64_t pass);
    std::vector<StepNote> WordsPass(_In_ GeneratorSettings const& generator, _In_ int64_t steps, _In_ uint64_t pass);
    std::vector<StepNote> EvolvePass(_In_ GeneratorSettings const& generator, _In_ uint32_t seed, _In_ int64_t steps, _In_ uint64_t pass, _In_opt_ PassHearing const* hearing = nullptr);
    std::vector<StepNote> InfinityPass(_In_ GeneratorSettings const& generator, _In_ uint32_t seed, _In_ int64_t steps, _In_ uint64_t pass);
    std::vector<StepNote> LSystemPass(_In_ GeneratorSettings const& generator, _In_ int64_t steps, _In_ uint64_t pass);
    std::vector<StepNote> RingingPass(_In_ GeneratorSettings const& generator, _In_ int64_t steps, _In_ uint64_t pass);

    // ---- shared by the files above ----

    // The same roll of the dice the other generators use: one number for a seed, a pass, a step
    // and what it's for.
    uint64_t GeneratorRoll(_In_ uint32_t seed, _In_ uint64_t pass, _In_ uint64_t step, _In_ uint64_t purpose) noexcept;

    // A number for every setting that changes what a generator plays, for keeping results.
    uint64_t GeneratorFingerprint(_In_ GeneratorSettings const& generator, _In_ uint32_t seed, _In_ int64_t steps) noexcept;
}
