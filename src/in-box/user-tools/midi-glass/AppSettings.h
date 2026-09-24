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

    // How the library orders what it shows. Last used first is the default, because the thing
    // somebody wants is nearly always the thing they had open yesterday.
    enum class LibrarySort : int32_t
    {
        LastUsed = 0,
        Name = 1,
        LastChanged = 2,
    };

    class AppSettings : public midiapp::MidiAppSettings
    {
    public:
        static AppSettings& Current() noexcept;

        void Load() noexcept;

        LibrarySort LibrarySortOrder() const noexcept { return m_librarySort; }
        void LibrarySortOrder(_In_ LibrarySort value) noexcept;

        bool LibraryShowsList() const noexcept { return m_libraryShowsList; }
        void LibraryShowsList(_In_ bool value) noexcept;

        // When a layout was last opened, as a FILETIME, or 0. Kept here rather than in the layout
        // file because "when did I last use this" is about this PC, and because writing the file
        // every time it opened would make every run look like an edit.
        int64_t LayoutLastUsed(_In_ std::wstring const& layoutFilePath) const noexcept;
        void RecordLayoutUse(_In_ std::wstring const& layoutFilePath) noexcept;

    private:
        AppSettings() noexcept;

        void LoadRecentLayouts() noexcept;
        void SaveRecentLayouts() const noexcept;

        // Bounded on purpose. A list nobody can see the end of is a list nobody can fix.
        static constexpr size_t MaximumRecentLayouts = 64;

        LibrarySort m_librarySort{ LibrarySort::LastUsed };
        bool m_libraryShowsList{ false };

        std::vector<std::pair<std::wstring, int64_t>> m_recentLayouts{};
    };
}
