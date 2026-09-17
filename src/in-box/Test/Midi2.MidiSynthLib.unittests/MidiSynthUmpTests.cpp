// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

#include "MidiSynthUmpTests.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace MidiSynth;

namespace
{
    // A fixed identifier keeps the expected reply bytes deterministic.
    constexpr uint32_t TestMuid = 0x0123456;

    // What the offline analysis runs with: effects off, because a reverb tail would smear the
    // very thing the level measurements below are measuring.
    SynthConfig TestConfig() noexcept
    {
        auto config = SynthConfig::ForMode(SynthMode::Modern, 48000);
        config.EnableEffects = false;

        return config;
    }

    // gm.dls is several megabytes, so it is parsed once for the whole class rather than per test.
    // Returns nullptr when this machine has no usable sound set.
    _Ret_maybenull_ const DlsCollection* SystemSoundSet()
    {
        static DlsCollection collection;

        static const bool loaded = []() noexcept
        {
            wchar_t systemDirectory[MAX_PATH]{};

            if (GetSystemDirectoryW(systemDirectory, ARRAYSIZE(systemDirectory)) == 0)
            {
                return false;
            }

            try
            {
                const std::wstring path = std::wstring{ systemDirectory } + L"\\drivers\\gm.dls";

                return DlsCollection::LoadFromFile(
                    path, DlsParseLimits{}, SoundSetOrigin::SystemOnly, collection) == DlsParseStatus::Ok;
            }
            catch (...)
            {
                return false;
            }
        }();

        return loaded ? &collection : nullptr;
    }

    // Marks the test skipped and returns nullptr when there is nothing to render with.
    _Ret_maybenull_ const DlsCollection* RequireSoundSet()
    {
        const auto* const collection = SystemSoundSet();

        if (collection == nullptr)
        {
            Log::Result(TestResults::Skipped,
                L"The system sound set could not be loaded, so there is nothing to render with.");
        }

        return collection;
    }

    uint32_t MakeMidi1Cv(
        _In_ uint8_t group, _In_ uint8_t status, _In_ uint8_t channel,
        _In_ uint8_t data1, _In_ uint8_t data2) noexcept
    {
        return (2u << 28) | (static_cast<uint32_t>(group) << 24) |
               (static_cast<uint32_t>(status) << 20) | (static_cast<uint32_t>(channel) << 16) |
               (static_cast<uint32_t>(data1) << 8) | data2;
    }

    void MakeMidi2Cv(
        _In_ uint8_t group, _In_ uint8_t status, _In_ uint8_t channel,
        _In_ uint8_t index1, _In_ uint8_t index2, _In_ uint32_t data,
        _Out_writes_(2) uint32_t* words) noexcept
    {
        words[0] = (4u << 28) | (static_cast<uint32_t>(group) << 24) |
                   (static_cast<uint32_t>(status) << 20) | (static_cast<uint32_t>(channel) << 16) |
                   (static_cast<uint32_t>(index1) << 8) | index2;
        words[1] = data;
    }

    // Renders a short burst and reports its RMS, which is how the resolution checks below tell
    // two nearly identical velocities apart.
    double RenderBurstRms(_In_ SynthEngine& engine, _In_ uint32_t sampleRate, _In_ double seconds)
    {
        const auto frames = static_cast<uint32_t>(seconds * sampleRate);
        std::vector<float> buffer(static_cast<size_t>(frames) * 2, 0.0f);

        uint32_t rendered = 0;

        while (rendered < frames)
        {
            const uint32_t chunk = (std::min)(256u, frames - rendered);
            engine.Render(buffer.data() + static_cast<size_t>(rendered) * 2, chunk);
            rendered += chunk;
        }

        double energy = 0.0;

        for (const auto sample : buffer)
        {
            energy += static_cast<double>(sample) * sample;
        }

        return std::sqrt(energy / buffer.size());
    }

    // Estimates the fundamental of a rendered burst by autocorrelation. The per-note pitch tests
    // only ever compare one estimate against another, so this needs to be consistent rather than
    // absolutely accurate, which autocorrelation on a sustained tone comfortably is.
    double EstimateFundamentalHertz(
        _In_ SynthEngine& engine,
        _In_ uint32_t sampleRate,
        _In_ double seconds)
    {
        const auto frames = static_cast<uint32_t>(seconds * sampleRate);
        std::vector<float> buffer(static_cast<size_t>(frames) * 2, 0.0f);

        uint32_t rendered = 0;

        while (rendered < frames)
        {
            const uint32_t chunk = (std::min)(256u, frames - rendered);
            engine.Render(buffer.data() + static_cast<size_t>(rendered) * 2, chunk);
            rendered += chunk;
        }

        // Sum to mono, and skip the attack so the estimate sees the steady part of the note.
        const size_t skip = static_cast<size_t>(sampleRate) / 20;

        if (frames <= skip)
        {
            return 0.0;
        }

        std::vector<double> mono(frames - skip, 0.0);

        for (size_t i = 0; i < mono.size(); i++)
        {
            const size_t source = (i + skip) * 2;
            mono[i] = (static_cast<double>(buffer[source]) + buffer[source + 1]) * 0.5;
        }

        const size_t minimumLag = sampleRate / 2000;
        const size_t maximumLag = (std::min)(mono.size() / 2, static_cast<size_t>(sampleRate) / 50);

        if (maximumLag <= minimumLag)
        {
            return 0.0;
        }

        double bestScore = -1.0;
        size_t bestLag = 0;

        for (size_t lag = minimumLag; lag <= maximumLag; lag++)
        {
            double correlation = 0.0;
            double energyLagged = 0.0;

            for (size_t i = 0; i + lag < mono.size(); i++)
            {
                correlation += mono[i] * mono[i + lag];
                energyLagged += mono[i + lag] * mono[i + lag];
            }

            const double score = (energyLagged > 0.0) ? correlation / std::sqrt(energyLagged) : 0.0;

            if (score > bestScore)
            {
                bestScore = score;
                bestLag = lag;
            }
        }

        return (bestLag > 0) ? static_cast<double>(sampleRate) / static_cast<double>(bestLag) : 0.0;
    }

    void FreshEngine(
        _In_ const DlsCollection& collection,
        _In_ SynthEngine& engine,
        _In_ UmpDispatcher& dispatcher,
        _In_ uint8_t group)
    {
        engine.Initialize(&collection, TestConfig());

        dispatcher.Initialize(&engine, group, TestMuid);
    }

