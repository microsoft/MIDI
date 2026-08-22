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

    // Shows the shared appearance controls in a light dismiss flyout anchored to a button.
    // onChanged fires after any setting is written, so the caller can re-apply the chrome.
    // extraContent, when supplied, is placed below the shared controls for app settings that
    // belong in the same flyout.
    void ShowAppearanceFlyout(
        winrt::Microsoft::UI::Xaml::FrameworkElement const& anchor,
        MidiAppSettings& settings,
        AppearanceStrings const& strings,
        std::function<void()> const& onChanged,
        winrt::Microsoft::UI::Xaml::UIElement const& extraContent = nullptr) noexcept;
}
