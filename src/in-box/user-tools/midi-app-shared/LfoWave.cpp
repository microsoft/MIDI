// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, WinRT and XAML, so the unit tests compile it unchanged.

#include "LfoWave.h"

#include <algorithm>
#include <cmath>

namespace midiapp
{
    namespace
    {
        constexpr double Tau = 6.283185307179586476925286766559;

        double Wrap(_In_ double phase) noexcept
        {
            if (!std::isfinite(phase))
            {
                return 0.0;
            }

            auto const wrapped = phase - std::floor(phase);

            // floor of a tiny negative number can land exactly on 1.0 after the subtraction.
            return wrapped >= 1.0 ? 0.0 : wrapped;
        }

        struct WaveName
        {
            LfoWave Wave;
            wchar_t const* Key;
        };

        constexpr WaveName WaveNames[]
        {
            { LfoWave::Sine, L"sine" },
            { LfoWave::Triangle, L"triangle" },
            { LfoWave::Square, L"square" },
            { LfoWave::RampUp, L"rampUp" },
            { LfoWave::RampDown, L"rampDown" },
            { LfoWave::WhiteNoise, L"whiteNoise" },
            { LfoWave::PinkNoise, L"pinkNoise" },
            { LfoWave::BrownNoise, L"brownNoise" },
            { LfoWave::BlueNoise, L"blueNoise" },
        };
    }

    _Use_decl_annotations_
    bool LfoWaveRepeats(LfoWave wave) noexcept
    {
        switch (wave)
        {
        case LfoWave::Sine:
        case LfoWave::Triangle:
        case LfoWave::Square:
        case LfoWave::RampUp:
        case LfoWave::RampDown:
            return true;

        default:
            return false;
        }
    }

    _Use_decl_annotations_
    double LfoWaveAt(LfoWave wave, double phase) noexcept
    {
        auto const t = Wrap(phase);

        switch (wave)
        {
        case LfoWave::Sine:
            return 0.5 + 0.5 * std::sin(Tau * t);

        // Same phase reference as the sine: starts in the middle, climbs to the top a quarter
        // of the way through, and is back in the middle at the halfway point. A triangle whose
        // corners did not line up with the sine's peaks would look wrong switching between them.
        case LfoWave::Triangle:
            if (t < 0.25)
            {
                return 0.5 + 2.0 * t;
            }

            if (t < 0.75)
            {
                return 1.5 - 2.0 * t;
            }

            return 2.0 * t - 1.5;

        case LfoWave::Square:
            return t < 0.5 ? 1.0 : 0.0;

        case LfoWave::RampUp:
            return t;

        case LfoWave::RampDown:
            return 1.0 - t;

        // Noise has no shape. The middle is the honest answer for anything that asks a noise
        // wave where it is at a given point in a cycle it does not have.
        default:
            return 0.5;
        }
    }

    _Use_decl_annotations_
    double LfoSweepValue(LfoWave wave, double phase, double noiseSample, double lowest, double highest) noexcept
    {
        auto const shape = std::clamp(
            LfoWaveRepeats(wave) ? LfoWaveAt(wave, phase) : noiseSample, 0.0, 1.0);

        auto const low = std::clamp(lowest, 0.0, 1.0);
        auto const high = std::clamp(highest, 0.0, 1.0);

        return std::clamp(low + shape * (high - low), 0.0, 1.0);
    }

    _Use_decl_annotations_
    std::wstring_view LfoWaveKey(LfoWave wave) noexcept
    {
        for (auto const& entry : WaveNames)
        {
            if (entry.Wave == wave)
            {
                return entry.Key;
            }
        }

        return WaveNames[0].Key;
    }

    _Use_decl_annotations_
    std::optional<LfoWave> LfoWaveFromKey(std::wstring_view key) noexcept
    {
        for (auto const& entry : WaveNames)
        {
            if (key == entry.Key)
            {
                return entry.Wave;
            }
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    LfoNoise::LfoNoise(uint32_t seed) noexcept
        : m_state{ seed == 0 ? 0x9E3779B9u : seed }
        , m_seed{ seed == 0 ? 0x9E3779B9u : seed }
    {
    }

    void LfoNoise::Reset() noexcept
    {
        m_state = m_seed;
        m_pink[0] = 0.0;
        m_pink[1] = 0.0;
        m_pink[2] = 0.0;
        m_brown = 0.0;
        m_lastWhite = 0.0;
    }

    double LfoNoise::NextWhite() noexcept
    {
        // xorshift32. Nothing here is a secret, so an ordinary pseudo-random sequence is the
        // right tool; a cryptographic one would cost more and sound identical.
        m_state ^= m_state << 13;
        m_state ^= m_state >> 17;
        m_state ^= m_state << 5;

        // -1 to 1.
        return static_cast<double>(m_state) / 2147483648.0 - 1.0;
    }

    _Use_decl_annotations_
    double LfoNoise::Next(LfoWave wave) noexcept
    {
        auto const white = NextWhite();

        switch (wave)
        {
        case LfoWave::PinkNoise:
        {
            m_pink[0] = 0.99765 * m_pink[0] + white * 0.0990460;
            m_pink[1] = 0.96300 * m_pink[1] + white * 0.2965164;
            m_pink[2] = 0.57000 * m_pink[2] + white * 1.0526913;

            auto const pink = m_pink[0] + m_pink[1] + m_pink[2] + white * 0.1848;

            // The filter's output runs a little wider than the white it was fed, so it is
            // brought back before it is clamped. Without this a pink sweep spends much of its
            // time parked against one end.
            return std::clamp(0.5 + pink * 0.14, 0.0, 1.0);
        }

        case LfoWave::BrownNoise:
        {
            // A random walk, pulled gently back toward the middle so it cannot wander off and
            // sit against an end for the rest of the set. Measured on the wire: tighter than
            // this and a sweep set to the full range only ever used a third of it, which reads
            // as a broken control rather than as gentle noise.
            m_brown = std::clamp(m_brown * 0.985 + white * 0.09, -1.0, 1.0);

            return std::clamp(0.5 + m_brown * 0.5, 0.0, 1.0);
        }

        case LfoWave::BlueNoise:
        {
            auto const difference = white - m_lastWhite;

            m_lastWhite = white;

            return std::clamp(0.5 + difference * 0.25, 0.0, 1.0);
        }

        default:
            return std::clamp(0.5 + white * 0.5, 0.0, 1.0);
        }
    }
}
