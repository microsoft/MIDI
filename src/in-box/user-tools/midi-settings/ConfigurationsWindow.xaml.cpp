// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// UNSUPPORTED - DO NOT COPY THIS APPROACH. This window reads and writes the Windows MIDI Services
// configuration file directly, through the configuration engine. Microsoft does not support direct
// manipulation of that file by anything other than the in-box MIDI tools. Its format, its name, its
// location and the registry value that selects it are implementation details and can change in any
// release without notice. Applications and third-party tools must use the Windows MIDI Services API
// (Windows.Devices.Midi2.ServiceConfig) instead.
// ============================================================================

#include "pch.h"
#include "ConfigurationsWindow.xaml.h"
#include "ConfigurationsWindow.g.cpp"

#include "App.xaml.h"
#include "AppSettings.h"
#include "BackgroundWork.h"
#include "MidiConfig.h"
#include "PopupBackdrop.h"
#include "StringResources.h"
#include "resource.h"

namespace native = ::midisettings;
namespace res = ::midisettings::resources;
namespace cfg = ::midisettings::configurations;
namespace engine = ::midisettings::configengine;

namespace winrt::midisettings::implementation
{
    namespace
    {
        // The comps are drawn 1180 wide. WinUI's InfoBar spacing is a little wider than the comps',
        // so at 1180 the administrator bar drops to three lines. 20 more keeps it on one, as drawn.
        constexpr int32_t DefaultWindowWidth{ 1200 };
        constexpr int32_t DefaultWindowHeight{ 820 };

        // The comps' wide dialog, and the width of what goes in it.
        constexpr double DialogMaxWidth{ 560 };
        constexpr double DialogContentWidth{ 512 };

        constexpr std::wstring_view PageConfigurations{ L"configurations" };
        constexpr std::wstring_view PageBackups{ L"backups" };
        constexpr std::wstring_view PageShare{ L"share" };
        constexpr std::wstring_view PageImport{ L"import" };
    }

    ConfigurationsWindow::ConfigurationsWindow()
    {
        InitializeComponent();
    }

    void ConfigurationsWindow::RestoreWindowPlacement() noexcept
    {
        // static, because this runs before the chrome is initialized
        midiapp::WindowChrome::RestorePlacement(
            *this,
            native::AppSettings::Current().ConfigurationsPlacement(),
            DefaultWindowWidth,
            DefaultWindowHeight);
    }