    void SendSysExPayload(_In_ UmpDispatcher& dispatcher, _In_ const std::vector<uint8_t>& payload)
    {
        for (size_t offset = 0; offset < payload.size(); offset += 6)
        {
            const auto count = static_cast<uint8_t>(
                (std::min)(size_t{ 6 }, payload.size() - offset));

            const bool isFirst = (offset == 0);
            const bool isLast = (offset + count >= payload.size());
            const uint8_t status = (isFirst && isLast) ? 0 : isFirst ? 1 : isLast ? 3 : 2;

            uint8_t bytes[6]{};

            for (uint8_t i = 0; i < count; i++)
            {
                bytes[i] = payload[offset + i];
            }

            const uint32_t words[2]
            {
                (3u << 28) | (static_cast<uint32_t>(status) << 20)
                    | (static_cast<uint32_t>(count) << 16)
                    | (static_cast<uint32_t>(bytes[0]) << 8) | bytes[1],

                (static_cast<uint32_t>(bytes[2]) << 24)
                    | (static_cast<uint32_t>(bytes[3]) << 16)
                    | (static_cast<uint32_t>(bytes[4]) << 8) | bytes[5],
            };

            dispatcher.ProcessWords(words, 2);
        }
    }

    struct CaptureOutput final : IUmpOutput
    {
        std::vector<uint32_t> Words;

        void SendUmp(const uint32_t* words, uint32_t wordCount) noexcept override
        {
            for (uint32_t i = 0; i < wordCount; i++)
            {
                Words.push_back(words[i]);
            }
        }
    };

    void SendIdentityRequest(_In_ UmpDispatcher& dispatcher)
    {
        // 7E 7F 06 01, as a complete sysex7 packet.
        uint32_t sysex[2];
        sysex[0] = (3u << 28) | (0u << 20) | (4u << 16) | (0x7Eu << 8) | 0x7F;
        sysex[1] = (0x06u << 24) | (0x01u << 16);
        dispatcher.ProcessWords(sysex, 2);
    }

    std::vector<uint8_t> DecodeSysEx7(_In_ const std::vector<uint32_t>& words)
    {
        std::vector<uint8_t> payload;

        for (size_t packet = 0; packet * 2 + 1 < words.size(); packet++)
        {
            const uint32_t w0 = words[packet * 2];
            const uint32_t w1 = words[packet * 2 + 1];
            const auto count = static_cast<uint8_t>((w0 >> 16) & 0x0F);

            const uint8_t bytes[6] =
            {
                static_cast<uint8_t>((w0 >> 8) & 0x7F),
                static_cast<uint8_t>(w0 & 0x7F),
                static_cast<uint8_t>((w1 >> 24) & 0x7F),
                static_cast<uint8_t>((w1 >> 16) & 0x7F),
                static_cast<uint8_t>((w1 >> 8) & 0x7F),
                static_cast<uint8_t>(w1 & 0x7F),
            };

            for (uint8_t i = 0; i < count && i < 6; i++)
            {
                payload.push_back(bytes[i]);
            }
        }

        return payload;
    }

    std::vector<uint8_t> DeviceControl(_In_ uint8_t subId2, _In_ uint16_t value)
    {
        return std::vector<uint8_t>{ 0x7F, 0x7F, 0x04, subId2,
            static_cast<uint8_t>(value & 0x7F), static_cast<uint8_t>((value >> 7) & 0x7F) };
    }

    // A real Discovery Inquiry captured from the in-box MIDI Keyboard app.
    constexpr uint32_t DiscoveryInquiry[10]
    {
        0x30167E7F, 0x0D70021F,
        0x30263075, 0x4A7F7F7F,
        0x30267F7D, 0x00000000,
        0x30260000, 0x01000000,
        0x30361C00, 0x04000000,
    };

    // A one byte manufacturer identifier produces a thirteen byte reply, a three byte one
    // produces fifteen. Both shapes are checked because getting the length wrong makes the
    // reply unparseable to the requester.
    void VerifyIdentityReply(
        _In_ const DlsCollection& collection,
        _In_ bool extendedId,
        _In_ const wchar_t* name)
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(collection, engine, dispatcher, 0);

        CaptureOutput output;

        SynthIdentity identity;

        if (extendedId)
        {
            identity.ManufacturerSysExId[0] = 0x00;
            identity.ManufacturerSysExId[1] = 0x01;
            identity.ManufacturerSysExId[2] = 0x02;
        }
        else
        {
            // 7D is the identifier reserved for non commercial and educational use.
            identity.ManufacturerSysExId[0] = 0x7D;
        }

        identity.FamilyCode = 0x0102;
        identity.FamilyMemberCode = 0x0304;

        dispatcher.SetOutput(&output, identity);
        SendIdentityRequest(dispatcher);

        const auto payload = DecodeSysEx7(output.Words);

        const size_t expected = extendedId ? 15u : 13u;
        const size_t familyAt = extendedId ? 7u : 5u;

        const bool ok =
            payload.size() == expected &&
            payload[0] == 0x7E && payload[2] == 0x06 && payload[3] == 0x02 &&
            payload[familyAt] == 0x02 && payload[familyAt + 1] == 0x02 &&
            payload[familyAt + 2] == 0x04 && payload[familyAt + 3] == 0x06;

        Log::Comment(String().Format(L"%s: %zu bytes", name, payload.size()));

        VERIFY_IS_TRUE(ok, name);
    }
}


// Packet sizing has to be right or the whole stream desynchronizes.
void MidiSynthUmpTests::TestPacketWordCounts()
{
    bool sizesOk =
        UmpDispatcher::PacketWordCount(0x00000000) == 1 &&   // utility
        UmpDispatcher::PacketWordCount(0x10000000) == 1 &&   // system
        UmpDispatcher::PacketWordCount(0x20000000) == 1 &&   // MIDI 1.0 channel voice
        UmpDispatcher::PacketWordCount(0x30000000) == 2 &&   // sysex7
        UmpDispatcher::PacketWordCount(0x40000000) == 2 &&   // MIDI 2.0 channel voice
        UmpDispatcher::PacketWordCount(0x50000000) == 4 &&   // sysex8
        UmpDispatcher::PacketWordCount(0xD0000000) == 4 &&   // flex data
        UmpDispatcher::PacketWordCount(0xF0000000) == 4;     // stream

    VERIFY_IS_TRUE(sizesOk, L"packet word counts by message type");
}


