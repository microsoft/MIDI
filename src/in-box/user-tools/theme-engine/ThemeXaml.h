// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// A theme's "resources" in a WinUI 3 app: the brushes XAML takes with {ThemeResource}. Like the
// files in midi-app-shared, this includes the app's own pch.h, which has to bring in the WinUI
// headers (Microsoft.UI.Xaml and Microsoft.UI.Xaml.Media).

#include "ThemeResolver.h"

namespace themeengine
{
    winrt::Windows::UI::Color ToWindowsColor(_In_ Color const& color) noexcept;
    Color FromWindowsColor(_In_ winrt::Windows::UI::Color const& color) noexcept;

    // Where a CSS linear-gradient() angle starts and ends in a box, as XAML's relative points.
    std::pair<winrt::Windows::Foundation::Point, winrt::Windows::Foundation::Point> GradientPoints(_In_ double cssAngle) noexcept;

    // Sets the brushes a theme names in a resource dictionary's theme dictionaries: the light look's
    // in "Light", the dark look's in "Default" and "Dark". A brush that's already there is recolored
    // where it is, so everything that took it with {ThemeResource} changes with it. A key a later
    // theme leaves out goes back to what it was before any theme set it.
    class XamlThemeResources
    {
    public:
        // True when a key was added or taken away, rather than only recolored: an element that
        // took the old one with {ThemeResource} keeps it until the theme changes, so the caller
        // should change the theme and back (see ThemeResources in README.md).
        bool Apply(
            _In_ winrt::Microsoft::UI::Xaml::ResourceDictionary const& host,
            _In_ ResolvedTheme const& light,
            _In_ ResolvedTheme const& dark) noexcept;

    private:
        struct Original
        {
            bool WasThere{ false };
            winrt::Windows::Foundation::IInspectable Value{ nullptr };
            std::optional<winrt::Windows::UI::Color> SolidColor{};
            std::vector<std::pair<winrt::Windows::UI::Color, double>> Stops{};
            winrt::Windows::Foundation::Point StartPoint{};
            winrt::Windows::Foundation::Point EndPoint{};
        };

        // True when the key wasn't there before.
        bool ApplyOne(
            _In_ winrt::Microsoft::UI::Xaml::ResourceDictionary const& dictionary,
            _In_ std::wstring const& dictionaryName,
            _In_ std::wstring const& key,
            _In_ ResolvedBackground const& value);

        // True when the key is taken away.
        bool Restore(
            _In_ winrt::Microsoft::UI::Xaml::ResourceDictionary const& dictionary,
            _In_ std::wstring const& key,
            _In_ Original const& original);

        // dictionary name + '\x1F' + key
        std::map<std::wstring, Original> m_originals{};
    };
}
