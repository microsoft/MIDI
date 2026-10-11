// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// How a sequence was made, worked out from how each of its clips began (design section 13). No
// XAML here, so the tests compile it unchanged.

#include <sal.h>

#include <string>
#include <unordered_set>

#include "SequenceModel.h"

namespace midisequencer
{
    // The ids of the clips the timeline or the launcher uses.
    std::unordered_set<std::wstring> ClipsInUse(_In_ Sequence const& sequence);

    // The IPTC digital source type the clips in use suggest for the whole sequence. One kind of
    // clip suggests that kind, and a mix suggests composite. Any clip an AI assistant wrote makes
    // it trainedAlgorithmicMedia, or compositeWithTrainedAlgorithmicMedia when other clips are
    // there too. Imported clips count toward a mix, but alone they say nothing about how the file
    // was made, so they leave the default, digitalCreation.
    std::wstring SuggestDigitalSourceType(_In_ Sequence const& sequence);
}
