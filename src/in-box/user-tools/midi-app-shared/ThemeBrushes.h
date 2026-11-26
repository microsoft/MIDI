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
    // in the tool's Appearance settings, which lands on each window's root. So each brush here is
    // copied from an element that XAML keeps current through {ThemeResource}: a zero sized panel
    // in each window's XAML holds one Border per key, named for the key, with its Background set
    // to that theme resource. Every caller shares one brush per key, and Refresh recolors those
    // brushes in place, so what's already built follows a theme change without being rebuilt.
    //
    // The brushes are shared by all of a tool's windows, so a theme change has to reach every
    // open window, as the Appearance setting is the tool's rather than one window's.
    class ThemeBrushes
    {
    public:
        static ThemeBrushes& Current() noexcept;

        // A window's brush source panel. Add it before WindowChrome::Initialize applies the saved
        // theme, and remove it when the window closes. The newest source still added supplies the
        // colors. Removing the last one lets go of every brush, so nothing XAML owns is left for
        // a static destructor to release after XAML has shut down.
        void AddSource(_In_ winrt::Microsoft::UI::Xaml::Controls::Panel const& source) noexcept;
        void RemoveSource(_In_ winrt::Microsoft::UI::Xaml::Controls::Panel const& source) noexcept;

        // The shared brush for a theme resource key.
        winrt::Microsoft::UI::Xaml::Media::Brush Get(_In_ wchar_t const* key);

        // Call from each window's ActualThemeChanged.
        void Refresh() noexcept;

    private:
        winrt::Microsoft::UI::Xaml::Controls::Border FindSource(_In_ std::wstring_view key) const;

        static void CopyInto(
            _In_ winrt::Microsoft::UI::Xaml::Controls::Border const& source,
            _In_ winrt::Microsoft::UI::Xaml::Media::SolidColorBrush const& target);

        std::vector<winrt::Microsoft::UI::Xaml::Controls::Panel> m_sources{};
        std::unordered_map<std::wstring, winrt::Microsoft::UI::Xaml::Media::SolidColorBrush> m_brushes{};
    };
}