// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// General MIDI 2 rhythm channels (sections 2.4, 3.3.1, 3.3.2, 3.3.7 to 3.3.18, 3.4.1, 3.6 and 3.7):
// what a channel playing a drum kit ignores, and how bank select moves a channel between drums and
// melody. Sent through the dispatcher, the way messages arrive from the wire.

#include "pch.h"

#include "SoundModuleTestRig.h"
#include "SynthPropertySource.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace SoundFontSynth;
using namespace SoundModuleTestRig;

namespace
{
    constexpr uint8_t DrumChannel = 9;

    // Becomes a rhythm channel when General MIDI 2 bank select 120 chooses a kit.
    constexpr uint8_t SecondChannel = 1;

    // Inside the test kit's key range, 35 to 81.
    constexpr uint8_t KitKey = 60;

    constexpr uint8_t NoAttribute = 0x00;

    constexpr uint8_t StatusControlChange = 0xB;
    constexpr uint8_t StatusProgramChange = 0xC;
    constexpr uint8_t StatusChannelPressure = 0xD;
    constexpr uint8_t StatusPitchBend = 0xE;
    constexpr uint8_t StatusRegisteredController = 0x2;

    // Modulation, the four pedals and the sound controllers.
    constexpr uint8_t IgnoredControllers[]{ 1, 64, 65, 66, 67, 71, 72, 73, 74, 75, 76, 77, 78 };

    // The basic font with brightness (CC 74) raising every zone by up to 100 cents, so a sound
    // controller has something to move. The default modulators already route modulation, channel
    // pressure and the bend.
    std::shared_ptr<SoundFont const> LoadFontWithBrightness()
    {
        auto source = Sf2Test::MakeBasicFont();

        Sf2Test::Modulator brightness{};
        brightness.Source = 0x0080 | 74;
        brightness.Destination = Sf2Test::Op::FineTune;
        brightness.Amount = 100;

        for (auto& instrument : source.Instruments)
        {
            for (auto& zone : instrument.Zones)
            {
                zone.Modulators.push_back(brightness);
            }
        }

        return LoadFont(source);
    }

    // Every message a rhythm channel ignores that this synthesizer acts on, each at full scale and
    // from both protocols.
    void SendAllPerformance(_Inout_ Rig& rig, _In_ uint8_t channel)
    {
        for (auto const controller : IgnoredControllers)
        {
            rig.SendMidi1(StatusControlChange, channel, controller, 127);
        }

        rig.Send(StatusControlChange, channel, 1, 0, 0xFFFFFFFFu);
        rig.Send(StatusControlChange, channel, 74, 0, 0xFFFFFFFFu);
        rig.SendMidi1(StatusPitchBend, channel, 0x7F, 0x7F);
        rig.Send(StatusPitchBend, channel, 0, 0, 0xFFFFFFFFu);
        rig.SendMidi1(StatusChannelPressure, channel, 127, 0);
        rig.Send(StatusChannelPressure, channel, 0, 0, 0xFFFFFFFFu);
        rig.SendMidi1(StatusControlChange, channel, 101, 0);
        rig.SendMidi1(StatusControlChange, channel, 100, 0);
        rig.SendMidi1(StatusControlChange, channel, 6, 24);
        rig.SendMidi1(StatusControlChange, channel, 38, 50);
        rig.SendMidi1(StatusControlChange, channel, 96, 0);
        rig.Send(StatusRegisteredController, channel, 0, 0, 12u << 25);
    }

    // Struck, then let go, so the tail shows whether a pedal held it. The test kit loops, so a held
    // note would keep sounding.
    std::vector<float> StrikeAndRelease(_Inout_ Rig& rig, _In_ uint8_t channel)
    {
        rig.NoteOn(channel, KitKey, NoAttribute, 0);
        auto output = rig.Render(0.1);

        rig.NoteOff(channel, KitKey);
        auto const tail = rig.Render(0.3);

        output.insert(output.end(), tail.begin(), tail.end());

        return output;
    }

    double Peak(_In_ std::vector<float> const& samples)
    {
        double peak{ 0.0 };

        for (auto const sample : samples)
        {
            peak = (std::max)(peak, std::fabs(static_cast<double>(sample)));
        }

        return peak;
    }

    bool SameAsPowerUp(_In_ ChannelReport const& report, _In_ ChannelReport const& powerUp)
    {
        if (report.PitchBend != powerUp.PitchBend ||
            report.ChannelPressure != powerUp.ChannelPressure ||
            report.PitchBendSensitivity != powerUp.PitchBendSensitivity)
        {
            return false;
        }

        for (size_t i = 0; i < ReportedControllerCount; i++)
        {
            if (report.Controllers[i] != powerUp.Controllers[i])
            {
                return false;
            }
        }

        return true;
    }

