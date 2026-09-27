// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h, XAML and WinRT, so the unit tests compile it unchanged. Which step
// plays next and when each note starts and stops is arithmetic on a spec and a tempo; the thread
// that keeps time only asks these questions.

#include <sal.h>
#include <cstdint>

#include "LayoutModel.h"

namespace glass
{
    // How many steps a sequence has, held to the range a sequence may have.
    int32_t SequencerStepCount(_In_ StepsSpec const& spec) noexcept;

    // The step that plays at this count, where 0 is the first step played since the sequence
    // started. A random walk takes its next number from the state, which it moves on; every other
    // direction leaves it alone and gives the same answer for the same count.
    int32_t StepIndexAt(
        _In_ StepsSpec const& spec,
        _In_ uint64_t count,
        _Inout_ uint32_t& randomState) noexcept;

    // How long one step lasts at this tempo, in microseconds.
    double StepMicroseconds(_In_ StepsSpec const& spec, _In_ double beatsPerMinute) noexcept;

    // When the step at this count starts, in microseconds from the start of the sequence. Swing
    // holds back every second step; the steps between stay on the grid.
    double StepStartMicroseconds(_In_ StepsSpec const& spec, _In_ uint64_t count, _In_ double stepMicroseconds) noexcept;

    // How long the note of the step at this count sounds. It is a share of the time until the
    // next step starts, so swing shortens the note before a late step rather than overlapping
    // it, and a note held for its whole step still ends just before the next one begins.
    double GateMicroseconds(_In_ StepsSpec const& spec, _In_ uint64_t count, _In_ double stepMicroseconds) noexcept;

    // The step rates offered in the inspector, as steps per beat, slowest first. One table
    // shared by the inspector and the edit path, so an index can never mean two things.
    constexpr double StepRateChoices[]
    {
        1.0,    // quarter notes
        2.0,    // eighth notes
        3.0,    // eighth note triplets
        4.0,    // sixteenth notes
        6.0,    // sixteenth note triplets
        8.0,    // thirty-second notes
    };

    constexpr StepDirection StepDirectionOrder[]
    {
        StepDirection::Forward,
        StepDirection::Backward,
        StepDirection::PingPong,
        StepDirection::Random,
    };

    // What a new sequencer plays: a minor arpeggio up and back down from this note, the first
    // step a little harder so the loop has somewhere to start.
    void FillStarterPattern(_Inout_ StepsSpec& spec, _In_ int32_t rootNote);
}
