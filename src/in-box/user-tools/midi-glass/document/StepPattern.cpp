// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, XAML and WinRT.

#include "StepPattern.h"

#include <algorithm>
#include <cmath>

namespace glass
{
    namespace
    {
        // A note that ends exactly as the next one starts can reach the far end of a busy cable
        // after it, which a synth hears as the new note being cut off. A millisecond is too short
        // to hear and keeps the two in order.
        constexpr double GapBeforeNextStepMicroseconds = 1000.0;

        // xorshift32. Zero is the one state it can never leave, so it is never allowed in.
        uint32_t NextRandom(_Inout_ uint32_t& state) noexcept
        {
            if (state == 0)
            {
                state = 0x9E3779B9u;
            }

            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;

            return state;
        }

        double SwingOf(_In_ StepsSpec const& spec) noexcept
        {
            return std::isfinite(spec.Swing)
                ? std::clamp(spec.Swing, MinimumStepSwing, MaximumStepSwing)
                : MinimumStepSwing;
        }
    }

    _Use_decl_annotations_
    int32_t SequencerStepCount(StepsSpec const& spec) noexcept
    {
        return static_cast<int32_t>(
            (std::min)(spec.Pattern.size(), static_cast<size_t>(MaximumSequencerSteps)));
    }

    _Use_decl_annotations_
    int32_t StepIndexAt(StepsSpec const& spec, uint64_t count, uint32_t& randomState) noexcept
    {
        auto const steps = SequencerStepCount(spec);

        if (steps <= 1)
        {
            return 0;
        }

        auto const n = static_cast<uint64_t>(steps);

        switch (spec.Direction)
        {
        case StepDirection::Backward:
            return static_cast<int32_t>(n - 1 - (count % n));

        case StepDirection::PingPong:
        {
            // Up and back is one period. Each end is played once per period, so the ends are
            // not doubled the way they are when a forward run is simply followed by a backward one.
            auto const period = (n - 1) * 2;
            auto const at = count % period;

            return static_cast<int32_t>(at < n ? at : period - at);
        }

        case StepDirection::Random:
            return static_cast<int32_t>(NextRandom(randomState) % n);

        default:
            return static_cast<int32_t>(count % n);
        }
    }

    _Use_decl_annotations_
    double StepMicroseconds(StepsSpec const& spec, double beatsPerMinute) noexcept
    {
        auto const bpm = std::isfinite(beatsPerMinute)
            ? std::clamp(beatsPerMinute, MinimumBeatsPerMinute, MaximumBeatsPerMinute)
            : 120.0;

        auto const perBeat = std::isfinite(spec.StepsPerBeat)
            ? std::clamp(spec.StepsPerBeat, MinimumStepsPerBeat, MaximumStepsPerBeat)
            : 4.0;

        return 60.0 * 1000000.0 / (bpm * perBeat);
    }

    _Use_decl_annotations_
    double StepStartMicroseconds(StepsSpec const& spec, uint64_t count, double stepMicroseconds) noexcept
    {
        // Swing is the share of each pair of steps the first one gets, so the second lands late
        // by however far that share is past half the pair.
        auto const late = (count % 2) == 1
            ? (SwingOf(spec) * 2.0 - 1.0) * stepMicroseconds
            : 0.0;

        return static_cast<double>(count) * stepMicroseconds + late;
    }

    _Use_decl_annotations_
    double GateMicroseconds(StepsSpec const& spec, uint64_t count, double stepMicroseconds) noexcept
    {
        auto const gap =
            StepStartMicroseconds(spec, count + 1, stepMicroseconds) -
            StepStartMicroseconds(spec, count, stepMicroseconds);

        auto const gate = std::isfinite(spec.Gate)
            ? std::clamp(spec.Gate, MinimumStepGate, MaximumStepGate)
            : 0.5;

        auto const wanted = gap * gate;
        auto const latest = gap - GapBeforeNextStepMicroseconds;

        return (std::max)((std::min)(wanted, latest), gap * MinimumStepGate);
    }

    _Use_decl_annotations_
    void FillStarterPattern(StepsSpec& spec, int32_t rootNote)
    {
        // Root, minor third, fifth, minor seventh, the octave, and back down.
        constexpr int32_t Intervals[DefaultSequencerSteps]{ 0, 3, 7, 10, 12, 10, 7, 3 };

        spec.Pattern.clear();

        for (int32_t index = 0; index < DefaultSequencerSteps; ++index)
        {
            SequencerStep step{};

            step.On = true;
            step.Note = std::clamp(rootNote + Intervals[index], 0, 127);
            step.Velocity = index == 0 ? 1.0 : 0.75;

            spec.Pattern.push_back(step);
        }
    }
}
