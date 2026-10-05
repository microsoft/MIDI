// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SharedGeneratorTests.h"
#include "TestMessages.h"

#include "ChannelVoiceWords.h"
#include "ClockFollower.h"
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

void SharedGeneratorTests::AFollowerMovesBetweenPulses()
{
    midiapp::ClockFollower clock{};

    // Nothing to follow before the first pulse.
    VERIFY_IS_FALSE(clock.PulsesAt(1000).has_value());
    VERIFY_ARE_EQUAL(uint64_t{ 0 }, clock.KnownUntil());

    clock.Pulse(1000);
    clock.Pulse(1100);
    clock.Pulse(1200);

    VERIFY_ARE_EQUAL(uint64_t{ 100 }, clock.PulseInterval());
    VERIFY_IS_FALSE(clock.PulsesAt(999).has_value());
    VERIFY_IS_TRUE(Near(0.5, clock.PulsesAt(1050).value()));
    VERIFY_IS_TRUE(Near(1.25, clock.PulsesAt(1125).value()));

    // Past the last pulse it carries on at the same pace, but never as far as a pulse that
    // hasn't come, so a clock that stops leaves it where it got to.
    VERIFY_IS_TRUE(Near(2.5, clock.PulsesAt(1250).value()));
    VERIFY_IS_TRUE(Near(3.0, clock.PulsesAt(1300).value()));
    VERIFY_IS_TRUE(Near(3.0, clock.PulsesAt(5000).value()));
    VERIFY_ARE_EQUAL(uint64_t{ 1300 }, clock.KnownUntil());
}

void SharedGeneratorTests::AFollowerTakesPulsesBeforeTheyPlay()
{
    // A clock generator hands over its pulses ahead of time, each with the time it plays.
    midiapp::ClockFollower clock{};

    for (uint64_t i = 0; i < 6; i++)
    {
        clock.Pulse(1000 + i * 100);
    }

    VERIFY_IS_TRUE(Near(0.0, clock.PulsesAt(1000).value()));
    VERIFY_IS_TRUE(Near(2.5, clock.PulsesAt(1250).value()));
    VERIFY_ARE_EQUAL(uint64_t{ 1600 }, clock.KnownUntil());

    // Swung pulses are followed as they come, rather than evened out.
    midiapp::ClockFollower swung{};

    swung.Pulse(0);
    swung.Pulse(130);
    swung.Pulse(200);

    VERIFY_IS_TRUE(Near(1.5, swung.PulsesAt(165).value()));

    // Pulses handed over for after a start belong to the run it replaces.
    clock.Start(1250);

    VERIFY_IS_TRUE(Near(2.1, clock.PulsesAt(1210).value()));

    clock.Pulse(1260);

    VERIFY_IS_TRUE(Near(0.0, clock.PulsesAt(1260).value()));
}

void SharedGeneratorTests::AFollowerStartsAgainOnStartAndSongPosition()
{
    midiapp::ClockFollower clock{};

    for (uint64_t i = 0; i < 4; i++)
    {
        clock.Pulse(1000 + i * 100);
    }

    // After a start the next pulse is the top again. Until it plays, the run before holds
    // where it got to.
    clock.Start(1350);
    clock.Pulse(1400);
    clock.Pulse(1500);

    VERIFY_IS_TRUE(Near(3.2, clock.PulsesAt(1320).value()));
    VERIFY_IS_TRUE(Near(3.9, clock.PulsesAt(1390).value()));
    VERIFY_IS_TRUE(Near(0.0, clock.PulsesAt(1400).value()));
    VERIFY_IS_TRUE(Near(0.5, clock.PulsesAt(1450).value()));

    // A song position counts in sixteenths, six pulses each.
    clock.SongPosition(1550, 4);
    clock.Pulse(1600);

    VERIFY_IS_TRUE(Near(24.0, clock.PulsesAt(1600).value()));
    VERIFY_IS_TRUE(Near(24.5, clock.PulsesAt(1650).value()));
}

void SharedGeneratorTests::AFollowerSitsOutAPause()
{
    midiapp::ClockFollower clock{};

    for (uint64_t i = 0; i < 4; i++)
    {
        clock.Pulse(i * 100);
    }

    // A pause, then the same pace again. The gap isn't taken for a tempo.
    clock.Pulse(10'000);
    clock.Pulse(10'100);

    VERIFY_ARE_EQUAL(uint64_t{ 100 }, clock.PulseInterval());

    // One long gap could be a hiccup. The same gap twice is a new tempo.
    clock.Pulse(10'400);

    VERIFY_ARE_EQUAL(uint64_t{ 100 }, clock.PulseInterval());

    clock.Pulse(10'700);

    VERIFY_ARE_EQUAL(uint64_t{ 300 }, clock.PulseInterval());

    // Out of order, from two clocks at once, is held to the last time rather than trusted.
    clock.Pulse(5);

    VERIFY_ARE_EQUAL(uint64_t{ 11'000 }, clock.KnownUntil());
}

void SharedGeneratorTests::AFollowerCanKeepToStartAndStop()
{
    midiapp::ClockFollower clock{};
    clock.KeepsToStartAndStop(true);

    // Nothing counts before Start.
    clock.Pulse(900);

    VERIFY_IS_TRUE(clock.IsStopped());
    VERIFY_IS_FALSE(clock.PulsesAt(950).has_value());

    clock.Start(950);

    for (uint64_t i = 0; i < 4; i++)
    {
        clock.Pulse(1000 + i * 100);
    }

    // A pulse handed over early for after the stop is taken back, and the ones while stopped
    // don't count.
    clock.Pulse(1400);
    clock.Stop(1350);
    clock.Pulse(1500);
    clock.Pulse(1600);

    VERIFY_IS_TRUE(clock.IsStopped());
    VERIFY_ARE_EQUAL(uint64_t{ 1400 }, clock.KnownUntil());
    VERIFY_IS_TRUE(Near(4.0, clock.PulsesAt(2000).value()));

    // Continue carries on from there.
    clock.Continue();
    clock.Pulse(3000);

    VERIFY_IS_FALSE(clock.IsStopped());
    VERIFY_IS_TRUE(Near(4.0, clock.PulsesAt(3000).value()));

    // Without it, Stop is ignored, the same as before.
    midiapp::ClockFollower plain{};

    plain.Pulse(0);
    plain.Stop(50);
    plain.Pulse(100);

    VERIFY_IS_FALSE(plain.IsStopped());
    VERIFY_IS_TRUE(Near(1.0, plain.PulsesAt(100).value()));
}
