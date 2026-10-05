// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SharedGeneratorTests.h"
#include "TestMessages.h"

#include "ChannelVoiceWords.h"
#include "LfoSweep.h"
#include "LfoWave.h"

#include <cmath>

using namespace patchbaytests;
using midiapp::ValueMessageKind;

namespace
{
    midiapp::ValueMessageTarget Target(ValueMessageKind kind, uint32_t number, bool midi1)
    {
        midiapp::ValueMessageTarget target{};

        target.Kind = kind;
        target.Group = 3;
        target.Channel = 9;
        target.Number = number;
        target.Midi1Protocol = midi1;

        return target;
    }

    Message Built(midiapp::ValueMessageTarget const& target, double fraction)
    {
        Message message{};
        message.Count = static_cast<uint8_t>(midiapp::BuildValueMessage(target, fraction, message.Words.data()));
        return message;
    }

    bool Near(double a, double b)
    {
        return std::abs(a - b) < 1e-9;
    }
}

void SharedGeneratorTests::ValueMessagesAreBuiltForEachKind()
{
    // Full scale is the top of the field, and half is the middle a pitch bend rests at.
    VERIFY_IS_TRUE(Built(Target(ValueMessageKind::ControlChange, 74, true), 1.0) == Midi1(3, 0xB, 9, 74, 127));
    VERIFY_IS_TRUE(Built(Target(ValueMessageKind::ControlChange, 74, false), 0.5) == Midi2(3, 0xB, 9, 74, 0, 0x80000000u));
    VERIFY_IS_TRUE(Built(Target(ValueMessageKind::PitchBend, 0, true), 0.5) == Midi1(3, 0xE, 9, 0x00, 0x40));
    VERIFY_IS_TRUE(Built(Target(ValueMessageKind::PitchBend, 0, false), 1.0) == Midi2(3, 0xE, 9, 0, 0, 0xFFFFFFFFu));
    VERIFY_IS_TRUE(Built(Target(ValueMessageKind::ChannelPressure, 0, true), 0.0) == Midi1(3, 0xD, 9, 0, 0));
    VERIFY_IS_TRUE(Built(Target(ValueMessageKind::PolyPressure, 60, true), 1.0) == Midi1(3, 0xA, 9, 60, 127));
    VERIFY_IS_TRUE(Built(Target(ValueMessageKind::PolyPressure, 60, false), 0.0) == Midi2(3, 0xA, 9, 60, 0, 0));

    // An RPN or an NRPN is a bank and an index, and goes as MIDI 2.0 even when MIDI 1.0 is asked
    // for: in MIDI 1.0 it would be four control changes.
    VERIFY_IS_TRUE(Built(Target(ValueMessageKind::RegisteredController, 0, true), 1.0) == Midi2(3, 0x2, 9, 0, 0, 0xFFFFFFFFu));
    VERIFY_IS_TRUE(Built(Target(ValueMessageKind::AssignableController, 300, false), 0.0) == Midi2(3, 0x3, 9, 2, 44, 0));

    // Out of range is held at the ends, never wrapped.
    VERIFY_IS_TRUE(Built(Target(ValueMessageKind::ControlChange, 7, true), 2.0) == Midi1(3, 0xB, 9, 7, 127));
    VERIFY_IS_TRUE(Built(Target(ValueMessageKind::ControlChange, 7, true), -1.0) == Midi1(3, 0xB, 9, 7, 0));
    VERIFY_IS_TRUE(Built(Target(ValueMessageKind::AssignableController, 99999, false), 0.0) == Midi2(3, 0x3, 9, 127, 127, 0));

    VERIFY_ARE_EQUAL(127u, midiapp::ValueMessageNumberMaximum(ValueMessageKind::ControlChange));
    VERIFY_ARE_EQUAL(127u, midiapp::ValueMessageNumberMaximum(ValueMessageKind::PolyPressure));
    VERIFY_ARE_EQUAL(16383u, midiapp::ValueMessageNumberMaximum(ValueMessageKind::RegisteredController));
    VERIFY_ARE_EQUAL(0u, midiapp::ValueMessageNumberMaximum(ValueMessageKind::PitchBend));
}

