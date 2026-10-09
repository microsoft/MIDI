// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <sal.h>
#include <string>

namespace midipatchbay
{
    // Moves an earlier version's patch folder to `current`, replacing nothing, and returns the folder to use.
    // That's `previous` while it can't be moved, so no patch seems to vanish; the next call tries again.
    std::wstring MoveEarlierPatchFolder(
        _In_ std::wstring const& previous,
        _In_ std::wstring const& current) noexcept;
}
