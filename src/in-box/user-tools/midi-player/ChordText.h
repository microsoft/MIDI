// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Chord symbols moved into the key the music is transposed to. Plain C++ with no WinRT, so the
// unit tests can check it directly.

#include <sal.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace midiplayer
{
    namespace chordtext
    {
        constexpr std::array<std::string_view, 12> SharpNames{ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        constexpr std::array<std::string_view, 12> FlatNames{ "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

        // The names lead sheets use most in C, where there is no key signature to follow.
        constexpr std::array<std::string_view, 12> CommonNames{ "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

        constexpr int32_t PitchClass(int32_t value) noexcept
        {
            return ((value % 12) + 12) % 12;
        }

        // A note name such as "F", "Bb" or "C#" at the start of text: its pitch class, and how
        // many characters it took. Nothing when text does not start with one.
        inline std::optional<std::pair<int32_t, size_t>> ReadNoteName(_In_ std::string_view text) noexcept
        {
            if (text.empty())
            {
                return std::nullopt;
            }

            int32_t pitch{ 0 };

            switch (text[0])
            {
            case 'C': pitch = 0; break;
            case 'D': pitch = 2; break;
            case 'E': pitch = 4; break;
            case 'F': pitch = 5; break;
            case 'G': pitch = 7; break;
            case 'A': pitch = 9; break;
            case 'B': pitch = 11; break;
            default: return std::nullopt;
            }

            // No chord quality starts with a lower case b, so one straight after the letter is
            // always a flat. Two at most, for a double sharp or flat.
            size_t used{ 1 };

            while (used < text.size() && used < 3 && (text[used] == '#' || text[used] == 'b'))
            {
                pitch += text[used] == '#' ? 1 : -1;
                ++used;
            }

            return std::make_pair(PitchClass(pitch), used);
        }
    }

    // The key signature music lands in once it is transposed, as a count of sharps, or of flats
    // when negative. Where a key can be written two ways, such as D flat and C sharp, the way
    // with fewer accidentals wins. A tie, such as F sharp and G flat, stays on the side the
    // original key was on.
    inline int32_t TransposeKeySignature(_In_ int32_t accidentals, _In_ int32_t semitones) noexcept
    {
        accidentals = std::clamp(accidentals, -7, 7);

        // Each sharp in a key signature moves its major key up a fifth, seven semitones.
        auto const tonic = chordtext::PitchClass(7 * accidentals + semitones);

        int32_t best{ 0 };
        auto found = false;

        for (int32_t candidate = -7; candidate <= 7; ++candidate)
        {
            if (chordtext::PitchClass(7 * candidate) != tonic)
            {
                continue;
            }

            auto const better = !found ||
                std::abs(candidate) < std::abs(best) ||
                (std::abs(candidate) == std::abs(best) && (candidate < 0) == (accidentals < 0));

            if (better)
            {
                best = candidate;
                found = true;
            }
        }

        return best;
    }

    // A chord symbol such as "Bb", "F#m7" or "D/F#" moved by a number of semitones. The root, and
    // a bass note after a slash, move. What is between them, such as "m7", stays as it is.
    //
    // keySignature is the key the music has moved to, from TransposeKeySignature, and decides
    // whether the new names use flats or sharps. Anything that does not start with a note name,
    // such as "N.C.", comes back unchanged.
    inline std::string TransposeChordSymbol(
        _In_ std::string_view symbol,
        _In_ int32_t semitones,
        _In_ int32_t keySignature)
    {
        auto const shift = chordtext::PitchClass(semitones);

        auto const root = chordtext::ReadNoteName(symbol);

        if (shift == 0 || !root.has_value())
        {
            return std::string{ symbol };
        }

        auto const& names = keySignature < 0
            ? chordtext::FlatNames
            : (keySignature > 0 ? chordtext::SharpNames : chordtext::CommonNames);

        std::string result{ names[static_cast<size_t>(chordtext::PitchClass(root->first + shift))] };

        auto const rest = symbol.substr(root->second);
        auto const slash = rest.rfind('/');

        // A slash can also be part of the chord, as in "C6/9". It is only a bass note when a whole
        // note name follows it.
        auto const bass = slash == std::string_view::npos
            ? std::optional<std::pair<int32_t, size_t>>{}
            : chordtext::ReadNoteName(rest.substr(slash + 1));

        if (!bass.has_value() || slash + 1 + bass->second != rest.size())
        {
            result.append(rest);
            return result;
        }

        result.append(rest.substr(0, slash + 1));
        result.append(names[static_cast<size_t>(chordtext::PitchClass(bass->first + shift))]);

        return result;
    }
}