    std::string ChannelListOf(_In_ Synthesizer const& synthesizer, _In_ SoundFont const& font)
    {
        SynthPropertySource properties;
        properties.Build(font, SynthIdentity{}, PropertyExchangeText{});

        auto const& blob = properties.RebuildChannelListJson(synthesizer, font);

        return std::string(blob.data(), blob.size());
    }
}

class RhythmChannelTests
{
public:
    BEGIN_TEST_CLASS(RhythmChannelTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // A rhythm channel doesn't respond to modulation, the pedals, the sound controllers, pitch bend,
    // channel pressure or pitch bend sensitivity. That is channel 10, and any channel bank select
    // turned into a kit.
    TEST_METHOD(RhythmChannelIgnoresPerformanceMessages)
    {
        auto const font = LoadFontWithBrightness();
        VERIFY_IS_NOT_NULL(font.get());

        struct Case
        {
            uint8_t Channel;
            bool KitByBankSelect;
        };

        for (auto const& test : { Case{ DrumChannel, false }, Case{ SecondChannel, true } })
        {
            auto render = [&](bool sent)
            {
                Rig rig;
                VERIFY_IS_TRUE(rig.Initialize(font, 48000, BankSelectMode::GeneralMidi2));

                if (test.KitByBankSelect)
                {
                    rig.Engine().ProgramChangeWithBank(test.Channel, 120, 0, 0);
                }

                VERIFY_IS_TRUE(rig.Engine().IsDrumChannel(test.Channel));

                if (sent)
                {
                    SendAllPerformance(rig, test.Channel);
                }

                VERIFY_IS_TRUE(SameAsPowerUp(rig.Engine().Report(test.Channel), Synthesizer::PowerUpReport(test.Channel)),
                    L"the channel took none of it");

                return StrikeAndRelease(rig, test.Channel);
            };

            auto const plain = render(false);
            auto const sent = render(true);

            Log::Comment(String().Format(L"Channel %u: drum peak %.3f, largest difference %.7f",
                test.Channel + 1, Peak(plain), LargestDifference(plain, sent)));

            VERIFY_IS_TRUE(Peak(plain) > 0.01, L"the drum is heard");
            VERIFY_IS_TRUE(LargestDifference(plain, sent) < 1e-6, L"and sounds exactly as it did");
        }

        // A melodic channel takes all of them.
        Rig melodic;
        VERIFY_IS_TRUE(melodic.Initialize(font, 48000, BankSelectMode::GeneralMidi2));
        SendAllPerformance(melodic, 0);

        auto const report = melodic.Engine().Report(0);
        auto const powerUp = Synthesizer::PowerUpReport(0);

        VERIFY_ARE_NOT_EQUAL(powerUp.PitchBend, report.PitchBend);
        VERIFY_ARE_NOT_EQUAL(powerUp.ChannelPressure, report.ChannelPressure);
        VERIFY_ARE_NOT_EQUAL(powerUp.PitchBendSensitivity, report.PitchBendSensitivity);
        VERIFY_ARE_NOT_EQUAL(powerUp.Controllers[0], report.Controllers[0], L"modulation");
    }

    // A melodic channel keeps what it was sent when it changes to a kit, because General MIDI 2
    // says a program change resets nothing (3.2). The kit's notes still ignore all of it.
    TEST_METHOD(DrumNotesIgnoreWhatTheChannelHeld)
    {
        auto const font = LoadFontWithBrightness();
        VERIFY_IS_NOT_NULL(font.get());

        std::vector<float> plain;
        {
            Rig rig;
            VERIFY_IS_TRUE(rig.Initialize(font, 48000, BankSelectMode::GeneralMidi2));
            rig.Engine().ProgramChangeWithBank(SecondChannel, 120, 0, 0);
            plain = StrikeAndRelease(rig, SecondChannel);
        }

        Rig rig;
        VERIFY_IS_TRUE(rig.Initialize(font, 48000, BankSelectMode::GeneralMidi2));
        rig.Engine().ProgramChangeWithBank(SecondChannel, 121, 0, 0);
        SendAllPerformance(rig, SecondChannel);
        rig.Engine().ProgramChangeWithBank(SecondChannel, 120, 0, 0);

        VERIFY_IS_TRUE(rig.Engine().IsDrumChannel(SecondChannel));
        VERIFY_IS_FALSE(SameAsPowerUp(rig.Engine().Report(SecondChannel), Synthesizer::PowerUpReport(SecondChannel)),
            L"the channel keeps what it was sent through the program change");

        auto const held = StrikeAndRelease(rig, SecondChannel);

        Log::Comment(String().Format(L"Largest difference from the untouched kit: %.7f", LargestDifference(plain, held)));

        VERIFY_IS_TRUE(LargestDifference(plain, held) < 1e-6, L"but the drum ignores all of it");
    }

    // A rhythm channel never presses the damper or sostenuto pedal, but it still lifts them.
    // Otherwise a melodic note a pedal held when the channel chose a kit would ring for good.
    TEST_METHOD(RhythmChannelStillLiftsThePedals)
    {
        auto const font = LoadFont(Sf2Test::MakeBasicFont());
        VERIFY_IS_NOT_NULL(font.get());

        for (uint8_t const pedal : { uint8_t{ 64 }, uint8_t{ 66 } })
        {
            Rig rig;
            VERIFY_IS_TRUE(rig.Initialize(font, 48000, BankSelectMode::GeneralMidi2));
            rig.Engine().ProgramChangeWithBank(SecondChannel, 121, 0, 0);

            // Sostenuto holds only notes already down, so the key goes down first.
            rig.NoteOn(SecondChannel, 60, NoAttribute, 0);
            rig.SendMidi1(StatusControlChange, SecondChannel, pedal, 127);
            (void)rig.Render(0.1);
            rig.NoteOff(SecondChannel, 60);

            rig.Engine().ProgramChangeWithBank(SecondChannel, 120, 0, 0);
            (void)rig.Render(1.0);

            Log::Comment(String().Format(L"Pedal %u: %u voice sounding through the change to a kit", pedal, rig.Engine().ActiveVoiceCount()));

            VERIFY_ARE_EQUAL(1u, rig.Engine().ActiveVoiceCount(), L"the pedal holds the melodic note");

            rig.SendMidi1(StatusControlChange, SecondChannel, pedal, 0);
            (void)rig.Render(1.0);

            VERIFY_ARE_EQUAL(0u, rig.Engine().ActiveVoiceCount(), L"and lifting it lets the note go");
        }
    }

    // General MIDI 2 lets any channel be a melody channel, channel 10 included (2.4): bank 121 and
    // a program change make a rhythm channel melodic again (3.3.1). MIDI-CI says so too.
    TEST_METHOD(Bank121MakesChannel10Melodic)
    {
        auto const font = LoadFont(Sf2Test::MakeBasicFont());
        VERIFY_IS_NOT_NULL(font.get());

        Rig rig;
        VERIFY_IS_TRUE(rig.Initialize(font, 48000, BankSelectMode::GeneralMidi2));

        rig.SendMidi1(StatusControlChange, DrumChannel, 0, 121);
        rig.SendMidi1(StatusControlChange, DrumChannel, 32, 0);
        rig.SendMidi1(StatusProgramChange, DrumChannel, 0, 0);

        VERIFY_IS_FALSE(rig.Engine().IsDrumChannel(DrumChannel), L"channel 10 is a melody channel");

        auto const selection = rig.Engine().Selection(DrumChannel);
        VERIFY_IS_TRUE(selection.PresetIndex >= 0);
        VERIFY_IS_TRUE(font->Presets()[static_cast<size_t>(selection.PresetIndex)].Name == L"Sine Piano", L"playing the piano");

        auto channels = ChannelListOf(rig.Engine(), *font);

        VERIFY_IS_TRUE(channels.find("{\"title\":\"Melodic\",\"channel\":10,") != std::string::npos,
            L"MIDI-CI calls channel 10 melodic");

        rig.SendMidi1(StatusControlChange, DrumChannel, 0, 120);
        rig.SendMidi1(StatusProgramChange, DrumChannel, 0, 0);

        VERIFY_IS_TRUE(rig.Engine().IsDrumChannel(DrumChannel), L"bank 120 makes it a rhythm channel again");

        channels = ChannelListOf(rig.Engine(), *font);

        VERIFY_IS_TRUE(channels.find("{\"title\":\"Percussion\",\"channel\":10,") != std::string::npos);

        // Without General MIDI 2 addressing, bank 121 means nothing to channel 10.
        Rig gs;
        VERIFY_IS_TRUE(gs.Initialize(font, 48000, BankSelectMode::RolandGS));
        gs.Engine().ProgramChangeWithBank(DrumChannel, 121, 0, 0);

        VERIFY_IS_TRUE(gs.Engine().IsDrumChannel(DrumChannel), L"GS addressing keeps channel 10 a drum channel");
    }
};
