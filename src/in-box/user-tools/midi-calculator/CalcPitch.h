// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Notes, frequencies, intervals and tunings. Plain C++ with no WinRT or XAML, so the unit tests
// compile it unchanged.
//
// Middle C is note 60. It's C3 unless the customer picks another octave, the same default as the
// Windows MIDI Services API (MidiMessageHelper::GetNoteOctaveFromNoteIndex), so note 0 is C-2.

#include <sal.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace midicalc
{
    inline constexpr uint8_t DefaultMiddleCOctave = 3;
    inline constexpr double DefaultReferenceHertz = 440.0;

    // The note the tuning reference is, A above middle C.
    inline constexpr int32_t ReferenceNote = 69;

    // The octave number of a note 0 to 127 when note 60 is called C in octave middleCOctave
    // (1 to 7; anything else counts as DefaultMiddleCOctave).
    int32_t NoteOctave(_In_ int32_t note, _In_ uint8_t middleCOctave) noexcept;

    // "A3", "C♯-1", "B♭4". Empty for a note outside 0 to 127.
    std::wstring NoteName(_In_ int32_t note, _In_ uint8_t middleCOctave, _In_ bool flats);

    // "A3 (69)". Empty for a note outside 0 to 127.
    std::wstring NoteNameWithNumber(_In_ int32_t note, _In_ uint8_t middleCOctave);

    // A note typed as a number ("69") or a name ("A3", "Bb1", "F♯0", "A3 (69)"), read with the
    // customer's middle C. Empty when it isn't a note, or names one outside 0 to 127.
    std::optional<int32_t> ParseNote(_In_ std::wstring_view text, _In_ uint8_t middleCOctave) noexcept;

    // ---- tunings ----

    enum class Temperament : int32_t
    {
        Equal = 0,
        JustIntonation = 1,
        Pythagorean = 2,
        QuarterCommaMeantone = 3,
        WerckmeisterIII = 4
    };

    inline constexpr int32_t TemperamentCount = 5;

    // Each pitch class's distance above the tuning's root, in cents, root first.
    std::array<double, 12> TemperamentCents(_In_ Temperament temperament) noexcept;

    // How far a pitch class sits from equal temperament in a tuning built on root (0 = C), in
    // cents. Shifted so the reference note's pitch class (A) is never moved, so A3 stays at the
    // reference frequency in every tuning.
    double CentsFromEqual(_In_ Temperament temperament, _In_ int32_t root, _In_ int32_t pitchClass) noexcept;

    struct Tuning
    {
        double ReferenceHertz{ DefaultReferenceHertz };
        Temperament Kind{ Temperament::Equal };
        int32_t Root{ 0 };
    };

    // The frequency of a note, which may have a fraction (69.5 is a quarter tone above A3). The
    // whole part follows the tuning; the fraction is added in equal-tempered cents.
    double NoteFrequency(_In_ double note, _In_ Tuning const& tuning) noexcept;

    struct NearestNote
    {
        int32_t Note{ 0 };
        // how far the frequency is above (positive) or below the note, in cents
        double Cents{ 0.0 };
    };

    // The note closest to a frequency in the tuning, and how far off it is. Empty for a frequency
    // that isn't above zero, or is outside notes 0 to 127 by more than half a semitone.
    std::optional<NearestNote> NearestNoteToFrequency(_In_ double hertz, _In_ Tuning const& tuning) noexcept;

    // The note number with a fraction, in equal temperament (450 Hz is 69.389 at A = 440).
    double FractionalNote(_In_ double hertz, _In_ double referenceHertz) noexcept;

    // ---- intervals ----

    struct Interval
    {
        int32_t Semitones{ 0 };
        // 0 to 11: unison, minor second ... major seventh; the UI turns it into a name
        int32_t NameIndex{ 0 };
        int32_t Octaves{ 0 };
        double EqualRatio{ 1.0 };
        double PureRatio{ 1.0 };
        // how much wider (positive) the pure interval is than the equal-tempered one
        double PureMinusEqualCents{ 0.0 };
    };

    // Between two notes, either way round. Semitones keeps its sign; the rest describe the
    // distance.
    Interval IntervalBetween(_In_ int32_t fromNote, _In_ int32_t toNote) noexcept;
}
