// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// MIDI 2.0 Registered Controllers 0,1 and 0,2: channel fine and coarse tuning (M2-104-UM section
// 7.4.7.1, with the ranges from the MIDI 1.0 specification), sent through the dispatcher the way
// they arrive from the wire.

#include "pch.h"

#include "Modulation.h"
#include "SoundModuleTestRig.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace SoundFontSynth;
using namespace SoundModuleTestRig;

namespace
{
    constexpr uint8_t StatusControlChange = 0xB;
    constexpr uint8_t StatusRegisteredController = 0x2;
    constexpr uint8_t FineTuning = 1;
    constexpr uint8_t CoarseTuning = 2;

    constexpr uint8_t NoAttribute = 0x00;
    constexpr uint8_t AttributePitch79 = 0x03;

    // A second of a sine is good to a few hundredths of a cent, so anything further off than this is wrong.
    constexpr double ToleranceCents = 0.3;

    double CentsBetween(_In_ double hertz, _In_ double referenceHertz)
    {
        return 1200.0 * std::log2(hertz / referenceHertz);
    }

    // Measured once the note has started, over a whole second.
    double MeasureAfterOnset(_Inout_ Rig& rig)
    {
        (void)rig.Render(0.1);

        return MeasureFrequency(rig.Render(1.0), 48000);
    }

    struct TuningCase
    {
        uint32_t Value;
        double Cents;
    };

    void CheckTuning(_In_ uint8_t index, _In_ std::initializer_list<TuningCase> cases)
    {
        auto const font = LoadFont(Sf2Test::MakeBasicFont());

        for (auto const& test : cases)
        {
            Rig rig;
            VERIFY_IS_TRUE(rig.Initialize(font));

            rig.Send(StatusRegisteredController, 0, 0, index, test.Value);
            rig.NoteOn(0, 69, NoAttribute, 0);

            auto const cents = CentsBetween(MeasureAfterOnset(rig), HertzOf(69.0));

            Log::Comment(String().Format(L"RPN 0,%u = 0x%08X: expected %+.3f cents, measured %+.3f cents",
                index, test.Value, test.Cents, cents));

            VERIFY_IS_TRUE(std::fabs(cents - test.Cents) < ToleranceCents);
        }
    }
}

