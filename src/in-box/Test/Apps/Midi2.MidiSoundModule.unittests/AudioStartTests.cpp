// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Getting from the first note to the audio device without losing it. While no audio runs, the
// worker answers everything that doesn't need sound, but a channel voice message waits for the audio
// thread; when no device can be had, notes are dropped so nothing else waits behind them.

#include "pch.h"

#include "SoundModuleTestRig.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace SoundFontSynth;
using namespace SoundModuleTestRig;

namespace
{
    constexpr int64_t QpcFrequency = 10'000'000;
    constexpr uint32_t BlockFrames = 256;

    // MIDI 1.0 on group 1, channel 1.
    constexpr uint32_t NoteOn = 0x20903C64;
    constexpr uint32_t ProgramChange = 0x20C00100;
    constexpr uint32_t QuietVolume = 0x20B00714;

    // MIDI 2.0 note on, channel 1, note 64, three quarters velocity.
    constexpr uint32_t Midi2NoteOn[2]{ 0x40904000, 0xC0000000 };

    // The volume controller's place in a channel report.
    constexpr size_t VolumeReportIndex = 1;

    std::unique_ptr<SynthCore> MakeCore()
    {
        auto font = LoadFont(Sf2Test::MakeBasicFont());

        if (font == nullptr)
        {
            return nullptr;
        }

        return std::make_unique<SynthCore>(font, SynthIdentity{}, "SF2SYNTH-TEST0002", PropertyExchangeText{});
    }
}

class AudioStartTests
{
public:
    BEGIN_TEST_CLASS(AudioStartTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // The audio owner lets the device go just as a note arrives, and clears the flag the note set.
    // The note must still reach the audio thread, and ask for audio again.
    TEST_METHOD(NoteWaitsForAudioWhenItsFlagWasCleared)
    {
        auto core = MakeCore();
        VERIFY_IS_NOT_NULL(core.get());

        core->QueueInbound(&NoteOn, 1, 1000);

        VERIFY_IS_TRUE(core->AudioWanted());

        core->ClearAudioWanted(core->LastChannelVoiceTimestamp());

        VERIFY_IS_FALSE(core->AudioWanted());

        core->PumpWithoutAudio(false);

        VERIFY_IS_TRUE(core->AudioWanted(), L"the queued note asks for audio again");
        VERIFY_ARE_EQUAL(0u, core->Engine().ActiveVoiceCount(), L"and is left for the audio thread");

        std::vector<float> block(static_cast<size_t>(BlockFrames) * 2);
        core->RenderAudio(block.data(), BlockFrames, 1000 + QpcFrequency * BlockFrames / SynthCore::DefaultSampleRate, QpcFrequency);

        VERIFY_IS_TRUE(core->LastActiveVoiceCount() > 0, L"which plays it");
    }

    // With no audio device to be had, notes are dropped in both protocols, and everything else
    // still reaches the engine.
    TEST_METHOD(DiscardingDropsOnlyNoteOns)
    {
        auto core = MakeCore();
        VERIFY_IS_NOT_NULL(core.get());

        auto const powerUp = core->Engine().Report(0);

        core->QueueInbound(&ProgramChange, 1, 1000);
        core->QueueInbound(&QuietVolume, 1, 1000);
        core->QueueInbound(&NoteOn, 1, 1000);
        core->QueueInbound(Midi2NoteOn, 2, 1000);

        core->PumpWithoutAudio(true);

        auto const report = core->Engine().Report(0);

        VERIFY_ARE_EQUAL(0u, core->Engine().ActiveVoiceCount(), L"no note started");
        VERIFY_ARE_EQUAL(1, static_cast<int>(report.Program), L"the program change arrived");
        VERIFY_ARE_NOT_EQUAL(powerUp.Controllers[VolumeReportIndex], report.Controllers[VolumeReportIndex], L"and the volume");

        // The same note dispatched does start a voice, so the check above means something.
        (void)core->Dispatcher().ProcessWords(&NoteOn, 1);

        VERIFY_IS_TRUE(core->Engine().ActiveVoiceCount() > 0);
    }
};
