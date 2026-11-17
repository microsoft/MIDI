// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midicalculator::ui
{
    // Supplies theme brushes to the visuals this app builds in code.
    //
    // A lookup in Application.Resources resolves against the Windows theme, not the theme picked
    // in the app's settings, which lands on the window root. So each brush here is copied from an
    // element that XAML keeps current through {ThemeResource}. Every caller shares one brush per
    // key, and Refresh recolors those brushes in place, so the pages follow a theme change
    // without being rebuilt and without losing what's typed into them.
    class ThemeBrushes
    {
    public:
        static ThemeBrushes& Current() noexcept;

        // Each Border child of the panel publishes one brush, named for the theme resource it carries.
        void Initialize(_In_ controls::Panel const& source) noexcept;

        // The shared brush for a theme resource key.
        media::Brush Get(_In_ wchar_t const* key);

        // Call when the window's actual theme changes.
        void Refresh() noexcept;

    private:
        struct Entry
        {
            controls::Border Source{ nullptr };
            media::SolidColorBrush Brush{ nullptr };
        };

        static void CopyInto(_In_ controls::Border const& source, _In_ media::SolidColorBrush const& target);

        std::unordered_map<std::wstring, Entry> m_entries{};
    };
}
