// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midiapp
{
    // "Layout cycle detected. Layout could not complete." XAML gives up on layout for every window
    // on the thread and never picks it up again: windows stop taking the mouse, and a window opened
    // after that never draws.
    inline constexpr HRESULT XamlLayoutCycle = static_cast<HRESULT>(0x802B0014L);

    // An error that leaves the app unusable even when Application.UnhandledException marks it
    // handled. A tool lets these crash instead, so the crash dump shows the moment it went wrong
    // rather than a window that silently stopped working.
    inline bool IsUnrecoverableXamlError(_In_ HRESULT hr) noexcept
    {
        return hr == XamlLayoutCycle;
    }
}
