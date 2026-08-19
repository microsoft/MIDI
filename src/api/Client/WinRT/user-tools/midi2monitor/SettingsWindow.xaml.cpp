// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "SettingsWindow.xaml.h"
#include "SettingsWindow.g.cpp"

#include "MainWindow.xaml.h"
#include "MessageRowPanel.h"
#include "StringResources.h"

#include "WindowsMidiServicesVersion.h"

namespace native = ::midi2monitor;
namespace res = ::midi2monitor::resources;

namespace winrt::midi2monitor::implementation
{
    _Use_decl_annotations_
    void SettingsWindow::OnRootLoaded(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_initialized)
        {
            return;
        }

        m_initialized = true;

        try
        {
            Title(res::GetString(L"SettingsWindowTitle"));

            if (auto appWindow = AppWindow())
            {
                appWindow.Resize(winrt::Windows::Graphics::SizeInt32{ 720, 720 });
            }

            auto& settings = native::AppSettings::Current();

            ThemeComboBox().SelectedIndex(static_cast<int32_t>(settings.Theme()));
            RetentionNumberBox().Value(static_cast<double>(settings.RetainedMessageCount()));
            ShowChicletsToggle().IsOn(settings.ShowMessageNameChiclets());

            VersionText().Text(res::FormatString(L"SettingsVersionFormat",
                std::wstring{ WINDOWS_MIDI_SERVICES_NUGET_BUILD_VERSION_FULL },
                std::wstring{ WINDOWS_MIDI_SERVICES_NUGET_BUILD_SOURCE }));

            ApplyOwnerTheme();

            m_initializing = false;
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to initialize the settings window.")
    }

    _Use_decl_annotations_
    void SettingsWindow::Owner(midi2monitor::MainWindow const& value) noexcept
    {
        m_owner = value;
    }

    void SettingsWindow::ApplyOwnerTheme() noexcept
    {
        try
        {
            auto const theme = native::AppSettings::Current().Theme();

            RootScrollViewer().RequestedTheme(
                theme == native::AppTheme::Light ? xaml::ElementTheme::Light :
                theme == native::AppTheme::Dark ? xaml::ElementTheme::Dark :
                xaml::ElementTheme::Default);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to apply the theme to the settings window.")
    }

    _Use_decl_annotations_
    void SettingsWindow::OnThemeChanged(foundation::IInspectable const&, controls::SelectionChangedEventArgs const&)
    {
        if (m_initializing)
        {
            return;
        }

        try
        {
            auto const index = ThemeComboBox().SelectedIndex();

            if (index < 0)
            {
                return;
            }

            native::AppSettings::Current().Theme(static_cast<native::AppTheme>(index));

            ApplyOwnerTheme();

            if (m_owner != nullptr)
            {
                winrt::get_self<MainWindow>(m_owner)->ApplyTheme();
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to change the theme.")
    }

    _Use_decl_annotations_
    void SettingsWindow::OnRetentionChanged(foundation::IInspectable const&, controls::NumberBoxValueChangedEventArgs const& args)
    {
        if (m_initializing)
        {
            return;
        }

        try
        {
            auto const value = args.NewValue();

            if (std::isnan(value))
            {
                RetentionNumberBox().Value(static_cast<double>(native::AppSettings::Current().RetainedMessageCount()));
                return;
            }

            native::AppSettings::Current().RetainedMessageCount(static_cast<uint32_t>(value));

            // clamped on the way in, so reflect whatever was actually stored
            RetentionNumberBox().Value(static_cast<double>(native::AppSettings::Current().RetainedMessageCount()));

            if (m_owner != nullptr)
            {
                winrt::get_self<MainWindow>(m_owner)->ApplyRetention();
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to change the retained message count.")
    }

    _Use_decl_annotations_
    void SettingsWindow::OnShowChicletsToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_initializing)
        {
            return;
        }

        try
        {
            native::AppSettings::Current().ShowMessageNameChiclets(ShowChicletsToggle().IsOn());

            if (m_owner != nullptr)
            {
                winrt::get_self<MainWindow>(m_owner)->ApplyMessageNameSetting();
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to change the message name setting.")
    }

    _Use_decl_annotations_
    void SettingsWindow::OnResetColumnsClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            native::ColumnLayoutState::ResetToDefault();
            native::AppSettings::Current().ColumnLayout(native::ColumnLayoutState::Serialize());

            if (m_owner != nullptr)
            {
                auto owner = winrt::get_self<MainWindow>(m_owner);
                owner->RefreshColumnsAfterReset();
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to reset the columns.")
    }

    _Use_decl_annotations_
    void SettingsWindow::OnCloseClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            Close();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to close the settings window.")
    }
}