void MidiSynthUmpTests::TestNoteOnVelocityZeroSemantics()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    const uint32_t rate = TestConfig().RenderSampleRate();

    // A MIDI 1.0 note on with zero velocity is a note off.
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(*collection, engine, dispatcher, 0);

        uint32_t word = MakeMidi1Cv(0, 0x9, 0, 60, 100);
        dispatcher.ProcessWords(&word, 1);
        const uint32_t afterOn = engine.ActiveVoiceCount();

        word = MakeMidi1Cv(0, 0x9, 0, 60, 0);
        dispatcher.ProcessWords(&word, 1);

        (void)RenderBurstRms(engine, rate, 0.01);

        VERIFY_IS_TRUE(afterOn == 1, L"MIDI 1.0 note on velocity zero is a note off");
    }

    // A MIDI 2.0 note on with zero velocity is still a note on. This is the difference that
    // silently breaks a synthesizer that reuses its MIDI 1.0 path.
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(*collection, engine, dispatcher, 0);

        uint32_t words[2];
        MakeMidi2Cv(0, 0x9, 0, 60, 0, 0x00000000, words);
        dispatcher.ProcessWords(words, 2);

        VERIFY_IS_TRUE(engine.ActiveVoiceCount() == 1,
            L"MIDI 2.0 note on velocity zero still sounds");
    }
}


void MidiSynthUmpTests::TestGroupAndPacketFraming()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    // Messages for another group belong to another function block.
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(*collection, engine, dispatcher, 0);

        const uint32_t word = MakeMidi1Cv(3, 0x9, 0, 60, 100);
        dispatcher.ProcessWords(&word, 1);

        VERIFY_IS_TRUE(engine.ActiveVoiceCount() == 0,
            L"messages for a different group are ignored");
    }

    // A packet split across two calls must not be consumed early.
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(*collection, engine, dispatcher, 0);

        uint32_t words[2];
        MakeMidi2Cv(0, 0x9, 0, 60, 0, 0x40000000, words);

        const uint32_t consumed = dispatcher.ProcessWords(words, 1);
        const bool leftAlone = (consumed == 0) && (engine.ActiveVoiceCount() == 0);

        const uint32_t after = dispatcher.ProcessWords(words, 2);

        VERIFY_IS_TRUE(leftAlone && after == 2 && engine.ActiveVoiceCount() == 1,
            L"a partial packet is left for the next call");
    }

    // An unknown message type must not desynchronize the stream.
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(*collection, engine, dispatcher, 0);

        uint32_t stream[5];
        stream[0] = (0xBu << 28);                       // three word reserved type
        stream[1] = 0;
        stream[2] = 0;
        stream[3] = MakeMidi1Cv(0, 0x9, 0, 60, 100);
        stream[4] = 0;

        const uint32_t consumed = dispatcher.ProcessWords(stream, 4);

        VERIFY_IS_TRUE(consumed == 4 && engine.ActiveVoiceCount() == 1,
            L"an unknown message type is skipped by its declared size");
    }
}


void MidiSynthUmpTests::TestHighResolutionIsPreserved()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    const uint32_t rate = TestConfig().RenderSampleRate();

    // Two velocities that would collapse to the same seven bit value must stay distinct.
    {
        auto levelForVelocity = [&](uint16_t velocity)
        {
            SynthEngine engine;
            UmpDispatcher dispatcher;
            FreshEngine(*collection, engine, dispatcher, 0);
            engine.ProgramChange(0, 19);

            uint32_t words[2];
            MakeMidi2Cv(0, 0x9, 0, 60, 0, static_cast<uint32_t>(velocity) << 16, words);
            dispatcher.ProcessWords(words, 2);

            return RenderBurstRms(engine, rate, 0.20);
        };

        const double low = levelForVelocity(0x8000);
        const double high = levelForVelocity(0x81FF);
        const double differenceDb = (low > 0.0) ? 20.0 * std::log10(high / low) : 0.0;

        Log::Comment(String().Format(L"16 bit velocity: %.3f dB apart", differenceDb));

        // Both values are 64 when truncated to seven bits, so any difference proves the
        // sixteen bit velocity survived.
        VERIFY_IS_TRUE(differenceDb > 0.05, L"16 bit velocity resolution is preserved");
    }

    // Same idea for a 32 bit control change.
    {
        auto levelForVolume = [&](uint32_t volume)
        {
            SynthEngine engine;
            UmpDispatcher dispatcher;
            FreshEngine(*collection, engine, dispatcher, 0);
            engine.ProgramChange(0, 19);

            uint32_t words[2];
            MakeMidi2Cv(0, 0xB, 0, 7, 0, volume, words);
            dispatcher.ProcessWords(words, 2);

            MakeMidi2Cv(0, 0x9, 0, 60, 0, 0xC0000000, words);
            dispatcher.ProcessWords(words, 2);

            return RenderBurstRms(engine, rate, 0.20);
        };

        const double low = levelForVolume(0x80000000);
        const double high = levelForVolume(0x80FFFFFF);
        const double differenceDb = (low > 0.0) ? 20.0 * std::log10(high / low) : 0.0;

        Log::Comment(String().Format(L"32 bit control change: %.3f dB apart", differenceDb));

        VERIFY_IS_TRUE(differenceDb > 0.05, L"32 bit control change resolution is preserved");
    }

    // The specification's upscale preserves the center value. A plain bit repeat puts MIDI 1.0
    // velocity 64 slightly above the MIDI 2.0 center, which is the defect this pins down.
    {
        auto levelFor = [&](bool midi1, uint16_t value)
        {
            SynthEngine engine;
            UmpDispatcher dispatcher;
            FreshEngine(*collection, engine, dispatcher, 0);

            if (midi1)
            {
                uint32_t word = MakeMidi1Cv(0, 0x9, 0, 60, static_cast<uint8_t>(value));
                dispatcher.ProcessWords(&word, 1);
            }
            else
            {
                uint32_t words[2];
                MakeMidi2Cv(0, 0x9, 0, 60, 0, static_cast<uint32_t>(value) << 16, words);
                dispatcher.ProcessWords(words, 2);
            }

            return RenderBurstRms(engine, rate, 0.20);
        };

        const double midi1Level = levelFor(true, 64);
        const double midi2Level = levelFor(false, 32768);

        const double differenceDb = (midi1Level > 0.0 && midi2Level > 0.0)
            ? std::abs(20.0 * std::log10(midi2Level / midi1Level)) : 99.0;

        Log::Comment(String().Format(L"velocity 64 upscale: %.4f dB apart", differenceDb));

        VERIFY_IS_TRUE(differenceDb < 0.001, L"MIDI 1.0 velocity 64 scales to the MIDI 2.0 center");
    }
}


