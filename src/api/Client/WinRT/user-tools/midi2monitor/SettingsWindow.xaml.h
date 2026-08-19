// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "SettingsWindow.g.h"

namespace winrt::midi2monitor::implementation
{
    struct MainWindow;

    struct SettingsWindow : SettingsWindowT<SettingsWindow>
    {
        SettingsWindow() = default;

        void OnRootLoaded(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void Owner(midi2monitor::MainWindow const& value) noexcept;

        void OnThemeChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnRetentionChanged(foundation::IInspectable const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        void OnShowChicletsToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnResetColumnsClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCloseClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

    private:
        void ApplyOwnerTheme() noexcept;

        midi2monitor::MainWindow m_owner{ nullptr };
        bool m_initialized{ false };
        bool m_initializing{ true };
    };
}

namespace winrt::midi2monitor::factory_implementation
{
    struct SettingsWindow : SettingsWindowT<SettingsWindow, implementation::SettingsWindow>
    {
    };
}
