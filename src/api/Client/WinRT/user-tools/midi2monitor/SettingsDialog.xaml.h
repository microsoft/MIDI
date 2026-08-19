// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "SettingsDialog.g.h"

namespace winrt::midi2monitor::implementation
{
    struct MainWindow;

    struct SettingsDialog : SettingsDialogT<SettingsDialog>
    {
        SettingsDialog() = default;

        void OnRootLoaded(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void Owner(midi2monitor::MainWindow const& value) noexcept;

        void OnThemeChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnRetentionChanged(foundation::IInspectable const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        void OnResetColumnsClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

    private:
        midi2monitor::MainWindow m_owner{ nullptr };
        bool m_initialized{ false };
        bool m_initializing{ true };
    };
}

namespace winrt::midi2monitor::factory_implementation
{
    struct SettingsDialog : SettingsDialogT<SettingsDialog, implementation::SettingsDialog>
    {
    };
}
