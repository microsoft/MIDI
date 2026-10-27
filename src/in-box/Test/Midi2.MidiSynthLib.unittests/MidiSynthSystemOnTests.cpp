// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

#include "MidiSynthSystemOnTests.h"
#include "MidiSynthTestRig.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace MidiSynth;
using namespace MidiSynthTestRig;

namespace
{
    constexpr uint8_t StatusControlChange = 0xB;
    constexpr uint8_t StatusProgramChange = 0xC;
    constexpr uint8_t StatusPitchBend = 0xE;
    constexpr uint8_t StatusNoteOn = 0x9;

    constexpr uint8_t DrawbarOrgan = 16;
    constexpr uint8_t ChurchOrgan = 19;

    struct ResetMessage
    {
        const wchar_t* Name;
        std::vector<uint8_t> Payload;
    };

    // Payloads without F0 and F7. The GS checksum is the real one, though the synthesizer does not
    // check it.
    std::vector<ResetMessage> ResetMessages()
    {
        return
        {
            { L"GM1 System On", { 0x7E, 0x7F, 0x09, 0x01 } },
            { L"GM2 System On", { 0x7E, 0x7F, 0x09, 0x03 } },
            { L"GS Reset", { 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41 } },
            { L"XG System On", { 0x43, 0x10, 0x4C, 0x00, 0x00, 0x7E, 0x00 } },
        };
    }

    // Moves everything a reset is supposed to put back away from its power-up value.
    void ChangeEverything(_Inout_ Rig& rig, _In_ uint8_t channel)
    {
        rig.SendMidi1(StatusControlChange, channel, 0, 8);
        rig.SendMidi1(StatusControlChange, channel, 32, 1);
        rig.SendMidi1(StatusProgramChange, channel, ChurchOrgan, 0);
        rig.SendMidi1(StatusControlChange, channel, 1, 100);
        rig.SendMidi1(StatusControlChange, channel, 7, 20);
        rig.SendMidi1(StatusControlChange, channel, 10, 0);
        rig.SendMidi1(StatusControlChange, channel, 11, 50);
        rig.SendMidi1(StatusControlChange, channel, 64, 127);
        rig.SendMidi1(StatusControlChange, channel, 91, 0);
        rig.SendMidi1(StatusControlChange, channel, 93, 100);
        rig.SendMidi1(StatusPitchBend, channel, 0x7F, 0x7F);

        // Pitch bend sensitivity 12, fine tuning, then coarse tuning, leaving RPN 0/2 selected.
        rig.SendMidi1(StatusControlChange, channel, 101, 0);
        rig.SendMidi1(StatusControlChange, channel, 100, 0);
        rig.SendMidi1(StatusControlChange, channel, 6, 12);
        rig.SendMidi1(StatusControlChange, channel, 100, 1);
        rig.SendMidi1(StatusControlChange, channel, 6, 96);
        rig.SendMidi1(StatusControlChange, channel, 100, 2);
        rig.SendMidi1(StatusControlChange, channel, 6, 70);
    }

    double PeakOfLast(_In_ const std::vector<float>& stereo, _In_ size_t frames)
    {
        const size_t samples = (std::min)(stereo.size(), frames * 2);

        return Peak(std::vector<float>(stereo.end() - static_cast<ptrdiff_t>(samples), stereo.end()));
    }

    // The largest step between neighboring samples of one side.
    double LargestStep(_In_ const std::vector<float>& stereo)
    {
        double largest = 0.0;

        for (size_t i = 2; i < stereo.size(); i++)
        {
            largest = (std::max)(largest, std::fabs(static_cast<double>(stereo[i]) - stereo[i - 2]));
        }

        return largest;
    }
}

