// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

#include "MidiSynthCompatibleModeTests.h"
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

    constexpr uint8_t LowTom = 45;

    // A steady tone, so its pitch can be measured to a small fraction of a cent.
    constexpr uint8_t ChurchOrgan = 19;

    constexpr uint8_t StatusControlChange = 0xB;
    constexpr uint8_t StatusPitchBend = 0xE;

    constexpr uint8_t NoAttribute = 0x00;

    void SetRegisteredParameter(_Inout_ Rig& rig, _In_ uint8_t channel, _In_ uint8_t index, _In_ uint8_t msb)
    {
        rig.SendMidi1(StatusControlChange, channel, 101, 0);
        rig.SendMidi1(StatusControlChange, channel, 100, index);
        rig.SendMidi1(StatusControlChange, channel, 6, msb);
    }

    // A drum struck after whatever setup the caller sends, then let go.
    template <typename Setup>
    std::vector<float> StrikeDrum(_In_ const DlsCollection& collection, _In_ Setup setup)
    {
        Rig rig(collection, CompatibleConfig());
        setup(rig);

        rig.NoteOn(DrumChannel, LowTom, NoAttribute, 0);
        auto output = rig.Render(0.2);

        rig.NoteOff(DrumChannel, LowTom);
        const auto tail = rig.Render(0.4);

        output.insert(output.end(), tail.begin(), tail.end());

        return output;
    }
}

// The older synthesizer's drums bend, and take modulation, the damper pedal, pitch bend sensitivity
// and fine tuning. Only coarse tuning leaves them alone.
void MidiSynthCompatibleModeTests::TestDrumsTakePerformanceControls()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    {
        Rig rig(*collection, CompatibleConfig());

        rig.SendMidi1(StatusControlChange, DrumChannel, 1, 127);
        rig.SendMidi1(StatusControlChange, DrumChannel, 64, 127);
        rig.SendMidi1(StatusPitchBend, DrumChannel, 0x7F, 0x7F);
        SetRegisteredParameter(rig, DrumChannel, 0, 12);
        SetRegisteredParameter(rig, DrumChannel, 2, 76);

        const auto state = rig.Engine().ChannelState(DrumChannel);

        VERIFY_IS_TRUE(state.PlaysDrumKit);
        VERIFY_ARE_EQUAL(1.0, state.Modulation, L"the drum channel takes modulation");
        VERIFY_IS_TRUE(state.SustainPedal, L"and the damper");
        VERIFY_IS_TRUE(state.PitchBendNormalized > 0.99, L"and the bend");
        VERIFY_ARE_EQUAL(12.0, state.PitchBendRangeSemitones, L"and its range");
        VERIFY_ARE_EQUAL(12.0, state.CoarseTuneSemitones, L"and keeps coarse tuning");
    }

    const auto plain = StrikeDrum(*collection, [](Rig&) {});

    const auto bent = StrikeDrum(*collection, [](Rig& rig)
    {
        rig.SendMidi1(StatusPitchBend, DrumChannel, 0x7F, 0x7F);
    });

    const auto fineTuned = StrikeDrum(*collection, [](Rig& rig)
    {
        SetRegisteredParameter(rig, DrumChannel, 1, 0x60);
    });

    const auto coarseTuned = StrikeDrum(*collection, [](Rig& rig)
    {
        SetRegisteredParameter(rig, DrumChannel, 2, 76);
    });

    const auto held = StrikeDrum(*collection, [](Rig& rig)
    {
        rig.SendMidi1(StatusControlChange, DrumChannel, 64, 127);
    });

    Log::Comment(String().Format(L"Tom peak %.3f. Largest difference: bent %.4f, fine tuned %.4f, coarse tuned %.7f, damper %.4f",
        Peak(plain), LargestDifference(plain, bent), LargestDifference(plain, fineTuned),
        LargestDifference(plain, coarseTuned), LargestDifference(plain, held)));

    VERIFY_IS_TRUE(Peak(plain) > 0.01, L"the tom is heard");
    VERIFY_IS_TRUE(LargestDifference(plain, bent) > 0.01, L"pitch bend moves it");
    VERIFY_IS_TRUE(LargestDifference(plain, fineTuned) > 0.01, L"fine tuning moves it");
    VERIFY_IS_TRUE(LargestDifference(plain, coarseTuned) < 1e-6, L"coarse tuning doesn't");

    // The damper keeps the tom from entering its release at the note off.
    VERIFY_IS_TRUE(LargestDifference(plain, held) > 0.001, L"the damper holds it");
}

