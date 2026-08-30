// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midisettings
{
    bool IsProcessElevated() noexcept;

    // Starts a second copy with the runas verb and hands back whether the prompt was accepted.
    // The caller closes this window afterwards.
    bool TryRelaunchElevated() noexcept;
}
