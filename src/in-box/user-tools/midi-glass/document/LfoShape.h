// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML. The arithmetic an LFO runs on is the same arithmetic the
// control draws itself with, so it lives here where both can reach it and where it is tested
// without a window.

#include <sal.h>
#include <cstdint>

#include "LayoutModel.h"

namespace glass
{
    // The order the waves are offered in, shallowest shape first and the noises last. One copy,
    // because the list the inspector shows and the list an edit reads back have to agree.
    constexpr LfoWave LfoWaveOrder[]
    {
        LfoWave::Sine,
        LfoWave::Triangle,
        LfoWave::Square,
        LfoWave::RampUp,
        LfoWave::RampDown,
        LfoWave::WhiteNoise,
        LfoWave::PinkNoise,
        LfoWave::BrownNoise,
        LfoWave::BlueNoise,
    };

    // How long one pass takes, in quarter notes. Musical figures rather than a free number, so a
    // sweep lines up with the music instead of drifting through it.
    constexpr double LfoRateChoices[]
    {
        0.25, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 8.0, 16.0, 32.0,
    };

    // Whether this shape repeats. The five periodic ones can be drawn as a single cycle with a
    // bead running along it; noise cannot, because there is no cycle to draw.
    bool LfoWaveRepeats(_In_ LfoWave wave) noexcept;

    // Where the wave sits at this point in its cycle, 0 to 1, with 0.5 as the middle.
    //
    // Phase is 0 to 1 over one pass and is wrapped rather than clamped, so a caller can hand in
    // elapsed cycles and get the right answer. Noise returns 0.5 here: it has no shape, and the
    // value of a noise sample comes from LfoNoise instead.
    double LfoWaveAt(_In_ LfoWave wave, _In_ double phase) noexcept;

    // The value an LFO actually sends, after the sweep's two ends are applied. Lowest above
    // highest turns the wave upside down, which falls out of the arithmetic rather than needing
    // its own switch.
    double LfoValueAt(_In_ LfoSpec const& spec, _In_ double phase, _In_ double waveValue) noexcept;

    // One noise source per running LFO.
    //
    // Noise is stateful: pink and brown are white passed through a filter, and blue is the
    // difference between one white sample and the last, so none of them can be worked out from
    // a phase the way a sine can. Each call is one sample.
    class LfoNoise
    {
    public:
        explicit LfoNoise(_In_ uint32_t seed = 0x9E3779B9u) noexcept;

        // The next sample, 0 to 1 with 0.5 as the middle.
        double Next(_In_ LfoWave wave) noexcept;

        void Reset() noexcept;

    private:
        double NextWhite() noexcept;

        uint32_t m_state{ 0x9E3779B9u };
        uint32_t m_seed{ 0x9E3779B9u };

        // Paul Kellet's economy pink filter: three poles, which is close enough to three
        // decibels an octave for something nobody is going to measure.
        double m_pink[3]{};

        // The running total a brown walk wanders around, and the last white sample a blue one
        // differentiates against.
        double m_brown{ 0.0 };
        double m_lastWhite{ 0.0 };
    };
}
