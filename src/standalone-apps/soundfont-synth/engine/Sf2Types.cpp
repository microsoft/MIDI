// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "Sf2Types.h"

namespace SoundFontSynth
{
    _Use_decl_annotations_
    int16_t GeneratorDefault(uint16_t generator) noexcept
    {
        switch (generator)
        {
        case Gen::InitialFilterFc:
            return 13500;

        case Gen::DelayModLfo:
        case Gen::DelayVibLfo:
        case Gen::DelayModEnv:
        case Gen::AttackModEnv:
        case Gen::HoldModEnv:
        case Gen::DecayModEnv:
        case Gen::ReleaseModEnv:
        case Gen::DelayVolEnv:
        case Gen::AttackVolEnv:
        case Gen::HoldVolEnv:
        case Gen::DecayVolEnv:
        case Gen::ReleaseVolEnv:
            return -12000;

        case Gen::Keynum:
        case Gen::Velocity:
        case Gen::OverridingRootKey:
            return -1;

        case Gen::ScaleTuning:
            return 100;

        default:
            return 0;
        }
    }

    _Use_decl_annotations_
    bool GeneratorAllowedAtPresetLevel(uint16_t generator) noexcept
    {
        switch (generator)
        {
        case Gen::StartAddrsOffset:
        case Gen::EndAddrsOffset:
        case Gen::StartloopAddrsOffset:
        case Gen::EndloopAddrsOffset:
        case Gen::StartAddrsCoarseOffset:
        case Gen::EndAddrsCoarseOffset:
        case Gen::StartloopAddrsCoarseOffset:
        case Gen::EndloopAddrsCoarseOffset:
        case Gen::Keynum:
        case Gen::Velocity:
        case Gen::SampleModes:
        case Gen::ExclusiveClass:
        case Gen::OverridingRootKey:
        case Gen::SampleId:
            return false;

        default:
            return GeneratorIsSettable(generator);
        }
    }

    _Use_decl_annotations_
    bool GeneratorIsSettable(uint16_t generator) noexcept
    {
        switch (generator)
        {
        case 14:
        case 18:
        case 19:
        case 20:
        case 42:
        case 49:
        case 55:
        case Gen::InitialPitch:
            return false;

        default:
            return generator < Gen::Count;
        }
    }
}
