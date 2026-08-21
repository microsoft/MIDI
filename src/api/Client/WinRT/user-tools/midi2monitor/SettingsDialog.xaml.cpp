// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "SettingsDialog.xaml.h"
#include "SettingsDialog.g.cpp"

#include "MainWindow.xaml.h"
#include "MessageRowPanel.h"
#include "StringResources.h"

#include "WindowsMidiServicesVersion.h"

namespace native = ::midi2monitor;
namespace res = ::midi2monitor::resources;

namespace winrt::midi2monitor::implementation
{
    _Use_decl_annotations_
    void SettingsDialog::OnRootLoaded(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_initialized)
        {
            return;
        }

        m_initialized = true;

        try
        {
            auto& settings = native::AppSettings::Current();

            ThemeComboBox().SelectedIndex(static_cast<int32_t>(settings.Theme()));
            BackdropComboBox().SelectedIndex(static_cast<int32_t>(settings.Backdrop()));

            CustomBackgroundColorCheckBox().IsChecked(settings.UseCustomBackgroundColor());

            auto const argb = settings.BackgroundColorArgb();

            winrt::Windows::UI::Color color{};

            color.A = 255;
            color.R = static_cast<uint8_t>((argb >> 16) & 0xFF);
            color.G = static_cast<uint8_t>((argb >> 8) & 0xFF);
            color.B = static_cast<uint8_t>(argb & 0xFF);

            BackgroundColorPicker().Color(color);
            UpdateBackgroundColorState();
            RetentionNumberBox().Value(static_cast<double>(settings.RetainedMessageCount()));

            VersionText().Text(res::FormatString(L"SettingsVersionFormat",
                std::wstring{ WINDOWS_MIDI_SERVICES_NUGET_BUILD_VERSION_FULL },
                std::wstring{ WINDOWS_MIDI_SERVICES_NUGET_BUILD_SOURCE }));

            m_initializing = false;
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to initialize the settings dialog.")
    }

    _Use_decl_annotations_
    void SettingsDialog::Owner(midi2monitor::MainWindow const& value) noexcept
    {
        m_owner = value;
    }

    _Use_decl_annotations_
    void SettingsDialog::OnThemeChanged(foundation::IInspectable const&, controls::SelectionChangedEventArgs const&)
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

            if (m_owner != nullptr)
            {
                winrt::get_self<MainWindow>(m_owner)->ApplyTheme();
            }

            // the dialog lives in its own popup tree, so it needs the theme applied directly
            auto const theme = native::AppSettings::Current().Theme();

            RequestedTheme(
                theme == native::AppTheme::Light ? xaml::ElementTheme::Light :
                theme == native::AppTheme::Dark ? xaml::ElementTheme::Dark :
                xaml::ElementTheme::Default);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to change the theme.")
    }

    _Use_decl_annotations_
    void SettingsDialog::OnBackdropChanged(foundation::IInspectable const&, controls::SelectionChangedEventArgs const&)
    {
        if (m_initializing)
        {
            return;
        }

        try
        {
            auto const index = BackdropComboBox().SelectedIndex();

            if (index < 0)
            {
                return;
            }

            auto const backdrop = static_cast<native::WindowBackdrop>(index);

            native::AppSettings::Current().Backdrop(backdrop);

            UpdateBackgroundColorState();

            if (m_owner != nullptr)
            {
                winrt::get_self<MainWindow>(m_owner)->ApplyBackdrop();
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to change the window backdrop.")
    }

    void SettingsDialog::UpdateBackgroundColorState() noexcept
    {
        try
        {
            auto const& settings = native::AppSettings::Current();

            BackgroundColorPicker().IsEnabled(settings.UseCustomBackgroundColor());
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to update the background colour controls.")
    }

    _Use_decl_annotations_
    void SettingsDialog::OnUseCustomBackgroundColorChanged(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_initializing)
        {
            return;
        }

        try
        {
            auto const isChecked = CustomBackgroundColorCheckBox().IsChecked();

            native::AppSettings::Current().UseCustomBackgroundColor(isChecked != nullptr && isChecked.Value());

            UpdateBackgroundColorState();

            if (m_owner != nullptr)
            {
                winrt::get_self<MainWindow>(m_owner)->ApplyBackgroundColor();
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to change the custom background colour setting.")
    }

    _Use_decl_annotations_
    void SettingsDialog::OnBackgroundColorChanged(controls::ColorPicker const&, controls::ColorChangedEventArgs const& args)
    {
        if (m_initializing)
        {
            return;
        }

        try
        {
            auto const color = args.NewColor();

            auto const argb =
                (static_cast<uint32_t>(0xFF) << 24) |
                (static_cast<uint32_t>(color.R) << 16) |
                (static_cast<uint32_t>(color.G) << 8) |
                static_cast<uint32_t>(color.B);

            native::AppSettings::Current().BackgroundColorArgb(argb);

            if (m_owner != nullptr)
            {
                winrt::get_self<MainWindow>(m_owner)->ApplyBackgroundColor();
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to change the background colour.")
    }

    _Use_decl_annotations_
    void SettingsDialog::OnRetentionChanged(foundation::IInspectable const&, controls::NumberBoxValueChangedEventArgs const& args)
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
    void SettingsDialog::OnResetColumnsClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            native::ColumnLayoutState::ResetToDefault();
            native::AppSettings::Current().ColumnLayout(native::ColumnLayoutState::Serialize());

            if (m_owner != nullptr)
            {
                winrt::get_self<MainWindow>(m_owner)->RefreshColumnsAfterReset();
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to reset the columns.")
    }
}