// MIDI-CI tells a client what a Compatible drum channel responds to: everything a melodic channel
// does except coarse tuning.
void MidiSynthCompatibleModeTests::TestDrumChannelListsWhatDrumsTake()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    PropertyExchangeSource source;
    source.Build(*collection, SynthIdentity{});

    VERIFY_IS_TRUE(PropertyExchangeSource::IsKnownControllerListResourceId(CompatibleDrumControllerListResourceId));

    const auto text = [](const std::vector<char>& blob) { return std::string(blob.data(), blob.size()); };

    const auto compatible = text(source.ControllerListJson(CompatibleDrumControllerListResourceId));
    const auto melodic = text(source.ControllerListJson(ControllerListResourceId));
    const auto modern = text(source.ControllerListJson(DrumControllerListResourceId));

    VERIFY_IS_TRUE(compatible.find("\"/pitchBend\"") != std::string::npos, L"the list offers pitch bend");
    VERIFY_IS_TRUE(compatible.find("\"/modulation\"") != std::string::npos, L"and modulation");
    VERIFY_IS_TRUE(compatible.find("\"/sustain\"") != std::string::npos, L"and the damper");
    VERIFY_IS_TRUE(compatible.find("/tuning/fine") != std::string::npos, L"and fine tuning");
    VERIFY_IS_TRUE(compatible.find("/tuning/coarse") == std::string::npos, L"but not coarse tuning");
    VERIFY_IS_TRUE(melodic.find("/tuning/coarse") != std::string::npos, L"which the melodic list has");
    VERIFY_IS_TRUE(modern.find("/tuning/fine") == std::string::npos, L"and the Modern drum list leaves out fine tuning too");

    auto channelLink = [&](const SynthConfig& config)
    {
        Rig rig(*collection, config);

        const auto& blob = source.RebuildChannelListJson(rig.Engine(), *collection);

        json::JsonArray parsed{ nullptr };
        VERIFY_IS_TRUE(json::JsonArray::TryParse(winrt::to_hstring(text(blob)), parsed));

        const auto links = parsed.GetObjectAt(DrumChannel).GetNamedArray(L"links");

        for (uint32_t i = 0; i < links.Size(); i++)
        {
            const auto link = links.GetObjectAt(i);

            if (link.GetNamedString(L"resource") == L"ChCtrlList")
            {
                return link.GetNamedString(L"resId");
            }
        }

        return winrt::hstring{};
    };

    VERIFY_IS_TRUE(channelLink(CompatibleConfig()) == winrt::to_hstring(CompatibleDrumControllerListResourceId),
        L"Compatible channel 10 links the Compatible drum list");
    VERIFY_IS_TRUE(channelLink(TestConfig()) == winrt::to_hstring(DrumControllerListResourceId),
        L"Modern channel 10 links the General MIDI 2 drum list");
}

