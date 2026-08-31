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
#include "Elevation.h"
#include "StringResources.h"
#include "ToolPaths.h"
#include "resource.h"

namespace native = ::miditroubleshooter;
namespace res = ::miditroubleshooter::resources;

namespace winrt::miditroubleshooter::implementation
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

    _Use_decl_annotations_
    void MainWindow::OnRootSizeChanged(foundation::IInspectable const&, xaml::SizeChangedEventArgs const&)
    {
        try
        {
            m_chrome.UpdateTitleBarInsets();
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to update the title bar insets.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRootLoaded(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
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

            SessionsListView().ItemsSource(m_sessions);
            TransportsListView().ItemsSource(m_transports);
            Drivers32ItemsControl().ItemsSource(m_drivers32Entries);
            Drivers32WowItemsControl().ItemsSource(m_drivers32WowEntries);
            ServiceRootItemsControl().ItemsSource(m_serviceRootEntries);
            RegistryTransportsItemsControl().ItemsSource(m_transportRegistryEntries);
            DriversListView().ItemsSource(m_driverDevices);

            m_elevated = ::winrt::miditroubleshooter::implementation::App::IsElevated();

            m_systemInformation = native::GatherSystemInformation();

            ApplySystemInformation();
            ApplyElevationState();

            native::RefreshToolLocations();

            auto const tools = native::GetToolLocations();

            CaptureTimeTravelCheck().IsEnabled(!tools.TimeTravelTracer.empty());

            CaptureProfileText().Text(tools.ReproProfile.empty() ?
                res::GetString(L"CaptureProfileMissing") :
                res::FormatString(L"CaptureProfileFoundFormat", winrt::hstring{ tools.ReproProfile }));

            Closed([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_closing = true;
                        strong->StopRefreshTimer();
                        strong->m_chrome.SavePlacement();
                        strong->m_chrome.Shutdown();
                    }
                });

            m_loaded = true;

            LoadApiMode();

            auto pageIndex = native::AppSettings::Current().SelectedPageIndex();

            auto const& startPage = App::StartupOptions().StartPage;

            if (!startPage.empty())
            {
                pageIndex = PageIndexForTag(winrt::hstring{ startPage });
            }

            // the panels first, because selecting an item which is already selected raises no
            // selection changed event to do it for us
            ShowPage(pageIndex);

            if (auto const item = NavigationItemForPage(pageIndex))
            {
                MainNavigation().SelectedItem(item);
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to finish loading the window.")
    }

    void MainWindow::ApplySystemInformation() noexcept
    {
        try
        {
            auto const& information = m_systemInformation;

            SystemInfoWindowsValue().Text(information.WindowsEdition.empty() ?
                winrt::hstring{ information.WindowsVersion } :
                res::FormatString(L"SystemInfoWindowsFormat",
                    winrt::hstring{ information.WindowsEdition },
                    winrt::hstring{ information.WindowsVersion }));

            SystemInfoArchitectureValue().Text(winrt::hstring{ information.WindowsArchitecture });
            SystemInfoCultureValue().Text(winrt::hstring{ information.CultureName });
            SystemInfoWindowsAppSdkValue().Text(winrt::hstring{ information.WindowsAppSdkVersion });

            // Developer mode is what lets an unsigned service transport load, so it belongs
            // next to the transports page rather than buried in a report.
            SystemInfoDeveloperModeValue().Text(information.DeveloperModeEnabled ?
                res::GetString(L"DeveloperModeEnabled") : res::GetString(L"DeveloperModeDisabled"));

            // Tooltips carry the values that are too long for the strip but still get asked for.
            controls::ToolTipService::SetToolTip(
                SystemInfoWindowsValue(),
                winrt::box_value(res::FormatString(L"SystemInfoComputerFormat",
                    winrt::hstring{ information.ComputerName },
                    winrt::hstring{ information.ToolVersion })));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the system information.")
    }

    void MainWindow::ApplyElevationState() noexcept
    {
        try
        {
            ElevationBar().IsOpen(!m_elevated);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the elevation state.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRestartElevatedClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            // The relaunched copy opens on the page the customer is looking at now.
            auto const page = std::format(L"--page {}", std::wstring{ MainNavigation().SelectedItem() != nullptr ?
                winrt::unbox_value_or<winrt::hstring>(
                    MainNavigation().SelectedItem().as<controls::NavigationViewItem>().Tag(), winrt::hstring{}) :
                winrt::hstring{} });

            if (native::TryRelaunchElevated(page))
            {
                Close();
            }
            else
            {
                ElevationBar().Message(res::GetString(L"ElevationDeclined"));
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to relaunch with administrator rights.")
    }

    bool MainWindow::RequireElevation() noexcept
    {
        if (m_elevated)
        {
            return true;
        }

        try
        {
            ElevationBar().IsOpen(true);
            ElevationBar().Message(res::GetString(L"ElevationRequiredForAction"));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the elevation requirement.")

        return false;
    }

    _Use_decl_annotations_
    uint32_t MainWindow::PageIndexForTag(winrt::hstring const& tag) noexcept
    {
        if (tag == L"diagnostics")  return native::AppSettings::PageIndexDiagnostics;
        if (tag == L"capture")      return native::AppSettings::PageIndexCaptureLog;
        if (tag == L"sessions")     return native::AppSettings::PageIndexSessions;
        if (tag == L"transports")   return native::AppSettings::PageIndexTransports;
        if (tag == L"service")      return native::AppSettings::PageIndexService;
        if (tag == L"registry")     return native::AppSettings::PageIndexRegistry;
        if (tag == L"drivers")      return native::AppSettings::PageIndexDrivers;

        return native::AppSettings::PageIndexApiMode;
    }

    _Use_decl_annotations_
    controls::NavigationViewItem MainWindow::NavigationItemForPage(uint32_t const pageIndex) noexcept
    {
        try
        {
            switch (pageIndex)
            {
            case native::AppSettings::PageIndexDiagnostics: return DiagnosticsNavigationItem();
            case native::AppSettings::PageIndexCaptureLog:  return CaptureNavigationItem();
            case native::AppSettings::PageIndexSessions:    return SessionsNavigationItem();
            case native::AppSettings::PageIndexTransports:  return TransportsNavigationItem();
            case native::AppSettings::PageIndexService:     return ServiceNavigationItem();
            case native::AppSettings::PageIndexRegistry:    return RegistryNavigationItem();
            case native::AppSettings::PageIndexDrivers:     return DriversNavigationItem();
            default:                                        return ApiModeNavigationItem();
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to resolve a navigation item.")

        return nullptr;
    }

    _Use_decl_annotations_
    void MainWindow::OnNavigationSelectionChanged(
        controls::NavigationView const&,
        controls::NavigationViewSelectionChangedEventArgs const& args)
    {
        try
        {
            auto const item = args.SelectedItem().try_as<controls::NavigationViewItem>();

            if (item == nullptr)
            {
                return;
            }

            auto const tag = winrt::unbox_value_or<winrt::hstring>(item.Tag(), winrt::hstring{});
            auto const pageIndex = PageIndexForTag(tag);

            ShowPage(pageIndex);

            native::AppSettings::Current().SelectedPageIndex(pageIndex);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to switch pages.")
    }

    _Use_decl_annotations_
    void MainWindow::ShowPage(uint32_t const pageIndex) noexcept
    {
        try
        {
            m_currentPageIndex = pageIndex;

            auto const visibility = [pageIndex](uint32_t const index)
                {
                    return pageIndex == index ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
                };

            ApiModePanel().Visibility(visibility(native::AppSettings::PageIndexApiMode));
            DiagnosticsPanel().Visibility(visibility(native::AppSettings::PageIndexDiagnostics));
            CapturePanel().Visibility(visibility(native::AppSettings::PageIndexCaptureLog));
            SessionsPanel().Visibility(visibility(native::AppSettings::PageIndexSessions));
            TransportsPanel().Visibility(visibility(native::AppSettings::PageIndexTransports));
            ServicePanel().Visibility(visibility(native::AppSettings::PageIndexService));
            RegistryPanel().Visibility(visibility(native::AppSettings::PageIndexRegistry));
            DriversPanel().Visibility(visibility(native::AppSettings::PageIndexDrivers));

            if (pageIndex == native::AppSettings::PageIndexApiMode)
            {
                LoadApiMode();
            }

            // Only the pages that show live service state pay for polling.
            if (ServiceHealthPageVisible())
            {
                StartRefreshTimer();
                RequestServiceRefreshAsync();
            }
            else
            {
                StopRefreshTimer();
            }

            if (pageIndex == native::AppSettings::PageIndexRegistry && m_registryScan.Drivers32Entries.empty())
            {
                OnRefreshRegistryClick(nullptr, nullptr);
            }

            if (pageIndex == native::AppSettings::PageIndexDrivers && m_driverDevices.Size() == 0)
            {
                OnRefreshDriversClick(nullptr, nullptr);
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the requested page.")
    }

    bool MainWindow::ServiceHealthPageVisible() const noexcept
    {
        return m_currentPageIndex == native::AppSettings::PageIndexSessions ||
            m_currentPageIndex == native::AppSettings::PageIndexTransports ||
            m_currentPageIndex == native::AppSettings::PageIndexService;
    }

    void MainWindow::StartRefreshTimer() noexcept
    {
        try
        {
            if (m_refreshTimer == nullptr)
            {
                m_refreshTimer = DispatcherQueue().CreateTimer();

                m_refreshTimer.Tick([weak = get_weak()](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->RequestServiceRefreshAsync();
                        }
                    });
            }

            auto const seconds = native::AppSettings::Current().RefreshIntervalSeconds();

            m_refreshTimer.Interval(std::chrono::seconds{ seconds == 0 ? 1 : seconds });
            m_refreshTimer.Start();
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to start the refresh timer.")
    }

    void MainWindow::StopRefreshTimer() noexcept
    {
        try
        {
            if (m_refreshTimer != nullptr)
            {
                m_refreshTimer.Stop();
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to stop the refresh timer.")
    }

    _Use_decl_annotations_
    void MainWindow::OnAlwaysOnTopToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const isChecked = AlwaysOnTopToggle().IsChecked();

            native::AppSettings::Current().AlwaysOnTop(isChecked != nullptr && isChecked.Value());

            m_chrome.ApplyAlwaysOnTop();
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to change the always on top setting.")
    }

    _Use_decl_annotations_
    void MainWindow::OnAppearanceButtonClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            midiapp::AppearanceStrings strings{};

            strings.Title = res::GetString(L"SettingsTitle");
            strings.ThemeLabel = res::GetString(L"SettingsThemeLabel");
            strings.ThemeSystem = res::GetString(L"SettingsThemeSystem");
            strings.ThemeLight = res::GetString(L"SettingsThemeLight");
            strings.ThemeDark = res::GetString(L"SettingsThemeDark");
            strings.BackdropLabel = res::GetString(L"SettingsBackdropLabel");
            strings.BackdropSolid = res::GetString(L"SettingsBackdropSolid");
            strings.BackdropMica = res::GetString(L"SettingsBackdropMica");
            strings.BackdropAcrylic = res::GetString(L"SettingsBackdropAcrylic");
            strings.CustomColorCheckBox = res::GetString(L"SettingsCustomColorCheckBox");
            strings.ColorPickerName = res::GetString(L"SettingsColorPickerName");

            // the refresh interval belongs with the other settings rather than in a panel of
            // its own, so it goes into the shared flyout's extra content slot
            controls::StackPanel extra{};
            extra.Spacing(4);
            extra.Margin(xaml::Thickness{ 0, 12, 0, 0 });

            controls::TextBlock label{};
            label.Text(res::GetString(L"SettingsRefreshIntervalLabel"));

            controls::NumberBox box{};
            box.Minimum(static_cast<double>(native::AppSettings::MinimumRefreshIntervalSeconds));
            box.Maximum(static_cast<double>(native::AppSettings::MaximumRefreshIntervalSeconds));
            box.Value(static_cast<double>(native::AppSettings::Current().RefreshIntervalSeconds()));
            box.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);
            box.HorizontalAlignment(xaml::HorizontalAlignment::Left);
            box.Width(160);
            xaml::Automation::AutomationProperties::SetName(box, res::GetString(L"SettingsRefreshIntervalLabel"));

            box.ValueChanged([weak = get_weak()](controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const&)
                {
                    auto strong = weak.get();

                    if (strong == nullptr)
                    {
                        return;
                    }

                    auto const value = sender.Value();

                    // an empty NumberBox reports NaN
                    if (value != value)
                    {
                        return;
                    }

                    native::AppSettings::Current().RefreshIntervalSeconds(static_cast<uint32_t>(value));

                    if (strong->ServiceHealthPageVisible())
                    {
                        strong->StartRefreshTimer();
                    }
                });

            controls::TextBlock help{};
            help.Text(res::GetString(L"SettingsRefreshIntervalHelp"));
            help.TextWrapping(xaml::TextWrapping::Wrap);
            help.FontSize(12);

            extra.Children().Append(label);
            extra.Children().Append(box);
            extra.Children().Append(help);

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
                },
                extra);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to open the settings.")
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<bool> MainWindow::ConfirmAsync(winrt::hstring const& title, winrt::hstring const& message)
    {
        if (m_openDialog != nullptr)
        {
            co_return false;
        }

        try
        {
            ConfirmDialog().Title(winrt::box_value(title));
            ConfirmDialogText().Text(message);
            ConfirmDialog().XamlRoot(Content().XamlRoot());

            m_openDialog = ConfirmDialog();

            auto const result = co_await ConfirmDialog().ShowAsync();

            m_openDialog = nullptr;

            co_return result == controls::ContentDialogResult::Primary;
        }
        catch (...)
        {
            m_openDialog = nullptr;

            co_return false;
        }
    }

    _Use_decl_annotations_
    foundation::IAsyncAction MainWindow::OfferRestartAsync(winrt::hstring const& message)
    {
        if (m_openDialog != nullptr)
        {
            co_return;
        }

        try
        {
            RestartComputerDialogText().Text(message);
            RestartComputerDialog().XamlRoot(Content().XamlRoot());

            m_openDialog = RestartComputerDialog();

            auto const result = co_await RestartComputerDialog().ShowAsync();

            m_openDialog = nullptr;

            // Nothing restarts this PC without the customer saying so here.
            if (result == controls::ContentDialogResult::Primary)
            {
                native::TryRestartComputer();
            }
        }
        catch (...)
        {
            m_openDialog = nullptr;
        }
    }

    HWND MainWindow::WindowHandle() noexcept
    {
        try
        {
            HWND handle{ nullptr };

            if (auto const native = this->try_as<::IWindowNative>())
            {
                native->get_WindowHandle(&handle);
            }

            return handle;
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to get the window handle.")

        return nullptr;
    }

    _Use_decl_annotations_
    void MainWindow::CopyToClipboard(winrt::hstring const& text) noexcept
    {
        try
        {
            if (text.empty())
            {
                return;
            }

            winrt::Windows::ApplicationModel::DataTransfer::DataPackage package{};

            package.SetText(text);

            winrt::Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to copy to the clipboard.")
    }
}
