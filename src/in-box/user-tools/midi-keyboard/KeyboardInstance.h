// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <sal.h>

#include <cstdint>

#include "CommandLineOptions.h"

namespace midikeyboard
{
    // Decides how many keyboard windows a launch ends up with, and where a new one goes.
    //
    // Opening this app twice by accident puts two windows on the same loopback, one exactly on
    // top of the other, and both send their MIDI-CI SysEx at the same moment. So a plain launch
    // brings the window that is already open forward instead. A launch that names a destination,
    // which is how MIDI Sequencer and MIDI Patchbay open one keyboard per track, always gets a
    // window of its own.
    class KeyboardInstance
    {
    public:
        // Call before any UI exists. False means an existing window was brought forward and this
        // process should exit.
        static bool AcquireOrActivateExisting(_In_ CommandLineOptions const& options) noexcept;

        // 0 for the first keyboard window open in this sign-in, 1 for the next and so on. A window
        // other than the first is placed away from the saved position so it doesn't cover the
        // first one, and doesn't save its position on close.
        static uint32_t WindowSlot() noexcept;

        // Lets the next launch find this window to bring forward.
        static void PublishMainWindow(_In_ HWND const window) noexcept;

        static void Release() noexcept;

    private:
        static void ClaimWindowSlot() noexcept;
    };
}
