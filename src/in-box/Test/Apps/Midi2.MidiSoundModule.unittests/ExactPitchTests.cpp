// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// MIDI 2.0 Note On with attribute #3, Pitch 7.9 (M2-104-UM section 7.4.15.3), sent through the
// dispatcher the way it arrives from the wire. With that attribute the note number is only an index,
// and a sender can rotate it through all 128 values to play any pitch (Appendix C.3).

#include "pch.h"

#include "SoundModuleTestRig.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace SoundFontSynth;
using namespace SoundModuleTestRig;

namespace
{
    constexpr uint8_t NoAttribute = 0x00;
    constexpr uint8_t AttributeManufacturer = 0x01;
    constexpr uint8_t AttributePitch79 = 0x03;

    constexpr uint8_t DrumChannel = 9;

    constexpr uint8_t NarrowProgram = 0;
    constexpr uint8_t FullRangeProgram = 1;

    // Seven bits of note number and nine of fraction.
    constexpr uint16_t Pitch79(_In_ uint32_t note, _In_ uint32_t fraction) noexcept
    {
        return static_cast<uint16_t>((note << 9) | fraction);
    }

    // A sine whose decay is scaled by key, so a render shows which key the synthesizer scaled by:
    // decay is a quarter of a second on key 60 and 8 seconds on key 0.
    Sf2Test::Zone MakeKeyScaledZone(_In_ uint8_t lowKey, _In_ uint8_t highKey)
    {
        constexpr uint16_t KeynumToVolumeEnvelopeDecay = 40;

        Sf2Test::Zone zone{};

        zone.Generators.push_back(Sf2Test::Range(Sf2Test::Op::KeyRange, lowKey, highKey));
        zone.Generators.push_back(Sf2Test::Signed(Sf2Test::Op::DecayVolumeEnvelope, -2400));
        zone.Generators.push_back({ Sf2Test::Op::SustainVolumeEnvelope, 200 });
        zone.Generators.push_back({ KeynumToVolumeEnvelopeDecay, 100 });
        zone.Generators.push_back({ Sf2Test::Op::SampleModes, 1 });
        zone.Generators.push_back({ Sf2Test::Op::SampleId, 0 });

        return zone;
    }

    // Program 0 covers only keys 48 to 72, the way a real bank's flute or violin does not reach the
    // bottom of the keyboard. Program 1 covers every key. The kit covers 35 to 81.
    Sf2Test::Font MakeFont()
    {
        Sf2Test::Font font{};

        font.Samples.push_back(Sf2Test::MakeSine("Sine"));

        Sf2Test::Instrument narrow{};
        narrow.Name = "Narrow Sine";
        narrow.Zones.push_back(MakeKeyScaledZone(48, 72));

        Sf2Test::Instrument fullRange{};
        fullRange.Name = "Full Range Sine";
        fullRange.Zones.push_back(MakeKeyScaledZone(0, 127));

        Sf2Test::Instrument drum{};
        drum.Name = "Sine Drum";

        Sf2Test::Zone drumZone{};
        drumZone.Generators.push_back(Sf2Test::Range(Sf2Test::Op::KeyRange, 35, 81));
        drumZone.Generators.push_back({ Sf2Test::Op::SampleModes, 1 });
        drumZone.Generators.push_back({ Sf2Test::Op::SampleId, 0 });
        drum.Zones.push_back(drumZone);

        font.Instruments.push_back(narrow);
        font.Instruments.push_back(fullRange);
        font.Instruments.push_back(drum);

        auto preset = [](char const* name, uint16_t bank, uint16_t program, uint16_t instrument)
        {
            Sf2Test::Preset result{};
            result.Name = name;
            result.Bank = bank;
            result.Program = program;

            Sf2Test::Zone zone{};
            zone.Generators.push_back({ Sf2Test::Op::Instrument, instrument });
            result.Zones.push_back(zone);

            return result;
        };

        font.Presets.push_back(preset("Narrow", 0, NarrowProgram, 0));
        font.Presets.push_back(preset("Full Range", 0, FullRangeProgram, 1));
        font.Presets.push_back(preset("Sine Kit", 128, 0, 2));

        return font;
    }

    std::shared_ptr<SoundFont const> LoadFont()
    {
        return SoundModuleTestRig::LoadFont(MakeFont());
    }
}

