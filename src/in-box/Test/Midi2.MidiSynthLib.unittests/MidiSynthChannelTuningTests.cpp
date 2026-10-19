// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

#include "MidiSynthChannelTuningTests.h"
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
    constexpr uint8_t StatusRegisteredController = 0x2;
    constexpr uint8_t FineTuning = 1;
    constexpr uint8_t CoarseTuning = 2;

    constexpr uint8_t NoAttribute = 0x00;
    constexpr uint8_t AttributePitch79 = 0x03;

    // The estimate is good to a few hundredths of a cent, so anything further off than this is wrong.
    constexpr double ToleranceCents = 0.3;

    double RatioOf(_In_ double cents) noexcept
    {
        return std::pow(2.0, cents / 1200.0);
    }

    // Measured once the attack is over, in a band around where the note should be.
    double MeasureAfterAttack(_Inout_ Rig& rig, _In_ double expectedHertz)
    {
        (void)rig.Render(0.3);

        return EstimateHertz(rig.Render(0.6), rig.SampleRate(), expectedHertz / 1.25, expectedHertz * 1.25);
    }

    // The untuned note 60 every test measures against.
    double ReferenceHertz(_In_ const DlsCollection& collection)
    {
        Rig rig(collection);
        rig.Engine().ProgramChange(0, ChurchOrgan);
        rig.NoteOn(0, 60, NoAttribute, 0);
        (void)rig.Render(0.3);

        return EstimateHertz(rig.Render(0.6), rig.SampleRate(), 100.0, 1000.0);
    }

    struct TuningCase
    {
        uint32_t Value;
        double Cents;
    };

    void CheckTuning(_In_ const DlsCollection& collection, _In_ uint8_t index, _In_ std::initializer_list<TuningCase> cases)
    {
        const double reference = ReferenceHertz(collection);

        VERIFY_IS_TRUE(reference > 0.0, L"the untuned note has a pitch to measure");

        for (const auto& test : cases)
        {
            Rig rig(collection);
            rig.Engine().ProgramChange(0, ChurchOrgan);
            rig.Send(StatusRegisteredController, 0, 0, index, test.Value);
            rig.NoteOn(0, 60, NoAttribute, 0);

            const double cents = Cents(MeasureAfterAttack(rig, reference * RatioOf(test.Cents)), reference);

            Log::Comment(String().Format(L"RPN 0,%u = 0x%08X: expected %+.3f cents, measured %+.3f cents",
                index, test.Value, test.Cents, cents));

            VERIFY_IS_TRUE(std::fabs(cents - test.Cents) < ToleranceCents);
        }
    }
}

// Fine tuning is 100 cents either side of 0x80000000, and every bit counts: 0xA1000000 falls
// between two of the steps the MIDI 1.0 data entry MSB can make.
void MidiSynthChannelTuningTests::TestFineTuningMovesThePitch()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    CheckTuning(*collection, FineTuning,
    {
        { 0x80000000, 0.0 },
        { 0x60000000, -25.0 },
        { 0xA0000000, 25.0 },
        { 0xA1000000, 25.78125 },
        { 0x00000000, -100.0 },
        { 0xFFFFFFFF, 100.0 },
    });
}

// Coarse tuning is whole semitones in the top seven bits, and 64 is in tune. The other 25 bits are
// undefined, so they change nothing.
void MidiSynthChannelTuningTests::TestCoarseTuningMovesThePitch()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    CheckTuning(*collection, CoarseTuning,
    {
        { 64u << 25, 0.0 },
        { 66u << 25, 200.0 },
        { 61u << 25, -300.0 },
        { (66u << 25) | 0x01FFFFFFu, 200.0 },
        { (61u << 25) | 0x00ABCDEFu, -300.0 },
    });
}

// A value both protocols can carry plays exactly what the MIDI 1.0 data entry plays.
void MidiSynthChannelTuningTests::TestMidi2TuningMatchesMidi1()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    auto render = [&](const auto& tune)
    {
        Rig rig(*collection);
        rig.Engine().ProgramChange(0, ChurchOrgan);
        tune(rig);
        rig.NoteOn(0, 60, NoAttribute, 0);

        return rig.Render(0.3);
    };

    const auto untuned = render([](Rig&) {});

    // 25 cents flat: data entry 48 for RPN 0,1, or 0x60000000.
    const auto fineMidi1 = render([](Rig& rig)
    {
        rig.SendMidi1(StatusControlChange, 0, 101, 0);
        rig.SendMidi1(StatusControlChange, 0, 100, 1);
        rig.SendMidi1(StatusControlChange, 0, 6, 48);
    });

    const auto fineMidi2 = render([](Rig& rig) { rig.Send(StatusRegisteredController, 0, 0, FineTuning, 0x60000000); });

    // Two semitones sharp: data entry 66 for RPN 0,2, or 66 in the top seven bits.
    const auto coarseMidi1 = render([](Rig& rig)
    {
        rig.SendMidi1(StatusControlChange, 0, 101, 0);
        rig.SendMidi1(StatusControlChange, 0, 100, 2);
        rig.SendMidi1(StatusControlChange, 0, 6, 66);
    });

    const auto coarseMidi2 = render([](Rig& rig) { rig.Send(StatusRegisteredController, 0, 0, CoarseTuning, 66u << 25); });

    VERIFY_IS_TRUE(LargestDifference(untuned, fineMidi1) > 1e-3 && LargestDifference(untuned, coarseMidi1) > 1e-3,
        L"both MIDI 1.0 tunings change the sound");

    VERIFY_IS_TRUE(LargestDifference(fineMidi1, fineMidi2) < 1e-6, L"fine tuning sounds the same from either protocol");
    VERIFY_IS_TRUE(LargestDifference(coarseMidi1, coarseMidi2) < 1e-6, L"coarse tuning sounds the same from either protocol");
}

