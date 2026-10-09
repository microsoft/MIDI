// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Writes small SoundFont 2 files in memory, so the tests need no bank on disk and can damage any
// field they like.

#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace Sf2Test
{
    struct Generator
    {
        uint16_t Operator{ 0 };
        uint16_t Amount{ 0 };
    };

    inline Generator Range(uint16_t op, uint8_t low, uint8_t high)
    {
        return { op, static_cast<uint16_t>(low | (high << 8)) };
    }

    inline Generator Signed(uint16_t op, int16_t value)
    {
        return { op, static_cast<uint16_t>(value) };
    }

    struct Modulator
    {
        uint16_t Source{ 0 };
        uint16_t Destination{ 0 };
        int16_t Amount{ 0 };
        uint16_t AmountSource{ 0 };
        uint16_t Transform{ 0 };
    };

    struct Zone
    {
        std::vector<Generator> Generators;
        std::vector<Modulator> Modulators;
    };

    struct Sample
    {
        std::string Name;
        std::vector<int16_t> Data;
        uint32_t LoopStart{ 0 };
        uint32_t LoopEnd{ 0 };
        uint32_t SampleRate{ 44100 };
        uint8_t OriginalPitch{ 60 };
        int8_t PitchCorrection{ 0 };
        uint16_t SampleLink{ 0 };
        uint16_t SampleType{ 1 };
    };

    struct Instrument
    {
        std::string Name;
        std::vector<Zone> Zones;
    };

    struct Preset
    {
        std::string Name;
        uint16_t Program{ 0 };
        uint16_t Bank{ 0 };
        std::vector<Zone> Zones;
    };

    struct Font
    {
        std::string Name{ "Test Font" };
        uint16_t VersionMajor{ 2 };
        uint16_t VersionMinor{ 4 };
        std::vector<Sample> Samples;
        std::vector<Instrument> Instruments;
        std::vector<Preset> Presets;
    };

    // Generator numbers from the SoundFont 2.04 specification, section 8.1.2.
    namespace Op
    {
        constexpr uint16_t StartAddressOffset = 0;
        constexpr uint16_t Pan = 17;
        constexpr uint16_t AttackVolumeEnvelope = 34;
        constexpr uint16_t HoldVolumeEnvelope = 35;
        constexpr uint16_t DecayVolumeEnvelope = 36;
        constexpr uint16_t SustainVolumeEnvelope = 37;
        constexpr uint16_t ReleaseVolumeEnvelope = 38;
        constexpr uint16_t Instrument = 41;
        constexpr uint16_t KeyRange = 43;
        constexpr uint16_t VelocityRange = 44;
        constexpr uint16_t InitialAttenuation = 48;
        constexpr uint16_t CoarseTune = 51;
        constexpr uint16_t FineTune = 52;
        constexpr uint16_t SampleId = 53;
        constexpr uint16_t SampleModes = 54;
        constexpr uint16_t ScaleTuning = 56;
        constexpr uint16_t ExclusiveClass = 57;
        constexpr uint16_t OverridingRootKey = 58;
    }

    class Writer
    {
    public:
        std::vector<uint8_t> Bytes;

        void U8(uint8_t value) { Bytes.push_back(value); }
        void U16(uint16_t value) { U8(static_cast<uint8_t>(value & 0xFF)); U8(static_cast<uint8_t>(value >> 8)); }
        void U32(uint32_t value) { U16(static_cast<uint16_t>(value & 0xFFFF)); U16(static_cast<uint16_t>(value >> 16)); }

        void FourCc(char const* code)
        {
            for (int i = 0; i < 4; i++)
            {
                U8(static_cast<uint8_t>(code[i]));
            }
        }

        void Name(std::string const& name, size_t length)
        {
            for (size_t i = 0; i < length; i++)
            {
                U8((i < name.size()) ? static_cast<uint8_t>(name[i]) : 0);
            }
        }

        size_t BeginChunk(char const* id)
        {
            FourCc(id);
            auto const sizeOffset = Bytes.size();
            U32(0);
            return sizeOffset;
        }

        void EndChunk(size_t sizeOffset)
        {
            auto const size = static_cast<uint32_t>(Bytes.size() - sizeOffset - 4);

            Bytes[sizeOffset] = static_cast<uint8_t>(size & 0xFF);
            Bytes[sizeOffset + 1] = static_cast<uint8_t>((size >> 8) & 0xFF);
            Bytes[sizeOffset + 2] = static_cast<uint8_t>((size >> 16) & 0xFF);
            Bytes[sizeOffset + 3] = static_cast<uint8_t>((size >> 24) & 0xFF);

            if ((size & 1) != 0)
            {
                U8(0);
            }
        }

        size_t BeginList(char const* id, char const* type)
        {
            auto const sizeOffset = BeginChunk(id);
            FourCc(type);
            return sizeOffset;
        }

        void ZoneBag(std::vector<Zone> const& zones, char const* bagId, uint16_t& generatorIndex, uint16_t& modulatorIndex)
        {
            auto const chunk = BeginChunk(bagId);

            for (auto const& zone : zones)
            {
                U16(generatorIndex);
                U16(modulatorIndex);
                generatorIndex = static_cast<uint16_t>(generatorIndex + zone.Generators.size());
                modulatorIndex = static_cast<uint16_t>(modulatorIndex + zone.Modulators.size());
            }

            U16(generatorIndex);
            U16(modulatorIndex);
            EndChunk(chunk);
        }
    };

    template <typename Owner>
    inline std::vector<Zone> AllZones(std::vector<Owner> const& owners)
    {
        std::vector<Zone> zones;

        for (auto const& owner : owners)
        {
            zones.insert(zones.end(), owner.Zones.begin(), owner.Zones.end());
        }

        return zones;
    }

    inline void WriteModulators(Writer& writer, char const* id, std::vector<Zone> const& zones)
    {
        auto const chunk = writer.BeginChunk(id);

        for (auto const& zone : zones)
        {
            for (auto const& modulator : zone.Modulators)
            {
                writer.U16(modulator.Source);
                writer.U16(modulator.Destination);
                writer.U16(static_cast<uint16_t>(modulator.Amount));
                writer.U16(modulator.AmountSource);
                writer.U16(modulator.Transform);
            }
        }

        for (int i = 0; i < 5; i++)
        {
            writer.U16(0);
        }

        writer.EndChunk(chunk);
    }

    inline void WriteGenerators(Writer& writer, char const* id, std::vector<Zone> const& zones)
    {
        auto const chunk = writer.BeginChunk(id);

        for (auto const& zone : zones)
        {
            for (auto const& generator : zone.Generators)
            {
                writer.U16(generator.Operator);
                writer.U16(generator.Amount);
            }
        }

        writer.U32(0);
        writer.EndChunk(chunk);
    }

    inline std::vector<uint8_t> Build(Font const& font)
    {
        Writer writer;

        auto const riff = writer.BeginList("RIFF", "sfbk");

        {
            auto const info = writer.BeginList("LIST", "INFO");

            auto const ifil = writer.BeginChunk("ifil");
            writer.U16(font.VersionMajor);
            writer.U16(font.VersionMinor);
            writer.EndChunk(ifil);

            auto const isng = writer.BeginChunk("isng");
            writer.Name("EMU8000", 8);
            writer.EndChunk(isng);

            auto const inam = writer.BeginChunk("INAM");
            writer.Name(font.Name, font.Name.size() + 1 + ((font.Name.size() + 1) & 1));
            writer.EndChunk(inam);

            writer.EndChunk(info);
        }

        std::vector<uint32_t> sampleStarts;

        {
            auto const sdta = writer.BeginList("LIST", "sdta");
            auto const smpl = writer.BeginChunk("smpl");

            uint32_t position{ 0 };

            for (auto const& sample : font.Samples)
            {
                sampleStarts.push_back(position);

                for (auto const value : sample.Data)
                {
                    writer.U16(static_cast<uint16_t>(value));
                }

                // The specification asks for 46 zero points after every sample.
                for (int i = 0; i < 46; i++)
                {
                    writer.U16(0);
                }

                position += static_cast<uint32_t>(sample.Data.size()) + 46;
            }

            writer.EndChunk(smpl);
            writer.EndChunk(sdta);
        }

        auto const pdta = writer.BeginList("LIST", "pdta");

        {
            auto const phdr = writer.BeginChunk("phdr");
            uint16_t bag{ 0 };

            for (auto const& preset : font.Presets)
            {
                writer.Name(preset.Name, 20);
                writer.U16(preset.Program);
                writer.U16(preset.Bank);
                writer.U16(bag);
                writer.U32(0);
                writer.U32(0);
                writer.U32(0);
                bag = static_cast<uint16_t>(bag + preset.Zones.size());
            }

            writer.Name("EOP", 20);
            writer.U16(0);
            writer.U16(0);
            writer.U16(bag);
            writer.U32(0);
            writer.U32(0);
            writer.U32(0);
            writer.EndChunk(phdr);
        }

        auto const presetZones = AllZones(font.Presets);

        {
            uint16_t generators{ 0 };
            uint16_t modulators{ 0 };
            writer.ZoneBag(presetZones, "pbag", generators, modulators);
        }

        WriteModulators(writer, "pmod", presetZones);
        WriteGenerators(writer, "pgen", presetZones);

        {
            auto const inst = writer.BeginChunk("inst");
            uint16_t bag{ 0 };

            for (auto const& instrument : font.Instruments)
            {
                writer.Name(instrument.Name, 20);
                writer.U16(bag);
                bag = static_cast<uint16_t>(bag + instrument.Zones.size());
            }

            writer.Name("EOI", 20);
            writer.U16(bag);
            writer.EndChunk(inst);
        }

        auto const instrumentZones = AllZones(font.Instruments);

        {
            uint16_t generators{ 0 };
            uint16_t modulators{ 0 };
            writer.ZoneBag(instrumentZones, "ibag", generators, modulators);
        }

        WriteModulators(writer, "imod", instrumentZones);
        WriteGenerators(writer, "igen", instrumentZones);

        {
            auto const shdr = writer.BeginChunk("shdr");

            for (size_t i = 0; i < font.Samples.size(); i++)
            {
                auto const& sample = font.Samples[i];
                auto const start = sampleStarts[i];

                writer.Name(sample.Name, 20);
                writer.U32(start);
                writer.U32(start + static_cast<uint32_t>(sample.Data.size()));
                writer.U32(start + sample.LoopStart);
                writer.U32(start + sample.LoopEnd);
                writer.U32(sample.SampleRate);
                writer.U8(sample.OriginalPitch);
                writer.U8(static_cast<uint8_t>(sample.PitchCorrection));
                writer.U16(sample.SampleLink);
                writer.U16(sample.SampleType);
            }

            writer.Name("EOS", 20);
            for (int i = 0; i < 26; i++)
            {
                writer.U8(0);
            }

            writer.EndChunk(shdr);
        }

        writer.EndChunk(pdta);
        writer.EndChunk(riff);

        return writer.Bytes;
    }

    // One period per hundred points at 44 kHz is exactly 440 Hz, so with a root key of 69 the
    // sample plays at its own pitch on A4.
    inline Sample MakeSine(std::string const& name, uint32_t periods = 200, int16_t peak = 16000)
    {
        Sample sample{};

        sample.Name = name;
        sample.SampleRate = 44000;
        sample.OriginalPitch = 69;

        constexpr uint32_t PointsPerPeriod = 100;

        sample.Data.resize(static_cast<size_t>(periods) * PointsPerPeriod);

        for (size_t i = 0; i < sample.Data.size(); i++)
        {
            sample.Data[i] = static_cast<int16_t>(std::lround(peak * std::sin(2.0 * 3.14159265358979323846 * static_cast<double>(i) / PointsPerPeriod)));
        }

        // A whole number of periods, so the loop is seamless.
        sample.LoopStart = PointsPerPeriod * 10;
        sample.LoopEnd = static_cast<uint32_t>(sample.Data.size()) - PointsPerPeriod * 10;

        return sample;
    }

    // One melodic preset (bank 0, program 0) and one drum kit (bank 128, program 0), both playing
    // a looped sine across the whole keyboard.
    inline Font MakeBasicFont()
    {
        Font font{};

        font.Samples.push_back(MakeSine("Sine"));

        Instrument melodic{};
        melodic.Name = "Sine Instrument";

        Zone melodicZone{};
        melodicZone.Generators.push_back(Range(Op::KeyRange, 0, 127));
        melodicZone.Generators.push_back(Signed(Op::ReleaseVolumeEnvelope, -1200));    // 0.5 s
        melodicZone.Generators.push_back({ Op::SampleModes, 1 });
        melodicZone.Generators.push_back({ Op::SampleId, 0 });
        melodic.Zones.push_back(melodicZone);

        Instrument drum{};
        drum.Name = "Sine Drum";

        Zone drumZone{};
        drumZone.Generators.push_back(Range(Op::KeyRange, 35, 81));
        drumZone.Generators.push_back({ Op::ExclusiveClass, 1 });
        drumZone.Generators.push_back({ Op::SampleModes, 1 });
        drumZone.Generators.push_back({ Op::SampleId, 0 });
        drum.Zones.push_back(drumZone);

        font.Instruments.push_back(melodic);
        font.Instruments.push_back(drum);

        Preset piano{};
        piano.Name = "Sine Piano";
        piano.Program = 0;
        piano.Bank = 0;

        Zone pianoZone{};
        pianoZone.Generators.push_back({ Op::Instrument, 0 });
        piano.Zones.push_back(pianoZone);

        Preset kit{};
        kit.Name = "Sine Kit";
        kit.Program = 0;
        kit.Bank = 128;

        Zone kitZone{};
        kitZone.Generators.push_back({ Op::Instrument, 1 });
        kit.Zones.push_back(kitZone);

        font.Presets.push_back(piano);
        font.Presets.push_back(kit);

        return font;
    }
}
