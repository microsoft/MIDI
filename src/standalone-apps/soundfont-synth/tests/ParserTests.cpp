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
    Sf2LoadStatus LoadBytes(std::vector<uint8_t> const& bytes, SoundFont& font, Sf2LoadStatistics* statistics = nullptr)
    {
        MemoryByteSource source(bytes.data(), bytes.size());
        return SoundFont::Load(source, Sf2LoadLimits{}, font, statistics);
    }

    int AsInt(Sf2LoadStatus status)
    {
        return static_cast<int>(status);
    }

    // Offset of a chunk's four character code. Only for files these tests built themselves.
    size_t FindChunk(std::vector<uint8_t> const& bytes, char const* id)
    {
        for (size_t i = 12; i + 8 <= bytes.size(); i++)
        {
            if (memcmp(&bytes[i], id, 4) == 0)
            {
                return i;
            }
        }

        return SIZE_MAX;
    }

    void PutU16(std::vector<uint8_t>& bytes, size_t at, uint16_t value)
    {
        bytes[at] = static_cast<uint8_t>(value & 0xFF);
        bytes[at + 1] = static_cast<uint8_t>(value >> 8);
    }

    void PutU32(std::vector<uint8_t>& bytes, size_t at, uint32_t value)
    {
        PutU16(bytes, at, static_cast<uint16_t>(value & 0xFFFF));
        PutU16(bytes, at + 2, static_cast<uint16_t>(value >> 16));
    }

    uint32_t GetU32(std::vector<uint8_t> const& bytes, size_t at)
    {
        return static_cast<uint32_t>(bytes[at]) |
            (static_cast<uint32_t>(bytes[at + 1]) << 8) |
            (static_cast<uint32_t>(bytes[at + 2]) << 16) |
            (static_cast<uint32_t>(bytes[at + 3]) << 24);
    }

    // Plays every preset a little, which is where damage the loader let through would show.
    bool PlayEveryPreset(std::shared_ptr<SoundFont const> const& font)
    {
        Synthesizer synthesizer;

        SynthesizerConfig config{};
        config.SampleRate = 48000;
        config.Polyphony = 64;

        if (!synthesizer.Initialize(font, config))
        {
            return false;
        }

        std::vector<float> buffer(512 * 2);

        for (auto const& preset : font->Presets())
        {
            auto const drum = (preset.Bank == SoundFont::PercussionBank);
            auto const channel = static_cast<uint8_t>(drum ? 9 : 0);

            synthesizer.ProgramChangeWithBank(channel, static_cast<uint8_t>(drum ? 0 : (preset.Bank & 0x7F)), 0, static_cast<uint8_t>(preset.Program & 0x7F));

            for (uint8_t note : { uint8_t{ 0 }, uint8_t{ 36 }, uint8_t{ 60 }, uint8_t{ 96 }, uint8_t{ 127 } })
            {
                synthesizer.NoteOn(channel, note, 0xFFFF);
            }

            synthesizer.Render(buffer.data(), 512);

            for (uint8_t note : { uint8_t{ 0 }, uint8_t{ 36 }, uint8_t{ 60 }, uint8_t{ 96 }, uint8_t{ 127 } })
            {
                synthesizer.NoteOff(channel, note);
            }

            synthesizer.Render(buffer.data(), 512);

            for (auto const value : buffer)
            {
                if (!std::isfinite(value))
                {
                    return false;
                }
            }

            synthesizer.AllSoundOff(channel);
        }

        return true;
    }
}