// Tuning moves notes that are already sounding, not only the next ones.
void MidiSynthChannelTuningTests::TestTuningRetunesSoundingNotes()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    const double reference = ReferenceHertz(*collection);

    Rig rig(*collection);
    rig.Engine().ProgramChange(0, ChurchOrgan);
    rig.NoteOn(0, 60, NoAttribute, 0);
    (void)rig.Render(0.1);

    rig.Send(StatusRegisteredController, 0, 0, CoarseTuning, 66u << 25);
    rig.Send(StatusRegisteredController, 0, 0, FineTuning, 0xA0000000);

    const double cents = Cents(MeasureAfterAttack(rig, reference * RatioOf(225.0)), reference);

    Log::Comment(String().Format(L"sounding note after +2 semitones and +25 cents: %+.3f cents", cents));

    VERIFY_IS_TRUE(std::fabs(cents - 225.0) < ToleranceCents);
}

// M2-104-UM 7.4.15: channel tuning moves the pitch from wherever the note put it, so it adds to a
// Pitch 7.9 attribute rather than being replaced by it.
void MidiSynthChannelTuningTests::TestTuningAddsToExactPitch()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    const double reference = ReferenceHertz(*collection);

    Rig rig(*collection);
    rig.Engine().ProgramChange(0, ChurchOrgan);
    rig.Send(StatusRegisteredController, 0, 0, FineTuning, 0xA0000000);
    rig.Send(StatusRegisteredController, 0, 0, CoarseTuning, 66u << 25);

    // Note 60 at Pitch 7.9 = 60.5.
    rig.NoteOn(0, 60, AttributePitch79, static_cast<uint16_t>((60u << 9) | 256u));

    const double cents = Cents(MeasureAfterAttack(rig, reference * RatioOf(275.0)), reference);

    Log::Comment(String().Format(L"60.5 with +25 cents and +2 semitones: %+.3f cents", cents));

    VERIFY_IS_TRUE(std::fabs(cents - 275.0) < ToleranceCents);
}

namespace
{
    constexpr uint8_t DrumChannel = 9;

    // Channel 11 becomes a rhythm channel when General MIDI 2 bank select 120 chooses a kit.
    constexpr uint8_t SecondRhythmChannel = 10;
    constexpr uint8_t StandardKit = 0;
    constexpr uint8_t AcousticSnare = 38;

    // Every way channel tuning can arrive: the MIDI 1.0 data entry and the MIDI 2.0 registered
    // controller, 25 cents and two semitones sharp.
    void SendAllTuning(_Inout_ Rig& rig, _In_ uint8_t channel)
    {
        rig.SendMidi1(StatusControlChange, channel, 101, 0);
        rig.SendMidi1(StatusControlChange, channel, 100, 1);
        rig.SendMidi1(StatusControlChange, channel, 6, 80);
        rig.SendMidi1(StatusControlChange, channel, 100, 2);
        rig.SendMidi1(StatusControlChange, channel, 6, 66);
        rig.Send(StatusRegisteredController, channel, 0, FineTuning, 0xA0000000);
        rig.Send(StatusRegisteredController, channel, 0, CoarseTuning, 66u << 25);
    }

    void SelectSecondRhythmChannelProgram(_Inout_ Rig& rig, _In_ uint8_t bankMsb, _In_ uint8_t program)
    {
        rig.Engine().SetBankSelectMode(BankSelectMode::GeneralMidi2);
        rig.Engine().ProgramChangeWithBank(SecondRhythmChannel, bankMsb, 0, program);
    }
}

