// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The two generators: the shape an LFO sweeps, and the angles a platter is pushed through.
// Both are pure arithmetic, which is why they live where a test can reach them.

#include "GeneratorTests.h"

#include "LfoShape.h"
#include "InputRules.h"

#include <cmath>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    constexpr double Tolerance = 0.0005;

    bool Near(_In_ double actual, _In_ double expected) noexcept
    {
        return std::abs(actual - expected) < Tolerance;
    }

    glass::LfoSpec Sweep(_In_ double lowest, _In_ double highest, _In_ glass::LfoWave wave)
    {
        glass::LfoSpec spec{};

        spec.Wave = wave;
        spec.Lowest = lowest;
        spec.Highest = highest;

        return spec;
    }
}

void GeneratorTests::ASineStartsInTheMiddleAndPeaksAtAQuarter()
{
    VERIFY_IS_TRUE(Near(glass::LfoWaveAt(glass::LfoWave::Sine, 0.0), 0.5));
    VERIFY_IS_TRUE(Near(glass::LfoWaveAt(glass::LfoWave::Sine, 0.25), 1.0));
    VERIFY_IS_TRUE(Near(glass::LfoWaveAt(glass::LfoWave::Sine, 0.5), 0.5));
    VERIFY_IS_TRUE(Near(glass::LfoWaveAt(glass::LfoWave::Sine, 0.75), 0.0));
}

void GeneratorTests::ATriangleTurnsAtTheSamePlacesTheSineDoes()
{
    // The corners have to land on the sine's peaks, or switching between the two shapes looks
    // like the phase jumped.
    VERIFY_IS_TRUE(Near(glass::LfoWaveAt(glass::LfoWave::Triangle, 0.0), 0.5));
    VERIFY_IS_TRUE(Near(glass::LfoWaveAt(glass::LfoWave::Triangle, 0.25), 1.0));
    VERIFY_IS_TRUE(Near(glass::LfoWaveAt(glass::LfoWave::Triangle, 0.5), 0.5));
    VERIFY_IS_TRUE(Near(glass::LfoWaveAt(glass::LfoWave::Triangle, 0.75), 0.0));

    // And it is straight between them, which a sine is not.
    VERIFY_IS_TRUE(Near(glass::LfoWaveAt(glass::LfoWave::Triangle, 0.125), 0.75));
}

void GeneratorTests::ASquareIsHighForTheFirstHalf()
{
    VERIFY_IS_TRUE(Near(glass::LfoWaveAt(glass::LfoWave::Square, 0.0), 1.0));
    VERIFY_IS_TRUE(Near(glass::LfoWaveAt(glass::LfoWave::Square, 0.49), 1.0));
    VERIFY_IS_TRUE(Near(glass::LfoWaveAt(glass::LfoWave::Square, 0.5), 0.0));
    VERIFY_IS_TRUE(Near(glass::LfoWaveAt(glass::LfoWave::Square, 0.99), 0.0));
}

void GeneratorTests::TheTwoRampsAreMirrorImages()
{
    for (auto const phase : { 0.0, 0.2, 0.5, 0.8 })
    {
        VERIFY_IS_TRUE(Near(
            glass::LfoWaveAt(glass::LfoWave::RampUp, phase) +
            glass::LfoWaveAt(glass::LfoWave::RampDown, phase),
            1.0));
    }
}

void GeneratorTests::PhaseWrapsRatherThanClamping()
{
    // The generator hands in elapsed cycles rather than a fraction, so 2.25 has to be the same
    // point as 0.25. Clamping here would park every sweep at its top end after one pass.
    VERIFY_IS_TRUE(Near(
        glass::LfoWaveAt(glass::LfoWave::Sine, 2.25),
        glass::LfoWaveAt(glass::LfoWave::Sine, 0.25)));

    VERIFY_IS_TRUE(Near(
        glass::LfoWaveAt(glass::LfoWave::RampUp, -0.25),
        glass::LfoWaveAt(glass::LfoWave::RampUp, 0.75)));
}