// RP-003 asks for pitch bend centered, volume 100 and every other controller at its normal
// position; GM2 4.9 adds that the whole device returns to its initial state. Each reset message
// has to do all of it, on every channel, or a file that leans on the reset plays on whatever the
// last file left behind.
void MidiSynthSystemOnTests::TestEveryResetRestoresChannelDefaults()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    const SynthChannelState defaults{};

    for (const auto& reset : ResetMessages())
    {
        Log::Comment(String().Format(L"%s", reset.Name));

        Rig rig{ *collection };

        for (uint8_t channel = 0; channel < 16; channel++)
        {
            ChangeEverything(rig, channel);
        }

        // Channel 3 a rhythm part and channel 10 a melodic one, by the GS Use For Rhythm Part
        // message (block 3 is channel 3, block 0 is channel 10).
        rig.SendSysEx({ 0x41, 0x10, 0x42, 0x12, 0x40, 0x13, 0x15, 0x01, 0x17 });
        rig.SendSysEx({ 0x41, 0x10, 0x42, 0x12, 0x40, 0x10, 0x15, 0x00, 0x1B });

        const auto changed = rig.Engine().ChannelState(0);

        VERIFY_ARE_EQUAL(changed.Program, ChurchOrgan, L"the setup took");
        VERIFY_IS_TRUE(rig.Engine().IsDrumChannel(2) && !rig.Engine().IsDrumChannel(9), L"and so did the rhythm parts");

        rig.SendSysEx(reset.Payload);

        for (uint8_t channel = 0; channel < 16; channel++)
        {
            const auto state = rig.Engine().ChannelState(channel);

            VERIFY_ARE_EQUAL(state.Program, uint8_t{ 0 }, L"program 1, the piano");
            VERIFY_ARE_EQUAL(state.BankMsb, uint8_t{ 0 }, L"bank select MSB 0");
            VERIFY_ARE_EQUAL(state.BankLsb, uint8_t{ 0 }, L"bank select LSB 0");
            VERIFY_ARE_EQUAL(state.Volume, defaults.Volume, L"volume 100");
            VERIFY_ARE_EQUAL(state.Expression, 1.0, L"expression 127");
            VERIFY_ARE_EQUAL(state.Modulation, 0.0, L"no modulation");
            VERIFY_ARE_EQUAL(state.PanOffset, 0.0, L"pan centered");
            VERIFY_IS_FALSE(state.SustainPedal, L"damper up");
            VERIFY_ARE_EQUAL(state.ReverbSend, defaults.ReverbSend, L"reverb send 40");
            VERIFY_ARE_EQUAL(state.ChorusSend, 0.0, L"chorus send 0");
            VERIFY_ARE_EQUAL(state.PitchBendNormalized, 0.0, L"pitch bend centered");
            VERIFY_ARE_EQUAL(state.PitchBendRangeSemitones, 2.0, L"bend range two semitones");
            VERIFY_ARE_EQUAL(state.FineTuneCents, 0.0, L"no fine tuning");
            VERIFY_ARE_EQUAL(state.CoarseTuneSemitones, 0.0, L"no coarse tuning");
            VERIFY_ARE_EQUAL(state.RpnMsb, uint8_t{ 0x7F }, L"no registered parameter selected");
            VERIFY_ARE_EQUAL(state.RpnLsb, uint8_t{ 0x7F }, L"no registered parameter selected");
            VERIFY_ARE_EQUAL(state.IsDrumChannel, channel == 9, L"channel 10, and only channel 10, plays drums");
        }
    }
}

// GM2 4.9.1: master volume is part of the initial state, so a reset turns it back up.
void MidiSynthSystemOnTests::TestResetRestoresMasterVolume()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    auto playOrgan = [](Rig& rig)
    {
        rig.SendMidi1(StatusProgramChange, 0, DrawbarOrgan, 0);
        rig.SendMidi1(StatusNoteOn, 0, 60, 100);

        return PeakOfLast(rig.Render(0.3), 4800);
    };

    Rig untouched{ *collection };
    const double expected = playOrgan(untouched);

    for (const auto& reset : ResetMessages())
    {
        Rig rig{ *collection };

        // Universal real time master volume, at about a quarter.
        rig.SendSysEx({ 0x7F, 0x7F, 0x04, 0x01, 0x00, 0x20 });
        rig.SendSysEx(reset.Payload);

        const double measured = playOrgan(rig);
        const double differenceDb = (expected > 0.0 && measured > 0.0) ? 20.0 * std::log10(measured / expected) : -999.0;

        Log::Comment(String().Format(L"%s: %.2f dB from a fresh synthesizer", reset.Name, differenceDb));

        VERIFY_IS_TRUE(std::fabs(differenceDb) < 0.5, L"the reset put master volume back to full");
    }
}

