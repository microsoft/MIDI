// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

#include "MidiSynthDataEntryTests.h"
#include "MidiSynthTestRig.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace MidiSynth;
using namespace MidiSynthTestRig;

namespace
{
    // A steady tone, so its pitch can be measured to a small fraction of a cent.
    constexpr uint8_t ChurchOrgan = 19;

    constexpr uint8_t StatusControlChange = 0xB;
    constexpr uint8_t StatusPitchBend = 0xE;
    constexpr uint8_t StatusRegisteredController = 0x2;

    constexpr uint8_t NoAttribute = 0x00;

    // The estimate is good to a few hundredths of a cent, so anything further off than this is wrong.
    constexpr double ToleranceCents = 0.3;

    // MIDI 1.0 fine tuning: 8192 is in tune, and each step is 100/8192 cents.
    constexpr double FineCents(_In_ uint32_t fourteenBit) noexcept
    {
        return (static_cast<double>(fourteenBit) - 8192.0) * 100.0 / 8192.0;
    }

    void SelectRegisteredParameter(_Inout_ Rig& rig, _In_ uint8_t channel, _In_ uint8_t index)
    {
        rig.SendMidi1(StatusControlChange, channel, 101, 0);
        rig.SendMidi1(StatusControlChange, channel, 100, index);
    }

    // The organ's note 60 against the same note untouched, once the attack is over.
    double MeasureCents(_Inout_ Rig& rig, _In_ const DlsCollection& collection, _In_ double expectedCents)
    {
        double reference = 0.0;
        {
            Rig plain(collection);
            plain.Engine().ProgramChange(0, ChurchOrgan);
            plain.NoteOn(0, 60, NoAttribute, 0);
            (void)plain.Render(0.3);
            reference = EstimateHertz(plain.Render(0.6), plain.SampleRate(), 100.0, 1000.0);
        }

        rig.NoteOn(0, 60, NoAttribute, 0);
        (void)rig.Render(0.3);

        const double expectedHertz = reference * std::pow(2.0, expectedCents / 1200.0);

        return Cents(EstimateHertz(rig.Render(0.6), rig.SampleRate(), expectedHertz / 1.25, expectedHertz * 1.25), reference);
    }
}

// Fine tuning is fourteen bits. The data entry MSB alone moves it in steps of 100/64 cents; the LSB
// fills in the rest. An MSB sets the LSB back to zero, and an LSB on its own adjusts the tuning
// already there, as the MIDI 1.0 specification describes.
void MidiSynthDataEntryTests::TestFineTuningTakesTheDataEntryLsb()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    Rig rig(*collection);
    const auto fine = [&]() { return rig.Engine().ChannelState(0).FineTuneCents; };

    SelectRegisteredParameter(rig, 0, 1);
    rig.SendMidi1(StatusControlChange, 0, 6, 0x40);
    rig.SendMidi1(StatusControlChange, 0, 38, 0x20);

    VERIFY_ARE_EQUAL(FineCents(0x2020), fine(), L"MSB and LSB together");

    rig.SendMidi1(StatusControlChange, 0, 6, 0x41);
    VERIFY_ARE_EQUAL(FineCents(0x2080), fine(), L"a new MSB sets the LSB to zero");

    rig.SendMidi1(StatusControlChange, 0, 38, 0x01);
    VERIFY_ARE_EQUAL(FineCents(0x2081), fine(), L"an LSB alone adjusts the MSB that is there");

    // Another parameter in between, then fine tuning chosen again: the lone LSB still finds the
    // fine tuning's own MSB rather than the bend range's.
    SelectRegisteredParameter(rig, 0, 0);
    rig.SendMidi1(StatusControlChange, 0, 6, 3);
    SelectRegisteredParameter(rig, 0, 1);
    rig.SendMidi1(StatusControlChange, 0, 38, 0x7F);

    VERIFY_ARE_EQUAL(FineCents(0x20FF), fine());

    // Coarse tuning has no LSB.
    SelectRegisteredParameter(rig, 0, 2);
    rig.SendMidi1(StatusControlChange, 0, 6, 0x42);
    rig.SendMidi1(StatusControlChange, 0, 38, 0x7F);

    VERIFY_ARE_EQUAL(2.0, rig.Engine().ChannelState(0).CoarseTuneSemitones);

    // The low bits are heard: 0x3FFF is almost 100 cents sharp, where the MSB alone stops at 98.44.
    Rig tuned(*collection);
    tuned.Engine().ProgramChange(0, ChurchOrgan);
    SelectRegisteredParameter(tuned, 0, 1);
    tuned.SendMidi1(StatusControlChange, 0, 6, 0x7F);
    tuned.SendMidi1(StatusControlChange, 0, 38, 0x7F);

    const double cents = MeasureCents(tuned, *collection, FineCents(0x3FFF));

    Log::Comment(String().Format(L"Fine tuning 0x3FFF: expected %+.3f cents, measured %+.3f cents", FineCents(0x3FFF), cents));

    VERIFY_IS_TRUE(std::fabs(cents - FineCents(0x3FFF)) < ToleranceCents);
}

