// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midiclock
{
    // False when nobody can see any of the window: minimized or hidden, on another virtual
    // desktop, off every monitor or covered by other windows, or the session is locked or its
    // display is off. A window that is partly covered, or just not the active one, still counts.
    bool IsWindowOnScreen(_In_ HWND window) noexcept;

    // The display state arrives as a power notification, so it has to be asked for. Start once,
    // and stop before the process ends.
    void StartWatchingDisplayPower() noexcept;
    void StopWatchingDisplayPower() noexcept;
}