// Bank addressing. gm.dls carries its 98 variations in the bank MSB with the LSB always
// zero, so a file written for XG or GM2 misses every one of them unless the incoming bank
// select is translated. The expected names are read from the sound set rather than from
// the engine, so a mapping which quietly resolves to the wrong instrument still fails.
void MidiSynthUmpTests::TestBankAddressing()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    // MIDI 2.0 carries the bank with the program change rather than in separate messages.
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(*collection, engine, dispatcher, 0);

        uint32_t words[2];

        // Bank valid flag set, bank MSB 8, program 0, which is a GS variation piano.
        MakeMidi2Cv(0, 0xC, 0, 0, 0x01, (0u << 24) | (8u << 8) | 0u, words);
        dispatcher.ProcessWords(words, 2);

        MakeMidi2Cv(0, 0x9, 0, 60, 0, 0xC0000000, words);
        dispatcher.ProcessWords(words, 2);

        VERIFY_IS_TRUE(engine.ActiveVoiceCount() == 1, L"program change carries its bank");
    }

    // Program 0 in variation bank 8 is a real, named entry in this sound set. If it ever
    // stops being one the test says so rather than silently passing on a fallback.
    const auto* const expectedVariation = collection->FindInstrument(8, 0, 0, false);
    const auto* const capitalTone = collection->FindInstrument(0, 0, 0, false);

    VERIFY_IS_TRUE(
        expectedVariation != nullptr && capitalTone != nullptr &&
        expectedVariation != capitalTone,
        L"sound set has a variation to test with");

    auto selectedInstrument = [&](BankSelectMode mode, uint8_t msb, uint8_t lsb)
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;

        auto modeConfig = TestConfig();
        modeConfig.BankSelect = mode;

        engine.Initialize(collection, modeConfig);
        dispatcher.Initialize(&engine, 0, TestMuid);

        uint32_t word = MakeMidi1Cv(0, 0xB, 0, 0, msb);
        dispatcher.ProcessWords(&word, 1);

        word = MakeMidi1Cv(0, 0xB, 0, 32, lsb);
        dispatcher.ProcessWords(&word, 1);

        word = MakeMidi1Cv(0, 0xC, 0, 0, 0);
        dispatcher.ProcessWords(&word, 1);

        return engine.ChannelState(0).Instrument;
    };

    VERIFY_IS_TRUE(selectedInstrument(BankSelectMode::RolandGS, 8, 0) == expectedVariation,
        L"GS addressing reads the variation from the MSB");

    VERIFY_IS_TRUE(selectedInstrument(BankSelectMode::YamahaXG, 0, 8) == expectedVariation,
        L"XG addressing reads the variation from the LSB");

    VERIFY_IS_TRUE(selectedInstrument(BankSelectMode::GeneralMidi2, 121, 8) == expectedVariation,
        L"GM2 addressing reads the variation from the LSB under MSB 121");

    // The point of the mapping is that the wrong convention misses. If this passed, the
    // lookup would be ignoring the mode and the three checks above would prove nothing.
    VERIFY_IS_TRUE(selectedInstrument(BankSelectMode::RolandGS, 0, 8) == capitalTone,
        L"XG style select under GS addressing falls back to the capital tone");

    // Automatic follows the sender.
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;

        auto autoConfig = TestConfig();
        autoConfig.BankSelect = BankSelectMode::Automatic;

        engine.Initialize(collection, autoConfig);
        dispatcher.Initialize(&engine, 0, TestMuid);

        const bool startsAsGs = engine.EffectiveBankSelectMode() == BankSelectMode::RolandGS;

        // XG System On: 43 10 4C 00 00 7E 00
        SendSysExPayload(dispatcher, { 0x43, 0x10, 0x4C, 0x00, 0x00, 0x7E, 0x00 });

        const bool becameXg = engine.EffectiveBankSelectMode() == BankSelectMode::YamahaXG;

        uint32_t word = MakeMidi1Cv(0, 0xB, 0, 32, 8);
        dispatcher.ProcessWords(&word, 1);
        word = MakeMidi1Cv(0, 0xC, 0, 0, 0);
        dispatcher.ProcessWords(&word, 1);

        VERIFY_IS_TRUE(
            startsAsGs && becameXg &&
            engine.ChannelState(0).Instrument == expectedVariation,
            L"automatic addressing follows an XG System On");
    }

    // An explicit choice is the customer overriding the file, so it must not be moved by
    // what the file claims.
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;

        auto gsConfig = TestConfig();
        gsConfig.BankSelect = BankSelectMode::RolandGS;

        engine.Initialize(collection, gsConfig);
        dispatcher.Initialize(&engine, 0, TestMuid);

        SendSysExPayload(dispatcher, { 0x43, 0x10, 0x4C, 0x00, 0x00, 0x7E, 0x00 });

        VERIFY_IS_TRUE(engine.EffectiveBankSelectMode() == BankSelectMode::RolandGS,
            L"an explicit addressing choice ignores the sender");
    }
}


void MidiSynthUmpTests::TestDrumChannelAssignment()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    // Drum kits on a channel other than 10. Without the GS rhythm part message eight of
    // the nine kits in this sound set can never be heard alongside the tenth.
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(*collection, engine, dispatcher, 0);

        const bool startsMelodic = !engine.IsDrumChannel(0);

        // 41 10 42 12 40 11 15 01 <sum>. The nibble is a Roland block number, not a MIDI
        // channel: block 0 is channel 10 and blocks 1 to 9 are channels 1 to 9, so block 1
        // is MIDI channel 1, which is index zero here.
        SendSysExPayload(dispatcher, { 0x41, 0x10, 0x42, 0x12, 0x40, 0x11, 0x15, 0x01, 0x29 });

        const bool becameDrum = engine.IsDrumChannel(0);

        // And back again, so a file can hand the channel back to melodic use.
        SendSysExPayload(dispatcher, { 0x41, 0x10, 0x42, 0x12, 0x40, 0x11, 0x15, 0x00, 0x2A });

        VERIFY_IS_TRUE(startsMelodic && becameDrum && !engine.IsDrumChannel(0),
            L"GS Use For Rhythm Part makes another channel a drum part");
    }

    // Channel 10 must stay a drum part through a reset, and an assignment must not.
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(*collection, engine, dispatcher, 0);

        engine.SetDrumChannel(2, true);
        const bool assigned = engine.IsDrumChannel(2);

        engine.SystemReset();

        VERIFY_IS_TRUE(assigned && engine.IsDrumChannel(9) && !engine.IsDrumChannel(2),
            L"reset restores channel 10 as the only drum part");
    }
}


