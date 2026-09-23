// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow.g.cpp"

#include "AppSettings.h"
#include "StringResources.h"
#include "AppearanceFlyout.h"
#include "SingleInstance.h"
#include "PreviewBuild.h"
#include "resource.h"

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        constexpr int32_t DefaultWindowWidth = 1280;
        constexpr int32_t DefaultWindowHeight = 860;
    }

    void MainWindow::RestoreWindowPlacement()
    {
        midiapp::WindowChrome::RestorePlacement(
            *this, ::midiglass::AppSettings::Current(), DefaultWindowWidth, DefaultWindowHeight);
    }

    _Use_decl_annotations_
    void MainWindow::OnRootLoaded(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            midiapp::ApplyPreviewBadgeVisibility(PreviewChiclet());

            midiapp::WindowChromeElements elements{};

            elements.Window = *this;
            elements.Root = RootGrid();
            elements.Fill = WindowFill();
            elements.Tint = WindowTint();
            elements.TitleBar = AppTitleBar();
            elements.LeftInset = TitleBarLeftInsetColumn();
            elements.RightInset = TitleBarRightInsetColumn();

            m_chrome.Initialize(elements, ::midiglass::AppSettings::Current());

            // Now that there is a window, a later launch has something to bring forward.
            ::midiapp::SingleInstance::PublishMainWindow(m_chrome.WindowHandle());

            m_chrome.SetWindowIconFromResource(IDI_APPICON);

            Title(resources::GetString(L"AppDisplayName"));
            AppTitleTextBlock().Text(resources::GetString(L"AppDisplayName"));

            // 32px source for a 16px slot, so it stays crisp on a high DPI display
            if (auto const icon = midiapp::WindowChrome::LoadIconImageSource(IDI_APPICON, 32))
            {
                AppTitleBarIcon().Source(icon);
            }

            AlwaysOnTopToggle().IsChecked(::midiglass::AppSettings::Current().AlwaysOnTop());

            Closed({ this, &MainWindow::OnWindowClosed });
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to set up the window chrome.")
    }

    _Use_decl_annotations_
    void MainWindow::OnWindowClosed(foundation::IInspectable const& sender, xaml::WindowEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            m_chrome.SavePlacement();
            m_chrome.Shutdown();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to shut the window down cleanly.")
    }

    _Use_decl_annotations_
    void MainWindow::OnAppearanceButtonClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            midiapp::AppearanceStrings strings{};

            strings.Title = resources::GetString(L"AppearanceTitle");
            strings.ThemeLabel = resources::GetString(L"AppearanceTheme");
            strings.ThemeSystem = resources::GetString(L"AppearanceThemeSystem");
            strings.ThemeLight = resources::GetString(L"AppearanceThemeLight");
            strings.ThemeDark = resources::GetString(L"AppearanceThemeDark");
            strings.BackdropLabel = resources::GetString(L"AppearanceBackdrop");
            strings.BackdropSolid = resources::GetString(L"AppearanceBackdropSolid");
            strings.BackdropMica = resources::GetString(L"AppearanceBackdropMica");
            strings.BackdropAcrylic = resources::GetString(L"AppearanceBackdropAcrylic");
            strings.CustomColorCheckBox = resources::GetString(L"AppearanceCustomColor");
            strings.ColorPickerName = resources::GetString(L"AppearanceColorPicker");

            auto weak = get_weak();

            midiapp::ShowAppearanceFlyout(
                AppearanceButton(),
                ::midiglass::AppSettings::Current(),
                strings,
                [weak]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_chrome.ApplyTheme();
                    }
                });
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the appearance flyout.")
    }

    _Use_decl_annotations_
    void MainWindow::OnAlwaysOnTopToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto const checked = AlwaysOnTopToggle().IsChecked();

            ::midiglass::AppSettings::Current().AlwaysOnTop(checked && checked.Value());

            m_chrome.ApplyAlwaysOnTop();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change the always on top setting.")
    }
}
