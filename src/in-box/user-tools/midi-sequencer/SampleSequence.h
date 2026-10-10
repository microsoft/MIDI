// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The sample sequence: a short piece with pinned rows, folders, the launcher, linked clips and
// tags, so a person can hear and see what the app does before making anything. It plays to one
// synth, the General MIDI Synth on most PCs, with each part on its own channel.
//
// Free of pch.h and XAML so the unit tests build it. Every name comes from the caller, which
// takes them from the app's resources.

#include <sal.h>

#include <string>

#include "SequenceModel.h"

namespace midisequencer
{
    struct SampleText
    {
        std::wstring SequenceName{};

        std::wstring SceneIntro{};
        std::wstring SceneGroove{};
        std::wstring SceneBreak{};
        std::wstring SceneDrop{};
        std::wstring SceneOutro{};

        std::wstring Drums{};
        std::wstring Synths{};
        std::wstring Bass{};
        std::wstring Pad{};
        std::wstring Arp{};
        std::wstring Texture{};
        std::wstring Keys{};
        std::wstring Rhodes{};
        std::wstring Organ{};
        std::wstring Lead{};
        std::wstring Chords{};

        std::wstring BeatA{};
        std::wstring BeatB{};
        std::wstring BeatFill{};
        std::wstring BassLineA{};
        std::wstring BassLineB{};
        std::wstring PadIntro{};
        std::wstring PadChords{};
        std::wstring PadSwell{};
        std::wstring EuclidFive{};
        std::wstring EuclidSeven{};
        std::wstring Wander{};
        std::wstring Drift{};
        std::wstring Comping{};
        std::wstring OrganPads{};
        std::wstring LeadHook{};
        std::wstring Stabs{};

        std::wstring TagFilterOpens{};
        std::wstring TagRetake{};
    };

    // Every track plays to the given endpoint. An empty endpoint leaves the tracks without one.
    Sequence MakeSampleSequence(_In_ SampleText const& text, _In_ EndpointRef const& synth);
}