class SoundFontParserTests
{
public:
    BEGIN_TEST_CLASS(SoundFontParserTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(LoadsAWellFormedBank)
    {
        auto const bytes = Sf2Test::Build(Sf2Test::MakeBasicFont());

        SoundFont font;
        Sf2LoadStatistics statistics{};

        VERIFY_ARE_EQUAL(AsInt(Sf2LoadStatus::Ok), AsInt(LoadBytes(bytes, font, &statistics)));

        VERIFY_ARE_EQUAL(size_t{ 2 }, font.Presets().size());
        VERIFY_ARE_EQUAL(std::wstring{ L"Test Font" }, font.Info().Name);
        VERIFY_ARE_EQUAL(2, static_cast<int>(font.Info().VersionMajor));

        VERIFY_IS_TRUE(font.FindPreset(0, 0) >= 0);
        VERIFY_IS_TRUE(font.FindPreset(SoundFont::PercussionBank, 0) >= 0);
        VERIFY_ARE_EQUAL(-1, font.FindPreset(5, 5));

        VERIFY_ARE_EQUAL(std::wstring{ L"Sine Piano" }, font.Presets()[static_cast<size_t>(font.FindPreset(0, 0))].Name);

        VERIFY_ARE_EQUAL(size_t{ 1 }, font.Samples().size());
        VERIFY_IS_TRUE(font.Samples()[0].Usable);
        VERIFY_IS_TRUE(font.Samples()[0].LoopValid);

        VERIFY_ARE_EQUAL(0u, statistics.PresetsDropped);
        VERIFY_ARE_EQUAL(0u, statistics.ZonesDropped);
        VERIFY_ARE_EQUAL(0u, statistics.SamplesUnusable);
    }

    TEST_METHOD(RefusesEveryTruncation)
    {
        auto const bytes = Sf2Test::Build(Sf2Test::MakeBasicFont());

        uint32_t accepted{ 0 };

        for (size_t length = 0; length < bytes.size(); length++)
        {
            // Every byte through the headers and the hydra, a sample of the rest.
            auto const inSampleData = length > 200 && length + 2000 < bytes.size();

            if (inSampleData && (length % 97) != 0)
            {
                continue;
            }

            std::vector<uint8_t> truncated(bytes.begin(), bytes.begin() + static_cast<ptrdiff_t>(length));

            SoundFont font;

            if (LoadBytes(truncated, font) == Sf2LoadStatus::Ok)
            {
                accepted++;
            }
        }

        VERIFY_ARE_EQUAL(0u, accepted);
    }

    TEST_METHOD(RefusesWhatIsNotASoundFont)
    {
        auto bytes = Sf2Test::Build(Sf2Test::MakeBasicFont());

        SoundFont font;

        VERIFY_ARE_NOT_EQUAL(AsInt(Sf2LoadStatus::Ok), AsInt(LoadBytes({}, font)));

        auto notRiff = bytes;
        memcpy(notRiff.data(), "RIFX", 4);
        VERIFY_ARE_EQUAL(AsInt(Sf2LoadStatus::NotRiff), AsInt(LoadBytes(notRiff, font)));

        auto notSoundFont = bytes;
        memcpy(notSoundFont.data() + 8, "WAVE", 4);
        VERIFY_ARE_EQUAL(AsInt(Sf2LoadStatus::NotSoundFont), AsInt(LoadBytes(notSoundFont, font)));
    }

    TEST_METHOD(RefusesCompressedSamples)
    {
        auto source = Sf2Test::MakeBasicFont();
        source.VersionMajor = 3;
        source.VersionMinor = 1;

        SoundFont font;

        VERIFY_ARE_EQUAL(AsInt(Sf2LoadStatus::CompressedSamples), AsInt(LoadBytes(Sf2Test::Build(source), font)));
    }

    TEST_METHOD(RefusesAChunkLargerThanTheFile)
    {
        auto bytes = Sf2Test::Build(Sf2Test::MakeBasicFont());

        auto const smpl = FindChunk(bytes, "smpl");
        VERIFY_ARE_NOT_EQUAL(SIZE_MAX, smpl);

        PutU32(bytes, smpl + 4, 0xFFFFFFF0);

        SoundFont font;
        VERIFY_ARE_NOT_EQUAL(AsInt(Sf2LoadStatus::Ok), AsInt(LoadBytes(bytes, font)));
    }

    TEST_METHOD(RefusesARiffSizeThatHidesTheContents)
    {
        auto bytes = Sf2Test::Build(Sf2Test::MakeBasicFont());

        PutU32(bytes, 4, 4);

        SoundFont font;
        VERIFY_ARE_NOT_EQUAL(AsInt(Sf2LoadStatus::Ok), AsInt(LoadBytes(bytes, font)));
    }

    TEST_METHOD(RefusesAPresetTableOfTheWrongSize)
    {
        auto bytes = Sf2Test::Build(Sf2Test::MakeBasicFont());

        auto const phdr = FindChunk(bytes, "phdr");
        VERIFY_ARE_NOT_EQUAL(SIZE_MAX, phdr);

        PutU32(bytes, phdr + 4, GetU32(bytes, phdr + 4) - 2);

        SoundFont font;
        VERIFY_ARE_NOT_EQUAL(AsInt(Sf2LoadStatus::Ok), AsInt(LoadBytes(bytes, font)));
    }

    TEST_METHOD(RefusesBagIndicesThatGoBackwards)
    {
        auto bytes = Sf2Test::Build(Sf2Test::MakeBasicFont());

        auto const pbag = FindChunk(bytes, "pbag");
        VERIFY_ARE_NOT_EQUAL(SIZE_MAX, pbag);

        // The second preset's first generator index, pointed before the first preset's.
        PutU16(bytes, pbag + 8 + 4, 0);
        PutU16(bytes, pbag + 8 + 0, 1);

        SoundFont font;
        VERIFY_ARE_NOT_EQUAL(AsInt(Sf2LoadStatus::Ok), AsInt(LoadBytes(bytes, font)));
    }

    TEST_METHOD(DropsAZoneThatNamesAMissingInstrument)
    {
        auto source = Sf2Test::MakeBasicFont();
        source.Presets[1].Zones[0].Generators[0].Amount = 999;

        SoundFont font;
        Sf2LoadStatistics statistics{};

        VERIFY_ARE_EQUAL(AsInt(Sf2LoadStatus::Ok), AsInt(LoadBytes(Sf2Test::Build(source), font, &statistics)));
        VERIFY_ARE_EQUAL(size_t{ 1 }, font.Presets().size());
        VERIFY_IS_TRUE(statistics.ZonesDropped >= 1);
        VERIFY_IS_TRUE(statistics.PresetsDropped >= 1);
    }

    TEST_METHOD(DropsASampleThatRunsPastTheData)
    {
        auto bytes = Sf2Test::Build(Sf2Test::MakeBasicFont());

        auto const shdr = FindChunk(bytes, "shdr");
        VERIFY_ARE_NOT_EQUAL(SIZE_MAX, shdr);

        // End of the first sample header.
        PutU32(bytes, shdr + 8 + 20 + 4, 0x7FFFFFFF);

        SoundFont font;
        Sf2LoadStatistics statistics{};

        // The only sample is unusable, so nothing can play and nothing is offered.
        VERIFY_ARE_EQUAL(AsInt(Sf2LoadStatus::NoPresets), AsInt(LoadBytes(bytes, font, &statistics)));
        VERIFY_ARE_EQUAL(1u, statistics.SamplesUnusable);
    }

    TEST_METHOD(KeepsASampleWhoseLoopIsBackwards)
    {
        auto source = Sf2Test::MakeBasicFont();
        std::swap(source.Samples[0].LoopStart, source.Samples[0].LoopEnd);

        SoundFont font;

        VERIFY_ARE_EQUAL(AsInt(Sf2LoadStatus::Ok), AsInt(LoadBytes(Sf2Test::Build(source), font)));
        VERIFY_IS_TRUE(font.Samples()[0].Usable);
        VERIFY_IS_FALSE(font.Samples()[0].LoopValid);

        VERIFY_IS_TRUE(PlayEveryPreset(std::make_shared<SoundFont const>(std::move(font))));
    }

    TEST_METHOD(DropsAnEmptySample)
    {
        auto source = Sf2Test::MakeBasicFont();
        source.Samples[0].Data.clear();
        source.Samples[0].LoopStart = 0;
        source.Samples[0].LoopEnd = 0;

        SoundFont font;

        VERIFY_ARE_EQUAL(AsInt(Sf2LoadStatus::NoPresets), AsInt(LoadBytes(Sf2Test::Build(source), font)));
    }

    TEST_METHOD(SurvivesRandomDamage)
    {
        auto const original = Sf2Test::Build(Sf2Test::MakeBasicFont());

        auto const pdta = FindChunk(original, "phdr");
        VERIFY_ARE_NOT_EQUAL(SIZE_MAX, pdta);

        std::mt19937 random(20261012);
        std::uniform_int_distribution<size_t> anywhere(0, original.size() - 1);
        std::uniform_int_distribution<size_t> inTables(pdta - 12, original.size() - 1);
        std::uniform_int_distribution<int> byteValue(0, 255);
        std::uniform_int_distribution<int> damageCount(1, 6);
        std::uniform_int_distribution<int> percent(0, 99);

        constexpr int Iterations = 4000;

        uint32_t accepted{ 0 };
        uint32_t played{ 0 };

        for (int iteration = 0; iteration < Iterations; iteration++)
        {
            auto bytes = original;

            auto const count = damageCount(random);

            for (int i = 0; i < count; i++)
            {
                // Mostly the tables, where damage changes meaning rather than just sound.
                auto const at = (percent(random) < 85) ? inTables(random) : anywhere(random);
                bytes[at] = static_cast<uint8_t>(byteValue(random));
            }

            auto font = std::make_shared<SoundFont>();

            if (LoadBytes(bytes, *font) != Sf2LoadStatus::Ok)
            {
                continue;
            }

            accepted++;

            if (!PlayEveryPreset(font))
            {
                Log::Error(String().Format(L"Iteration %d produced a value that is not finite.", iteration));
                VERIFY_FAIL();
            }

            played++;
        }

        Log::Comment(String().Format(L"%u of %d damaged banks were accepted and played.", accepted, Iterations));

        // A fuzzer that never gets past the front door proves nothing about what is behind it.
        VERIFY_IS_TRUE(accepted >= Iterations / 10);
        VERIFY_ARE_EQUAL(accepted, played);
    }
};