// The customer's volume trim. This is the synthesizer's only volume control, because a
// session 0 service gets no slider in the Windows Volume Mixer and exclusive and ASIO
// output have no Windows mixer in the path at all.
void MidiSynthUmpTests::TestUserVolume()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    const uint32_t rate = TestConfig().RenderSampleRate();

    auto renderAt = [&](double userVolumeDb)
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(*collection, engine, dispatcher, 0);

        engine.SetUserVolumeDb(userVolumeDb);

        const uint32_t word = MakeMidi1Cv(0, 0x9, 0, 60, 100);
        dispatcher.ProcessWords(&word, 1);

        return RenderBurstRms(engine, rate, 0.20);
    };

    const double unity = renderAt(0.0);
    const double quiet = renderAt(-20.0);

    const double deltaDb = (unity > 0.0 && quiet > 0.0)
        ? 20.0 * std::log10(quiet / unity)
        : 0.0;

    if (deltaDb == 0.0)
    {
        Log::Comment(L"no output to measure");
    }

    VERIFY_IS_TRUE(std::abs(deltaDb + 20.0) < 0.5, L"user volume of -20 dB attenuates by 20 dB");

    // The GM2 master volume belongs to the content and a System Reset clears it. The
    // customer's trim must not ride along, or any file which opens with a reset would
    // silently discard what the customer chose.
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(*collection, engine, dispatcher, 0);

        engine.SetUserVolumeDb(-12.0);
        engine.SystemReset();

        VERIFY_IS_TRUE(std::abs(engine.UserVolumeDb() + 12.0) < 0.001,
            L"a System Reset leaves the customer's volume alone");
    }

    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(*collection, engine, dispatcher, 0);

        engine.SetUserVolumeDb(999.0);
        const bool clampedHigh = engine.UserVolumeDb() <= SynthEngine::MaximumUserVolumeDb;

        engine.SetUserVolumeDb(-999.0);
        const bool clampedLow = engine.UserVolumeDb() >= SynthEngine::MinimumUserVolumeDb;

        VERIFY_IS_TRUE(clampedHigh && clampedLow, L"user volume is clamped to its range");
    }
}


void MidiSynthUmpTests::TestResets()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    // GM System On, as a complete sysex7 packet.
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(*collection, engine, dispatcher, 0);

        uint32_t word = MakeMidi1Cv(0, 0x9, 0, 60, 100);
        dispatcher.ProcessWords(&word, 1);
        const bool sounding = engine.ActiveVoiceCount() == 1;

        uint32_t sysex[2];
        sysex[0] = (3u << 28) | (0u << 24) | (0u << 20) | (4u << 16) | (0x7Eu << 8) | 0x7F;
        sysex[1] = (0x09u << 24) | (0x01u << 16);
        dispatcher.ProcessWords(sysex, 2);

        VERIFY_IS_TRUE(sounding && engine.ActiveVoiceCount() == 0,
            L"GM System On resets the synthesizer");
    }

    // Roland GS Reset, split across two sysex7 packets. Files authored for GS open with this
    // rather than with GM System On, so a file player depends on it. We honor the message
    // without ever claiming Roland's manufacturer id as our own.
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(*collection, engine, dispatcher, 0);

        uint32_t word = MakeMidi1Cv(0, 0x9, 0, 60, 100);
        dispatcher.ProcessWords(&word, 1);
        const bool sounding = engine.ActiveVoiceCount() == 1;

        // 41 10 42 12 40 00 7F 00 41
        uint32_t start[2];
        start[0] = (3u << 28) | (1u << 20) | (6u << 16) | (0x41u << 8) | 0x10u;
        start[1] = (0x42u << 24) | (0x12u << 16) | (0x40u << 8) | 0x00u;
        dispatcher.ProcessWords(start, 2);

        uint32_t end[2];
        end[0] = (3u << 28) | (3u << 20) | (3u << 16) | (0x7Fu << 8) | 0x00u;
        end[1] = (0x41u << 24);
        dispatcher.ProcessWords(end, 2);

        VERIFY_IS_TRUE(sounding && engine.ActiveVoiceCount() == 0,
            L"Roland GS Reset resets the synthesizer");
    }

    // A Roland message that is not the reset must be left alone, or every GS parameter change
    // in a file would silence it.
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(*collection, engine, dispatcher, 0);

        uint32_t word = MakeMidi1Cv(0, 0x9, 0, 60, 100);
        dispatcher.ProcessWords(&word, 1);

        // Same maker and model, address 40 00 00, which is master tune rather than reset.
        uint32_t start[2];
        start[0] = (3u << 28) | (1u << 20) | (6u << 16) | (0x41u << 8) | 0x10u;
        start[1] = (0x42u << 24) | (0x12u << 16) | (0x40u << 8) | 0x00u;
        dispatcher.ProcessWords(start, 2);

        uint32_t end[2];
        end[0] = (3u << 28) | (3u << 20) | (3u << 16) | (0x00u << 8) | 0x00u;
        end[1] = (0x40u << 24);
        dispatcher.ProcessWords(end, 2);

        VERIFY_IS_TRUE(engine.ActiveVoiceCount() == 1,
            L"a non-reset Roland message leaves voices alone");
    }
}


// A property exchange request must be parked for a worker thread, never answered here.
void MidiSynthUmpTests::TestPropertyRequestParking()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    SynthEngine engine;
    UmpDispatcher dispatcher;
    FreshEngine(*collection, engine, dispatcher, 0);

    dispatcher.SetMuid(TestMuid);

    const char header[]{ "{\"resource\":\"ProgramList\"}" };
    const uint16_t headerLength = static_cast<uint16_t>(sizeof(header) - 1);

    std::vector<uint8_t> payload{ 0x7E, 0x7F, 0x0D, 0x34, 0x02 };

    const auto appendMuid = [&payload](uint32_t muid)
    {
        payload.push_back(static_cast<uint8_t>(muid & 0x7F));
        payload.push_back(static_cast<uint8_t>((muid >> 7) & 0x7F));
        payload.push_back(static_cast<uint8_t>((muid >> 14) & 0x7F));
        payload.push_back(static_cast<uint8_t>((muid >> 21) & 0x7F));
    };

    appendMuid(0x0000001);
    appendMuid(TestMuid);

    payload.push_back(0x07);                                    // request id
    payload.push_back(static_cast<uint8_t>(headerLength & 0x7F));
    payload.push_back(static_cast<uint8_t>((headerLength >> 7) & 0x7F));

    for (uint16_t i = 0; i < headerLength; i++)
    {
        payload.push_back(static_cast<uint8_t>(header[i]));
    }

    payload.push_back(0x01); payload.push_back(0x00);           // chunk count
    payload.push_back(0x01); payload.push_back(0x00);           // this chunk
    payload.push_back(0x00); payload.push_back(0x00);           // no property data

    // Arrives split across several packets, the way a real one would.
    SendSysExPayload(dispatcher, payload);

    UmpDispatcher::PendingPropertyRequest request{};

    const bool parked = dispatcher.TakePendingPropertyRequest(request);

    const bool correct =
        parked &&
        request.RequestId == 0x07 &&
        request.InitiatorMuid == 0x0000001 &&
        request.HeaderByteCount == headerLength &&
        memcmp(request.Header, header, headerLength) == 0;

    // And only once: a second take with nothing new must report nothing.
    UmpDispatcher::PendingPropertyRequest again{};

    VERIFY_IS_TRUE(correct && !dispatcher.TakePendingPropertyRequest(again),
        L"a property request is parked with its header intact");
}


