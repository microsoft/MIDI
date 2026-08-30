// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow.g.cpp"

#include "App.xaml.h"
#include "BackgroundWork.h"
#include "MidiPanic.h"
#include "StringResources.h"
#include "resource.h"

namespace native = ::midisettings;
namespace res = ::midisettings::resources;

namespace winrt::midisettings::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
    }

    void MainWindow::RestoreWindowPlacement()
    {
        // static, because this runs before the chrome is initialized
        midiapp::WindowChrome::RestorePlacement(*this, native::AppSettings::Current(), 1240, 900);
    }

    HWND MainWindow::WindowHandle() noexcept
    {
        try
        {
            HWND handle{ nullptr };

            if (auto const windowNative = this->try_as<::IWindowNative>())
            {
                windowNative->get_WindowHandle(&handle);
            }

            return handle;
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to resolve the window handle.")

        return nullptr;
    }

    _Use_decl_annotations_
    void MainWindow::OnRootSizeChanged(foundation::IInspectable const&, xaml::SizeChangedEventArgs const&)
    {
        try
        {
            m_chrome.UpdateTitleBarInsets();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to update the title bar insets.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRootLoaded(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            m_dispatcherQueue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

            Title(res::GetString(L"AppTitle"));
            AppTitleTextBlock().Text(res::GetString(L"AppTitle"));

            midiapp::WindowChromeElements elements{};

            elements.Window = *this;
            elements.Root = RootGrid();
            elements.Fill = WindowFill();
            elements.Tint = WindowTint();
            elements.TitleBar = AppTitleBar();
            elements.LeftInset = TitleBarLeftInsetColumn();
            elements.RightInset = TitleBarRightInsetColumn();

            m_chrome.Initialize(elements, native::AppSettings::Current());
            m_chrome.SetWindowIconFromResource(IDI_APPICON);

            // 32px source for a 16px slot, so it stays crisp on a high DPI display
            AppTitleBarIcon().Source(midiapp::WindowChrome::LoadIconImageSource(IDI_APPICON, 32));

            AlwaysOnTopToggle().IsChecked(native::AppSettings::Current().AlwaysOnTop());

            EndpointCardView().ItemsSource(m_endpointItems);
            EndpointListView().ItemsSource(m_endpointItems);
            DetailSourcePortsList().ItemsSource(m_sourcePortItems);
            DetailDestinationPortsList().ItemsSource(m_destinationPortItems);
            TransportFilterComboBox().ItemsSource(m_transportChoices);
            ConfigFileComboBox().ItemsSource(m_configFileChoices);

            ApplyViewMode();
            ApplyToolButtons();
            ShowFirstRunInvitation();

            Closed([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_closing = true;
                        strong->StopHealthTimer();
                        strong->StopWatchers();
                        ::midisettings::ShutDownPanicSession();
                        strong->m_chrome.SavePlacement();
                        strong->m_chrome.Shutdown();
                    }
                });

            m_loaded = true;

            StartWatchersAsync();
            StartHealthTimer();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to finish loading the window.")
    }

    _Use_decl_annotations_
    void MainWindow::OnAlwaysOnTopToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const checkedState = AlwaysOnTopToggle().IsChecked();

            native::AppSettings::Current().AlwaysOnTop(checkedState && checkedState.Value());

            m_chrome.ApplyAlwaysOnTop();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to change the always on top setting.")
    }

    _Use_decl_annotations_
    void MainWindow::OnAppearanceButtonClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            midiapp::AppearanceStrings strings{};

            strings.Title = res::GetString(L"AppearanceTitle");
            strings.ThemeLabel = res::GetString(L"AppearanceThemeLabel");
            strings.ThemeSystem = res::GetString(L"AppearanceThemeSystem");
            strings.ThemeLight = res::GetString(L"AppearanceThemeLight");
            strings.ThemeDark = res::GetString(L"AppearanceThemeDark");
            strings.BackdropLabel = res::GetString(L"AppearanceBackdropLabel");
            strings.BackdropSolid = res::GetString(L"AppearanceBackdropSolid");
            strings.BackdropMica = res::GetString(L"AppearanceBackdropMica");
            strings.BackdropAcrylic = res::GetString(L"AppearanceBackdropAcrylic");
            strings.CustomColorCheckBox = res::GetString(L"AppearanceCustomColorCheckBox");
            strings.ColorPickerName = res::GetString(L"AppearanceColorPickerName");

            midiapp::ShowAppearanceFlyout(
                AppearanceButton(),
                native::AppSettings::Current(),
                strings,
                [weak = get_weak()]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_chrome.ApplyTheme();
                    }
                });
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show the appearance settings.")
    }

    // ------------------------------------------------------------------------------------
    // Toolbar
    // ------------------------------------------------------------------------------------

    void MainWindow::ApplyToolButtons() noexcept
    {
        try
        {
            native::RefreshToolLocations();

            ApplyToolButton(LoopbackSetupButton(), LoopbackSetupIcon(), native::MidiTool::LoopbackSetup);
            ApplyToolButton(BluetoothSetupButton(), BluetoothSetupIcon(), native::MidiTool::BluetoothSetup);
            ApplyToolButton(NetworkSetupButton(), NetworkSetupIcon(), native::MidiTool::NetworkSetup);
            ApplyToolButton(SysExButton(), SysExIcon(), native::MidiTool::SysEx);
            ApplyToolButton(ScratchPadButton(), ScratchPadIcon(), native::MidiTool::ScratchPad);
            ApplyToolButton(TroubleshooterButton(), TroubleshooterIcon(), native::MidiTool::Troubleshooter);

            // A separator with nothing on one side of it reads as a stray line, so each one
            // only appears when both of the groups it divides have something in them.
            auto const setupInstalled =
                native::GetToolLocation(native::MidiTool::LoopbackSetup).Installed ||
                native::GetToolLocation(native::MidiTool::BluetoothSetup).Installed ||
                native::GetToolLocation(native::MidiTool::NetworkSetup).Installed;

            auto const utilitiesInstalled =
                native::GetToolLocation(native::MidiTool::SysEx).Installed ||
                native::GetToolLocation(native::MidiTool::ScratchPad).Installed ||
                native::GetToolLocation(native::MidiTool::Troubleshooter).Installed;

            SetupToolsSeparator().Visibility(setupInstalled && utilitiesInstalled ?
                xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            UtilityToolsSeparator().Visibility(setupInstalled || utilitiesInstalled ?
                xaml::Visibility::Visible : xaml::Visibility::Collapsed);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to build the toolbar.")
    }

    _Use_decl_annotations_
    void MainWindow::ApplyToolButton(
        controls::Button const& button,
        controls::Image const& icon,
        native::MidiTool const tool) noexcept
    {
        try
        {
            auto const& location = native::GetToolLocation(tool);

            if (!location.Installed)
            {
                button.Visibility(xaml::Visibility::Collapsed);
                return;
            }

            button.Visibility(xaml::Visibility::Visible);

            // The real icon out of the installed executable, so the toolbar matches the Start
            // Menu without this app carrying a second copy of everyone else's artwork.
            if (icon.Source() == nullptr)
            {
                icon.Source(midiapp::WindowChrome::LoadIconImageSourceFromFile(location.FullPath, 48));
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to set up a toolbar button.")
    }

    _Use_decl_annotations_
    void MainWindow::OnLoopbackSetupClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        native::LaunchTool(native::MidiTool::LoopbackSetup);
    }

    _Use_decl_annotations_
    void MainWindow::OnBluetoothSetupClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        native::LaunchTool(native::MidiTool::BluetoothSetup);
    }

    _Use_decl_annotations_
    void MainWindow::OnNetworkSetupClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        native::LaunchTool(native::MidiTool::NetworkSetup);
    }

    _Use_decl_annotations_
    void MainWindow::OnSysExClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        native::LaunchTool(native::MidiTool::SysEx);
    }

    _Use_decl_annotations_
    void MainWindow::OnScratchPadClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        native::LaunchTool(native::MidiTool::ScratchPad);
    }

    _Use_decl_annotations_
    void MainWindow::OnTroubleshooterClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        native::LaunchTool(native::MidiTool::Troubleshooter);
    }

    // ------------------------------------------------------------------------------------
    // View mode
    // ------------------------------------------------------------------------------------

    void MainWindow::ApplyViewMode() noexcept
    {
        try
        {
            auto const cards = native::AppSettings::Current().ViewMode() == native::EndpointViewMode::Cards;

            CardViewToggle().IsChecked(cards);
            ListViewToggle().IsChecked(!cards);

            auto const hasEndpoints = m_endpointItems.Size() > 0;

            EndpointCardView().Visibility(cards && hasEndpoints ?
                xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            EndpointListView().Visibility(!cards && hasEndpoints ?
                xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            NoEndpointsText().Visibility(hasEndpoints ?
                xaml::Visibility::Collapsed : xaml::Visibility::Visible);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to switch the endpoint view.")
    }

    _Use_decl_annotations_
    void MainWindow::OnCardViewClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            native::AppSettings::Current().ViewMode(native::EndpointViewMode::Cards);

            ApplyViewMode();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to switch to the card view.")
    }

    _Use_decl_annotations_
    void MainWindow::OnListViewClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            native::AppSettings::Current().ViewMode(native::EndpointViewMode::List);

            ApplyViewMode();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to switch to the list view.")
    }
}