class ExactPitchTests
{
public:
    BEGIN_TEST_CLASS(ExactPitchTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(PlaysTheAttributePitch)
    {
        auto const font = LoadFont();

        struct Case
        {
            uint8_t AttributeType;
            uint16_t AttributeData;
            double ExpectedPitch;
        };

        // Half a semitone up, a third of a semitone up, and an attribute that is not a pitch.
        for (auto const test : { Case{ AttributePitch79, Pitch79(69, 256), 69.5 },
                                 Case{ AttributePitch79, Pitch79(69, 171), 69.0 + 171.0 / 512.0 },
                                 Case{ AttributeManufacturer, Pitch79(69, 256), 69.0 } })
        {
            Rig rig;
            VERIFY_IS_TRUE(rig.Initialize(font));
            rig.Engine().ProgramChange(0, FullRangeProgram);

            rig.NoteOn(0, 69, test.AttributeType, test.AttributeData);
            (void)rig.Render(0.1);

            auto const measured = MeasureFrequency(rig.Render(0.5), 48000);
            auto const expected = HertzOf(test.ExpectedPitch);

            Log::Comment(String().Format(L"Attribute type %u: expected %.2f Hz, measured %.2f Hz",
                test.AttributeType, expected, measured));

            VERIFY_IS_TRUE(IsNear(measured, expected, 0.001));
        }
    }

    // Pitch Bend and Per-Note Pitch Bend are offsets from the attribute's pitch, and a later
    // Registered Per-Note Controller #3 (Pitch 7.25) sets a new absolute pitch for the note.
    TEST_METHOD(OtherPitchMessagesWorkFromTheAttributePitch)
    {
        auto const font = LoadFont();

        auto measure = [&](auto const& play)
        {
            Rig rig;
            VERIFY_IS_TRUE(rig.Initialize(font));
            rig.Engine().ProgramChange(0, FullRangeProgram);

            play(rig);
            (void)rig.Render(0.1);

            return MeasureFrequency(rig.Render(0.5), 48000);
        };

        // A quarter of the full range up is one semitone with the default range of two.
        constexpr uint32_t BendUpOneSemitone = 0xC0000000u;

        auto const channelBend = measure([](Rig& rig)
        {
            rig.Send(0xE, 0, 0, 0, BendUpOneSemitone);
            rig.NoteOn(0, 69, AttributePitch79, Pitch79(69, 256));
        });

        auto const perNoteBend = measure([](Rig& rig)
        {
            rig.NoteOn(0, 69, AttributePitch79, Pitch79(69, 256));
            rig.Send(0x6, 0, 69, 0, BendUpOneSemitone);
        });

        auto const pitch725 = measure([](Rig& rig)
        {
            rig.NoteOn(0, 69, AttributePitch79, Pitch79(69, 256));
            rig.Send(0x0, 0, 69, PerNoteControllerPitch, 71u << 25);
        });

        Log::Comment(String().Format(L"Channel bend %.2f Hz, per-note bend %.2f Hz, Pitch 7.25 %.2f Hz",
            channelBend, perNoteBend, pitch725));

        VERIFY_IS_TRUE(IsNear(channelBend, HertzOf(70.5), 0.001), L"the channel bend adds to the attribute pitch");
        VERIFY_IS_TRUE(IsNear(perNoteBend, HertzOf(70.5), 0.001), L"the per-note bend adds to the attribute pitch");
        VERIFY_IS_TRUE(IsNear(pitch725, HertzOf(71.0), 0.001), L"Pitch 7.25 replaces the attribute pitch");
    }

    // The sender in Appendix C.3 plays Middle C as note 0, then note 1, and so on, each with
    // Pitch 7.9 = 60.0. Each must sound exactly like a plain Middle C, even on an instrument that
    // has no zone for the note number, and with envelopes scaled for the key that is heard.
    TEST_METHOD(NoteIndexDoesNotChooseTheZones)
    {
        auto const font = LoadFont();

        for (auto const program : { NarrowProgram, FullRangeProgram })
        {
            auto render = [&](uint8_t note, uint8_t attributeType, uint16_t attributeData, uint32_t& voices)
            {
                Rig rig;
                VERIFY_IS_TRUE(rig.Initialize(font));
                rig.Engine().ProgramChange(0, program);

                rig.NoteOn(0, note, attributeType, attributeData);
                auto output = rig.Render(0.3);

                voices = rig.Engine().ActiveVoiceCount();

                return output;
            };

            uint32_t voices{ 0 };
            auto const middleC = render(60, NoAttribute, 0, voices);

            // Measured after the onset: the first upward step out of silence is not a real crossing.
            auto const middleCHertz = MeasureFrequency(std::vector<float>(middleC.begin() + 2 * 2400, middleC.end()), 48000);

            Log::Comment(String().Format(L"Program %u, plain note 60: %u voices, %.2f Hz", program, voices, middleCHertz));

            VERIFY_ARE_EQUAL(1u, voices, L"the plain note sounds");
            VERIFY_IS_TRUE(IsNear(middleCHertz, HertzOf(60.0), 0.002));

            for (uint8_t const index : { uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 100 }, uint8_t{ 127 } })
            {
                auto const rotated = render(index, AttributePitch79, Pitch79(60, 0), voices);
                auto const difference = LargestDifference(middleC, rotated);

                Log::Comment(String().Format(L"Program %u, note index %u at pitch 60.0: %u voices, largest difference %.7f",
                    program, index, voices, difference));

                VERIFY_ARE_EQUAL(1u, voices, L"a note index with no zone of its own still sounds");
                VERIFY_IS_TRUE(difference < 1e-6, L"and sounds exactly like the plain note");
            }
        }

        // The narrow program really has no zone for note 0, so the check above means something.
        Rig plain;
        VERIFY_IS_TRUE(plain.Initialize(font));
        plain.Engine().ProgramChange(0, NarrowProgram);
        plain.NoteOn(0, 0, NoAttribute, 0);
        (void)plain.Render(0.01);

        VERIFY_ARE_EQUAL(0u, plain.Engine().ActiveVoiceCount());
    }

