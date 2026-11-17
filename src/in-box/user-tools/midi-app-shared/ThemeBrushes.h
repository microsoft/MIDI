// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midiapp
{
    // Supplies theme brushes to the visuals a tool builds in code.
    //
    // A lookup in Application.Resources resolves against the Windows theme, not the theme picked
    // in the tool's Appearance settings, which lands on the window root. So each brush here is
    // copied from an element that XAML keeps current through {ThemeResource}: a zero sized panel
    // in the window's XAML holds one Border per key, named for the key, with its Background set
    // to that theme resource. Every caller shares one brush per key, and Refresh recolors those
    // brushes in place, so what's already built follows a theme change without being rebuilt.
    class ThemeBrushes
    {
    public:
        static ThemeBrushes& Current() noexcept;

        // Each Border child of the panel publishes one brush, named for the theme resource it carries.
        void Initialize(_In_ winrt::Microsoft::UI::Xaml::Controls::Panel const& source) noexcept;

        // The shared brush for a theme resource key.
        winrt::Microsoft::UI::Xaml::Media::Brush Get(_In_ wchar_t const* key);

        // Call when the window's actual theme changes.
        void Refresh() noexcept;

    private:
        struct Entry
        {
            winrt::Microsoft::UI::Xaml::Controls::Border Source{ nullptr };
            winrt::Microsoft::UI::Xaml::Media::SolidColorBrush Brush{ nullptr };
        };

        static void CopyInto(
            _In_ winrt::Microsoft::UI::Xaml::Controls::Border const& source,
            _In_ winrt::Microsoft::UI::Xaml::Media::SolidColorBrush const& target);

        std::unordered_map<std::wstring, Entry> m_entries{};
    };
}