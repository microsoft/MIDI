// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

#include "MidiSynthRhythmChannelTests.h"
#include "MidiSynthTestRig.h"

// windows.h defines GetObject, which collides with a member of the JSON projection.
#pragma push_macro("GetObject")
#undef GetObject
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>
#pragma pop_macro("GetObject")

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace MidiSynth;
using namespace MidiSynthTestRig;

namespace json = winrt::Windows::Data::Json;

namespace
{
    constexpr uint8_t DrumChannel = 9;

    // Channel 11 becomes a rhythm channel when General MIDI 2 bank select 120 chooses a kit.
    constexpr uint8_t SecondRhythmChannel = 10;

    constexpr uint8_t StandardKit = 0;
    constexpr uint8_t ChurchOrgan = 19;

    // Rings for seconds, so a damper that held it would show long after the note off.
    constexpr uint8_t CrashCymbal = 49;

    constexpr uint8_t StatusProgramChange = 0xC;
    constexpr uint8_t StatusControlChange = 0xB;
    constexpr uint8_t StatusPitchBend = 0xE;
    constexpr uint8_t StatusRegisteredController = 0x2;

    constexpr uint8_t NoAttribute = 0x00;

    // Every message a rhythm channel ignores that this synthesizer acts on, each at full scale and
    // from both protocols: modulation, the damper pedal, pitch bend, and pitch bend sensitivity.
    void SendAllPerformance(_Inout_ Rig& rig, _In_ uint8_t channel)
    {
        rig.SendMidi1(StatusControlChange, channel, 1, 127);
        rig.Send(StatusControlChange, channel, 1, 0, 0xFFFFFFFFu);
        rig.SendMidi1(StatusControlChange, channel, 64, 127);
        rig.SendMidi1(StatusPitchBend, channel, 0x7F, 0x7F);
        rig.Send(StatusPitchBend, channel, 0, 0, 0xFFFFFFFFu);
        rig.SendMidi1(StatusControlChange, channel, 101, 0);
        rig.SendMidi1(StatusControlChange, channel, 100, 0);
        rig.SendMidi1(StatusControlChange, channel, 6, 24);
        rig.SendMidi1(StatusControlChange, channel, 38, 50);
        rig.Send(StatusRegisteredController, channel, 0, 0, 12u << 25);
    }

    void SelectProgram(_Inout_ Rig& rig, _In_ uint8_t channel, _In_ uint8_t bankMsb, _In_ uint8_t program)
    {
        rig.Engine().SetBankSelectMode(BankSelectMode::GeneralMidi2);
        rig.Engine().ProgramChangeWithBank(channel, bankMsb, 0, program);
    }

    // Struck, then let go, so the tail shows whether anything held it.
    std::vector<float> StrikeAndRelease(_Inout_ Rig& rig, _In_ uint8_t channel, _In_ uint8_t note)
    {
        rig.NoteOn(channel, note, NoAttribute, 0);
        auto output = rig.Render(0.1);

        rig.NoteOff(channel, note);
        const auto tail = rig.Render(0.6);

        output.insert(output.end(), tail.begin(), tail.end());

        return output;
    }

    json::JsonArray ChannelList(_In_ const SynthEngine& engine, _In_ const DlsCollection& collection)
    {
        PropertyExchangeSource source;
        source.Build(collection, SynthIdentity{});

        const auto& blob = source.RebuildChannelListJson(engine, collection);

        json::JsonArray parsed{ nullptr };
        VERIFY_IS_TRUE(json::JsonArray::TryParse(winrt::to_hstring(std::string(blob.data(), blob.size())), parsed));

        return parsed;
    }

    // The resource id a ChannelList entry links to for one resource.
    winrt::hstring LinkedId(_In_ const json::JsonObject& entry, _In_ const wchar_t* resource)
    {
        const auto links = entry.GetNamedArray(L"links");

        for (uint32_t i = 0; i < links.Size(); i++)
        {
            const auto link = links.GetObjectAt(i);

            if (link.GetNamedString(L"resource") == resource)
            {
                return link.GetNamedString(L"resId");
            }
        }

        return {};
    }
}

