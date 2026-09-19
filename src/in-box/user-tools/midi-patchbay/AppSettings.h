// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiAppSettings.h"

namespace midipatchbay
{
    using midiapp::AppTheme;
    using midiapp::WindowBackdrop;
    using midiapp::WindowPlacementInfo;

    enum class PatchSortOrder
    {
        Newest = 0,
        Name = 1,
    };

    // Window appearance comes from the shared base. The rest is this app's own behavior, and it
    // matters more here than in the other tools: routing only exists while this process runs, so
    // whether it starts with Windows and whether closing the window really closes it are the two
    // settings a customer is most likely to change.
    class AppSettings : public midiapp::MidiAppSettings
    {
    public:
        static AppSettings& Current() noexcept;

        void Load() noexcept;

        PatchSortOrder SortOrder() const noexcept { return m_sortOrder; }
        void SortOrder(_In_ PatchSortOrder value) noexcept;

        // Deliberately off by default. A tool that quietly lives in the notification area without
        // the customer asking for it is not welcome, so this only turns on when they say so.
        bool MinimizeToNotificationArea() const noexcept { return m_minimizeToNotificationArea; }
        void MinimizeToNotificationArea(_In_ bool value) noexcept;

        bool StartMinimized() const noexcept { return m_startMinimized; }
        void StartMinimized(_In_ bool value) noexcept;

        bool WarnAboutLoops() const noexcept { return m_warnAboutLoops; }
        void WarnAboutLoops(_In_ bool value) noexcept;

        bool ActivateSavedPatchesAtStartup() const noexcept { return m_activateSavedPatchesAtStartup; }
        void ActivateSavedPatchesAtStartup(_In_ bool value) noexcept;

        // The per-user Run entry. Reads and writes HKCU directly rather than caching, because the
        // customer can change it outside the app.
        static bool StartsWithWindows() noexcept;
        static bool TrySetStartsWithWindows(_In_ bool value) noexcept;

    private:
        AppSettings() noexcept;

        PatchSortOrder m_sortOrder{ PatchSortOrder::Newest };
        bool m_minimizeToNotificationArea{ false };
        bool m_startMinimized{ false };
        bool m_warnAboutLoops{ true };
        bool m_activateSavedPatchesAtStartup{ true };
    };
}