// The older synthesizer read channel tuning when each note started. A tuning change leaves the notes
// already sounding where they are, and only the notes after it take the new tuning. Modern retunes
// the sounding notes too.
void MidiSynthCompatibleModeTests::TestTuningIsFixedAtNoteOn()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    for (const bool compatible : { true, false })
    {
        const auto config = compatible ? CompatibleConfig() : TestConfig();

        double reference = 0.0;
        {
            Rig plain(*collection, config);
            plain.Engine().ProgramChange(0, ChurchOrgan);
            plain.NoteOn(0, 60, NoAttribute, 0);
            (void)plain.Render(0.3);
            reference = EstimateHertz(plain.Render(0.6), plain.SampleRate(), 150.0, 400.0);
        }

        Rig rig(*collection, config);
        rig.Engine().ProgramChange(0, ChurchOrgan);
        rig.NoteOn(0, 60, NoAttribute, 0);
        (void)rig.Render(0.3);

        // Fine tuning 0x5000, 25 cents sharp, while the note sounds.
        SetRegisteredParameter(rig, 0, 1, 0x50);
        (void)rig.Render(0.05);

        const double held = Cents(EstimateHertz(rig.Render(0.6), rig.SampleRate(), 150.0, 400.0), reference);

        rig.NoteOff(0, 60);
        (void)rig.Render(0.5);

        rig.NoteOn(0, 60, NoAttribute, 0);
        (void)rig.Render(0.3);

        const double next = Cents(EstimateHertz(rig.Render(0.6), rig.SampleRate(), 150.0, 400.0), reference);

        Log::Comment(String().Format(L"%s: the held note moved %+.2f cents, the next note is %+.2f cents",
            compatible ? L"Compatible" : L"Modern", held, next));

        VERIFY_IS_TRUE(std::fabs(next - 25.0) < 1.0, L"the next note takes the new tuning");

        if (compatible)
        {
            VERIFY_IS_TRUE(std::fabs(held) < 1.0, L"the held note stays where it started");
        }
        else
        {
            VERIFY_IS_TRUE(std::fabs(held - 25.0) < 1.0, L"Modern retunes the held note");
        }
    }
}

namespace
{
    // Holds count organ notes on a channel, from firstNote up. Nothing is rendered, so no voice is
    // freed or faded in between.
    void HoldNotes(_Inout_ Rig& rig, _In_ uint8_t channel, _In_ uint8_t firstNote, _In_ uint8_t count)
    {
        rig.Engine().ProgramChange(channel, ChurchOrgan);

        for (uint8_t i = 0; i < count; i++)
        {
            rig.NoteOn(channel, static_cast<uint8_t>(firstNote + i), NoAttribute, 0);
        }
    }
}

// The older synthesizer ranks channel 10 first, then channels 1 to 9, then 11 to 16, and a note can
// only take a voice from a channel ranked the same or lower. So drums are the last thing to be cut.
void MidiSynthCompatibleModeTests::TestChannel10HasTheHighestPriority()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    struct Case
    {
        uint8_t Channel;
        bool Plays;
        const wchar_t* Name;
    };

    const Case cases[]
    {
        { DrumChannel, true, L"channel 10 takes a voice from channel 1" },
        { 1, false, L"channel 2 can't take one from channel 1" },
        { 10, false, L"and neither can channel 11" },
        { 0, true, L"channel 1 takes one from itself" },
    };

    for (const auto& test : cases)
    {
        Rig rig(*collection, CompatibleConfig());

        // Every voice, spares included, held on channel 1.
        HoldNotes(rig, 0, 30, 40);

        const auto dropped = rig.Engine().DroppedNoteCount();

        rig.NoteOn(test.Channel, (test.Channel == DrumChannel) ? LowTom : 90, NoAttribute, 0);

        const bool plays = rig.Engine().DroppedNoteCount() == dropped;

        Log::Comment(String().Format(L"Channel %u: %s", test.Channel + 1, plays ? L"plays" : L"dropped"));

        VERIFY_ARE_EQUAL(test.Plays, plays, test.Name);
    }
}