class ChannelTuningTests
{
public:
    BEGIN_TEST_CLASS(ChannelTuningTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // Fine tuning is 100 cents either side of 0x80000000, and every bit counts: 0xA1000000 falls
    // between two of the steps a MIDI 1.0 data entry MSB can make.
    TEST_METHOD(FineTuningMovesThePitch)
    {
        CheckTuning(FineTuning,
        {
            { 0x80000000, 0.0 },
            { 0x60000000, -25.0 },
            { 0xA0000000, 25.0 },
            { 0xA1000000, 25.78125 },
            { 0x00000000, -100.0 },
            { 0xFFFFFFFF, 100.0 },
        });
    }

    // Coarse tuning is whole semitones in the top seven bits, and 64 is in tune. The other 25 bits
    // are undefined, so they change nothing.
    TEST_METHOD(CoarseTuningMovesThePitch)
    {
        CheckTuning(CoarseTuning,
        {
            { 64u << 25, 0.0 },
            { 66u << 25, 200.0 },
            { 61u << 25, -300.0 },
            { (66u << 25) | 0x01FFFFFFu, 200.0 },
            { (61u << 25) | 0x00ABCDEFu, -300.0 },
        });
    }

    // A value both protocols can carry plays exactly what the MIDI 1.0 data entry plays.
    TEST_METHOD(Midi2TuningMatchesMidi1)
    {
        auto const font = LoadFont(Sf2Test::MakeBasicFont());

        auto render = [&](auto const& tune)
        {
            Rig rig;
            VERIFY_IS_TRUE(rig.Initialize(font));

            tune(rig);
            rig.NoteOn(0, 69, NoAttribute, 0);

            return rig.Render(0.3);
        };

        auto const untuned = render([](Rig&) {});

        // 25 cents flat: data entry 48 for RPN 0,1, or 0x60000000.
        auto const fineMidi1 = render([](Rig& rig)
        {
            rig.SendMidi1(StatusControlChange, 0, 101, 0);
            rig.SendMidi1(StatusControlChange, 0, 100, 1);
            rig.SendMidi1(StatusControlChange, 0, 6, 48);
        });

        auto const fineMidi2 = render([](Rig& rig) { rig.Send(StatusRegisteredController, 0, 0, FineTuning, 0x60000000); });

        // Two semitones sharp: data entry 66 for RPN 0,2, or 66 in the top seven bits.
        auto const coarseMidi1 = render([](Rig& rig)
        {
            rig.SendMidi1(StatusControlChange, 0, 101, 0);
            rig.SendMidi1(StatusControlChange, 0, 100, 2);
            rig.SendMidi1(StatusControlChange, 0, 6, 66);
        });

        auto const coarseMidi2 = render([](Rig& rig) { rig.Send(StatusRegisteredController, 0, 0, CoarseTuning, 66u << 25); });

        VERIFY_IS_TRUE(LargestDifference(untuned, fineMidi1) > 1e-3 && LargestDifference(untuned, coarseMidi1) > 1e-3,
            L"both MIDI 1.0 tunings change the sound");

        VERIFY_IS_TRUE(LargestDifference(fineMidi1, fineMidi2) < 1e-6, L"fine tuning sounds the same from either protocol");
        VERIFY_IS_TRUE(LargestDifference(coarseMidi1, coarseMidi2) < 1e-6, L"coarse tuning sounds the same from either protocol");
    }

    // Tuning moves notes that are already sounding, not only the next ones.
    TEST_METHOD(TuningRetunesSoundingNotes)
    {
        Rig rig;
        VERIFY_IS_TRUE(rig.Initialize(LoadFont(Sf2Test::MakeBasicFont())));

        rig.NoteOn(0, 69, NoAttribute, 0);
        (void)rig.Render(0.1);

        rig.Send(StatusRegisteredController, 0, 0, CoarseTuning, 66u << 25);
        rig.Send(StatusRegisteredController, 0, 0, FineTuning, 0xA0000000);

        auto const cents = CentsBetween(MeasureAfterOnset(rig), HertzOf(69.0));

        Log::Comment(String().Format(L"Sounding note after +2 semitones and +25 cents: %+.3f cents", cents));

        VERIFY_IS_TRUE(std::fabs(cents - 225.0) < ToleranceCents);
    }

    // M2-104-UM 7.4.15: channel tuning moves the pitch from wherever the note put it, so it adds to
    // a Pitch 7.9 attribute rather than being replaced by it.
    TEST_METHOD(TuningAddsToExactPitch)
    {
        Rig rig;
        VERIFY_IS_TRUE(rig.Initialize(LoadFont(Sf2Test::MakeBasicFont())));

        rig.Send(StatusRegisteredController, 0, 0, FineTuning, 0xA0000000);
        rig.Send(StatusRegisteredController, 0, 0, CoarseTuning, 66u << 25);

        // Note 69 at Pitch 7.9 = 69.5.
        rig.NoteOn(0, 69, AttributePitch79, static_cast<uint16_t>((69u << 9) | 256u));

        auto const cents = CentsBetween(MeasureAfterOnset(rig), HertzOf(69.0));

        Log::Comment(String().Format(L"69.5 with +25 cents and +2 semitones: %+.3f cents", cents));

        VERIFY_IS_TRUE(std::fabs(cents - 275.0) < ToleranceCents);
    }

    // General MIDI 2 sections 3.4.2 and 3.4.3: a rhythm channel doesn't respond to channel fine or
    // coarse tuning. That is channel 10, and any channel that bank select turned into a kit.
    TEST_METHOD(DrumKitIgnoresChannelTuning)
    {
        auto const font = LoadFont(Sf2Test::MakeBasicFont());

        struct Case
        {
            uint8_t Channel;
            bool KitByBankSelect;
        };

        for (auto const test : { Case{ DrumChannel, false }, Case{ 0, true } })
        {
            auto render = [&](bool tuned)
            {
                Rig rig;
                VERIFY_IS_TRUE(rig.Initialize(font, 48000, BankSelectMode::GeneralMidi2));

                if (test.KitByBankSelect)
                {
                    rig.Engine().ProgramChangeWithBank(test.Channel, 120, 0, 0);
                }

                VERIFY_IS_TRUE(rig.Engine().IsDrumChannel(test.Channel));

                if (tuned)
                {
                    SendAllTuning(rig, test.Channel);
                }

                auto const report = rig.Engine().Report(test.Channel);
                auto const powerUp = Synthesizer::PowerUpReport(test.Channel);

                VERIFY_ARE_EQUAL(powerUp.FineTuning, report.FineTuning, L"fine tuning is not taken");
                VERIFY_ARE_EQUAL(powerUp.CoarseTuning, report.CoarseTuning, L"coarse tuning is not taken");

                rig.NoteOn(test.Channel, 69, NoAttribute, 0);

                return rig.Render(0.3);
            };

            auto const plain = render(false);
            auto const tuned = render(true);

            // Measured after the onset: the first step up out of silence is not a real crossing.
            auto const hertz = MeasureFrequency(std::vector<float>(tuned.begin() + 2 * 4800, tuned.end()), 48000);

            Log::Comment(String().Format(L"Channel %u: the drum plays at %.3f Hz after tuning was sent", test.Channel + 1, hertz));

            VERIFY_IS_TRUE(IsNear(hertz, 440.0, 0.0005), L"the drum plays at its own pitch");
            VERIFY_IS_TRUE(LargestDifference(plain, tuned) < 1e-6, L"and sounds exactly as it did");
        }

        // A melodic channel still takes both.
        Rig melodic;
        VERIFY_IS_TRUE(melodic.Initialize(font));

        SendAllTuning(melodic, 0);

        auto const report = melodic.Engine().Report(0);
        auto const powerUp = Synthesizer::PowerUpReport(0);

        VERIFY_ARE_NOT_EQUAL(powerUp.FineTuning, report.FineTuning);
        VERIFY_ARE_NOT_EQUAL(powerUp.CoarseTuning, report.CoarseTuning);
    }

    // A melodic channel keeps its tuning when it changes to a kit, because General MIDI 2 says a
    // program change resets no RPN. The tuning still doesn't reach the drums, and it's there again
    // when the channel goes back to a melodic program.
    TEST_METHOD(KeptTuningLeavesDrumsAlone)
    {
        Rig rig;
        VERIFY_IS_TRUE(rig.Initialize(LoadFont(Sf2Test::MakeBasicFont()), 48000, BankSelectMode::GeneralMidi2));

        SendAllTuning(rig, 0);
        rig.Engine().ProgramChangeWithBank(0, 120, 0, 0);

        VERIFY_IS_TRUE(rig.Engine().IsDrumChannel(0));

        rig.NoteOn(0, 69, NoAttribute, 0);
        auto const drum = CentsBetween(MeasureAfterOnset(rig), HertzOf(69.0));

        rig.Engine().AllSoundOff(0);
        (void)rig.Render(0.1);

        rig.Engine().ProgramChangeWithBank(0, 121, 0, 0);
        VERIFY_IS_FALSE(rig.Engine().IsDrumChannel(0));

        rig.NoteOn(0, 69, NoAttribute, 0);
        auto const melodic = CentsBetween(MeasureAfterOnset(rig), HertzOf(69.0));

        Log::Comment(String().Format(L"The kit played %+.3f cents, then the melodic program %+.3f cents", drum, melodic));

        VERIFY_IS_TRUE(std::fabs(drum) < ToleranceCents, L"the drum isn't tuned");
        VERIFY_IS_TRUE(std::fabs(melodic - 225.0) < ToleranceCents, L"the melodic program is");
    }

    // A note already sounding when its channel changes to a kit keeps its tuning, because it is
    // still the melodic sound it started as.
    TEST_METHOD(HeldNoteKeepsItsTuning)
    {
        Rig rig;
        VERIFY_IS_TRUE(rig.Initialize(LoadFont(Sf2Test::MakeBasicFont()), 48000, BankSelectMode::GeneralMidi2));

        SendAllTuning(rig, 0);
        rig.NoteOn(0, 69, NoAttribute, 0);
        (void)rig.Render(0.1);

        rig.Engine().ProgramChangeWithBank(0, 120, 0, 0);

        // A controller recalculates every sounding note on the channel, so it must not take the
        // tuning away either.
        rig.SendMidi1(StatusControlChange, 0, 7, 100);

        auto const cents = CentsBetween(MeasureAfterOnset(rig), HertzOf(69.0));

        Log::Comment(String().Format(L"The held note after the change to a kit: %+.3f cents", cents));

        VERIFY_IS_TRUE(std::fabs(cents - 225.0) < ToleranceCents);
    }

    // General MIDI 2 sections 4.2 and 4.3: master tuning doesn't change drums either.
    TEST_METHOD(MasterTuningLeavesDrumsAlone)
    {
        auto const font = LoadFont(Sf2Test::MakeBasicFont());

        struct Case
        {
            uint8_t Channel;
            bool KitByBankSelect;
            double ExpectedCents;
        };

        // A melodic channel moves an octave and 25 cents. Channel 10 and a kit chosen by bank
        // select stay where they are.
        for (auto const test : { Case{ 0, false, 1225.0 }, Case{ DrumChannel, false, 0.0 }, Case{ 1, true, 0.0 } })
        {
            Rig rig;
            VERIFY_IS_TRUE(rig.Initialize(font, 48000, BankSelectMode::GeneralMidi2));

            if (test.KitByBankSelect)
            {
                rig.Engine().ProgramChangeWithBank(test.Channel, 120, 0, 0);
            }

            rig.Engine().SetMasterCoarseTuning(76);
            rig.Engine().SetMasterFineTuning(8192 + 2048);

            rig.NoteOn(test.Channel, 69, NoAttribute, 0);

            auto const cents = CentsBetween(MeasureAfterOnset(rig), HertzOf(69.0));

            Log::Comment(String().Format(L"Channel %u: expected %+.1f cents, measured %+.3f cents",
                test.Channel + 1, test.ExpectedCents, cents));

            VERIFY_IS_TRUE(std::fabs(cents - test.ExpectedCents) < ToleranceCents);
        }
    }

    // The MIDI 1.0 specification asks a receiver to set its LSB to zero when an MSB arrives, so a
    // new data entry MSB does not keep the low bits of the last value.
    TEST_METHOD(DataEntryMsbClearsTheLsb)
    {
        Rig rig;
        VERIFY_IS_TRUE(rig.Initialize(LoadFont(Sf2Test::MakeBasicFont())));

        rig.SendMidi1(StatusControlChange, 0, 101, 0);
        rig.SendMidi1(StatusControlChange, 0, 100, 1);
        rig.SendMidi1(StatusControlChange, 0, 6, 0x40);
        rig.SendMidi1(StatusControlChange, 0, 38, 0x20);

        VERIFY_ARE_EQUAL(ScaleUpValue(0x2020, 14, 32), rig.Engine().Report(0).FineTuning, L"MSB and LSB together");

        rig.SendMidi1(StatusControlChange, 0, 6, 0x41);

        VERIFY_ARE_EQUAL(ScaleUpValue(0x2080, 14, 32), rig.Engine().Report(0).FineTuning, L"a new MSB sets the LSB to zero");
    }

private:
    static constexpr uint8_t DrumChannel = 9;

    // Every way channel tuning can arrive: the MIDI 1.0 data entry and the MIDI 2.0 registered
    // controller, 25 cents and two semitones sharp.
    static void SendAllTuning(_Inout_ Rig& rig, _In_ uint8_t channel)
    {
        rig.SendMidi1(StatusControlChange, channel, 101, 0);
        rig.SendMidi1(StatusControlChange, channel, 100, 1);
        rig.SendMidi1(StatusControlChange, channel, 6, 80);
        rig.SendMidi1(StatusControlChange, channel, 100, 2);
        rig.SendMidi1(StatusControlChange, channel, 6, 66);
        rig.Send(StatusRegisteredController, channel, 0, FineTuning, 0xA0000000);
        rig.Send(StatusRegisteredController, channel, 0, CoarseTuning, 66u << 25);
    }
};
