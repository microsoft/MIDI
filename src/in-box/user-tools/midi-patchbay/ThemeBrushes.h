// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midipatchbay
{
    // Supplies theme brushes to the visuals this app builds in code.
    //
    // A lookup in Application.Resources resolves against the application theme, so it hands back
    // dark brushes while the window is showing light: the theme override lands on the window root,
    // not on the application. These brushes come from elements that XAML itself keeps current
    // through {ThemeResource}, so they always match the theme actually in effect.
    class ThemeBrushes
    {
    public:
        static ThemeBrushes& Current() noexcept;

        // Each child of the panel publishes one brush, named for the theme resource it carries.
        void Initialize(_In_ controls::Panel const& source) noexcept;

        // Null when nothing publishes that name, so a caller can fall back to a literal color.
        media::Brush Get(_In_ std::wstring_view name) const noexcept;

    private:
        std::vector<std::pair<std::wstring, controls::Border>> m_sources{};
    };
}
