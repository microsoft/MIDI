// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, WinRT and XAML, so the unit tests compile it unchanged.

#include "CalcPitch.h"

#include <cmath>
#include <cwctype>

namespace midicalc
{
    namespace
    {
        constexpr wchar_t Sharp = L'\u266F';
        constexpr wchar_t Flat = L'\u266D';

        // 5-limit ratios: the just intonation table, and the "pure" interval each equal-tempered
        // interval is compared with.
        constexpr double PureRatios[12]{
            1.0, 16.0 / 15.0, 9.0 / 8.0, 6.0 / 5.0, 5.0 / 4.0, 4.0 / 3.0,
            45.0 / 32.0, 3.0 / 2.0, 8.0 / 5.0, 5.0 / 3.0, 9.0 / 5.0, 15.0 / 8.0 };

        // Pythagorean: a chain of pure fifths from the root, folded into one octave.
        constexpr double PythagoreanRatios[12]{
            1.0, 256.0 / 243.0, 9.0 / 8.0, 32.0 / 27.0, 81.0 / 64.0, 4.0 / 3.0,
            729.0 / 512.0, 3.0 / 2.0, 128.0 / 81.0, 27.0 / 16.0, 16.0 / 9.0, 243.0 / 128.0 };

        // Werckmeister III ("Werckmeister I (III)" in his own numbering), in cents above C.
        constexpr double WerckmeisterCents[12]{
            0.0, 90.225, 192.180, 294.135, 390.225, 498.045,
            588.270, 696.090, 792.180, 888.270, 996.090, 1092.180 };

        double RatioToCents(_In_ double ratio) noexcept
        {
            return 1200.0 * std::log2(ratio);
        }

        int32_t PitchClass(_In_ int32_t value) noexcept
        {
            return ((value % 12) + 12) % 12;
        }

        std::wstring_view Trim(_In_ std::wstring_view text) noexcept
        {
            auto const first = text.find_first_not_of(L" \t");

            if (first == std::wstring_view::npos)
            {
                return {};
            }

            return text.substr(first, text.find_last_not_of(L" \t") - first + 1);
        }

        std::optional<int32_t> ReadWhole(_In_ std::wstring_view text) noexcept
        {
            text = Trim(text);

            auto negative = false;

            if (!text.empty() && (text.front() == L'-' || text.front() == L'\u2212'))
            {
                negative = true;
                text.remove_prefix(1);
            }

            if (text.empty() || text.size() > 4)
            {
                return std::nullopt;
            }

            int32_t value{ 0 };

            for (auto const c : text)
            {
                if (c < L'0' || c > L'9')
                {
                    return std::nullopt;
                }

                value = value * 10 + (c - L'0');
            }

            return negative ? -value : value;
        }

        uint8_t CheckedMiddleC(_In_ uint8_t middleCOctave) noexcept
        {
            return (middleCOctave >= 1 && middleCOctave <= 7) ? middleCOctave : DefaultMiddleCOctave;
        }
    }

    _Use_decl_annotations_
    int32_t NoteOctave(int32_t note, uint8_t middleCOctave) noexcept
    {
        auto const whole = note >= 0 ? note / 12 : -((11 - note) / 12);
        // note 60 is middle C, which is octave 5 when counting from note 0
        return whole - (5 - static_cast<int32_t>(CheckedMiddleC(middleCOctave)));
    }

    _Use_decl_annotations_
    std::wstring NoteName(int32_t note, uint8_t middleCOctave, bool flats)
    {
        if (note < 0 || note > 127)
        {
            return {};
        }

        static constexpr wchar_t Letters[12]{ L'C', L'C', L'D', L'D', L'E', L'F', L'F', L'G', L'G', L'A', L'A', L'B' };
        static constexpr bool Black[12]{ false, true, false, true, false, false, true, false, true, false, true, false };

        auto const pc = PitchClass(note);
        std::wstring name;

        if (Black[pc] && flats)
        {
            name.push_back(Letters[pc + 1]);
            name.push_back(Flat);
        }
        else
        {
            name.push_back(Letters[pc]);

            if (Black[pc])
            {
                name.push_back(Sharp);
            }
        }

        name += std::to_wstring(NoteOctave(note, middleCOctave));
        return name;
    }

    _Use_decl_annotations_
    std::wstring NoteNameWithNumber(int32_t note, uint8_t middleCOctave)
    {
        if (note < 0 || note > 127)
        {
            return {};
        }

        return NoteName(note, middleCOctave, false) + L" (" + std::to_wstring(note) + L")";
    }

    _Use_decl_annotations_
    std::optional<int32_t> ParseNote(std::wstring_view text, uint8_t middleCOctave) noexcept
    {
        text = Trim(text);

        if (text.empty())
        {
            return std::nullopt;
        }

        // a number in brackets wins: it's what a box showing "A3 (69)" gives back untouched
        if (auto const open = text.find(L'('); open != std::wstring_view::npos)
        {
            auto const close = text.find(L')', open);
            text = Trim(text.substr(open + 1, close == std::wstring_view::npos ? std::wstring_view::npos : close - open - 1));
        }

        if (text.empty())
        {
            return std::nullopt;
        }

        std::optional<int32_t> number{};

        if (std::iswdigit(text.front()))
        {
            number = ReadWhole(text);
        }
        else
        {
            static constexpr int32_t Pitches[7]{ 9, 11, 0, 2, 4, 5, 7 };

            auto const letter = static_cast<wchar_t>(std::towupper(text.front()));

            if (letter < L'A' || letter > L'G')
            {
                return std::nullopt;
            }

            auto pitch = Pitches[letter - L'A'];
            size_t at{ 1 };

            while (at < text.size() && at < 3)
            {
                auto const c = text[at];

                if (c == L'#' || c == Sharp)
                {
                    ++pitch;
                }
                else if (c == L'b' || c == Flat)
                {
                    --pitch;
                }
                else
                {
                    break;
                }

                ++at;
            }

            auto const octave = ReadWhole(text.substr(at));

            if (!octave.has_value())
            {
                return std::nullopt;
            }

            number = (*octave + 5 - static_cast<int32_t>(CheckedMiddleC(middleCOctave))) * 12 + pitch;
        }

        if (!number.has_value() || *number < 0 || *number > 127)
        {
            return std::nullopt;
        }

        return number;
    }

