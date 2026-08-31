// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiAppSettings.h"

namespace midibluetoothsetup
{
    // appearance and window placement live in the shared base, but stay reachable through the
    // app's own namespace so call sites read the same
    using midiapp::AppTheme;
    using midiapp::WindowBackdrop;
    using midiapp::WindowPlacementInfo;

    class AppSettings : public midiapp::MidiAppSettings
    {
    public:
        static AppSettings& Current() noexcept;

        void Load() noexcept;

        // how often the pages ask the service for connection state and statistics
        uint32_t RefreshIntervalSeconds() const noexcept { return m_refreshIntervalSeconds; }
        void RefreshIntervalSeconds(uint32_t value) noexcept;

        // the page the window opens on, so the tool comes back where it was left
        uint32_t SelectedPageIndex() const noexcept { return m_selectedPageIndex; }
        void SelectedPageIndex(uint32_t value) noexcept;

        static constexpr uint32_t MinimumRefreshIntervalSeconds = 1;
        static constexpr uint32_t MaximumRefreshIntervalSeconds = 60;
        static constexpr uint32_t DefaultRefreshIntervalSeconds = 2;

        static constexpr uint32_t PageIndexDevices = 0;
        static constexpr uint32_t PageIndexThisPc = 1;
        static constexpr uint32_t PageIndexSettings = 2;

    private:
        AppSettings() noexcept;

        uint32_t m_refreshIntervalSeconds{ DefaultRefreshIntervalSeconds };
        uint32_t m_selectedPageIndex{ PageIndexDevices };
    };
}
