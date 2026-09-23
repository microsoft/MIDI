// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiAppSettings.h"

namespace midiglass
{
    // Appearance and window placement live in the shared base, but stay reachable through the
    // app's own namespace so call sites read the same as in every other tool.
    using midiapp::AppTheme;
    using midiapp::WindowBackdrop;
    using midiapp::WindowPlacementInfo;

    class AppSettings : public midiapp::MidiAppSettings
    {
    public:
        static AppSettings& Current() noexcept;

        void Load() noexcept;

    private:
        AppSettings() noexcept;
    };
}
