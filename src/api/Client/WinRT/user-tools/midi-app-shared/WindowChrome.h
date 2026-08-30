// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiAppSettings.h"

namespace midiapp
{
    // The parts of the app's XAML tree the shared chrome drives. Every tool lays these out the
    // same way: a root grid, a full bleed fill and tint layer behind everything, and a title bar
    // grid whose outer columns reserve space for the system caption buttons.
    struct WindowChromeElements
    {
        winrt::Microsoft::UI::Xaml::Window Window{ nullptr };
        winrt::Microsoft::UI::Xaml::FrameworkElement Root{ nullptr };
        winrt::Microsoft::UI::Xaml::UIElement Fill{ nullptr };
        winrt::Microsoft::UI::Xaml::Controls::Border Tint{ nullptr };
        winrt::Microsoft::UI::Xaml::UIElement TitleBar{ nullptr };
        winrt::Microsoft::UI::Xaml::Controls::ColumnDefinition LeftInset{ nullptr };
        winrt::Microsoft::UI::Xaml::Controls::ColumnDefinition RightInset{ nullptr };
    };

    // Title bar, theme, backdrop material and window placement, shared by the MIDI user tools.
    // Nothing here throws; failures leave the window with its previous appearance.
    class WindowChrome
    {
    public:
        // call once the root element has loaded, before the window is activated
        void Initialize(WindowChromeElements const& elements, MidiAppSettings& settings) noexcept;

        // theme, then everything whose colors are derived from it
        void ApplyTheme() noexcept;

        void ApplyBackdrop() noexcept;
        void ApplyBackgroundColor() noexcept;
        void ApplyTitleBarColors() noexcept;
        void UpdateTitleBarInsets() noexcept;
        void ApplyAlwaysOnTop() noexcept;

        // Sets the taskbar, Alt-Tab and title bar icon from an icon resource embedded in the
        // executable. Uses WM_SETICON rather than AppWindow.SetIcon, which has been unreliable.
        void SetWindowIconFromResource(uint16_t resourceId) noexcept;

        // The same embedded icon as an image source, for the icon drawn in a custom title bar.
        // Sourced from the executable's own resource so there is one icon to keep up to date,
        // and so it works without the app being packaged. Returns null on any failure.
        static winrt::Microsoft::UI::Xaml::Media::Imaging::WriteableBitmap LoadIconImageSource(
            uint16_t resourceId,
            int32_t sizePixels) noexcept;

        // The main icon of another executable, for a tool that launches its siblings. The file
        // is mapped as data only, so nothing in it is ever executed or initialized.
        static winrt::Microsoft::UI::Xaml::Media::Imaging::WriteableBitmap LoadIconImageSourceFromFile(
            std::wstring const& executablePath,
            int32_t sizePixels) noexcept;

        // Static, because this runs before Activate and therefore before Initialize.
        static void RestorePlacement(
            winrt::Microsoft::UI::Xaml::Window const& window,
            MidiAppSettings const& settings,
            int32_t defaultWidth,
            int32_t defaultHeight) noexcept;

        void SavePlacement() noexcept;

        // releases the backdrop controllers; call from the window's Closed handler
        void Shutdown() noexcept;

    private:
        void UpdateBackdropConfiguration() noexcept;
        void ReleaseBackdropControllers() noexcept;

        winrt::Microsoft::UI::Windowing::AppWindow GetAppWindow() const noexcept;

        WindowChromeElements m_elements{};
        MidiAppSettings* m_settings{ nullptr };

        // the material is driven through the controllers rather than the XAML SystemBackdrop
        // property, because only the controllers expose TintColor
        winrt::Microsoft::UI::Composition::SystemBackdrops::SystemBackdropConfiguration m_backdropConfiguration{ nullptr };
        winrt::Microsoft::UI::Composition::SystemBackdrops::MicaController m_micaController{ nullptr };
        winrt::Microsoft::UI::Composition::SystemBackdrops::DesktopAcrylicController m_acrylicController{ nullptr };
        winrt::event_token m_activatedToken{};

        bool m_backdropApplied{ false };
        WindowBackdrop m_appliedBackdrop{ WindowBackdrop::Solid };

        HICON m_largeIcon{ nullptr };
        HICON m_smallIcon{ nullptr };
    };
}
