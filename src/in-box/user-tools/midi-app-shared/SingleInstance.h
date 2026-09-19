// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <windows.h>
#include <string>

namespace midiapp
{
    // Keeps a tool to one copy per signed in customer.
    //
    // The names are in the Local namespace, so this is per session rather than per machine. Two
    // people signed in at once each get their own window; a Global name would give the second
    // one nothing and no explanation.
    //
    // The mutex only answers whether anyone else is here. Finding their window needs a second
    // step, because every WinUI desktop window shares one class name and there is nothing in a
    // mutex to find a window with. The first instance publishes its window handle into a small
    // named section, and later ones read it from there.
    class SingleInstance
    {
    public:
        // Call before creating any UI. False means another copy is already running and has been
        // brought forward, and this process should exit without doing anything else.
        static bool AcquireOrActivateExisting(_In_ std::wstring const& appKey) noexcept;

        // Called once the main window exists, so the next launch has something to activate.
        // Doing nothing when this instance did not acquire ownership is deliberate.
        static void PublishMainWindow(_In_ HWND const window) noexcept;

        // The running instance's window, or null. A tool which opens documents needs this so
        // that a second launch can hand its file over rather than losing it.
        static HWND FindExistingWindow(_In_ std::wstring const& appKey) noexcept;

        // Whether a tool is running, asked from a process which is not that tool. Only the mutex
        // is consulted, so this also answers for a tool which has no window at all.
        static bool IsRunning(_In_ std::wstring const& appKey) noexcept;

        static void Release() noexcept;

    private:
        static bool TryActivateExisting(_In_ std::wstring const& appKey) noexcept;

        static std::wstring MutexName(_In_ std::wstring const& appKey) noexcept;
        static std::wstring SectionName(_In_ std::wstring const& appKey) noexcept;

        static HANDLE s_instanceMutex;
        static HANDLE s_windowSection;
        static void* s_windowView;
    };
}
