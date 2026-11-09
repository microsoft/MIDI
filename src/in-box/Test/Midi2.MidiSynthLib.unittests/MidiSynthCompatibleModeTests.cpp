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
