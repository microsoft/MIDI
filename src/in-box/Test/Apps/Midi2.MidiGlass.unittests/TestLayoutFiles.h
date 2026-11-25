// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <string>

namespace glasstests
{
    // A layout written by hand rather than by the app: keys in a different order from the one the
    // writer uses, odd whitespace, and fields from a version that does not exist. This is what a
    // file shared on a forum actually looks like, and it is the input the round trip has to
    // survive.
    std::wstring HandAuthoredLayout();

    // The same file, claiming a file version from the future and carrying a whole object this
    // build knows nothing about.
    std::wstring LayoutFromANewerVersion();

    // Deliberately hostile: strings past every limit, counts past every cap, a system exclusive
    // blob that is not hex, numbers out of range and a value of the wrong type in every slot.
    std::wstring HostileLayout();
}