// General MIDI 2 recommends that a rhythm channel not respond to modulation (3.3.2), the damper
// pedal (3.3.7), pitch bend sensitivity (3.4.1) or pitch bend (3.6). That is channel 10, and any
// channel that bank select turned into a kit.
void MidiSynthRhythmChannelTests::TestRhythmChannelIgnoresPerformanceMessages()
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
        auto render = [&](bool sent)
        {
            Rig rig(*collection);

            if (test.KitByBankSelect)
            {
                SelectProgram(rig, SecondRhythmChannel, 120, StandardKit);
            }

            if (sent)
            {
                SendAllPerformance(rig, test.Channel);
            }

            const auto state = rig.Engine().ChannelState(test.Channel);

            VERIFY_IS_TRUE(state.PlaysDrumKit);
            VERIFY_ARE_EQUAL(0.0, state.Modulation, L"modulation is not taken");
            VERIFY_IS_FALSE(state.SustainPedal, L"the damper is not pressed");
            VERIFY_ARE_EQUAL(0.0, state.PitchBendNormalized, L"the bend is not taken");
            VERIFY_ARE_EQUAL(2.0, state.PitchBendRangeSemitones, L"and neither is its range");

            return StrikeAndRelease(rig, test.Channel, CrashCymbal);
        };

        const auto plain = render(false);
        const auto sent = render(true);

        Log::Comment(String().Format(L"Channel %u: cymbal peak %.3f, largest difference %.7f",
            test.Channel + 1, Peak(plain), LargestDifference(plain, sent)));

        VERIFY_IS_TRUE(Peak(plain) > 0.01, L"the cymbal is heard");
        VERIFY_IS_TRUE(LargestDifference(plain, sent) < 1e-6, L"and sounds exactly as it did");
    }

    // A melodic channel takes all of them.
    Rig melodic(*collection);
    SendAllPerformance(melodic, 0);

    const auto state = melodic.Engine().ChannelState(0);

    VERIFY_ARE_EQUAL(1.0, state.Modulation);
    VERIFY_IS_TRUE(state.SustainPedal);
    VERIFY_ARE_EQUAL(1.0, state.PitchBendNormalized);
    VERIFY_ARE_EQUAL(12.0, state.PitchBendRangeSemitones);
}

// A melodic channel keeps what it was sent when it changes to a kit, because General MIDI 2 says
// a program change resets nothing (3.2). The kit's notes still ignore all of it.
void MidiSynthRhythmChannelTests::TestDrumNotesIgnoreWhatTheChannelHeld()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    std::vector<float> plain;
    {
        Rig rig(*collection);
        SelectProgram(rig, SecondRhythmChannel, 120, StandardKit);
        plain = StrikeAndRelease(rig, SecondRhythmChannel, CrashCymbal);
    }

    Rig rig(*collection);
    SelectProgram(rig, SecondRhythmChannel, 121, ChurchOrgan);
    SendAllPerformance(rig, SecondRhythmChannel);
    SelectProgram(rig, SecondRhythmChannel, 120, StandardKit);

    const auto state = rig.Engine().ChannelState(SecondRhythmChannel);

    VERIFY_ARE_EQUAL(1.0, state.Modulation, L"the channel keeps its modulation through the program change");
    VERIFY_IS_TRUE(state.SustainPedal, L"and its damper");
    VERIFY_ARE_EQUAL(1.0, state.PitchBendNormalized, L"and its bend");

    const auto held = StrikeAndRelease(rig, SecondRhythmChannel, CrashCymbal);

    Log::Comment(String().Format(L"Largest difference from the untouched kit: %.7f", LargestDifference(plain, held)));

    VERIFY_IS_TRUE(LargestDifference(plain, held) < 1e-6, L"but the cymbal ignores all of it");
}

// A rhythm channel never presses the damper, but it still lifts it. Otherwise a melodic note the
// damper held when the channel chose a kit would ring for good.
void MidiSynthRhythmChannelTests::TestRhythmChannelStillLiftsTheDamper()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    Rig rig(*collection);
    SelectProgram(rig, SecondRhythmChannel, 121, ChurchOrgan);

    rig.SendMidi1(StatusControlChange, SecondRhythmChannel, 64, 127);
    rig.NoteOn(SecondRhythmChannel, 60, NoAttribute, 0);
    (void)rig.Render(0.1);
    rig.NoteOff(SecondRhythmChannel, 60);

    SelectProgram(rig, SecondRhythmChannel, 120, StandardKit);
    (void)rig.Render(1.0);

    VERIFY_IS_TRUE(rig.Engine().ActiveVoiceCount() == 1, L"the damper holds the organ through the change to a kit");

    rig.SendMidi1(StatusControlChange, SecondRhythmChannel, 64, 0);

    VERIFY_IS_FALSE(rig.Engine().ChannelState(SecondRhythmChannel).SustainPedal);

    (void)rig.Render(3.0);

    VERIFY_IS_TRUE(rig.Engine().ActiveVoiceCount() == 0, L"and lifting it lets the organ go");
}