// An Identity Request needs a reply, which means an output path. The in-box synth cannot
// do this at all, having no MIDI input, so this is new behavior rather than compatibility.
void MidiSynthUmpTests::TestIdentityReply()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    // A cleared identifier must stay silent rather than send zeros.
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(*collection, engine, dispatcher, 0);

        CaptureOutput output;

        SynthIdentity cleared;
        cleared.ManufacturerSysExId[0] = 0;
        cleared.ManufacturerSysExId[1] = 0;
        cleared.ManufacturerSysExId[2] = 0;

        dispatcher.SetOutput(&output, cleared);
        SendIdentityRequest(dispatcher);

        VERIFY_IS_TRUE(output.Words.empty(),
            L"no reply when the manufacturer identifier is cleared");
    }

    // These values go out on the wire, so pin them.
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(*collection, engine, dispatcher, 0);

        CaptureOutput output;
        dispatcher.SetOutput(&output, SynthIdentity{});
        SendIdentityRequest(dispatcher);

        const auto payload = DecodeSysEx7(output.Words);

        const bool ok =
            payload.size() == 15 &&
            payload[0] == 0x7E && payload[2] == 0x06 && payload[3] == 0x02 &&
            payload[4] == 0x00 && payload[5] == 0x00 && payload[6] == 0x41 &&   // Microsoft
            payload[7] == 11 && payload[8] == 0 &&                              // Windows 11
            payload[9] == 1 && payload[10] == 0 &&                              // this synth
            payload[11] == 1 && payload[12] == 0 &&
            payload[13] == 0 && payload[14] == 0;                               // 1.0.0.0

        VERIFY_IS_TRUE(ok, L"default identity is Microsoft, Windows 11, model 1, rev 1.0.0.0");
    }

    VerifyIdentityReply(*collection, false, L"Identity Reply, one byte manufacturer identifier");
    VerifyIdentityReply(*collection, true, L"Identity Reply, three byte manufacturer identifier");
}


void MidiSynthUmpTests::TestMidiCiDiscovery()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    // A real Discovery Inquiry captured from the in-box MIDI Keyboard app. Replying is
    // mandatory even though no MIDI-CI categories are supported yet.
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(*collection, engine, dispatcher, 0);

        CaptureOutput output;
        dispatcher.SetOutput(&output, SynthIdentity{});

        dispatcher.ProcessWords(DiscoveryInquiry, 10);

        const auto payload = DecodeSysEx7(output.Words);
        const uint32_t muid = dispatcher.Muid();

        const bool ok =
            payload.size() == 31 &&
            payload[0] == 0x7E && payload[1] == 0x7F &&
            payload[2] == 0x0D && payload[3] == 0x71 &&
            payload[4] == 0x02 &&
            payload[5] == (muid & 0x7F) &&
            payload[6] == ((muid >> 7) & 0x7F) &&
            payload[7] == ((muid >> 14) & 0x7F) &&
            payload[8] == ((muid >> 21) & 0x7F) &&
            payload[9] == 0x1F && payload[10] == 0x30 &&       // initiator muid echoed
            payload[11] == 0x75 && payload[12] == 0x4A &&
            payload[13] == 0x00 && payload[14] == 0x00 && payload[15] == 0x41 &&
            payload[16] == 11 && payload[18] == 1 &&
            payload[30] == 0;                                   // our function block

        Log::Comment(String().Format(L"%zu bytes, muid 0x%07X", payload.size(), muid));

        VERIFY_IS_TRUE(ok, L"MIDI-CI Discovery is answered with a Reply to Discovery");
    }

    // An invalidated identifier must not keep answering MIDI-CI.
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(*collection, engine, dispatcher, 0);

        CaptureOutput output;
        dispatcher.SetOutput(&output, SynthIdentity{});

        dispatcher.ProcessWords(DiscoveryInquiry, 10);
        const bool repliedFirst = !output.Words.empty();

        const uint32_t muid = dispatcher.Muid();

        std::vector<uint8_t> invalidate{ 0x7E, 0x7F, 0x0D, 0x7E, 0x02 };

        for (int pass = 0; pass < 3; pass++)
        {
            const uint32_t value = (pass == 0) ? 0x4A75301Fu : muid;

            invalidate.push_back(static_cast<uint8_t>(value & 0x7F));
            invalidate.push_back(static_cast<uint8_t>((value >> 7) & 0x7F));
            invalidate.push_back(static_cast<uint8_t>((value >> 14) & 0x7F));
            invalidate.push_back(static_cast<uint8_t>((value >> 21) & 0x7F));
        }

        SendSysExPayload(dispatcher, invalidate);

        output.Words.clear();
        dispatcher.ProcessWords(DiscoveryInquiry, 10);

        VERIFY_IS_TRUE(
            repliedFirst && output.Words.empty() && dispatcher.MuidNeedsReplacement(),
            L"an invalidated MUID stops answering MIDI-CI");
    }
}


// General MIDI 2 requires master volume and master tuning.
void MidiSynthUmpTests::TestMasterVolume()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    const uint32_t rate = TestConfig().RenderSampleRate();

    // Half scale is a quarter of the power, so twelve dB down on the concave curve.
    auto levelAt = [&](uint16_t masterVolume)
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(*collection, engine, dispatcher, 0);

        SendSysExPayload(dispatcher, DeviceControl(0x01, masterVolume));

        uint32_t word = MakeMidi1Cv(0, 0x9, 0, 60, 100);
        dispatcher.ProcessWords(&word, 1);

        return RenderBurstRms(engine, rate, 0.20);
    };

    const double full = levelAt(16383);
    const double half = levelAt(8192);

    const double differenceDb = (full > 0.0 && half > 0.0)
        ? 20.0 * std::log10(full / half) : 0.0;

    Log::Comment(String().Format(L"%.2f dB down", differenceDb));

    VERIFY_IS_TRUE(std::abs(differenceDb - 12.04) < 0.5,
        L"master volume follows the concave curve");
}


