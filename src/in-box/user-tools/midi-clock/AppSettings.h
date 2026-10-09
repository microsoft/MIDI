// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiAppSettings.h"

namespace midiclock
{
    // appearance and window placement live in the shared base, but stay reachable through the
    // app's own namespace so call sites read the same
    using midiapp::AppTheme;
    using midiapp::WindowBackdrop;
    using midiapp::WindowPlacementInfo;

    // The clocks themselves are machine wide and live in the configuration folder, not here.
    // See ClockStore. This is only the per-user appearance of the window.
    class AppSettings : public midiapp::MidiAppSettings
    {
    public:
        static AppSettings& Current() noexcept;

        void Load() noexcept;

        // Off by default: redrawing the time while a clock runs costs CPU the clock does not need.
        bool ShowRunningTimeCode() const noexcept { return m_showRunningTimeCode; }
        void ShowRunningTimeCode(_In_ bool value) noexcept;

    private:
        AppSettings() noexcept;

        bool m_showRunningTimeCode{ false };
    };
}
