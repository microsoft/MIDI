// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SequenceProvenance.h"

namespace midisequencer
{
    _Use_decl_annotations_
    std::unordered_set<std::wstring> ClipsInUse(Sequence const& sequence)
    {
        std::unordered_set<std::wstring> used{};

        ForEachTrack(sequence, [&used](Track const& track, size_t)
        {
            for (auto const& placement : track.Timeline)
            {
                used.insert(placement.ClipId);
            }

            for (auto const& slot : track.Slots)
            {
                if (!slot.empty())
                {
                    used.insert(slot);
                }
            }

            return true;
        });

        return used;
    }

    _Use_decl_annotations_
    std::wstring SuggestDigitalSourceType(Sequence const& sequence)
    {
        namespace types = midiapp::DigitalSourceTypes;

        auto const used = ClipsInUse(sequence);

        bool recorded{ false };
        bool drawn{ false };
        bool generated{ false };
        bool imported{ false };
        bool assistant{ false };

        for (auto const& clip : sequence.Clips)
        {
            if (!used.contains(clip.Id))
            {
                continue;
            }

            switch (clip.Origin)
            {
            case ClipOrigin::Recorded: recorded = true; break;
            case ClipOrigin::Drawn: drawn = true; break;
            case ClipOrigin::Generated: generated = true; break;
            case ClipOrigin::Imported: imported = true; break;
            case ClipOrigin::Assistant: assistant = true; break;
            default: break;
            }
        }

        auto const others = static_cast<int>(recorded) + static_cast<int>(drawn) + static_cast<int>(generated) + static_cast<int>(imported);

        if (assistant)
        {
            return others > 0 ? types::CompositeWithTrainedAlgorithmicMedia : types::TrainedAlgorithmicMedia;
        }

        if (others > 1)
        {
            return types::Composite;
        }

        if (recorded)
        {
            return types::DigitalCapture;
        }

        if (generated)
        {
            return types::AlgorithmicMedia;
        }

        return types::DigitalCreation;
    }
}