// Pitch bend sensitivity is whole semitones and cents, from the MIDI 1.0 data entry MSB and LSB or
// from the two seven bit fields of the MIDI 2.0 Registered Controller.
void MidiSynthDataEntryTests::TestPitchBendRangeTakesCents()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    Rig rig(*collection);
    const auto range = [&]() { return rig.Engine().ChannelState(0).PitchBendRangeSemitones; };

    SelectRegisteredParameter(rig, 0, 0);
    rig.SendMidi1(StatusControlChange, 0, 6, 4);
    rig.SendMidi1(StatusControlChange, 0, 38, 50);

    VERIFY_ARE_EQUAL(4.5, range(), L"four semitones and fifty cents from MIDI 1.0");

    rig.SendMidi1(StatusControlChange, 0, 6, 3);
    VERIFY_ARE_EQUAL(3.0, range(), L"a new MSB sets the cents to zero");

    rig.Send(StatusRegisteredController, 0, 0, 0, (5u << 25) | (25u << 18));
    VERIFY_ARE_EQUAL(5.25, range(), L"five semitones and twenty five cents from MIDI 2.0");

    // A full bend up over four and a half semitones.
    Rig bent(*collection);
    bent.Engine().ProgramChange(0, ChurchOrgan);
    bent.Send(StatusRegisteredController, 0, 0, 0, (4u << 25) | (50u << 18));
    bent.Send(StatusPitchBend, 0, 0, 0, 0xFFFFFFFFu);

    const double cents = MeasureCents(bent, *collection, 450.0);

    Log::Comment(String().Format(L"Full bend over 4.5 semitones: measured %+.3f cents", cents));

    VERIFY_IS_TRUE(std::fabs(cents - 450.0) < ToleranceCents);
}

// In MIDI 2.0 a registered controller is one message. It must not move the parameter that MIDI 1.0
// data entry on the same channel changes, or a later data entry lands on the wrong parameter.
void MidiSynthDataEntryTests::TestMidi2BendRangeLeavesDataEntryAlone()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    Rig rig(*collection);

    SelectRegisteredParameter(rig, 0, 1);
    rig.Send(StatusRegisteredController, 0, 0, 0, 12u << 25);
    rig.SendMidi1(StatusControlChange, 0, 6, 80);

    const auto state = rig.Engine().ChannelState(0);

    VERIFY_ARE_EQUAL(12.0, state.PitchBendRangeSemitones, L"the bend range is what MIDI 2.0 set");
    VERIFY_ARE_EQUAL(FineCents(80u << 7), state.FineTuneCents, L"and the data entry still reached fine tuning");
    VERIFY_IS_TRUE(state.RpnMsb == 0 && state.RpnLsb == 1, L"which stayed the selected parameter");
}