// General MIDI 2 sections 3.4.2 and 3.4.3: a rhythm channel doesn't respond to channel fine or
// coarse tuning. That is channel 10, and any channel that bank select turned into a kit.
void MidiSynthChannelTuningTests::TestDrumKitIgnoresChannelTuning()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    struct Case
    {
        uint8_t Channel;
        bool KitByBankSelect;
    };

    for (const auto& test : { Case{ DrumChannel, false }, Case{ SecondRhythmChannel, true } })
    {
        auto render = [&](bool tuned)
        {
            Rig rig(*collection);

            if (test.KitByBankSelect)
            {
                SelectSecondRhythmChannelProgram(rig, 120, StandardKit);
            }

            if (tuned)
            {
                SendAllTuning(rig, test.Channel);
            }

            const auto state = rig.Engine().ChannelState(test.Channel);

            VERIFY_IS_TRUE(state.PlaysDrumKit);
            VERIFY_ARE_EQUAL(0.0, state.FineTuneCents, L"fine tuning is not taken");
            VERIFY_ARE_EQUAL(0.0, state.CoarseTuneSemitones, L"coarse tuning is not taken");

            rig.NoteOn(test.Channel, AcousticSnare, NoAttribute, 0);

            return rig.Render(0.25);
        };

        const auto plain = render(false);
        const auto tuned = render(true);

        Log::Comment(String().Format(L"Channel %u: drum peak %.3f, largest difference after tuning %.7f",
            test.Channel + 1, Peak(plain), LargestDifference(plain, tuned)));

        VERIFY_IS_TRUE(Peak(plain) > 0.01, L"the drum is heard");
        VERIFY_IS_TRUE(LargestDifference(plain, tuned) < 1e-6, L"and sounds exactly as it did");
    }

    // A melodic channel still takes both.
    Rig melodic(*collection);
    SendAllTuning(melodic, 0);

    const auto state = melodic.Engine().ChannelState(0);

    VERIFY_IS_TRUE(std::fabs(state.FineTuneCents - 25.0) < 1e-6);
    VERIFY_ARE_EQUAL(2.0, state.CoarseTuneSemitones);
}

// A melodic channel keeps its tuning when it changes to a kit, because General MIDI 2 says a
// program change resets no RPN. The tuning still doesn't reach the drums, and it's there again
// when the channel goes back to a melodic program.
void MidiSynthChannelTuningTests::TestKeptTuningLeavesDrumsAlone()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    // The kit on channel 11 as it sounds untuned.
    std::vector<float> plainDrum;
    {
        Rig rig(*collection);
        SelectSecondRhythmChannelProgram(rig, 120, StandardKit);
        rig.NoteOn(SecondRhythmChannel, AcousticSnare, NoAttribute, 0);
        plainDrum = rig.Render(0.25);
    }

    Rig rig(*collection);
    SelectSecondRhythmChannelProgram(rig, 121, ChurchOrgan);
    SendAllTuning(rig, SecondRhythmChannel);
    SelectSecondRhythmChannelProgram(rig, 120, StandardKit);

    VERIFY_ARE_EQUAL(2.0, rig.Engine().ChannelState(SecondRhythmChannel).CoarseTuneSemitones,
        L"the channel keeps its tuning through the program change");

    rig.NoteOn(SecondRhythmChannel, AcousticSnare, NoAttribute, 0);

    VERIFY_IS_TRUE(LargestDifference(plainDrum, rig.Render(0.25)) < 1e-6, L"but the drum isn't tuned");

    rig.Engine().AllSoundOff(SecondRhythmChannel);
    (void)rig.Render(0.1);

    SelectSecondRhythmChannelProgram(rig, 121, ChurchOrgan);
    rig.NoteOn(SecondRhythmChannel, 60, NoAttribute, 0);

    const double reference = ReferenceHertz(*collection);
    const double cents = Cents(MeasureAfterAttack(rig, reference * RatioOf(225.0)), reference);

    Log::Comment(String().Format(L"The organ after going back to it: %+.3f cents", cents));

    VERIFY_IS_TRUE(std::fabs(cents - 225.0) < ToleranceCents, L"the tuning is there again for the organ");
}

// A note already sounding when its channel changes to a kit keeps its tuning, because it is still
// the melodic sound it started as.
void MidiSynthChannelTuningTests::TestHeldNoteKeepsItsTuning()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    const double reference = ReferenceHertz(*collection);

    Rig rig(*collection);
    SelectSecondRhythmChannelProgram(rig, 121, ChurchOrgan);
    SendAllTuning(rig, SecondRhythmChannel);

    rig.NoteOn(SecondRhythmChannel, 60, NoAttribute, 0);
    (void)rig.Render(0.1);

    SelectSecondRhythmChannelProgram(rig, 120, StandardKit);

    const double cents = Cents(MeasureAfterAttack(rig, reference * RatioOf(225.0)), reference);

    Log::Comment(String().Format(L"The held organ note after the change to a kit: %+.3f cents", cents));

    VERIFY_IS_TRUE(std::fabs(cents - 225.0) < ToleranceCents);
}
