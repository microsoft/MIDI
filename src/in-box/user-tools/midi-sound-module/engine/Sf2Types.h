// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <sal.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace SoundFontSynth
{
    // SoundFont 2.04 generator operators, numbered as the file stores them.
    namespace Gen
    {
        constexpr uint16_t StartAddrsOffset = 0;
        constexpr uint16_t EndAddrsOffset = 1;
        constexpr uint16_t StartloopAddrsOffset = 2;
        constexpr uint16_t EndloopAddrsOffset = 3;
        constexpr uint16_t StartAddrsCoarseOffset = 4;
        constexpr uint16_t ModLfoToPitch = 5;
        constexpr uint16_t VibLfoToPitch = 6;
        constexpr uint16_t ModEnvToPitch = 7;
        constexpr uint16_t InitialFilterFc = 8;
        constexpr uint16_t InitialFilterQ = 9;
        constexpr uint16_t ModLfoToFilterFc = 10;
        constexpr uint16_t ModEnvToFilterFc = 11;
        constexpr uint16_t EndAddrsCoarseOffset = 12;
        constexpr uint16_t ModLfoToVolume = 13;
        constexpr uint16_t ChorusEffectsSend = 15;
        constexpr uint16_t ReverbEffectsSend = 16;
        constexpr uint16_t Pan = 17;
        constexpr uint16_t DelayModLfo = 21;
        constexpr uint16_t FreqModLfo = 22;
        constexpr uint16_t DelayVibLfo = 23;
        constexpr uint16_t FreqVibLfo = 24;
        constexpr uint16_t DelayModEnv = 25;
        constexpr uint16_t AttackModEnv = 26;
        constexpr uint16_t HoldModEnv = 27;
        constexpr uint16_t DecayModEnv = 28;
        constexpr uint16_t SustainModEnv = 29;
        constexpr uint16_t ReleaseModEnv = 30;
        constexpr uint16_t KeynumToModEnvHold = 31;
        constexpr uint16_t KeynumToModEnvDecay = 32;
        constexpr uint16_t DelayVolEnv = 33;
        constexpr uint16_t AttackVolEnv = 34;
        constexpr uint16_t HoldVolEnv = 35;
        constexpr uint16_t DecayVolEnv = 36;
        constexpr uint16_t SustainVolEnv = 37;
        constexpr uint16_t ReleaseVolEnv = 38;
        constexpr uint16_t KeynumToVolEnvHold = 39;
        constexpr uint16_t KeynumToVolEnvDecay = 40;
        constexpr uint16_t Instrument = 41;
        constexpr uint16_t KeyRange = 43;
        constexpr uint16_t VelRange = 44;
        constexpr uint16_t StartloopAddrsCoarseOffset = 45;
        constexpr uint16_t Keynum = 46;
        constexpr uint16_t Velocity = 47;
        constexpr uint16_t InitialAttenuation = 48;
        constexpr uint16_t EndloopAddrsCoarseOffset = 50;
        constexpr uint16_t CoarseTune = 51;
        constexpr uint16_t FineTune = 52;
        constexpr uint16_t SampleId = 53;
        constexpr uint16_t SampleModes = 54;
        constexpr uint16_t ScaleTuning = 56;
        constexpr uint16_t ExclusiveClass = 57;
        constexpr uint16_t OverridingRootKey = 58;

        // Not a generator a file may set. The default pitch wheel modulator targets it.
        constexpr uint16_t InitialPitch = 59;

        constexpr uint16_t Count = 60;
    }

    // Values used when no zone sets a generator (SoundFont 2.04 section 8.1.3).
    int16_t GeneratorDefault(_In_ uint16_t generator) noexcept;

    // Whether a preset zone may carry this generator. Sample addressing, key and velocity
    // overrides, loop mode, exclusive class and root key are instrument only.
    bool GeneratorAllowedAtPresetLevel(_In_ uint16_t generator) noexcept;

    // Whether the file may set it at all. Unused and reserved numbers are dropped on load.
    bool GeneratorIsSettable(_In_ uint16_t generator) noexcept;

    struct Sf2Modulator
    {
        uint16_t Source{ 0 };
        uint16_t Destination{ 0 };
        int16_t Amount{ 0 };
        uint16_t AmountSource{ 0 };
        uint16_t Transform{ 0 };

        // Two modulators are the same modulator when these match (SoundFont 2.04 section 9.5.1).
        bool IsSameAs(_In_ Sf2Modulator const& other) const noexcept
        {
            return Source == other.Source &&
                Destination == other.Destination &&
                AmountSource == other.AmountSource &&
                Transform == other.Transform;
        }
    };

    // The most modulators a single zone keeps. A hostile file could declare thousands, and every
    // one of them is evaluated for every voice the zone starts.
    constexpr size_t MaximumModulatorsPerZone = 32;

    struct Sf2Zone
    {
        uint8_t KeyLow{ 0 };
        uint8_t KeyHigh{ 127 };
        uint8_t VelocityLow{ 0 };
        uint8_t VelocityHigh{ 127 };

        std::array<int16_t, Gen::Count> Values{};

        // One bit per generator this zone sets.
        uint64_t SetMask{ 0 };

        std::vector<Sf2Modulator> Modulators{};

        // The instrument a preset zone plays, or the sample an instrument zone plays.
        uint32_t Link{ 0 };

        bool IsSet(_In_ uint16_t generator) const noexcept
        {
            return generator < Gen::Count && (SetMask & (1ull << generator)) != 0;
        }

        bool Matches(_In_ uint8_t key, _In_ uint8_t velocity) const noexcept
        {
            return key >= KeyLow && key <= KeyHigh && velocity >= VelocityLow && velocity <= VelocityHigh;
        }
    };

    struct Sf2Instrument
    {
        std::wstring Name{};

        bool HasGlobalZone{ false };
        Sf2Zone GlobalZone{};

        std::vector<Sf2Zone> Zones{};
    };

    struct Sf2Preset
    {
        std::wstring Name{};

        uint16_t Bank{ 0 };
        uint16_t Program{ 0 };

        bool HasGlobalZone{ false };
        Sf2Zone GlobalZone{};

        std::vector<Sf2Zone> Zones{};
    };

    struct Sf2Sample
    {
        std::wstring Name{};

        // Sample points into the sample data, already checked against its size.
        uint32_t Start{ 0 };
        uint32_t End{ 0 };
        uint32_t LoopStart{ 0 };
        uint32_t LoopEnd{ 0 };

        // False when the loop points in the file do not describe a usable loop.
        bool LoopValid{ false };

        uint32_t SampleRate{ 44100 };
        uint8_t OriginalPitch{ 60 };
        int8_t PitchCorrection{ 0 };

        // False for ROM, compressed and otherwise unplayable samples. Zones that use one are
        // dropped on load, so the engine never sees it.
        bool Usable{ false };
    };

    struct Sf2Info
    {
        uint16_t VersionMajor{ 0 };
        uint16_t VersionMinor{ 0 };

        std::wstring Name{};
        std::wstring SoundEngine{};
        std::wstring Engineers{};
        std::wstring Copyright{};
        std::wstring Comment{};
        std::wstring Software{};
        std::wstring CreationDate{};
        std::wstring Product{};
    };
}
