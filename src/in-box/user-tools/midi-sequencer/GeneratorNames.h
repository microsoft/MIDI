// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The name of each kind of generator, and the line about what it does, as resources, in
// GeneratorType order. The type picker, the New generator menus and About this sequence use them.

#include <array>

#include "SequenceModel.h"

namespace midisequencer
{
    inline constexpr std::array<wchar_t const*, 11> GeneratorTypeKeys{
        L"GeneratorTypeEuclidean", L"GeneratorTypeChanceMelody", L"GeneratorTypeArpeggiator", L"GeneratorTypeLife",
        L"GeneratorTypeLifeDrums", L"GeneratorTypeLearn", L"GeneratorTypeWords", L"GeneratorTypeEvolve",
        L"GeneratorTypeInfinity", L"GeneratorTypeLSystem", L"GeneratorTypeChangeRinging" };

    inline constexpr std::array<wchar_t const*, 11> GeneratorAboutKeys{
        L"GeneratorAboutEuclidean", L"GeneratorAboutMelody", L"GeneratorAboutArpeggiator", L"GeneratorAboutLife",
        L"GeneratorAboutLifeDrums", L"GeneratorAboutLearn", L"GeneratorAboutWords", L"GeneratorAboutEvolve",
        L"GeneratorAboutInfinity", L"GeneratorAboutLSystem", L"GeneratorAboutChangeRinging" };

    // The picker's groups, and the menus' order: rhythms, melodies, then what plays along.
    inline constexpr std::array<GeneratorType, 2> RhythmGenerators{ GeneratorType::Euclidean, GeneratorType::LifeDrums };

    inline constexpr std::array<GeneratorType, 8> MelodyGenerators{
        GeneratorType::ChanceMelody, GeneratorType::Life, GeneratorType::Learn, GeneratorType::Words,
        GeneratorType::Evolve, GeneratorType::Infinity, GeneratorType::LSystem, GeneratorType::ChangeRinging };

    inline constexpr std::array<GeneratorType, 1> PlayAlongGenerators{ GeneratorType::Arpeggiator };

    constexpr wchar_t const* GeneratorTypeKey(GeneratorType type) noexcept
    {
        return GeneratorTypeKeys[static_cast<size_t>(type) < GeneratorTypeKeys.size() ? static_cast<size_t>(type) : 0];
    }

    constexpr wchar_t const* GeneratorAboutKey(GeneratorType type) noexcept
    {
        return GeneratorAboutKeys[static_cast<size_t>(type) < GeneratorAboutKeys.size() ? static_cast<size_t>(type) : 0];
    }

    // What a new clip of each type is called, before a number: "Euclid 2", "Life 3".
    inline constexpr std::array<wchar_t const*, 11> GeneratorClipNameKeys{
        L"DefaultEuclidName", L"DefaultMelodyName", L"DefaultArpName", L"DefaultLifeName", L"DefaultLifeDrumsName",
        L"DefaultLearnName", L"DefaultWordsName", L"DefaultEvolveName", L"DefaultInfinityName", L"DefaultLSystemName",
        L"DefaultRingingName" };

    constexpr wchar_t const* GeneratorClipNameKey(GeneratorType type) noexcept
    {
        return GeneratorClipNameKeys[static_cast<size_t>(type) < GeneratorClipNameKeys.size() ? static_cast<size_t>(type) : 0];
    }

    // The melodies that need room to unfold start two bars long, like comp 4's chance melody.
    constexpr bool StartsTwoBarsLong(GeneratorType type) noexcept
    {
        return type == GeneratorType::ChanceMelody || type == GeneratorType::Learn;
    }
}