// A burst of notes can use eight voices beyond the 32 for a moment. Before the next block renders,
// the oldest notes on the lowest priority channel fade out, so 32 are left.
void MidiSynthCompatibleModeTests::TestSpareVoicesFadeTheLowestPriority()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    Rig rig(*collection, CompatibleConfig());

    HoldNotes(rig, 15, 40, 20);
    HoldNotes(rig, 0, 40, 20);

    VERIFY_ARE_EQUAL(20u, rig.Engine().SoundingVoiceCount(15));
    VERIFY_ARE_EQUAL(20u, rig.Engine().SoundingVoiceCount(0), L"40 notes sound at once");

    (void)rig.Render(0.1);

    Log::Comment(String().Format(L"After one render: channel 16 has %u, channel 1 has %u",
        rig.Engine().SoundingVoiceCount(15), rig.Engine().SoundingVoiceCount(0)));

    VERIFY_ARE_EQUAL(12u, rig.Engine().SoundingVoiceCount(15), L"channel 16 gives up 8");
    VERIFY_ARE_EQUAL(20u, rig.Engine().SoundingVoiceCount(0), L"channel 1 keeps all of its notes");
    VERIFY_IS_FALSE(rig.Engine().IsNoteSounding(15, 40), L"the oldest went first");
    VERIFY_IS_TRUE(rig.Engine().IsNoteSounding(15, 59), L"the newest stayed");
    VERIFY_ARE_EQUAL(32u, rig.Engine().ActiveVoiceCount(), L"and the faded ones are gone");
}

// With every voice in use, a note takes a released note's voice before a held one, even a released
// note newer than every held one.
void MidiSynthCompatibleModeTests::TestStealingPrefersAReleasedNote()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    for (const bool release : { true, false })
    {
        Rig rig(*collection, CompatibleConfig());

        HoldNotes(rig, 0, 30, 40);

        if (release)
        {
            rig.NoteOff(0, 69);
        }

        rig.NoteOn(0, 90, NoAttribute, 0);

        VERIFY_IS_TRUE(rig.Engine().IsNoteSounding(0, 90), L"the new note plays");

        if (release)
        {
            VERIFY_IS_FALSE(rig.Engine().IsNoteSounding(0, 69), L"in the released note's voice");
            VERIFY_IS_TRUE(rig.Engine().IsNoteSounding(0, 30), L"and the oldest held note keeps playing");
        }
        else
        {
            VERIFY_IS_FALSE(rig.Engine().IsNoteSounding(0, 30), L"with nothing released, the oldest note gives way");
        }
    }
}

// The older synthesizer moved pitch in straight lines between control points up to about 10 ms
// apart, so a bend glides to its new value. Modern lands it at the next block.
void MidiSynthCompatibleModeTests::TestPitchBendGlides()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    // A nearly pure tone, high enough that a few milliseconds hold several cycles.
    constexpr uint8_t Note = 84;
    constexpr uint8_t Ocarina = 79;

    for (const bool compatible : { true, false })
    {
        const auto config = compatible ? CompatibleConfig() : TestConfig();

        double reference = 0.0;
        {
            Rig plain(*collection, config);
            plain.Engine().ProgramChange(0, Ocarina);
            plain.NoteOn(0, Note, NoAttribute, 0);
            (void)plain.Render(0.3);
            reference = EstimateHertz(plain.Render(0.3), plain.SampleRate(), 600.0, 2400.0);
        }

        Rig rig(*collection, config);
        rig.Engine().ProgramChange(0, Ocarina);
        rig.NoteOn(0, Note, NoAttribute, 0);
        (void)rig.Render(0.3);

        // All the way up: 200 cents at the default range.
        rig.SendMidi1(StatusPitchBend, 0, 0x7F, 0x7F);

        const double start = Cents(EstimateHertz(rig.Render(0.008), rig.SampleRate(), 600.0, 2400.0), reference);
        (void)rig.Render(0.004);
        const double arrived = Cents(EstimateHertz(rig.Render(0.008), rig.SampleRate(), 600.0, 2400.0), reference);
        (void)rig.Render(0.05);
        const double settled = Cents(EstimateHertz(rig.Render(0.1), rig.SampleRate(), 600.0, 2400.0), reference);

        Log::Comment(String().Format(L"%s: %+.1f cents in the first 8 ms, %+.1f cents at 12 to 20 ms, %+.1f cents after 70 ms",
            compatible ? L"Compatible" : L"Modern", start, arrived, settled));

        VERIFY_IS_TRUE(std::fabs(arrived - 200.0) < 3.0, L"the bend has arrived within about 10 ms, as on the older synthesizer");
        VERIFY_IS_TRUE(std::fabs(settled - 200.0) < 3.0, L"and stays there");

        if (compatible)
        {
            VERIFY_IS_TRUE(start < 150.0, L"gliding there");
        }
        else
        {
            VERIFY_IS_TRUE(start > 185.0, L"Modern is there at once");
        }
    }
}