    // On a drum kit the note number picks the drum, so it keeps doing that with a pitch attached, and
    // the pitch only tunes the drum. That holds for a kit chosen by bank select on a melodic channel too.
    TEST_METHOD(DrumKitKeepsTheNoteNumber)
    {
        auto const font = LoadFont();

        struct Case
        {
            uint8_t Channel;
            bool KitByBankSelect;
        };

        for (auto const test : { Case{ DrumChannel, false }, Case{ 0, true } })
        {
            auto start = [&](Rig& rig)
            {
                VERIFY_IS_TRUE(rig.Initialize(font, 48000, test.KitByBankSelect ? BankSelectMode::GeneralMidi2 : BankSelectMode::Automatic));

                if (test.KitByBankSelect)
                {
                    // General MIDI 2 addressing: bank 120 is a drum kit on any channel.
                    rig.Engine().ProgramChangeWithBank(test.Channel, 120, 0, 0);
                }

                VERIFY_IS_TRUE(rig.Engine().Selection(test.Channel).IsDrumChannel);
            };

            // The kit has nothing on note 20, whatever pitch the note asks for.
            Rig outsideTheKit;
            start(outsideTheKit);
            outsideTheKit.NoteOn(test.Channel, 20, AttributePitch79, Pitch79(60, 0));
            (void)outsideTheKit.Render(0.01);

            VERIFY_ARE_EQUAL(0u, outsideTheKit.Engine().ActiveVoiceCount(), L"the note number, not the pitch, picks the drum");

            // Note 40 is in the kit, and the pitch tunes it.
            Rig insideTheKit;
            start(insideTheKit);
            insideTheKit.NoteOn(test.Channel, 40, AttributePitch79, Pitch79(64, 0));
            (void)insideTheKit.Render(0.1);

            auto const measured = MeasureFrequency(insideTheKit.Render(0.5), 48000);

            Log::Comment(String().Format(L"Channel %u: drum at pitch 64 measured %.2f Hz", test.Channel, measured));

            VERIFY_IS_TRUE(IsNear(measured, HertzOf(64.0), 0.002), L"the pitch tunes the drum");
        }
    }

    // Note Off and the per-note messages address the note index. Two notes on one pitch are two notes,
    // and the key the pitch falls on is not a note at all.
    TEST_METHOD(NoteOffAddressesTheNoteIndex)
    {
        Rig rig;
        VERIFY_IS_TRUE(rig.Initialize(LoadFont()));
        rig.Engine().ProgramChange(0, FullRangeProgram);

        rig.NoteOn(0, 0, AttributePitch79, Pitch79(60, 0));
        rig.NoteOn(0, 1, AttributePitch79, Pitch79(60, 0));
        (void)rig.Render(0.1);

        VERIFY_ARE_EQUAL(2u, rig.Engine().ActiveVoiceCount(), L"two note indexes on one pitch are two notes");

        rig.NoteOff(0, 60);
        (void)rig.Render(1.0);

        VERIFY_ARE_EQUAL(2u, rig.Engine().ActiveVoiceCount(), L"a Note Off for the pitch's key ends neither");

        rig.NoteOff(0, 0);
        (void)rig.Render(1.0);

        VERIFY_ARE_EQUAL(1u, rig.Engine().ActiveVoiceCount(), L"a Note Off for one index ends only that note");

        rig.NoteOff(0, 1);
        (void)rig.Render(1.0);

        VERIFY_ARE_EQUAL(0u, rig.Engine().ActiveVoiceCount());
    }

    // Pitch 7.9 reaches 127 and 511/512, about 13.3 kHz. Rendered fast enough to measure.
    TEST_METHOD(PlaysTheTopOfThePitchRange)
    {
        auto const font = LoadFont();

        constexpr uint32_t FastRate = 192000;

        for (auto const fraction : { 0u, 256u, 511u })
        {
            Rig rig;
            VERIFY_IS_TRUE(rig.Initialize(font, FastRate));
            rig.Engine().ProgramChange(0, FullRangeProgram);

            rig.NoteOn(0, 69, AttributePitch79, Pitch79(127, fraction));
            (void)rig.Render(0.05);

            auto const measured = MeasureFrequency(rig.Render(0.3), FastRate);
            auto const expected = HertzOf(127.0 + fraction / 512.0);

            Log::Comment(String().Format(L"Pitch 127 + %u/512: expected %.1f Hz, measured %.1f Hz", fraction, expected, measured));

            VERIFY_IS_TRUE(IsNear(measured, expected, 0.002));
        }
    }
};
