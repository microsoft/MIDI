// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace SoundFontSynth;

namespace
{
    constexpr uint32_t SampleRate = 48000;
    constexpr uint16_t Forte = 0xC000;

    std::shared_ptr<SoundFont const> LoadFont(Sf2Test::Font const& source)
    {
        auto const bytes = Sf2Test::Build(source);

        MemoryByteSource bytesSource(bytes.data(), bytes.size());

        auto font = std::make_shared<SoundFont>();

        if (SoundFont::Load(bytesSource, Sf2LoadLimits{}, *font, nullptr) != Sf2LoadStatus::Ok)
        {
            return nullptr;
        }

        return font;
    }

    // Effects off: reverb returns on both sides, which would blur every level measured here.
    std::unique_ptr<Synthesizer> MakeSynthesizer(std::shared_ptr<SoundFont const> const& font)
    {
        auto synthesizer = std::make_unique<Synthesizer>();

        SynthesizerConfig config{};
        config.SampleRate = SampleRate;
        config.EnableEffects = false;

        if (font == nullptr || !synthesizer->Initialize(font, config))
        {
            return nullptr;
        }

        return synthesizer;
    }

    std::vector<float> Render(Synthesizer& synthesizer, double seconds)
    {
        auto const frames = static_cast<uint32_t>(seconds * SampleRate);

        std::vector<float> output(static_cast<size_t>(frames) * 2);

        uint32_t done{ 0 };

        while (done < frames)
        {
            auto const block = (std::min)(frames - done, 256u);
            synthesizer.Render(output.data() + static_cast<size_t>(done) * 2, block);
            done += block;
        }

        return output;
    }

    double Rms(std::vector<float> const& stereo, size_t channel)
    {
        double sum{ 0.0 };
        size_t count{ 0 };

        for (size_t i = channel; i < stereo.size(); i += 2)
        {
            sum += static_cast<double>(stereo[i]) * stereo[i];
            count++;
        }

        return (count > 0) ? std::sqrt(sum / static_cast<double>(count)) : 0.0;
    }

    // Upward zero crossings per second on one channel.
    double MeasureFrequency(std::vector<float> const& stereo, size_t channel)
    {
        uint32_t crossings{ 0 };
        size_t first{ SIZE_MAX };
        size_t last{ 0 };

        for (size_t i = channel + 2; i < stereo.size(); i += 2)
        {
            if (stereo[i - 2] <= 0.0f && stereo[i] > 0.0f)
            {
                if (first == SIZE_MAX)
                {
                    first = i;
                }
                else
                {
                    crossings++;
                }

                last = i;
            }
        }

        if (first == SIZE_MAX || last <= first)
        {
            return 0.0;
        }

        return static_cast<double>(crossings) * SampleRate / (static_cast<double>(last - first) / 2.0);
    }

    bool AllFinite(std::vector<float> const& stereo)
    {
        return std::all_of(stereo.begin(), stereo.end(), [](float value) { return std::isfinite(value); });
    }
}