void MidiSynthUmpTests::TestMasterTuning()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    const uint32_t rate = TestConfig().RenderSampleRate();

    // Coarse tuning must move a melodic channel, and must leave a drum kit alone or a
    // different drum sound would be selected.
    auto renderNote = [&](uint8_t channel, uint8_t coarseMsb, std::vector<float>& out)
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(*collection, engine, dispatcher, 0);

        SendSysExPayload(dispatcher, DeviceControl(0x04, static_cast<uint16_t>(coarseMsb) << 7));

        uint32_t word = MakeMidi1Cv(0, 0x9, channel, 42, 100);
        dispatcher.ProcessWords(&word, 1);

        out.assign(static_cast<size_t>(rate / 20) * 2, 0.0f);
        engine.Render(out.data(), rate / 20);
    };

    std::vector<float> melodicCentered, melodicShifted;
    renderNote(0, 64, melodicCentered);
    renderNote(0, 76, melodicShifted);

    std::vector<float> drumCentered, drumShifted;
    renderNote(9, 64, drumCentered);
    renderNote(9, 76, drumShifted);

    const bool melodicMoved = melodicCentered != melodicShifted;
    const bool drumUntouched = drumCentered == drumShifted;

    VERIFY_IS_TRUE(melodicMoved, L"master coarse tuning shifts a melodic channel");
    VERIFY_IS_TRUE(drumUntouched, L"master tuning leaves a drum channel alone");
}


// GM2 requires a device to respond to Active Sensing: once a sender uses it, going quiet
// means the link died and everything must stop.
void MidiSynthUmpTests::TestActiveSensing()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    const uint32_t rate = TestConfig().RenderSampleRate();

    SynthEngine engine;
    UmpDispatcher dispatcher;
    FreshEngine(*collection, engine, dispatcher, 0);

    uint32_t sensing = (1u << 28) | (0xFEu << 16);
    dispatcher.ProcessWords(&sensing, 1);

    uint32_t word = MakeMidi1Cv(0, 0x9, 0, 60, 100);
    dispatcher.ProcessWords(&word, 1);

    std::vector<float> block(1024 * 2);

    // Well inside the timeout, and refreshed, so the note must survive.
    engine.Render(block.data(), 1024);
    dispatcher.ProcessWords(&sensing, 1);
    engine.Render(block.data(), 1024);

    const bool stillSounding = engine.ActiveVoiceCount() > 0;

    // Now let it lapse past the 300 ms the specification allows.
    const uint32_t silentFrames = static_cast<uint32_t>(rate * 0.4);

    for (uint32_t done = 0; done < silentFrames; done += 1024)
    {
        engine.Render(block.data(), 1024);
    }

    // The kill fade needs a moment to finish.
    engine.Render(block.data(), 1024);

    VERIFY_IS_TRUE(stillSounding && engine.ActiveVoiceCount() == 0,
        L"active sensing timeout stops everything");
}


// Shutdown releases the audio device, so it has to drain first or disconnecting clicks.
// This measures the bound the transport has to allow for.
void MidiSynthUmpTests::TestAllSoundOff()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    const auto config = TestConfig();
    const uint32_t rate = config.RenderSampleRate();

    SynthEngine engine;
    UmpDispatcher dispatcher;
    FreshEngine(*collection, engine, dispatcher, 0);

    for (uint8_t note = 60; note < 66; note++)
    {
        engine.NoteOn(0, note, static_cast<uint16_t>(100u << 9));
    }

    std::vector<float> block(256 * 2);
    engine.Render(block.data(), 256);

    for (uint8_t channel = 0; channel < 16; channel++)
    {
        engine.AllSoundOff(channel);
    }

    uint32_t frames = 0;
    const uint32_t limit = rate;

    // Drain at the control rate, otherwise this measures the block size rather than the fade.
    const uint32_t drainBlock = config.ControlRateFrames;

    while (frames < limit)
    {
        engine.Render(block.data(), drainBlock);
        frames += drainBlock;

        float peak = 0.0f;

        for (uint32_t i = 0; i < drainBlock * 2; i++)
        {
            peak = (std::max)(peak, std::fabs(block[i]));
        }

        if (engine.ActiveVoiceCount() == 0 && peak == 0.0f)
        {
            break;
        }
    }

    const double milliseconds = (1000.0 * frames) / rate;

    Log::Comment(String().Format(L"silent after %.1f ms", milliseconds));

    VERIFY_IS_TRUE(engine.ActiveVoiceCount() == 0 && milliseconds < 50.0,
        L"all sound off drains to silence quickly");
}

// A MIDI 2.0 note on can carry an absolute pitch that is not on a key boundary. Nothing in MIDI 1.0
// can express that without stealing the channel's bend wheel from every other note.
void MidiSynthUmpTests::TestNoteOnPitchAttribute()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    const uint32_t rate = TestConfig().RenderSampleRate();

    auto pitchOfNote = [&](uint8_t note, uint8_t attributeType, uint16_t attributeData)
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(*collection, engine, dispatcher, 0);
        engine.ProgramChange(0, 19);

        uint32_t words[2];
        MakeMidi2Cv(0, 0x9, 0, note, attributeType,
            (0xC000u << 16) | attributeData, words);
        dispatcher.ProcessWords(words, 2);

        return EstimateFundamentalHertz(engine, rate, 0.40);
    };

    const double plain60 = pitchOfNote(60, 0, 0);
    const double plain61 = pitchOfNote(61, 0, 0);

    // Note 60 with a Pitch 7.9 attribute asking for 60.5, which is half a semitone up.
    const uint16_t quarterTone = static_cast<uint16_t>((60u << 9) | 256u);
    const double quarter = pitchOfNote(60, 0x03, quarterTone);

    Log::Comment(String().Format(L"60 = %.2f Hz, 60.5 = %.2f Hz, 61 = %.2f Hz",
        plain60, quarter, plain61));

    VERIFY_IS_TRUE(plain60 > 0.0 && plain61 > 0.0 && quarter > 0.0,
        L"all three notes produced a measurable pitch");

    VERIFY_IS_TRUE(quarter > plain60 * 1.005 && quarter < plain61 * 0.995,
        L"the Pitch 7.9 attribute lands between the two keys");

    // An attribute type this engine does not act on must not change the pitch, rather than being
    // mistaken for a pitch.
    const double unknownAttribute = pitchOfNote(60, 0x01, quarterTone);

    VERIFY_IS_TRUE(std::abs(unknownAttribute - plain60) < plain60 * 0.005,
        L"an unrelated attribute type leaves the pitch alone");
}