void SharedGeneratorTests::KindsAndWavesReadBackFromTheirKeys()
{
    for (auto const kind : midiapp::AllValueMessageKinds)
    {
        auto const back = midiapp::ValueMessageKindFromKey(midiapp::ValueMessageKindKey(kind));

        VERIFY_IS_TRUE(back.has_value());
        VERIFY_IS_TRUE(back.value() == kind);
    }

    // The names the MIDI Glass assistant tools use, spelled exactly.
    VERIFY_IS_TRUE(midiapp::ValueMessageKindKey(ValueMessageKind::RegisteredController) == L"rpn");
    VERIFY_IS_TRUE(midiapp::ValueMessageKindKey(ValueMessageKind::PitchBend) == L"pitchBend");
    VERIFY_IS_FALSE(midiapp::ValueMessageKindFromKey(L"RPN").has_value());

    for (auto const wave : midiapp::LfoWaveOrder)
    {
        auto const back = midiapp::LfoWaveFromKey(midiapp::LfoWaveKey(wave));

        VERIFY_IS_TRUE(back.has_value());
        VERIFY_IS_TRUE(back.value() == wave);
    }

    VERIFY_IS_TRUE(midiapp::LfoWaveKey(midiapp::LfoWave::BlueNoise) == L"blueNoise");
    VERIFY_IS_FALSE(midiapp::LfoWaveFromKey(L"sawtooth").has_value());
}

void SharedGeneratorTests::ASweepKeepsItsPlaceWhenItsTempoChanges()
{
    midiapp::LfoSweep sweep{};

    // One bar at 120 is two seconds, so a sample every 25 milliseconds moves 1/80 of the way.
    sweep.Begin(1000, 1'000'000, 4.0, 120.0, 25);

    VERIFY_ARE_EQUAL(uint64_t{ 25'000 }, sweep.Interval());
    VERIFY_ARE_EQUAL(uint64_t{ 1000 }, sweep.NextDue());
    VERIFY_IS_TRUE(Near(0.0, sweep.NextPhase()));

    for (int i = 0; i < 40; i++)
    {
        sweep.Advance();
    }

    VERIFY_ARE_EQUAL(uint64_t{ 1000 + 40 * 25'000 }, sweep.NextDue());
    VERIFY_IS_TRUE(Near(0.5, sweep.NextPhase()));

    // Half the tempo: the next sample keeps its time and its place, and the rest move half as far.
    sweep.Retime(4.0, 60.0, 25);

    VERIFY_ARE_EQUAL(uint64_t{ 1000 + 40 * 25'000 }, sweep.NextDue());
    VERIFY_IS_TRUE(Near(0.5, sweep.NextPhase()));

    sweep.Advance();

    VERIFY_IS_TRUE(Near(0.5 + 1.0 / 160.0, sweep.NextPhase()));

    // A whole cycle later it is back where it was, not past the end.
    for (int i = 0; i < 160; i++)
    {
        sweep.Advance();
    }

    VERIFY_IS_TRUE(Near(0.5 + 1.0 / 160.0, sweep.NextPhase()));
}

void SharedGeneratorTests::ASweepStartsAgainAfterALongStall()
{
    midiapp::LfoSweep sweep{};

    sweep.Begin(0, 1'000'000, 4.0, 120.0, 25);

    for (int i = 0; i < 10; i++)
    {
        sweep.Advance();
    }

    VERIFY_ARE_EQUAL(uint64_t{ 250'000 }, sweep.NextDue());

    // Eight samples late is still caught up one sample at a time.
    VERIFY_IS_FALSE(sweep.CatchUp(250'000 + 8 * 25'000));
    VERIFY_ARE_EQUAL(uint64_t{ 250'000 }, sweep.NextDue());

    // Any later starts again from now, at the place the sweep had reached.
    VERIFY_IS_TRUE(sweep.CatchUp(250'000 + 8 * 25'000 + 1));
    VERIFY_ARE_EQUAL(uint64_t{ 450'001 }, sweep.NextDue());
    VERIFY_IS_TRUE(Near(0.125, sweep.NextPhase()));

    // Early is never a stall.
    VERIFY_IS_FALSE(sweep.CatchUp(0));
}