// GM2 4.9.1 and 4.9.2: on a reset, sounding notes mute immediately but without a click. Cutting a
// voice off mid waveform is a step from wherever the wave was to zero, which is the click. The
// voice must fade, quickly, and be gone well inside the 100 ms GM2 allows for the whole reset.
void MidiSynthSystemOnTests::TestResetFadesInsteadOfCutting()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    for (const auto& reset : ResetMessages())
    {
        Rig rig{ *collection };

        // A sustained organ low enough that its own steps between samples are small.
        rig.SendMidi1(StatusProgramChange, 0, DrawbarOrgan, 0);
        rig.SendMidi1(StatusNoteOn, 0, 48, 120);

        const auto before = rig.Render(0.25);
        const double peakBefore = PeakOfLast(before, 2400);
        const double stepBefore = LargestStep(before);

        rig.SendSysEx(reset.Payload);

        const auto fade = rig.Render(0.002);
        const double peakFade = Peak(fade);

        // The first sample after the reset continues the wave rather than jumping to silence.
        const double jump = (std::max)(
            std::fabs(static_cast<double>(fade[0]) - before[before.size() - 2]),
            std::fabs(static_cast<double>(fade[1]) - before[before.size() - 1]));

        (void)rig.Render(0.05);
        const auto tail = rig.Render(0.01);

        Log::Comment(String().Format(
            L"%s: peak before %.4f, peak in the first 2 ms after %.4f, step at the reset %.5f, largest step before %.5f",
            reset.Name, peakBefore, peakFade, jump, stepBefore));

        VERIFY_IS_TRUE(peakBefore > 0.01, L"the organ was playing");
        VERIFY_IS_TRUE(peakFade > peakBefore * 0.05, L"the note fades rather than stopping dead");
        VERIFY_IS_TRUE(jump <= stepBefore * 2.0, L"and there is no step in the wave where the reset landed");
        VERIFY_ARE_EQUAL(rig.Engine().ActiveVoiceCount(), 0u, L"the note is gone within about 60 ms");
        VERIFY_ARE_EQUAL(Peak(tail), 0.0, L"and the output is silent");

        // A note started right after the reset is not mistaken for the one fading out.
        rig.SendMidi1(StatusNoteOn, 0, 60, 100);
        VERIFY_ARE_EQUAL(rig.Engine().ChannelState(0).Program, uint8_t{ 0 }, L"the next note plays the piano");
        VERIFY_ARE_EQUAL(rig.Engine().ActiveVoiceCount(), 1u, L"and sounds");
    }
}

// GM2 4.9.3: a device that is exclusively GM ignores GM System Off. This synthesizer has no other
// mode, so turning GM off must not reset it, and must not change how bank select is read.
void MidiSynthSystemOnTests::TestGmSystemOffIsIgnored()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    Rig rig{ *collection };

    // GM2 addressing first, so a GM System Off that was taken for GM1 System On would show.
    rig.SendSysEx({ 0x7E, 0x7F, 0x09, 0x03 });

    rig.SendMidi1(StatusProgramChange, 0, ChurchOrgan, 0);
    rig.SendMidi1(StatusControlChange, 0, 7, 20);
    rig.SendMidi1(StatusNoteOn, 0, 60, 100);

    (void)rig.Render(0.05);

    rig.SendSysEx({ 0x7E, 0x7F, 0x09, 0x02 });

    const auto state = rig.Engine().ChannelState(0);

    VERIFY_ARE_EQUAL(state.Program, ChurchOrgan, L"the program is kept");
    VERIFY_ARE_EQUAL(state.Volume, 20.0 / 127.0, L"and so is the volume");
    VERIFY_ARE_EQUAL(rig.Engine().ActiveVoiceCount(), 1u, L"the note keeps sounding");
    VERIFY_IS_TRUE(rig.Engine().EffectiveBankSelectMode() == BankSelectMode::GeneralMidi2, L"bank select is still read as GM2");
}