namespace
{
    constexpr uint8_t StatusProgramChange = 0xC;

    // GS Reset, then GM System On, as files send them.
    const std::vector<uint8_t> GsReset{ 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41 };
    const std::vector<uint8_t> GmSystemOn{ 0x7E, 0x7F, 0x09, 0x01 };

    void SelectBank(_Inout_ Rig& rig, _In_ uint8_t channel, _In_ uint8_t msb, _In_ uint8_t program)
    {
        rig.SendMidi1(StatusControlChange, channel, 0, msb);
        rig.SendMidi1(StatusControlChange, channel, 32, 0);
        rig.SendMidi1(StatusProgramChange, channel, program, 0);
    }
}

// The older synthesizer reads bank select only after a GS Reset, and a GM System On turns that off
// again. Modern always reads it.
void MidiSynthCompatibleModeTests::TestBankSelectNeedsGsReset()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    // Program 0 in bank 16 is "Piano 1d", which has a name of its own.
    const auto* const variation = collection->FindInstrument(16, 0, 0, false);
    const auto* const capitalTone = collection->FindInstrument(0, 0, 0, false);
    VERIFY_IS_TRUE(variation != nullptr && capitalTone != nullptr && variation != capitalTone);

    Rig rig(*collection, CompatibleConfig());

    SelectBank(rig, 0, 16, 0);
    VERIFY_IS_TRUE(rig.Engine().ChannelState(0).Instrument == capitalTone, L"without a GS Reset, bank select is ignored");

    rig.SendSysEx(GsReset);
    SelectBank(rig, 0, 16, 0);
    VERIFY_IS_TRUE(rig.Engine().ChannelState(0).Instrument == variation, L"after a GS Reset, it's read");

    rig.SendSysEx(GmSystemOn);
    SelectBank(rig, 0, 16, 0);
    VERIFY_IS_TRUE(rig.Engine().ChannelState(0).Instrument == capitalTone, L"a GM System On turns it off again");

    Rig modern(*collection);
    SelectBank(modern, 0, 16, 0);
    VERIFY_IS_TRUE(modern.Engine().ChannelState(0).Instrument == variation, L"Modern always reads it");
}