// General MIDI 2 lets any channel be a melody channel, channel 10 included (2.4): bank 121 and a
// program change make a rhythm channel melodic again (3.3.1). MIDI-CI says so too.
void MidiSynthRhythmChannelTests::TestBank121MakesChannel10Melodic()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    const auto* const organ = collection->FindInstrument(0, 0, ChurchOrgan, false);
    VERIFY_IS_NOT_NULL(organ);

    Rig rig(*collection);
    rig.Engine().SetBankSelectMode(BankSelectMode::GeneralMidi2);

    // As a file sends it, through the dispatcher.
    rig.SendMidi1(StatusControlChange, DrumChannel, 0, 121);
    rig.SendMidi1(StatusControlChange, DrumChannel, 32, 0);
    rig.SendMidi1(StatusProgramChange, DrumChannel, ChurchOrgan, 0);

    auto state = rig.Engine().ChannelState(DrumChannel);

    VERIFY_IS_FALSE(state.PlaysDrumKit, L"channel 10 is a melody channel");
    VERIFY_IS_TRUE(state.Instrument == organ, L"playing the organ");

    // And it is melodic in every way: it bends now.
    rig.Send(StatusPitchBend, DrumChannel, 0, 0, 0xFFFFFFFFu);
    VERIFY_ARE_EQUAL(1.0, rig.Engine().ChannelState(DrumChannel).PitchBendNormalized);

    auto channels = ChannelList(rig.Engine(), *collection);
    auto entry = channels.GetObjectAt(DrumChannel);

    Log::Comment(String().Format(L"Channel 10: '%s' playing '%s'",
        entry.GetNamedString(L"title").c_str(), entry.GetNamedString(L"programTitle", L"").c_str()));

    VERIFY_IS_TRUE(entry.GetNamedString(L"title") == L"Melodic", L"MIDI-CI calls channel 10 melodic");
    VERIFY_IS_TRUE(entry.GetNamedString(L"programTitle", L"") == winrt::hstring{ organ->Name }, L"names the organ");
    VERIFY_IS_TRUE(LinkedId(entry, L"ProgramList") == L"melodic", L"links the melodic programs");
    VERIFY_IS_TRUE(LinkedId(entry, L"ChCtrlList") == L"channel", L"and the melodic controllers");

    // Bank 120 makes it a rhythm channel again.
    rig.SendMidi1(StatusControlChange, DrumChannel, 0, 120);
    rig.SendMidi1(StatusProgramChange, DrumChannel, StandardKit, 0);

    VERIFY_IS_TRUE(rig.Engine().ChannelState(DrumChannel).PlaysDrumKit);

    channels = ChannelList(rig.Engine(), *collection);
    entry = channels.GetObjectAt(DrumChannel);

    VERIFY_IS_TRUE(entry.GetNamedString(L"title") == L"Percussion");
    VERIFY_IS_TRUE(LinkedId(entry, L"ChCtrlList") == L"drums");

    // Without General MIDI 2 addressing, bank 121 means nothing to channel 10.
    Rig gs(*collection);
    gs.Engine().SetBankSelectMode(BankSelectMode::RolandGS);
    gs.Engine().ProgramChangeWithBank(DrumChannel, 121, 0, StandardKit);

    VERIFY_IS_TRUE(gs.Engine().ChannelState(DrumChannel).PlaysDrumKit, L"GS addressing keeps channel 10 a drum channel");
}

// MIDI-CI names the program a channel plays the way the engine finds it: the variation after XG or
// General MIDI 2 addressing, and the capital tone when the sound set has no such variation.
void MidiSynthRhythmChannelTests::TestChannelListNamesWhatPlays()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    // Program 0 in variation bank 16 is "Piano 1d". Bank 8 holds a separate instrument for it, but
    // that one is called "Piano 1" like the capital tone, so it couldn't show which was named.
    const auto* const variation = collection->FindInstrument(16, 0, 0, false);
    const auto* const capitalTone = collection->FindInstrument(0, 0, 0, false);

    VERIFY_IS_TRUE(variation != nullptr && capitalTone != nullptr && variation->Name != capitalTone->Name,
        L"the sound set has a variation with a name of its own");

    struct Case
    {
        BankSelectMode Mode;
        uint8_t Msb;
        uint8_t Lsb;
        const DlsInstrument* Expected;
        const wchar_t* Name;
    };

    const Case cases[]
    {
        { BankSelectMode::RolandGS, 16, 0, variation, L"GS carries the variation in the MSB" },
        { BankSelectMode::YamahaXG, 0, 16, variation, L"XG carries it in the LSB" },
        { BankSelectMode::GeneralMidi2, 121, 16, variation, L"so does General MIDI 2, under bank 121" },
        { BankSelectMode::RolandGS, 99, 0, capitalTone, L"a variation the sound set lacks plays the capital tone" },
    };

    for (const auto& test : cases)
    {
        Rig rig(*collection);
        rig.Engine().SetBankSelectMode(test.Mode);
        rig.Engine().ProgramChangeWithBank(0, test.Msb, test.Lsb, 0);

        VERIFY_IS_TRUE(rig.Engine().ChannelState(0).Instrument == test.Expected, L"the engine plays it");

        const auto title = ChannelList(rig.Engine(), *collection).GetObjectAt(0).GetNamedString(L"programTitle", L"");

        Log::Comment(String().Format(L"Bank %u, %u: '%s'", test.Msb, test.Lsb, title.c_str()));

        VERIFY_IS_TRUE(title == winrt::hstring{ test.Expected->Name }, test.Name);
    }

    // A System On can change what the same bank select plays, so a subscriber hears about it.
    Rig rig(*collection);
    rig.Engine().ProgramChangeWithBank(0, 0, 16, 0);

    PropertyExchangeSource source;
    source.Build(*collection, SynthIdentity{});

    (void)source.ChannelListChanged(rig.Engine());

    rig.Engine().NotifyAddressingConvention(BankSelectMode::YamahaXG);

    VERIFY_IS_TRUE(rig.Engine().ChannelState(0).Instrument == variation, L"under XG the same select plays the variation");
    VERIFY_IS_TRUE(source.ChannelListChanged(rig.Engine()), L"so the channel list changed");
}
