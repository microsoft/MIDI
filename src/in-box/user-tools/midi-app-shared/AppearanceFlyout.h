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
    // Captions come from the calling app's own resources, so each tool stays localizable
    // through the normal pipeline rather than the shared code carrying strings of its own.
    struct AppearanceStrings
    {
        winrt::hstring Title{};
        winrt::hstring ThemeLabel{};
        winrt::hstring ThemeSystem{};
        winrt::hstring ThemeLight{};
        winrt::hstring ThemeDark{};
        winrt::hstring BackdropLabel{};
        winrt::hstring BackdropSolid{};
        winrt::hstring BackdropMica{};
        winrt::hstring BackdropAcrylic{};
        winrt::hstring CustomColorCheckBox{};
        winrt::hstring ColorPickerName{};
    };

    // A color of the app's own, such as MIDI Keyboard's keys, set the same way as the window color.
    struct AppearanceColorChoice
    {
        winrt::hstring TabLabel{};
        winrt::hstring CustomColorCheckBox{};
        winrt::hstring ColorPickerName{};

        bool UseCustomColor{ false };

        // 0xAARRGGBB
        uint32_t ColorArgb{ 0xFF000000 };

        // called after either control changes, with both values as they now stand
        std::function<void(bool useCustomColor, uint32_t colorArgb)> Changed{};
    };

    // Any colors here put the window color and each of them on tabs; with none there are no tabs.
    struct AppearanceColorTabs
    {
        // accessible name for the row of tabs
        winrt::hstring TabsName{};

        winrt::hstring WindowTabLabel{};

        std::vector<AppearanceColorChoice> Colors{};
    };

    // Shows the shared appearance controls in a light dismiss flyout anchored to a button.
    // onChanged fires after any setting is written, so the caller can re-apply the chrome.
    // extraContent, when supplied, is placed below the shared controls for app settings that
    // belong in the same flyout. topContent goes directly under the title, for a note the
    // customer should read before the controls rather than after them.
    void ShowAppearanceFlyout(
        _In_ winrt::Microsoft::UI::Xaml::FrameworkElement const& anchor,
        _Inout_ MidiAppSettings& settings,
        _In_ AppearanceStrings const& strings,
        _In_ std::function<void()> const& onChanged,
        _In_ winrt::Microsoft::UI::Xaml::UIElement const& extraContent = nullptr,
        _In_ winrt::Microsoft::UI::Xaml::UIElement const& topContent = nullptr,
        _In_ AppearanceColorTabs const& colorTabs = {}) noexcept;
}
