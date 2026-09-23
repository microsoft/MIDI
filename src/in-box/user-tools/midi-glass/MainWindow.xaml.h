// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MainWindow.g.h"

#include "WindowChrome.h"

namespace winrt::midiglass::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow() = default;

        // Called before Activate, so the window is sized and placed before its first paint.
        void RestoreWindowPlacement();

        void OnRootLoaded(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        void OnAppearanceButtonClick(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        void OnAlwaysOnTopToggled(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

    private:
        void OnWindowClosed(
            foundation::IInspectable const& sender,
            xaml::WindowEventArgs const& args);

        midiapp::WindowChrome m_chrome{};
    };
}

namespace winrt::midiglass::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