void GeneratorTests::NoiseHasNoShapeToReadAtAPhase()
{
    VERIFY_IS_TRUE(Near(glass::LfoWaveAt(glass::LfoWave::WhiteNoise, 0.3), 0.5));
    VERIFY_IS_TRUE(Near(glass::LfoWaveAt(glass::LfoWave::PinkNoise, 0.9), 0.5));
}

void GeneratorTests::OnlyThePeriodicShapesRepeat()
{
    VERIFY_IS_TRUE(glass::LfoWaveRepeats(glass::LfoWave::Sine));
    VERIFY_IS_TRUE(glass::LfoWaveRepeats(glass::LfoWave::Triangle));
    VERIFY_IS_TRUE(glass::LfoWaveRepeats(glass::LfoWave::Square));
    VERIFY_IS_TRUE(glass::LfoWaveRepeats(glass::LfoWave::RampUp));
    VERIFY_IS_TRUE(glass::LfoWaveRepeats(glass::LfoWave::RampDown));

    VERIFY_IS_FALSE(glass::LfoWaveRepeats(glass::LfoWave::WhiteNoise));
    VERIFY_IS_FALSE(glass::LfoWaveRepeats(glass::LfoWave::PinkNoise));
    VERIFY_IS_FALSE(glass::LfoWaveRepeats(glass::LfoWave::BrownNoise));
    VERIFY_IS_FALSE(glass::LfoWaveRepeats(glass::LfoWave::BlueNoise));
}

void GeneratorTests::AFullSweepReachesBothEnds()
{
    auto const spec = Sweep(0.0, 1.0, glass::LfoWave::Sine);

    VERIFY_IS_TRUE(Near(glass::LfoValueAt(spec, 0.25, 0.5), 1.0));
    VERIFY_IS_TRUE(Near(glass::LfoValueAt(spec, 0.75, 0.5), 0.0));
}

void GeneratorTests::ANarrowSweepStaysInsideItsEnds()
{
    auto const spec = Sweep(0.4, 0.6, glass::LfoWave::Sine);

    VERIFY_IS_TRUE(Near(glass::LfoValueAt(spec, 0.25, 0.5), 0.6));
    VERIFY_IS_TRUE(Near(glass::LfoValueAt(spec, 0.75, 0.5), 0.4));

    // The middle of the wave lands halfway between the two ends. That is the whole promise.
    VERIFY_IS_TRUE(Near(glass::LfoValueAt(spec, 0.0, 0.5), 0.5));
}

void GeneratorTests::ALowEndAboveTheHighOneTurnsTheWaveOver()
{
    auto const spec = Sweep(1.0, 0.0, glass::LfoWave::Sine);

    VERIFY_IS_TRUE(Near(glass::LfoValueAt(spec, 0.25, 0.5), 0.0));
    VERIFY_IS_TRUE(Near(glass::LfoValueAt(spec, 0.75, 0.5), 1.0));
}

void GeneratorTests::ANoiseSampleIsScaledByTheSameEnds()
{
    auto const spec = Sweep(0.2, 0.8, glass::LfoWave::WhiteNoise);

    // A noise wave ignores the phase and takes the sample it is handed.
    VERIFY_IS_TRUE(Near(glass::LfoValueAt(spec, 0.37, 1.0), 0.8));
    VERIFY_IS_TRUE(Near(glass::LfoValueAt(spec, 0.37, 0.0), 0.2));
    VERIFY_IS_TRUE(Near(glass::LfoValueAt(spec, 0.37, 0.5), 0.5));
}

void GeneratorTests::EveryNoiseStaysInRange()
{
    for (auto const wave : { glass::LfoWave::WhiteNoise, glass::LfoWave::PinkNoise,
        glass::LfoWave::BrownNoise, glass::LfoWave::BlueNoise })
    {
        glass::LfoNoise noise{};

        for (int32_t sample = 0; sample < 5000; ++sample)
        {
            auto const value = noise.Next(wave);

            VERIFY_IS_TRUE(value >= 0.0 && value <= 1.0);
        }
    }
}