class SynthesizerRenderTests
{
public:
    BEGIN_TEST_CLASS(SynthesizerRenderTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(IsSilentWithNothingPlaying)
    {
        auto synthesizer = MakeSynthesizer(LoadFont(Sf2Test::MakeBasicFont()));
        VERIFY_IS_NOT_NULL(synthesizer.get());

        auto const output = Render(*synthesizer, 0.1);

        VERIFY_ARE_EQUAL(0.0, Rms(output, 0));
        VERIFY_ARE_EQUAL(0u, synthesizer->ActiveVoiceCount());
    }

    TEST_METHOD(PlaysAtTheRightPitch)
    {
        auto synthesizer = MakeSynthesizer(LoadFont(Sf2Test::MakeBasicFont()));
        VERIFY_IS_NOT_NULL(synthesizer.get());

        struct Case
        {
            uint8_t Note;
            double Hertz;
        };

        for (auto const test : { Case{ 69, 440.0 }, Case{ 81, 880.0 }, Case{ 57, 220.0 }, Case{ 72, 523.2511 } })
        {
            synthesizer->NoteOn(0, test.Note, Forte);

            (void)Render(*synthesizer, 0.2);
            auto const steady = Render(*synthesizer, 0.5);

            auto const measured = MeasureFrequency(steady, 0);

            Log::Comment(String().Format(L"Note %u: expected %.2f Hz, measured %.2f Hz", test.Note, test.Hertz, measured));

            VERIFY_IS_TRUE(std::fabs(measured - test.Hertz) <= test.Hertz * 0.005);

            synthesizer->AllSoundOff(0);
            (void)Render(*synthesizer, 0.05);
        }
    }

    TEST_METHOD(PitchBendMovesThePitch)
    {
        auto synthesizer = MakeSynthesizer(LoadFont(Sf2Test::MakeBasicFont()));
        VERIFY_IS_NOT_NULL(synthesizer.get());

        // Full bend up with the default range of two semitones.
        synthesizer->PitchBend(0, 16383);
        synthesizer->NoteOn(0, 69, Forte);

        (void)Render(*synthesizer, 0.2);
        auto const measured = MeasureFrequency(Render(*synthesizer, 0.5), 0);

        auto const expected = 440.0 * std::pow(2.0, 2.0 / 12.0);

        Log::Comment(String().Format(L"Expected %.2f Hz, measured %.2f Hz", expected, measured));
        VERIFY_IS_TRUE(std::fabs(measured - expected) <= expected * 0.005);
    }

    TEST_METHOD(SustainsThroughTheLoop)
    {
        auto synthesizer = MakeSynthesizer(LoadFont(Sf2Test::MakeBasicFont()));
        VERIFY_IS_NOT_NULL(synthesizer.get());

        synthesizer->NoteOn(0, 69, Forte);

        // The sample is under half a second long, so anything heard after this is the loop.
        (void)Render(*synthesizer, 2.0);
        auto const late = Render(*synthesizer, 0.25);

        VERIFY_IS_TRUE(Rms(late, 0) > 0.01);
        VERIFY_IS_TRUE(AllFinite(late));
    }

    TEST_METHOD(ReleasesAfterNoteOff)
    {
        auto synthesizer = MakeSynthesizer(LoadFont(Sf2Test::MakeBasicFont()));
        VERIFY_IS_NOT_NULL(synthesizer.get());

        synthesizer->NoteOn(0, 60, Forte);
        (void)Render(*synthesizer, 0.2);
        VERIFY_ARE_EQUAL(1u, synthesizer->ActiveVoiceCount());

        synthesizer->NoteOff(0, 60);

        // Half a second of release, with margin.
        (void)Render(*synthesizer, 1.0);

        VERIFY_ARE_EQUAL(0u, synthesizer->ActiveVoiceCount());
        VERIFY_IS_TRUE(Rms(Render(*synthesizer, 0.05), 0) < 1e-6);
    }

    TEST_METHOD(SustainPedalHoldsTheNote)
    {
        auto synthesizer = MakeSynthesizer(LoadFont(Sf2Test::MakeBasicFont()));
        VERIFY_IS_NOT_NULL(synthesizer.get());

        synthesizer->ControlChange(0, 64, 127);
        synthesizer->NoteOn(0, 60, Forte);
        (void)Render(*synthesizer, 0.1);
        synthesizer->NoteOff(0, 60);
        (void)Render(*synthesizer, 1.0);

        VERIFY_ARE_EQUAL(1u, synthesizer->ActiveVoiceCount());

        synthesizer->ControlChange(0, 64, 0);
        (void)Render(*synthesizer, 1.0);

        VERIFY_ARE_EQUAL(0u, synthesizer->ActiveVoiceCount());
    }

    TEST_METHOD(PanMovesTheSound)
    {
        auto synthesizer = MakeSynthesizer(LoadFont(Sf2Test::MakeBasicFont()));
        VERIFY_IS_NOT_NULL(synthesizer.get());

        synthesizer->ControlChange(0, 10, 0);
        synthesizer->NoteOn(0, 69, Forte);
        (void)Render(*synthesizer, 0.1);

        auto const left = Render(*synthesizer, 0.2);

        Log::Comment(String().Format(L"Hard left: L %.4f, R %.4f", Rms(left, 0), Rms(left, 1)));
        VERIFY_IS_TRUE(Rms(left, 0) > 10.0 * Rms(left, 1));

        synthesizer->AllSoundOff(0);
        (void)Render(*synthesizer, 0.05);

        synthesizer->ControlChange(0, 10, 127);
        synthesizer->NoteOn(0, 69, Forte);
        (void)Render(*synthesizer, 0.1);

        auto const right = Render(*synthesizer, 0.2);

        Log::Comment(String().Format(L"Hard right: L %.4f, R %.4f", Rms(right, 0), Rms(right, 1)));
        VERIFY_IS_TRUE(Rms(right, 1) > 10.0 * Rms(right, 0));
    }

    TEST_METHOD(VolumeAndExpressionLowerTheLevel)
    {
        auto synthesizer = MakeSynthesizer(LoadFont(Sf2Test::MakeBasicFont()));
        VERIFY_IS_NOT_NULL(synthesizer.get());

        // The General MIDI default is 100, not full.
        synthesizer->ControlChange(0, 7, 127);
        synthesizer->NoteOn(0, 69, Forte);
        (void)Render(*synthesizer, 0.1);
        auto const full = Rms(Render(*synthesizer, 0.2), 0);

        synthesizer->ControlChange(0, 7, 64);
        (void)Render(*synthesizer, 0.05);
        auto const half = Rms(Render(*synthesizer, 0.2), 0);

        synthesizer->ControlChange(0, 7, 127);
        synthesizer->ControlChange(0, 11, 64);
        (void)Render(*synthesizer, 0.05);
        auto const expression = Rms(Render(*synthesizer, 0.2), 0);

        Log::Comment(String().Format(L"CC7 127: %.4f, CC7 64: %.4f, CC11 64: %.4f", full, half, expression));

        // The SoundFont concave curve puts the middle of either controller 12 dB down.
        VERIFY_IS_TRUE(half > full * 0.22 && half < full * 0.28);
        VERIFY_IS_TRUE(expression > full * 0.22 && expression < full * 0.28);
    }

    TEST_METHOD(ExclusiveClassCutsTheEarlierNote)
    {
        auto synthesizer = MakeSynthesizer(LoadFont(Sf2Test::MakeBasicFont()));
        VERIFY_IS_NOT_NULL(synthesizer.get());

        VERIFY_IS_TRUE(synthesizer->IsDrumChannel(9));

        // An open and a closed hi-hat, the textbook case: both in the same class.
        synthesizer->NoteOn(9, 46, Forte);
        (void)Render(*synthesizer, 0.05);
        synthesizer->NoteOn(9, 42, Forte);
        (void)Render(*synthesizer, 0.05);

        VERIFY_ARE_EQUAL(1u, synthesizer->ActiveVoiceCount());
    }

    TEST_METHOD(DrumChannelPlaysTheKit)
    {
        auto font = LoadFont(Sf2Test::MakeBasicFont());
        auto synthesizer = MakeSynthesizer(font);
        VERIFY_IS_NOT_NULL(synthesizer.get());

        auto const selection = synthesizer->Selection(9);

        VERIFY_IS_TRUE(selection.IsDrumChannel);
        VERIFY_IS_TRUE(selection.PresetIndex >= 0);
        VERIFY_ARE_EQUAL(std::wstring{ L"Sine Kit" }, font->Presets()[static_cast<size_t>(selection.PresetIndex)].Name);

        // The kit only covers 35 to 81.
        synthesizer->NoteOn(9, 20, Forte);
        (void)Render(*synthesizer, 0.01);
        VERIFY_ARE_EQUAL(0u, synthesizer->ActiveVoiceCount());

        synthesizer->NoteOn(9, 36, Forte);
        (void)Render(*synthesizer, 0.01);
        VERIFY_ARE_EQUAL(1u, synthesizer->ActiveVoiceCount());
    }

    TEST_METHOD(UnknownProgramFallsBackToSomethingPlayable)
    {
        auto font = LoadFont(Sf2Test::MakeBasicFont());
        auto synthesizer = MakeSynthesizer(font);
        VERIFY_IS_NOT_NULL(synthesizer.get());

        synthesizer->ProgramChangeWithBank(0, 5, 0, 40);

        auto const selection = synthesizer->Selection(0);

        VERIFY_ARE_EQUAL(40, static_cast<int>(selection.Program));
        VERIFY_IS_TRUE(selection.PresetIndex >= 0);

        synthesizer->NoteOn(0, 60, Forte);
        (void)Render(*synthesizer, 0.01);
        VERIFY_ARE_EQUAL(1u, synthesizer->ActiveVoiceCount());
    }

    TEST_METHOD(PolyphonyIsBounded)
    {
        auto synthesizer = MakeSynthesizer(LoadFont(Sf2Test::MakeBasicFont()));
        VERIFY_IS_NOT_NULL(synthesizer.get());

        for (uint8_t channel = 0; channel < 16; channel++)
        {
            if (channel == 9)
            {
                continue;
            }

            for (uint8_t note = 0; note < 128; note++)
            {
                synthesizer->NoteOn(channel, note, Forte);
            }

            (void)Render(*synthesizer, 0.005);
        }

        auto const output = Render(*synthesizer, 0.1);

        Log::Comment(String().Format(L"%u voices sounding", synthesizer->ActiveVoiceCount()));

        VERIFY_IS_TRUE(synthesizer->ActiveVoiceCount() <= 256u + 32u);
        VERIFY_IS_TRUE(AllFinite(output));
        VERIFY_IS_TRUE(synthesizer->Statistics().StolenVoices > 0);
    }

    TEST_METHOD(SampleRateChangeKeepsTheChannelSetup)
    {
        auto font = LoadFont(Sf2Test::MakeBasicFont());
        auto synthesizer = MakeSynthesizer(font);
        VERIFY_IS_NOT_NULL(synthesizer.get());

        synthesizer->ProgramChangeWithBank(3, 0, 0, 0);
        synthesizer->SetDrumChannel(3, true);

        SynthesizerConfig config{};
        config.SampleRate = 44100;
        config.EnableEffects = false;

        VERIFY_IS_TRUE(synthesizer->Initialize(font, config));
        VERIFY_ARE_EQUAL(44100u, synthesizer->SampleRate());
        VERIFY_IS_TRUE(synthesizer->IsDrumChannel(3));
    }
};