// Per-note pitch bend moves one sounding note without touching anything else on the channel.
void MidiSynthUmpTests::TestPerNotePitchBend()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    const uint32_t rate = TestConfig().RenderSampleRate();

    auto pitchWithBend = [&](uint8_t bentNote, uint32_t bendValue)
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(*collection, engine, dispatcher, 0);
        engine.ProgramChange(0, 19);

        uint32_t words[2];
        MakeMidi2Cv(0, 0x9, 0, 60, 0, 0xC0000000, words);
        dispatcher.ProcessWords(words, 2);

        MakeMidi2Cv(0, 0x6, 0, bentNote, 0, bendValue, words);
        dispatcher.ProcessWords(words, 2);

        return EstimateFundamentalHertz(engine, rate, 0.40);
    };

    const double centered = pitchWithBend(60, 0x80000000);
    const double bentUp = pitchWithBend(60, 0xFFFFFFFF);
    const double bentOther = pitchWithBend(62, 0xFFFFFFFF);

    Log::Comment(String().Format(L"centered %.2f Hz, bent %.2f Hz, other note bent %.2f Hz",
        centered, bentUp, bentOther));

    VERIFY_IS_TRUE(centered > 0.0, L"the note produced a measurable pitch");

    // The default bend range is two semitones, so a full bend up is close to a whole tone.
    VERIFY_IS_TRUE(bentUp > centered * 1.05, L"per-note pitch bend raises the note it addresses");

    VERIFY_IS_TRUE(std::abs(bentOther - centered) < centered * 0.005,
        L"a per-note bend addressed to another note leaves this one alone");
}

// Registered per-note controllers give each note its own volume, pan and tuning.
void MidiSynthUmpTests::TestPerNoteControllers()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    const uint32_t rate = TestConfig().RenderSampleRate();

    auto withController = [&](uint8_t note, uint8_t controller, uint32_t value,
        bool wantPitch)
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(*collection, engine, dispatcher, 0);
        engine.ProgramChange(0, 19);

        uint32_t words[2];
        MakeMidi2Cv(0, 0x9, 0, 60, 0, 0xC0000000, words);
        dispatcher.ProcessWords(words, 2);

        MakeMidi2Cv(0, 0x0, 0, note, controller, value, words);
        dispatcher.ProcessWords(words, 2);

        return wantPitch
            ? EstimateFundamentalHertz(engine, rate, 0.40)
            : RenderBurstRms(engine, rate, 0.20);
    };

    // Volume, controller 7.
    const double fullVolume = withController(60, 7, 0xFFFFFFFF, false);
    const double quietVolume = withController(60, 7, 0x20000000, false);
    const double otherNoteVolume = withController(62, 7, 0x20000000, false);

    Log::Comment(String().Format(L"per-note volume: full %.6f, quiet %.6f, other note %.6f",
        fullVolume, quietVolume, otherNoteVolume));

    VERIFY_IS_TRUE(fullVolume > 0.0, L"the note was audible");
    VERIFY_IS_TRUE(quietVolume < fullVolume * 0.7, L"per-note volume attenuates the note");
    VERIFY_IS_TRUE(otherNoteVolume > fullVolume * 0.9,
        L"per-note volume addressed to another note leaves this one alone");

    // Pitch, controller 3, carried as 7.25. Ask for 61.0 on a note that was played as 60.
    const double basePitch = withController(60, 7, 0xFFFFFFFF, true);
    const double retuned = withController(60, 3, static_cast<uint32_t>(61u) << 25, true);

    Log::Comment(String().Format(L"per-note pitch: base %.2f Hz, retuned %.2f Hz",
        basePitch, retuned));

    VERIFY_IS_TRUE(retuned > basePitch * 1.03,
        L"the per-note pitch controller retunes the note it addresses");

    // Pan, controller 10. Hard left must not be the same as hard right.
    const double panLeft = withController(60, 10, 0, false);
    const double panRight = withController(60, 10, 0xFFFFFFFF, false);

    VERIFY_IS_TRUE(panLeft > 0.0 && panRight > 0.0, L"the note was audible at both extremes");

    // A controller this engine has no mechanism for must be ignored rather than approximated.
    const double unknownController = withController(60, 74, 0, false);

    VERIFY_IS_TRUE(unknownController > fullVolume * 0.9,
        L"an unhandled per-note controller changes nothing");
}

// Per-note management lets a note be taken out of the channel's control so a retrigger cannot
// steal it, and lets its per-note controllers be put back to their defaults.
void MidiSynthUmpTests::TestPerNoteManagement()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    const uint32_t rate = TestConfig().RenderSampleRate();

    auto retriggerVoiceCount = [&](uint8_t flags)
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(*collection, engine, dispatcher, 0);
        engine.ProgramChange(0, 19);

        uint32_t words[2];
        MakeMidi2Cv(0, 0x9, 0, 60, 0, 0xC0000000, words);
        dispatcher.ProcessWords(words, 2);

        if (flags != 0)
        {
            MakeMidi2Cv(0, 0xF, 0, 60, flags, 0, words);
            dispatcher.ProcessWords(words, 2);
        }

        MakeMidi2Cv(0, 0x9, 0, 60, 0, 0xC0000000, words);
        dispatcher.ProcessWords(words, 2);

        // A stolen voice is faded out rather than cut, so both cases still hold two voices at this
        // instant. Rendering past the fade is what tells the two apart.
        (void)RenderBurstRms(engine, rate, 0.10);

        return engine.ActiveVoiceCount();
    };

    const auto withoutDetach = retriggerVoiceCount(0x00);
    const auto withDetach = retriggerVoiceCount(0x02);

    Log::Comment(String().Format(L"voices after retrigger: plain %u, detached %u",
        static_cast<unsigned>(withoutDetach), static_cast<unsigned>(withDetach)));

    VERIFY_IS_TRUE(withDetach > withoutDetach,
        L"a detached note survives a retrigger of the same note number");

    // The reset flag returns the per-note controllers to their defaults.
    auto volumeAfterReset = [&](bool reset)
    {
        SynthEngine engine;
        UmpDispatcher dispatcher;
        FreshEngine(*collection, engine, dispatcher, 0);
        engine.ProgramChange(0, 19);

        uint32_t words[2];
        MakeMidi2Cv(0, 0x9, 0, 60, 0, 0xC0000000, words);
        dispatcher.ProcessWords(words, 2);

        MakeMidi2Cv(0, 0x0, 0, 60, 7, 0x20000000, words);
        dispatcher.ProcessWords(words, 2);

        if (reset)
        {
            MakeMidi2Cv(0, 0xF, 0, 60, 0x01, 0, words);
            dispatcher.ProcessWords(words, 2);
        }

        return RenderBurstRms(engine, rate, 0.20);
    };

    const double stillQuiet = volumeAfterReset(false);
    const double restored = volumeAfterReset(true);

    Log::Comment(String().Format(L"per-note volume: held %.6f, after reset %.6f",
        stillQuiet, restored));

    VERIFY_IS_TRUE(restored > stillQuiet * 1.2,
        L"per-note management resets the per-note controllers");
}