void GeneratorTests::WhiteNoiseDoesNotRepeatItself()
{
    glass::LfoNoise noise{};

    auto const first = noise.Next(glass::LfoWave::WhiteNoise);
    auto changed = false;

    for (int32_t sample = 0; sample < 20; ++sample)
    {
        if (std::abs(noise.Next(glass::LfoWave::WhiteNoise) - first) > 0.01)
        {
            changed = true;
            break;
        }
    }

    VERIFY_IS_TRUE(changed);
}

void GeneratorTests::BrownNoiseMovesLessThanWhite()
{
    // Six decibels an octave means it wanders rather than jumps, and the size of one step is the
    // only thing that says so. Without this the filter could be wired up wrong and every noise
    // would still pass the range check above.
    auto const averageStep = [](glass::LfoWave wave)
        {
            glass::LfoNoise noise{};

            auto previous = noise.Next(wave);
            auto total = 0.0;

            for (int32_t sample = 0; sample < 2000; ++sample)
            {
                auto const value = noise.Next(wave);

                total += std::abs(value - previous);
                previous = value;
            }

            return total / 2000.0;
        };

    VERIFY_IS_TRUE(averageStep(glass::LfoWave::BrownNoise) < averageStep(glass::LfoWave::WhiteNoise));
}

void GeneratorTests::ResettingANoiseSourceRepeatsIt()
{
    glass::LfoNoise noise{};

    auto const first = noise.Next(glass::LfoWave::PinkNoise);
    auto const second = noise.Next(glass::LfoWave::PinkNoise);

    noise.Reset();

    VERIFY_IS_TRUE(Near(noise.Next(glass::LfoWave::PinkNoise), first));
    VERIFY_IS_TRUE(Near(noise.Next(glass::LfoWave::PinkNoise), second));
}

void GeneratorTests::StraightUpIsZeroDegrees()
{
    VERIFY_IS_TRUE(Near(glass::AngleAtPosition(100.0, 100.0, 50.0, 0.0), 0.0));
}

void GeneratorTests::AngleRunsClockwise()
{
    VERIFY_IS_TRUE(Near(glass::AngleAtPosition(100.0, 100.0, 100.0, 50.0), 90.0));
    VERIFY_IS_TRUE(Near(glass::AngleAtPosition(100.0, 100.0, 50.0, 100.0), 180.0));
    VERIFY_IS_TRUE(Near(glass::AngleAtPosition(100.0, 100.0, 0.0, 50.0), 270.0));
}

void GeneratorTests::APlatterPushedPastTheTopKeepsGoing()
{
    // 350 degrees to 10 degrees is twenty degrees clockwise, not 340 back the other way.
    VERIFY_IS_TRUE(Near(glass::AngleDelta(350.0, 10.0), 20.0));
    VERIFY_IS_TRUE(Near(glass::AngleDelta(10.0, 350.0), -20.0));
}

void GeneratorTests::AnAngleDeltaTakesTheShortWayRound()
{
    VERIFY_IS_TRUE(Near(glass::AngleDelta(0.0, 90.0), 90.0));
    VERIFY_IS_TRUE(Near(glass::AngleDelta(0.0, 270.0), -90.0));
    VERIFY_IS_TRUE(Near(glass::AngleDelta(90.0, 45.0), -45.0));
}

void GeneratorTests::OnlyThePlatterIsTurnedByHand()
{
    VERIFY_IS_TRUE(glass::IsTurnedByHand(glass::ControlKind::Turntable));

    VERIFY_IS_FALSE(glass::IsTurnedByHand(glass::ControlKind::Knob));
    VERIFY_IS_FALSE(glass::IsTurnedByHand(glass::ControlKind::Encoder));
    VERIFY_IS_FALSE(glass::IsTurnedByHand(glass::ControlKind::Joystick));
}