// The older synthesizer takes whole semitones of pitch bend sensitivity, keeps the fine tuning LSB
// when a new MSB arrives, resets volume and pan on a Reset All Controllers with a value, and has no
// master tuning.
void MidiSynthCompatibleModeTests::TestOlderDataEntryAndResets()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    Rig rig(*collection, CompatibleConfig());
    const auto state = [&]() { return rig.Engine().ChannelState(0); };

    SetRegisteredParameter(rig, 0, 0, 4);
    rig.SendMidi1(StatusControlChange, 0, 38, 50);
    VERIFY_ARE_EQUAL(4.0, state().PitchBendRangeSemitones, L"the cents of the bend range are ignored");

    SetRegisteredParameter(rig, 0, 1, 0x40);
    rig.SendMidi1(StatusControlChange, 0, 38, 0x20);
    rig.SendMidi1(StatusControlChange, 0, 6, 0x41);
    VERIFY_ARE_EQUAL((static_cast<double>(0x20A0) - 8192.0) * 100.0 / 8192.0, state().FineTuneCents, L"a new MSB keeps the LSB");

    rig.SendMidi1(StatusControlChange, 0, 7, 20);
    rig.SendMidi1(StatusControlChange, 0, 10, 0);
    rig.SendMidi1(StatusControlChange, 0, 121, 0);
    VERIFY_ARE_EQUAL(20.0 / 127.0, state().Volume, L"Reset All Controllers with zero leaves volume alone");
    VERIFY_ARE_EQUAL(1, static_cast<int>(state().RpnLsb), L"and keeps the parameter selected");

    rig.SendMidi1(StatusControlChange, 0, 121, 127);
    VERIFY_ARE_EQUAL(100.0 / 127.0, state().Volume, L"with a value, it resets volume");
    VERIFY_ARE_EQUAL(0.0, state().PanOffset, L"and pan");

    // Master fine tuning to the top, which would be nearly 100 cents sharp.
    std::vector<float> plain;
    {
        Rig untuned(*collection, CompatibleConfig());
        untuned.Engine().ProgramChange(0, ChurchOrgan);
        untuned.NoteOn(0, 60, NoAttribute, 0);
        plain = untuned.Render(0.2);
    }

    Rig tuned(*collection, CompatibleConfig());
    tuned.SendSysEx({ 0x7F, 0x7F, 0x04, 0x03, 0x7F, 0x7F });
    tuned.SendSysEx({ 0x7F, 0x7F, 0x04, 0x04, 0x00, 0x50 });
    tuned.Engine().ProgramChange(0, ChurchOrgan);
    tuned.NoteOn(0, 60, NoAttribute, 0);

    VERIFY_IS_TRUE(LargestDifference(plain, tuned.Render(0.2)) < 1e-6, L"master tuning is ignored");
}

// Mono mode: a new note ends the one the channel holds. Poly mode puts it back.
void MidiSynthCompatibleModeTests::TestMonoMode()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    for (const bool mono : { true, false })
    {
        Rig rig(*collection, CompatibleConfig());
        rig.Engine().ProgramChange(0, ChurchOrgan);

        rig.SendMidi1(StatusControlChange, 0, mono ? 126 : 127, 1);

        rig.NoteOn(0, 60, NoAttribute, 0);
        rig.NoteOn(0, 64, NoAttribute, 0);
        (void)rig.Render(1.0);

        VERIFY_ARE_EQUAL(mono ? 1u : 2u, rig.Engine().SoundingVoiceCount(0), mono ? L"mono plays one note" : L"poly plays both");
        VERIFY_IS_TRUE(rig.Engine().IsNoteSounding(0, 64), L"the newest note sounds");
    }

    // Modern ignores both.
    Rig modern(*collection);
    modern.Engine().ProgramChange(0, ChurchOrgan);
    modern.SendMidi1(StatusControlChange, 0, 126, 1);
    modern.NoteOn(0, 60, NoAttribute, 0);
    modern.NoteOn(0, 64, NoAttribute, 0);
    (void)modern.Render(1.0);

    VERIFY_ARE_EQUAL(2u, modern.Engine().SoundingVoiceCount(0));
}