    _Use_decl_annotations_
    std::array<double, 12> TemperamentCents(Temperament temperament) noexcept
    {
        std::array<double, 12> cents{};

        switch (temperament)
        {
        case Temperament::JustIntonation:
            for (size_t i = 0; i < 12; ++i)
            {
                cents[i] = RatioToCents(PureRatios[i]);
            }
            break;

        case Temperament::Pythagorean:
            for (size_t i = 0; i < 12; ++i)
            {
                cents[i] = RatioToCents(PythagoreanRatios[i]);
            }
            break;

        case Temperament::QuarterCommaMeantone:
        {
            // fifths a quarter of a syntonic comma narrow, so four of them make a pure major
            // third; the chain runs from the minor third (three fifths down) to the augmented
            // fifth (eight up), the usual layout
            auto const fifth = 1200.0 * std::log2(5.0) / 4.0;

            for (int32_t k = -3; k <= 8; ++k)
            {
                auto value = std::fmod(k * fifth, 1200.0);

                if (value < 0.0)
                {
                    value += 1200.0;
                }

                cents[static_cast<size_t>(PitchClass(7 * k))] = value;
            }
            break;
        }

        case Temperament::WerckmeisterIII:
            for (size_t i = 0; i < 12; ++i)
            {
                cents[i] = WerckmeisterCents[i];
            }
            break;

        default:
            for (size_t i = 0; i < 12; ++i)
            {
                cents[i] = 100.0 * static_cast<double>(i);
            }
            break;
        }

        return cents;
    }

    _Use_decl_annotations_
    double CentsFromEqual(Temperament temperament, int32_t root, int32_t pitchClass) noexcept
    {
        if (temperament == Temperament::Equal)
        {
            return 0.0;
        }

        auto const cents = TemperamentCents(temperament);
        auto const offset = [&](int32_t pc)
        {
            auto const relative = PitchClass(pc - root);
            return cents[static_cast<size_t>(relative)] - 100.0 * relative;
        };

        return offset(pitchClass) - offset(PitchClass(ReferenceNote));
    }

    _Use_decl_annotations_
    double NoteFrequency(double note, Tuning const& tuning) noexcept
    {
        if (!std::isfinite(note) || !(tuning.ReferenceHertz > 0.0))
        {
            return 0.0;
        }

        auto const whole = std::floor(note);
        auto const fraction = note - whole;
        auto const wholeNote = static_cast<int32_t>(whole);

        auto const cents =
            (wholeNote - ReferenceNote) * 100.0 +
            CentsFromEqual(tuning.Kind, tuning.Root, PitchClass(wholeNote)) +
            fraction * 100.0;

        return tuning.ReferenceHertz * std::exp2(cents / 1200.0);
    }

    _Use_decl_annotations_
    double FractionalNote(double hertz, double referenceHertz) noexcept
    {
        if (!(hertz > 0.0) || !(referenceHertz > 0.0))
        {
            return 0.0;
        }

        return ReferenceNote + 12.0 * std::log2(hertz / referenceHertz);
    }

    _Use_decl_annotations_
    std::optional<NearestNote> NearestNoteToFrequency(double hertz, Tuning const& tuning) noexcept
    {
        if (!(hertz > 0.0) || !std::isfinite(hertz) || !(tuning.ReferenceHertz > 0.0))
        {
            return std::nullopt;
        }

        auto const guess = static_cast<int32_t>(std::lround(FractionalNote(hertz, tuning.ReferenceHertz)));
        std::optional<NearestNote> best{};

        for (auto candidate = guess - 1; candidate <= guess + 1; ++candidate)
        {
            if (candidate < 0 || candidate > 127)
            {
                continue;
            }

            auto const cents = 1200.0 * std::log2(hertz / NoteFrequency(candidate, tuning));

            if (!best.has_value() || std::abs(cents) < std::abs(best->Cents))
            {
                best = NearestNote{ candidate, cents };
            }
        }

        // 50 cents is as far as a note can be from its nearest neighbor, give or take what a
        // tuning moves things; past that, the frequency is off the end of the note range
        if (!best.has_value() || std::abs(best->Cents) > 75.0)
        {
            return std::nullopt;
        }

        return best;
    }

    _Use_decl_annotations_
    Interval IntervalBetween(int32_t fromNote, int32_t toNote) noexcept
    {
        Interval interval{};
        interval.Semitones = toNote - fromNote;

        auto const distance = interval.Semitones < 0 ? -interval.Semitones : interval.Semitones;
        interval.NameIndex = distance % 12;
        interval.Octaves = distance / 12;
        interval.EqualRatio = std::exp2(distance / 12.0);
        interval.PureRatio = PureRatios[interval.NameIndex] * std::exp2(static_cast<double>(interval.Octaves));
        interval.PureMinusEqualCents = RatioToCents(interval.PureRatio) - 100.0 * distance;

        return interval;
    }
}
