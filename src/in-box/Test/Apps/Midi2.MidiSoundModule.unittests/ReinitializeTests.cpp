// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// What has to come through when the engine is initialized again. The app starts every synth at
// 48 kHz, so MIDI-CI can describe the bank before there is any audio device, and an audio device
// at another rate initializes the engine again when the first note opens it.

#include "pch.h"

#include "SoundModuleTestRig.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace SoundFontSynth;
using namespace SoundModuleTestRig;

class ReinitializeTests
{
public:
    BEGIN_TEST_CLASS(ReinitializeTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // A GM2 or XG System On is answered before any audio device is open. Initializing again must
    // keep the convention the sender chose, or a kit chosen by bank select plays as a melodic
    // program.
    TEST_METHOD(ResetMessageSurvivesInitialize)
    {
        auto const font = LoadFont(Sf2Test::MakeBasicFont());

        struct Case
        {
            std::vector<uint8_t> SystemOn;
            uint8_t KitBankMsb;
        };

        Case const cases[]
        {
            { { 0x7E, 0x7F, 0x09, 0x03 }, 120 },
            { { 0x43, 0x10, 0x4C, 0x00, 0x00, 0x7E, 0x00 }, 127 },
        };

        for (auto const& test : cases)
        {
            // Automatic addressing at 48 kHz, which is how the app starts every synth.
            Rig rig;
            VERIFY_IS_TRUE(rig.Initialize(font));

            rig.SendSysEx(test.SystemOn);

            // What SynthCore::PrepareForSampleRate does when the device runs at 96 kHz.
            SynthesizerConfig config{};
            config.SampleRate = 96000;
            config.EnableEffects = false;

            VERIFY_IS_TRUE(rig.Engine().Initialize(font, config));

            rig.Engine().ProgramChangeWithBank(1, test.KitBankMsb, 0, 0);

            Log::Comment(String().Format(L"Bank %u after the System On: drum channel %s",
                test.KitBankMsb, rig.Engine().IsDrumChannel(1) ? L"yes" : L"no"));

            VERIFY_IS_TRUE(rig.Engine().IsDrumChannel(1), L"a kit chosen by bank select plays a kit");
        }

        // A convention chosen in the configuration still wins over the one a sender used.
        Rig rig;
        VERIFY_IS_TRUE(rig.Initialize(font));

        rig.SendSysEx({ 0x7E, 0x7F, 0x09, 0x03 });

        SynthesizerConfig config{};
        config.SampleRate = 96000;
        config.EnableEffects = false;
        config.BankSelect = BankSelectMode::RolandGS;

        VERIFY_IS_TRUE(rig.Engine().Initialize(font, config));

        rig.Engine().ProgramChangeWithBank(1, 120, 0, 0);

        VERIFY_IS_FALSE(rig.Engine().IsDrumChannel(1), L"GS addressing reads bank 120 as a melodic bank");
    }

    // The volume belongs to the window, not to the engine's configuration. Preparing for a device
    // rate does not save and restore it, because a volume the window set in between would be put
    // back to the old one, so initializing must leave it alone.
    TEST_METHOD(VolumeSurvivesARateChange)
    {
        auto const font = LoadFont(Sf2Test::MakeBasicFont());
        VERIFY_IS_NOT_NULL(font.get());

        // On the heap: a SynthCore holds its queues inline and is far too big for a test's stack.
        auto const core = std::make_unique<SynthCore>(font, SynthIdentity{}, "SF2SYNTH-TEST0002", PropertyExchangeText{});

        core->SetUserVolumeDb(-12.0);

        VERIFY_IS_TRUE(core->PrepareForSampleRate(96000));

        VERIFY_ARE_EQUAL(96000u, core->Engine().SampleRate(), L"the engine runs at the device rate");
        VERIFY_ARE_EQUAL(-12.0, core->UserVolumeDb(), L"the volume came through");
    }
};