// GS Scale Tuning moves each pitch class by up to 64 cents. It needs a GS Reset first.
void MidiSynthCompatibleModeTests::TestGsScaleTuning()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    // Part 1 is channel 1. C is 20 cents sharp, the rest are in tune.
    const std::vector<uint8_t> scaleTuning
    {
        0x41, 0x10, 0x42, 0x12, 0x40, 0x11, 0x40,
        84, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        0x00,
    };

    auto measure = [&](bool gsReset, uint8_t note)
    {
        double reference = 0.0;
        {
            Rig plain(*collection, CompatibleConfig());
            plain.Engine().ProgramChange(0, ChurchOrgan);
            plain.NoteOn(0, note, NoAttribute, 0);
            (void)plain.Render(0.3);
            reference = EstimateHertz(plain.Render(0.6), plain.SampleRate(), 100.0, 1000.0);
        }

        Rig rig(*collection, CompatibleConfig());

        if (gsReset)
        {
            rig.SendSysEx(GsReset);
        }

        rig.SendSysEx(scaleTuning);
        rig.Engine().ProgramChange(0, ChurchOrgan);
        rig.NoteOn(0, note, NoAttribute, 0);
        (void)rig.Render(0.3);

        return Cents(EstimateHertz(rig.Render(0.6), rig.SampleRate(), 100.0, 1000.0), reference);
    };

    const double tunedC = measure(true, 60);
    const double untunedE = measure(true, 64);
    const double withoutReset = measure(false, 60);

    Log::Comment(String().Format(L"C %+.2f cents, E %+.2f cents, C without a GS Reset %+.2f cents", tunedC, untunedE, withoutReset));

    VERIFY_IS_TRUE(std::fabs(tunedC - 20.0) < 1.0, L"C is 20 cents sharp");
    VERIFY_IS_TRUE(std::fabs(untunedE) < 1.0, L"E is in tune");
    VERIFY_IS_TRUE(std::fabs(withoutReset) < 1.0, L"and nothing changes without a GS Reset");
}

// GS Rx. Channel moves a part to another MIDI channel, read the way the older synthesizer read it:
// the value counts from 1.
void MidiSynthCompatibleModeTests::TestGsReceiveChannel()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    Rig rig(*collection, CompatibleConfig());
    rig.SendSysEx(GsReset);

    // Part 11 receives on channel 1 as well.
    rig.SendSysEx({ 0x41, 0x10, 0x42, 0x12, 0x40, 0x1A, 0x02, 0x01, 0x00 });

    rig.SendMidi1(StatusProgramChange, 0, ChurchOrgan, 0);

    VERIFY_ARE_EQUAL(ChurchOrgan, rig.Engine().ChannelState(0).Program, L"channel 1 takes the program");
    VERIFY_ARE_EQUAL(ChurchOrgan, rig.Engine().ChannelState(10).Program, L"and so does part 11");

    rig.SendMidi1(StatusProgramChange, 10, 5, 0);
    VERIFY_ARE_EQUAL(ChurchOrgan, rig.Engine().ChannelState(10).Program, L"which no longer listens to channel 11");

    rig.SendSysEx(GsReset);
    rig.SendMidi1(StatusProgramChange, 10, 5, 0);
    VERIFY_ARE_EQUAL(5, static_cast<int>(rig.Engine().ChannelState(10).Program), L"until a reset puts it back");

    // Modern ignores the message.
    Rig modern(*collection);
    modern.SendSysEx(GsReset);
    modern.SendSysEx({ 0x41, 0x10, 0x42, 0x12, 0x40, 0x1A, 0x02, 0x01, 0x00 });
    modern.SendMidi1(StatusProgramChange, 0, ChurchOrgan, 0);
    VERIFY_ARE_EQUAL(0, static_cast<int>(modern.Engine().ChannelState(10).Program));
}

// Voices fading out after All Sound Off are no longer heard, so the next notes take them, even on
// the lowest priority channel and before anything has rendered.
void MidiSynthCompatibleModeTests::TestFadingVoicesAreReusedFirst()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    Rig rig(*collection, CompatibleConfig());

    HoldNotes(rig, 0, 30, 40);
    rig.SendMidi1(StatusControlChange, 0, 120, 0);

    const auto dropped = rig.Engine().DroppedNoteCount();
    const auto stolen = rig.Engine().StolenVoiceCount();

    HoldNotes(rig, 15, 30, 40);

    VERIFY_ARE_EQUAL(dropped, rig.Engine().DroppedNoteCount(), L"no note is dropped");
    VERIFY_ARE_EQUAL(stolen, rig.Engine().StolenVoiceCount(), L"and nothing sounding was stolen");
    VERIFY_ARE_EQUAL(40u, rig.Engine().SoundingVoiceCount(15));
}