    void ConfigurationsWindow::SaveWindowPlacement() noexcept
    {
        try
        {
            // its own placement, so it doesn't move the main window the next time that opens
            auto const placement = midiapp::WindowChrome::CapturePlacement(*this);

            if (placement.Valid)
            {
                native::AppSettings::Current().ConfigurationsPlacement(placement);
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to remember where the Configurations window was.")
    }

    HWND ConfigurationsWindow::WindowHandle() noexcept
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
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to get the Configurations window handle.")

        return nullptr;
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::OnRootSizeChanged(foundation::IInspectable const&, xaml::SizeChangedEventArgs const&)
    {
        try
        {
            m_chrome.UpdateTitleBarInsets();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to update the title bar insets.")
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::OnRootLoaded(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (m_loaded)
            {
                return;
            }

            m_dispatcherQueue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
            m_elevated = App::IsElevated();

            auto const title = res::GetString(L"CfgWindowTitle");

            Title(title);
            AppTitleTextBlock().Text(title);

            midiapp::ApplyPreviewBadgeVisibility(PreviewChiclet());

            // A closed InfoBar is still a child of its stack, so the stack's spacing would leave a
            // gap for each one. Closed ones are collapsed instead. This is registered before the
            // live regions below, so a bar is visible again by the time it's announced.
            for (auto const& bar : {
                ConfigurationsElevationBar(), SwitchPendingBar(), ChangesPendingBar(), ConfigurationsStatusBar(),
                BackupsElevationBar(), BackupsStatusBar(),
                ShareElevationBar(), ShareSavedBar(), ShareStatusBar(),
                ImportStartStatusBar(), ImportElevationBar(), ImportReviewStatusBar(), ImportRestartBar(), ImportDoneStatusBar() })
            {
                bar.Visibility(bar.IsOpen() ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

                bar.RegisterPropertyChangedCallback(controls::InfoBar::IsOpenProperty(),
                    [](xaml::DependencyObject const& sender, xaml::DependencyProperty const&)
                    {
                        if (auto const changed = sender.try_as<controls::InfoBar>())
                        {
                            changed.Visibility(changed.IsOpen() ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
                        }
                    });
            }

            // What each change did is announced, because focus stays on the button that made it.
            for (auto const& bar : {
                ConfigurationsStatusBar(), BackupsStatusBar(), ShareStatusBar(), ShareSavedBar(),
                ImportStartStatusBar(), ImportReviewStatusBar(), ImportDoneStatusBar() })
            {
                midiapp::MakeLiveStatusRegion(bar);
            }

            midiapp::MakeLiveStatusRegion(SwitchPendingBar(), false);
            midiapp::MakeLiveStatusRegion(ChangesPendingBar(), false);

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

            // Lists and other pop-ups take on the context menus' tinted look, so they match the window.
            midiapp::UseMenuBackdropForLists();

            // 32px source for a 16px slot, so it stays crisp on a high DPI display
            AppTitleBarIcon().Source(midiapp::WindowChrome::LoadIconImageSource(IDI_APPICON, 32));

            m_environment = cfg::MakeEnvironment();

            ThisPcText().Text(res::FormatString(L"CfgThisPcFormat", m_environment.ComputerName));

            ConfigurationsElevationBar().IsOpen(!m_elevated);
            BackupsElevationBar().IsOpen(!m_elevated);
            ShareElevationBar().IsOpen(!m_elevated);

            controls::ToolTipService::SetToolTip(DeleteBackupButton(), winrt::box_value(res::GetString(L"CfgDeleteBackupTip")));

            Closed([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_closing = true;
                        strong->SaveWindowPlacement();
                        strong->m_chrome.Shutdown();
                    }
                });

            m_loaded = true;

            SelectPage(m_pageOnLoad.empty() ? PageConfigurations : std::wstring_view{ m_pageOnLoad });
            m_pageOnLoad.clear();

            Refresh();

            if (!m_packageOnLoad.empty())
            {
                auto const path = std::move(m_packageOnLoad);
                m_packageOnLoad.clear();

                OpenPackage(path);
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to set up the Configurations window.")
    }

    void ConfigurationsWindow::BringToFront() noexcept
    {
        try
        {
            if (auto const appWindow = AppWindow())
            {
                if (auto const presenter = appWindow.Presenter().try_as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>())
                {
                    if (presenter.State() == winrt::Microsoft::UI::Windowing::OverlappedPresenterState::Minimized)
                    {
                        presenter.Restore();
                    }
                }
            }

            Activate();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to bring the Configurations window forward.")
    }

    void ConfigurationsWindow::ApplyAppearance() noexcept
    {
        if (m_loaded && !m_closing)
        {
            m_chrome.ApplyTheme();
        }
    }

    void ConfigurationsWindow::ApplyAlwaysOnTop() noexcept
    {
        if (m_loaded && !m_closing)
        {
            m_chrome.ApplyAlwaysOnTop();
        }
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::SetRelaunchHandler(std::function<bool(std::wstring const&)> handler) noexcept
    {
        m_relaunchHandler = std::move(handler);
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::ShowPage(std::wstring_view const page) noexcept
    {
        try
        {
            if (!m_loaded)
            {
                m_pageOnLoad = std::wstring{ page };
                return;
            }

            SelectPage(page);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show a Configurations page.")
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::OpenPackage(std::wstring const& path) noexcept
    {
        try
        {
            if (path.empty())
            {
                return;
            }

            if (!m_loaded)
            {
                m_packageOnLoad = path;
                m_pageOnLoad = std::wstring{ PageImport };
                return;
            }

            SelectPage(PageImport);

            // runs on, and reports, by itself
            OpenPackageAsync(path);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to open a configuration package.")
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::SelectPage(std::wstring_view const page) noexcept
    {
        try
        {
            controls::NavigationViewItem item{ nullptr };

            if (engine::SameText(page, PageBackups))
            {
                item = BackupsNavItem();
            }
            else if (engine::SameText(page, PageShare))
            {
                item = ShareNavItem();
            }
            else if (engine::SameText(page, PageImport))
            {
                item = ImportNavItem();
            }
            else
            {
                item = ConfigurationsNavItem();
            }

            // raises SelectionChanged, which shows the page
            Navigation().SelectedItem(item);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to select a Configurations page.")
    }

    std::wstring ConfigurationsWindow::CurrentPageName() noexcept
    {
        try
        {
            if (BackupsPage().Visibility() == xaml::Visibility::Visible)
            {
                return std::wstring{ PageBackups };
            }

            if (SharePage().Visibility() == xaml::Visibility::Visible)
            {
                return std::wstring{ PageShare };
            }

            if (ImportPage().Visibility() == xaml::Visibility::Visible)
            {
                return std::wstring{ PageImport };
            }
        }
        catch (...)
        {
        }

        return std::wstring{ PageConfigurations };
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::OnNavigationSelectionChanged(
        controls::NavigationView const&,
        controls::NavigationViewSelectionChangedEventArgs const& args)
    {
        try
        {
            auto const container = args.SelectedItemContainer();

            if (container == nullptr)
            {
                return;
            }

            auto const tag = winrt::unbox_value_or<winrt::hstring>(container.Tag(), winrt::hstring{});

            auto const show = [&tag](std::wstring_view const page)
                {
                    return engine::SameText(std::wstring_view{ tag }, page) ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
                };

            ConfigurationsPage().Visibility(show(PageConfigurations));
            BackupsPage().Visibility(show(PageBackups));
            SharePage().Visibility(show(PageShare));
            ImportPage().Visibility(show(PageImport));
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to change the Configurations page.")
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::OnReopenElevatedClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            // The elevated copy opens on the same page, and on the same package when one is open.
            std::wstring parameters{};

            if (m_importPanel == ImportPanel::Review && !m_importPath.empty() &&
                m_importPath.find(L'"') == std::wstring::npos)
            {
                parameters = std::format(L"--{} \"{}\"", MIDI_SETTINGS_SWITCH_IMPORT, m_importPath);
            }
            else
            {
                parameters = std::format(L"--{} {}", MIDI_SETTINGS_SWITCH_CONFIGURATIONS, CurrentPageName());
            }

            if (m_relaunchHandler && m_relaunchHandler(parameters))
            {
                return;
            }

            auto const bar = CurrentPageName() == PageBackups ? BackupsStatusBar() :
                CurrentPageName() == PageShare ? ShareStatusBar() :
                CurrentPageName() == PageImport ? (m_importPanel == ImportPanel::Review ? ImportReviewStatusBar() : ImportStartStatusBar()) :
                ConfigurationsStatusBar();

            ShowStatus(bar, controls::InfoBarSeverity::Warning, res::GetString(L"CfgElevationDeclinedTitle"), res::GetString(L"CfgElevationDeclinedMessage"));
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to reopen with administrator rights.")
    }

    winrt::fire_and_forget ConfigurationsWindow::Refresh() noexcept
    {
        auto lifetime = get_strong();

        try
        {
            co_await RefreshAsync();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to read the configurations.")
    }

    foundation::IAsyncAction ConfigurationsWindow::RefreshAsync()
    {
        auto lifetime = get_strong();

        // A refresh asked for while one runs is folded into it, so the last one always wins.
        if (m_refreshing)
        {
            m_refreshAgain = true;
            co_return;
        }

        m_refreshing = true;

        try
        {
            do
            {
                m_refreshAgain = false;

                auto const environment = m_environment;

                std::vector<engine::ConfigFileEntry> configurations{};
                std::vector<engine::BackupEntry> backups{};
                std::wstring currentFileName{};
                uint32_t serviceProcessId{ 0 };

                co_await native::RunOnBackgroundAsync([&]()
                    {
                        configurations = engine::ListConfigurations(environment);
                        backups = engine::ListBackups(environment);
                        currentFileName = environment.ReadCurrentFileName ? environment.ReadCurrentFileName() : std::wstring{};
                        serviceProcessId = cfg::ServiceProcessId();
                    });

                if (m_closing)
                {
                    break;
                }

                m_configurations = std::move(configurations);

                // Two files can carry the same name inside, such as a copy made by hand. Those are
                // shown by file name instead, so the list never has two rows that look alike.
                std::vector<std::wstring> sharedNames{};

                for (size_t first = 0; first < m_configurations.size(); first++)
                {
                    for (size_t second = first + 1; second < m_configurations.size(); second++)
                    {
                        if (!m_configurations[first].ConfigName.empty() &&
                            engine::SameText(m_configurations[first].ConfigName, m_configurations[second].ConfigName))
                        {
                            sharedNames.push_back(m_configurations[first].ConfigName);
                        }
                    }
                }

                for (auto& entry : m_configurations)
                {
                    if (std::any_of(sharedNames.begin(), sharedNames.end(), [&entry](auto const& name) { return engine::SameText(name, entry.ConfigName); }))
                    {
                        entry.ConfigName.clear();
                    }
                }

                m_backups = std::move(backups);
                m_currentFileName = std::move(currentFileName);
                m_serviceProcessId = serviceProcessId;

                m_pending = cfg::ReadPendingRestart(serviceProcessId);

                // the service has restarted since, so whatever was waiting for it is done
                if (m_pending.Kinds == 0 && native::AppSettings::Current().PendingRestartKinds() != 0)
                {
                    cfg::ClearPendingRestart();
                }

                RenderAll();
            }
            while (m_refreshAgain && !m_closing);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to read the configurations.")

        m_refreshing = false;
    }

    void ConfigurationsWindow::RenderAll() noexcept
    {
        try
        {
            if (!m_loaded || m_closing)
            {
                return;
            }

            ApplyWorkState();
            RenderConfigurationsPage();
            RenderBackupsPage();
            RenderSharePage();

            if (m_importPanel == ImportPanel::Review)
            {
                RenderImportTarget();
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show the configurations.")
    }

    bool ConfigurationsWindow::BeginWork() noexcept
    {
        if (m_busy || m_closing || !m_loaded)
        {
            return false;
        }

        m_busy = true;
        ApplyWorkState();

        return true;
    }

    void ConfigurationsWindow::EndWork() noexcept
    {
        m_busy = false;

        if (!m_closing)
        {
            ApplyWorkState();
        }
    }

    // The buttons that are always there. The ones in lists check BeginWork when clicked.
    void ConfigurationsWindow::ApplyWorkState() noexcept
    {
        try
        {
            auto const canChange = m_elevated && !m_busy;
            auto const hasCurrent = FindConfiguration(RunningFileName()) != nullptr;

            RenameInUseButton().IsEnabled(canChange);
            KeepUsingButton().IsEnabled(canChange);
            BackUpInUseButton().IsEnabled(canChange && hasCurrent);
            ShareInUseButton().IsEnabled(!m_busy);
            NewConfigurationButton().IsEnabled(canChange);
            SwitchPendingRestartButton().IsEnabled(canChange);
            ChangesPendingRestartButton().IsEnabled(canChange);
            ImportRestartButton().IsEnabled(canChange);

            BackUpNowButton().IsEnabled(canChange && FindConfiguration(m_currentFileName) != nullptr);
            DeleteOldBackupsButton().IsEnabled(canChange && !m_backups.empty());
            RestoreBackupButton().IsEnabled(canChange && m_selectedBackup.has_value());
            DeleteBackupButton().IsEnabled(canChange && m_selectedBackup.has_value());
            ShareBackupButton().IsEnabled(!m_busy && m_selectedBackup.has_value());

            SavePackageButton().IsEnabled(!m_busy && !m_shareFileName.empty());

            ChoosePackageButton().IsEnabled(!m_busy);
            ImportButton().IsEnabled(canChange && m_importHasWork);
            UndoImportButton().IsEnabled(canChange);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to update the buttons.")
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::ShowStatus(
        controls::InfoBar const& bar,
        controls::InfoBarSeverity const severity,
        winrt::hstring const& title,
        winrt::hstring const& message) noexcept
    {
        try
        {
            bar.IsOpen(false);
            bar.ActionButton(nullptr);
            bar.Severity(severity);
            bar.Title(title);
            bar.Message(message);
            bar.IsOpen(true);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show a message.")
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::ShowFailure(
        controls::InfoBar const& bar,
        winrt::hstring const& title,
        engine::ConfigResult const& result) noexcept
    {
        ShowStatus(bar, controls::InfoBarSeverity::Error, title, cfg::ErrorText(result));
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::ShowSaved(controls::InfoBar const& bar, std::wstring const& path) noexcept
    {
        try
        {
            auto const separator = path.find_last_of(L"\\/");
            auto const fileName = separator == std::wstring::npos ? path : path.substr(separator + 1);

            ShowStatus(
                bar,
                controls::InfoBarSeverity::Success,
                res::FormatString(L"CfgSavedTitleFormat", fileName),
                res::GetString(L"CfgSavedMessage"));

            controls::Button button{};
            button.Content(winrt::box_value(res::GetString(L"CfgShowInFolder")));
            button.Click([path](auto&&, auto&&) { cfg::ShowInFolder(path); });

            bar.ActionButton(button);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show where a package was saved.")
    }

    _Use_decl_annotations_
    controls::ContentDialog ConfigurationsWindow::MakeDialog(
        winrt::hstring const& title,
        winrt::hstring const& primary,
        winrt::hstring const& secondary,
        winrt::hstring const& close) noexcept
    {
        controls::ContentDialog dialog{ nullptr };

        try
        {
            dialog = controls::ContentDialog{};

            dialog.XamlRoot(Content().XamlRoot());

            // A dialog made in code gets the old look unless it's given the default style.
            if (auto const style = xaml::Application::Current().Resources().TryLookup(winrt::box_value(L"DefaultContentDialogStyle")))
            {
                dialog.Style(style.try_as<xaml::Style>());
            }

            dialog.Resources().Insert(winrt::box_value(L"ContentDialogMaxWidth"), winrt::box_value(DialogMaxWidth));
            dialog.RequestedTheme(RootGrid().ActualTheme());
            dialog.Title(winrt::box_value(title));

            if (!primary.empty())
            {
                dialog.PrimaryButtonText(primary);
                dialog.DefaultButton(controls::ContentDialogButton::Primary);
            }

            if (!secondary.empty())
            {
                dialog.SecondaryButtonText(secondary);
            }

            dialog.CloseButtonText(close);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to make a dialog.")

        return dialog;
    }

    _Use_decl_annotations_
    controls::TextBlock ConfigurationsWindow::MakeDialogText(winrt::hstring const& text, bool const caption) noexcept
    {
        controls::TextBlock block{};

        try
        {
            block.Text(text);
            block.TextWrapping(xaml::TextWrapping::Wrap);
            block.Width(DialogContentWidth);

            if (caption)
            {
                block.Style(xaml::Application::Current().Resources().Lookup(winrt::box_value(L"CaptionTextBlockStyle")).as<xaml::Style>());
                block.Foreground(xaml::Application::Current().Resources().Lookup(winrt::box_value(L"TextFillColorTertiaryBrush")).as<media::Brush>());
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to make dialog text.")

        return block;
    }

    _Use_decl_annotations_
    xaml::Style ConfigurationsWindow::StyleNamed(std::wstring_view const key) noexcept
    {
        try
        {
            auto const found = RootGrid().Resources().TryLookup(winrt::box_value(winrt::hstring{ key }));

            return found != nullptr ? found.try_as<xaml::Style>() : nullptr;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    _Use_decl_annotations_
    controls::TextBlock ConfigurationsWindow::MakeText(winrt::hstring const& text, std::wstring_view const styleKey)
    {
        controls::TextBlock block{};

        if (auto const style = StyleNamed(styleKey))
        {
            block.Style(style);
        }

        block.Text(text);

        return block;
    }

    _Use_decl_annotations_
    controls::FontIcon ConfigurationsWindow::MakeGlyph(wchar_t const glyph, std::wstring_view const styleKey)
    {
        controls::FontIcon icon{};

        if (auto const style = StyleNamed(styleKey))
        {
            icon.Style(style);
        }

        icon.Glyph(winrt::hstring{ std::wstring(1, glyph) });

        // the text beside it says the same thing
        xaml::Automation::AutomationProperties::SetAccessibilityView(icon, xaml::Automation::Peers::AccessibilityView::Raw);

        return icon;
    }

    _Use_decl_annotations_
    controls::Grid ConfigurationsWindow::MakeChip(winrt::hstring const& text, std::wstring_view const kind)
    {
        controls::Grid chip{};
        chip.Height(22);
        chip.VerticalAlignment(xaml::VerticalAlignment::Center);

        xaml::Shapes::Rectangle shape{};

        if (auto const style = StyleNamed(std::format(L"Cfg{}ChipShapeStyle", kind)))
        {
            shape.Style(style);
        }

        chip.Children().Append(shape);
        chip.Children().Append(MakeText(text, std::format(L"Cfg{}ChipTextStyle", kind)));

        return chip;
    }

    foundation::IAsyncOperation<winrt::hstring> ConfigurationsWindow::AskForNameAsync(
        winrt::hstring title,
        winrt::hstring primary,
        winrt::hstring initialName,
        std::wstring renamingFileName)
    {
        auto lifetime = get_strong();

        controls::StackPanel panel{};
        panel.Spacing(8);
        panel.Width(DialogContentWidth);

        controls::TextBox box{};
        box.Header(winrt::box_value(res::GetString(L"CfgNameHeader")));
        box.Text(initialName);
        box.MaxLength(100);

        auto problem = MakeDialogText({}, true);
        problem.Visibility(xaml::Visibility::Collapsed);

        panel.Children().Append(box);
        panel.Children().Append(problem);

        auto dialog = MakeDialog(title, primary, {}, res::GetString(L"CfgCancel"));
        dialog.Content(panel);

        // Two configurations with the same name can't be told apart in the list.
        std::vector<std::wstring> taken{};

        for (auto const& entry : m_configurations)
        {
            if (!engine::SameText(entry.FileName, renamingFileName))
            {
                taken.push_back(cfg::ConfigDisplayName(entry));
            }
        }

        auto const validate = [box, problem, dialog, taken]()
            {
                try
                {
                    auto const name = engine::CleanConfigName(std::wstring{ box.Text() });

                    auto usable = !name.empty();
                    winrt::hstring message{};

                    if (usable && std::any_of(taken.begin(), taken.end(), [&name](auto const& existing) { return engine::SameText(existing, name); }))
                    {
                        usable = false;
                        message = res::FormatString(L"CfgNameTakenFormat", name);
                    }

                    problem.Text(message);
                    problem.Visibility(message.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
                    dialog.IsPrimaryButtonEnabled(usable);
                }
                catch (...)
                {
                }
            };

        box.TextChanged([validate](auto&&, auto&&) { validate(); });

        dialog.Opened([box](auto&&, auto&&)
            {
                box.Focus(xaml::FocusState::Programmatic);
                box.SelectAll();
            });

        validate();

        auto const result = co_await dialog.ShowAsync();

        if (result != controls::ContentDialogResult::Primary || m_closing)
        {
            co_return winrt::hstring{};
        }

        co_return winrt::hstring{ engine::CleanConfigName(std::wstring{ box.Text() }) };
    }

    foundation::IAsyncAction ConfigurationsWindow::RestartServiceAsync(controls::InfoBar statusBar)
    {
        auto lifetime = get_strong();

        if (!m_elevated)
        {
            co_return;
        }

        ShowStatus(statusBar, controls::InfoBarSeverity::Informational, res::GetString(L"CfgRestartingTitle"), {});

        bool succeeded{ false };
        std::wstring errorMessage{};

        co_await native::RunOnBackgroundAsync([&succeeded, &errorMessage]()
            {
                succeeded = native::config::RestartService(errorMessage);
            });

        if (m_closing)
        {
            co_return;
        }

        if (succeeded)
        {
            cfg::ClearPendingRestart();

            ShowStatus(statusBar, controls::InfoBarSeverity::Success, res::GetString(L"CfgRestartedTitle"), res::GetString(L"CfgRestartedMessage"));
        }
        else
        {
            ShowStatus(statusBar, controls::InfoBarSeverity::Error, res::GetString(L"CfgRestartFailedTitle"), winrt::hstring{ errorMessage });
        }

        co_await RefreshAsync();
    }

    _Use_decl_annotations_
    winrt::fire_and_forget ConfigurationsWindow::OnRestartServiceClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            if (!BeginWork())
            {
                co_return;
            }

            auto const fromImport = sender == ImportRestartButton();
            auto const bar = fromImport ? ImportDoneStatusBar() : ConfigurationsStatusBar();

            co_await RestartServiceAsync(bar);

            if (fromImport && !m_closing && bar.Severity() == controls::InfoBarSeverity::Success)
            {
                ImportRestartBar().IsOpen(false);
            }

            EndWork();
        }
        catch (...)
        {
            EndWork();
            MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(L"Unable to restart the MIDI service.");
        }
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::RecordPendingRestart(uint32_t const kinds, std::wstring const& runningFileName) noexcept
    {
        try
        {
            // asked again, because the service may have restarted since the last look
            auto const serviceProcessId = cfg::ServiceProcessId();

            if (serviceProcessId != m_serviceProcessId)
            {
                m_serviceProcessId = serviceProcessId;
                m_pending = cfg::ReadPendingRestart(serviceProcessId);
            }

            cfg::AddPendingRestart(serviceProcessId, kinds, runningFileName);

            m_pending = cfg::ReadPendingRestart(serviceProcessId);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to remember that the MIDI service needs a restart.")
    }

    _Use_decl_annotations_
    engine::ConfigFileEntry const* ConfigurationsWindow::FindConfiguration(std::wstring const& fileName) const noexcept
    {
        if (fileName.empty())
        {
            return nullptr;
        }

        for (auto const& entry : m_configurations)
        {
            if (engine::SameText(entry.FileName, fileName))
            {
                return &entry;
            }
        }

        return nullptr;
    }

    std::wstring ConfigurationsWindow::RunningFileName() const noexcept
    {
        try
        {
            if ((m_pending.Kinds & cfg::restartkinds::Switch) != 0)
            {
                return m_pending.RunningFileName;
            }

            return m_currentFileName;
        }
        catch (...)
        {
            return {};
        }
    }
}
