// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The shapes an LFO sweeps. Deliberately free of pch.h, WinRT and XAML, so the unit tests compile
// it unchanged. MIDI Glass draws its LFO control with these and sends what they give, and MIDI
// Patchbay's LFO step sends the same, so one wave is the same wave in both apps.

#include <sal.h>
#include <cstdint>
#include <optional>
#include <string_view>

namespace midiapp
{
    // The shape an LFO sweeps. The first five repeat, so one cycle of them can be drawn with a
    // bead running along it; the noises do not, so there is no cycle to draw and the bead only
    // moves up and down.
    enum class LfoWave
    {
        Sine = 0,
        Triangle = 1,
        Square = 2,

        // A sawtooth. Up is the one that climbs and falls off a cliff; down is the reverse.
        RampUp = 3,
        RampDown = 4,

        // Flat spectrum. Every sample is as likely to be anything as any other.
        WhiteNoise = 5,

        // Falls away three decibels an octave. Sounds and reads as more natural than white.
        PinkNoise = 6,

        // Six decibels an octave: a random walk, so it wanders rather than jumps. Also called
        // red noise, which is the same thing under a different name rather than a sixth shape.
        BrownNoise = 7,

        // The mirror of pink. Rises three decibels an octave, so it is all jitter and no drift.
        BlueNoise = 8,
    };

    // The order the waves are offered in, shallowest shape first and the noises last. One copy,
    // because the list a picker shows and the list an edit reads back have to agree.
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

    // One pass may not be so slow that nobody can tell it is running, nor so fast that the
    // update rate is doing all the shaping.
    constexpr double MinimumLfoBeatsPerCycle = 0.0625;
    constexpr double MaximumLfoBeatsPerCycle = 64.0;

    // How often a value is taken off the wave and sent. A sweep is not audio: past about forty a
    // second nobody can hear the difference and a DIN cable certainly cannot carry it.
    constexpr int32_t MinimumLfoIntervalMilliseconds = 5;
    constexpr int32_t MaximumLfoIntervalMilliseconds = 1000;
    constexpr int32_t DefaultLfoIntervalMilliseconds = 25;

    // The tempo a sweep's length is measured against.
    constexpr double MinimumLfoBeatsPerMinute = 20.0;
    constexpr double MaximumLfoBeatsPerMinute = 300.0;

    // Whether this shape repeats. The five periodic ones can be drawn as a single cycle with a
    // bead running along it; noise cannot, because there is no cycle to draw.
    bool LfoWaveRepeats(_In_ LfoWave wave) noexcept;

    // Where the wave sits at this point in its cycle, 0 to 1, with 0.5 as the middle.
    //
    // Phase is 0 to 1 over one pass and is wrapped rather than clamped, so a caller can hand in
    // elapsed cycles and get the right answer. Noise returns 0.5 here: it has no shape, and the
    // value of a noise sample comes from LfoNoise instead.
    double LfoWaveAt(_In_ LfoWave wave, _In_ double phase) noexcept;

    // The value a sweep sends, after its two ends are applied. A repeating wave is read at the
    // phase; a noise wave uses noiseSample, which comes from LfoNoise. Lowest above highest turns
    // the wave upside down, which falls out of the arithmetic rather than needing its own switch.
    double LfoSweepValue(
        _In_ LfoWave wave,
        _In_ double phase,
        _In_ double noiseSample,
        _In_ double lowest,
        _In_ double highest) noexcept;

    // The names files use, "sine" through "blueNoise". MIDI Glass layouts and MIDI Patchbay
    // patches spell them the same way.
    std::wstring_view LfoWaveKey(_In_ LfoWave wave) noexcept;
    std::optional<LfoWave> LfoWaveFromKey(_In_ std::wstring_view key) noexcept;

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
