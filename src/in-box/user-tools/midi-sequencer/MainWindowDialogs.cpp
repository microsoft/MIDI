// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Dialogs in the window's colors. A ContentDialog opens in a layer of its own, outside the window's
// content, so the style's brushes (XamlThemeResources, on RootGrid) never reach it, and WinUI
// fills it with its own gray. This puts the dialog's fills in the app's own resources:
//
// - A style made of a material (Tactile, Aluminum, Vellum) names its dialog surface, "dialog".
// - Night drive, whose color is the window's backdrop, takes what the context menus' acrylic
//   shows over the window, so a dialog matches the menus and lists.
//
// They're for the theme the window shows, and change with it: a dialog takes its text from the
// window's theme but looked its fills up in the app's, so fills in the app's theme dictionaries
// came out light under light text. The part with the buttons is a shade deeper than the rest, as
// WinUI's is. High contrast keeps WinUI's own colors.

#include "pch.h"
#include "MainWindow.xaml.h"

#include <winrt/Windows.UI.ViewManagement.h>

namespace winrt::midisequencer::implementation
{
    namespace
    {
        constexpr wchar_t TopKey[]{ L"ContentDialogTopOverlay" };
        constexpr wchar_t ButtonsKey[]{ L"ContentDialogBackground" };

        winrt::Windows::UI::Color Deeper(winrt::Windows::UI::Color color, bool dark) noexcept
        {
            return midiapp::details::Mix(color, winrt::Windows::UI::Color{ 255, 0, 0, 0 }, dark ? 0.18 : 0.035);
        }

        void SetAppBrush(xaml::ResourceDictionary const& resources, wchar_t const* key, winrt::Windows::UI::Color color)
        {
            auto const boxed = winrt::box_value(key);

            if (auto const brush = resources.TryLookup(boxed).try_as<media::SolidColorBrush>(); brush != nullptr && resources.HasKey(boxed))
            {
                brush.Color(color);
            }
            else
            {
                resources.Insert(boxed, media::SolidColorBrush{ color });
            }
        }
    }

    void MainWindow::ColorDialogs() noexcept
    {
        try
        {
            auto const resources = xaml::Application::Current().Resources();

            if (winrt::Windows::UI::ViewManagement::AccessibilitySettings{}.HighContrast())
            {
                for (auto const key : { TopKey, ButtonsKey })
                {
                    if (resources.HasKey(winrt::box_value(key)))
                    {
                        resources.Remove(winrt::box_value(key));
                    }
                }

                return;
            }

            auto const dark = RootGrid().ActualTheme() == xaml::ElementTheme::Dark;
            auto const& style = dark ? m_styleDark : m_styleLight;
            winrt::Windows::UI::Color surface{};

            if (style != nullptr && style->HasToken(L"dialog"))
            {
                surface = seq::ToUiColor(style->Token(L"dialog"));
                surface.A = 255;
            }
            else
            {
                surface = midiapp::details::MenuColorOver(midiapp::details::WindowColor(dark), dark);
            }

            SetAppBrush(resources, TopKey, surface);
            SetAppBrush(resources, ButtonsKey, Deeper(surface, dark));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to color the dialogs.")
    }
}
