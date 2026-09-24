// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include "App.xaml.h"
#include "BackgroundWork.h"
#include "StringResources.h"
#include "resource.h"

using namespace winrt::Microsoft::UI::Xaml;

namespace patchbay = ::midipatchbay;
namespace resources = ::midipatchbay::resources;

namespace winrt::midipatchbay::implementation
{
    namespace
    {
        constexpr int32_t DefaultWindowWidth = 1280;
        constexpr int32_t DefaultWindowHeight = 860;

        constexpr int32_t RefreshIntervalMilliseconds = 500;
    }

    MainWindow::MainWindow()
    {
        // XAML objects must not call InitializeComponent during construction; winrt::make does it
    }

    void MainWindow::RestoreWindowPlacement() noexcept
    {
        midiapp::WindowChrome::RestorePlacement(
            *this, patchbay::AppSettings::Current(), DefaultWindowWidth, DefaultWindowHeight);
    }

    void MainWindow::MinimizeAtStartup() noexcept
    {
        try
        {
            if (auto const handle = m_chrome.WindowHandle())
            {
                ::ShowWindow(handle, SW_MINIMIZE);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to start minimized.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRootLoaded(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            if (m_loaded)
            {
                return;
            }

            m_loaded = true;

            // Before anything builds a visual in code, because that is where its brushes come from.
            patchbay::ThemeBrushes::Current().Initialize(ThemeBrushSource());

            InitializeWindowChrome();
            InitializeStaticText();

            m_canvas.Initialize(
                CanvasScroller(),
                CanvasSurface(),
                MinimapSurface(),
                patchbay::PatchCanvas::Callbacks
                {
                    [this]() { OnCanvasSelectionChanged(); },
                    [this](patchbay::PatchConnection connection) { OnConnectionRequested(std::move(connection)); },
                    [this]() { OnCanvasLayoutChanged(); },
                    [this](std::wstring endpointId, foundation::Point position)
                        { ShowEndpointMenu(endpointId, position); },
                    [this](std::wstring connectionId, patchbay::PatchConnection updated)
                        { OnConnectionRetargetRequested(connectionId, std::move(updated)); },
                    [this]() { UpdateZoomText(CanvasScroller().ZoomFactor()); }
                });

            // The catalog raises its change on a watcher thread, so everything that touches XAML
            // is marshalled back rather than run there.
            auto weak = get_weak();
            auto queue = DispatcherQueue();

            // Visuals built in code cannot re-theme themselves, and changing the Windows theme
            // raises no settings change, so this is the one event that catches both.
            RootGrid().ActualThemeChanged([weak](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->RebuildCanvas();
                        strong->RefreshInspector();
                    }
                });

            // Delete is handled here rather than on the canvas so it works whichever part of the
            // canvas has focus, and it is given up to anything the customer might be typing in.
            RootGrid().KeyDown([weak](auto&&, input::KeyRoutedEventArgs const& args)
                {
                    auto strong = weak.get();

                    if (strong == nullptr || args.Key() != winrt::Windows::System::VirtualKey::Delete)
                    {
                        return;
                    }

                    if (strong->m_canvas.SelectionKind() == patchbay::CanvasSelectionKind::None)
                    {
                        return;
                    }

                    auto const focused = xaml::Input::FocusManager::GetFocusedElement(strong->Content().XamlRoot());

                    if (focused.try_as<controls::TextBox>() != nullptr ||
                        focused.try_as<controls::NumberBox>() != nullptr ||
                        focused.try_as<controls::AutoSuggestBox>() != nullptr ||
                        focused.try_as<controls::RichEditBox>() != nullptr)
                    {
                        return;
                    }

                    args.Handled(true);
                    strong->DeleteSelection();
                });

            patchbay::EndpointCatalog::Current().SetChangedHandler([weak, queue]()
                {
                    if (queue == nullptr)
                    {
                        return;
                    }

                    queue.TryEnqueue([weak]()
                        {
                            if (auto strong = weak.get())
                            {
                                strong->m_liveEndpoints = patchbay::EndpointCatalog::Current().Snapshot();
                                strong->RefreshAnalysis();
                                strong->RebuildCanvas();
                                strong->RefreshNavigationBadges();
                                strong->UpdateMessages();
                                strong->ApplyRouting();
                            }
                        });
                });

            LoadPatches();

            m_refreshTimer = xaml::DispatcherTimer{};
            m_refreshTimer.Interval(std::chrono::milliseconds{ RefreshIntervalMilliseconds });
            m_refreshTimer.Tick([weak](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->OnRefreshTimerTick();
                    }
                });
            m_refreshTimer.Start();

            // Closing the window is turned into a hide when the customer has asked us to keep
            // running, so the handler below only ever runs on a real exit.
            m_closingToken = AppWindow().Closing(
                [weak](auto&&, windowing::AppWindowClosingEventArgs const& args)
                {
                    if (auto strong = weak.get())
                    {
                        if (strong->TryHideToNotificationArea())
                        {
                            args.Cancel(true);
                        }
                    }
                });

            // Minimizing goes the same way, so the app is in one place rather than two.
            m_windowChangedToken = AppWindow().Changed(
                [weak](windowing::AppWindow const& sender, windowing::AppWindowChangedEventArgs const& args)
                {
                    if (!args.DidPresenterChange() && !args.DidVisibilityChange())
                    {
                        return;
                    }

                    if (auto strong = weak.get())
                    {
                        if (auto const presenter = sender.Presenter().try_as<windowing::OverlappedPresenter>())
                        {
                            if (presenter.State() == windowing::OverlappedPresenterState::Minimized)
                            {
                                // the live state as well, so a change raised on the way back out
                                // of the notification area cannot hide the window again
                                if (::IsIconic(strong->m_chrome.WindowHandle()))
                                {
                                    strong->TryHideToNotificationArea();
                                }
                            }
                        }
                    }
                });

            m_closedToken = this->Closed([weak](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_closing = true;
                        strong->m_canvas.Shutdown();

                        if (strong->m_refreshTimer != nullptr)
                        {
                            strong->m_refreshTimer.Stop();
                        }

                        // A window hidden in the notification area reports no useful placement,
                        // and the one from just before it was hidden is already saved.
                        if (::IsWindowVisible(strong->m_chrome.WindowHandle()))
                        {
                            strong->m_chrome.SavePlacement();
                        }

                        strong->m_chrome.Shutdown();

                        patchbay::EndpointCatalog::Current().SetChangedHandler(nullptr);
                        patchbay::EndpointCatalog::Current().Stop();

                        // the engine holds service connections, so it is torn down off the UI
                        // thread and the window waits for nothing
                        std::thread([]() { patchbay::RouteEngine::Current().Shutdown(); }).detach();
                    }
                });

            // The catalog is shared with the other MIDI tools, so it cannot reach this app's
            // telemetry by itself. Give it the same sink everything else here logs to.
            midiapp::SetEndpointErrorHandler([](std::wstring_view message)
                {
                    MIDI_PATCHBAY_LOG_GENERAL_EXCEPTION(std::wstring{ message }.c_str());
                });

            // Starting the watcher blocks on the service, so it never happens on this thread.
            patchbay::RunOnBackgroundAsync([]()
                {
                    patchbay::EndpointCatalog::Current().Start();
                });

            InitializeNotificationArea();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to finish loading the window.")
    }

    void MainWindow::InitializeNotificationArea() noexcept
    {
        try
        {
            auto weak = get_weak();

            // The handlers are wired up whether or not the icon is showing, because the setting
            // can be turned on later and an icon whose menu does nothing is worse than no icon.
            m_tray.SetOpenHandler([weak]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->RestoreFromNotificationArea();
                    }
                });

            m_tray.SetExitHandler([weak]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_exiting = true;
                        strong->Close();
                    }
                });

            m_tray.SetStopAllHandler([weak]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->StopAllRouting();
                    }
                });

            m_tray.SetTogglePatchHandler([weak](std::wstring const& key)
                {
                    if (auto strong = weak.get())
                    {
                        strong->SetPatchRouting(key, strong->m_routingPatchKeys.count(key) == 0);
                    }
                });

            if (patchbay::AppSettings::Current().MinimizeToNotificationArea())
            {
                m_tray.Show();
                UpdateTray();

                // a start minimized launch happened before there was an icon to hide behind
                if (auto const handle = m_chrome.WindowHandle())
                {
                    if (::IsIconic(handle))
                    {
                        TryHideToNotificationArea();
                    }
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to set up the notification area.")
    }

    bool MainWindow::TryHideToNotificationArea() noexcept
    {
        try
        {
            if (m_exiting || m_closing || m_restoringFromNotificationArea)
            {
                return false;
            }

            // IsVisible rather than the setting, so a machine where the icon could not be added
            // never hides the only way back to the app.
            if (!m_tray.IsVisible())
            {
                return false;
            }

            auto const handle = m_chrome.WindowHandle();

            if (handle == nullptr)
            {
                return false;
            }

            auto const minimized = ::IsIconic(handle) != FALSE;

            // already hidden, so there is nothing to do and nothing to cancel for
            if (!::IsWindowVisible(handle))
            {
                return false;
            }

            // a minimized window has no placement worth keeping, and the last good one is saved
            if (!minimized)
            {
                m_chrome.SavePlacement();
            }

            ::ShowWindow(handle, SW_HIDE);

            return true;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to hide the window to the notification area.")

        return false;
    }

    void MainWindow::RestoreFromNotificationArea() noexcept
    {
        try
        {
            auto const handle = m_chrome.WindowHandle();

            if (handle == nullptr)
            {
                return;
            }

            m_restoringFromNotificationArea = true;

            auto const reset = wil::scope_exit([this]() { m_restoringFromNotificationArea = false; });

            // A window hidden while minimized needs both steps, and the flag above is what makes
            // that safe: it is briefly visible and still minimized in between. Showing a window
            // hidden while maximized keeps it maximized, so there is no restore in that case.
            ::ShowWindow(handle, SW_SHOW);

            if (::IsIconic(handle))
            {
                ::ShowWindow(handle, SW_RESTORE);
            }

            ::SetForegroundWindow(handle);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the window from the notification area.")
    }

    void MainWindow::InitializeWindowChrome() noexcept
    {
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

            m_chrome.Initialize(elements, patchbay::AppSettings::Current());

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

            AlwaysOnTopToggle().IsChecked(patchbay::AppSettings::Current().AlwaysOnTop());
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to set up the window chrome.")
    }

    void MainWindow::InitializeStaticText() noexcept
    {
        try
        {
            ZoomText().Text(resources::FormatString(L"ZoomPercentFormat", 100));
            MinimapPanel().Visibility(xaml::Visibility::Collapsed);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to set the static text.")
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

            controls::InfoBar note{};
            note.Severity(controls::InfoBarSeverity::Informational);
            note.IsOpen(true);
            note.IsClosable(false);
            note.Message(resources::GetString(L"AppearanceRoutingNote"));

            auto weak = get_weak();

            midiapp::ShowAppearanceFlyout(
                AppearanceButton(),
                patchbay::AppSettings::Current(),
                strings,
                [weak]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_chrome.ApplyTheme();
                        strong->RebuildCanvas();
                    }
                },
                BuildAppSettingsPanel(),
                note);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the appearance flyout.")
    }

    xaml::UIElement MainWindow::BuildAppSettingsPanel() noexcept
    {
        try
        {
            controls::StackPanel panel{};
            panel.Spacing(10);
            panel.Margin(xaml::ThicknessHelper::FromLengths(0, 12, 0, 0));

            controls::TextBlock heading{};
            heading.Text(resources::GetString(L"SettingsSectionHeading"));
            heading.FontSize(12);
            heading.Foreground(patchbay::ThemeBrushes::Current().Get(L"TextFillColorTertiaryBrush"));
            panel.Children().Append(heading);

            auto weak = get_weak();

            auto const addToggle = [&panel](winrt::hstring const& header, winrt::hstring const& description,
                bool isOn, std::function<void(bool)> onChanged)
                {
                    controls::ToggleSwitch toggle{};

                    toggle.Header(winrt::box_value(header));
                    toggle.IsOn(isOn);
                    toggle.OnContent(winrt::box_value(winrt::hstring{ L"" }));
                    toggle.OffContent(winrt::box_value(winrt::hstring{ L"" }));

                    toggle.Toggled([onChanged](foundation::IInspectable const& s, xaml::RoutedEventArgs const&)
                        {
                            if (auto const control = s.try_as<controls::ToggleSwitch>())
                            {
                                onChanged(control.IsOn());
                            }
                        });

                    panel.Children().Append(toggle);

                    if (!description.empty())
                    {
                        controls::TextBlock hint{};
                        hint.Text(description);
                        hint.FontSize(11);
                        hint.TextWrapping(xaml::TextWrapping::Wrap);
                        hint.Margin(xaml::ThicknessHelper::FromLengths(0, -6, 0, 0));
                        hint.Foreground(patchbay::ThemeBrushes::Current().Get(L"TextFillColorTertiaryBrush"));
                        panel.Children().Append(hint);
                    }
                };

            addToggle(
                resources::GetString(L"SettingStartWithWindows"),
                resources::GetString(L"SettingStartWithWindowsHint"),
                patchbay::AppSettings::StartsWithWindows(),
                [weak](bool value)
                {
                    if (!patchbay::AppSettings::TrySetStartsWithWindows(value))
                    {
                        if (auto strong = weak.get())
                        {
                            strong->ShowStatus(
                                resources::GetString(L"ErrorStartupEntry"),
                                controls::InfoBarSeverity::Warning);
                        }
                    }
                });

            addToggle(
                resources::GetString(L"SettingNotificationArea"),
                resources::GetString(L"SettingNotificationAreaHint"),
                patchbay::AppSettings::Current().MinimizeToNotificationArea(),
                [weak](bool value)
                {
                    patchbay::AppSettings::Current().MinimizeToNotificationArea(value);

                    if (auto strong = weak.get())
                    {
                        if (value)
                        {
                            strong->m_tray.Show();
                            strong->UpdateTray();
                        }
                        else
                        {
                            strong->m_tray.Hide();
                        }
                    }
                });

            addToggle(
                resources::GetString(L"SettingStartMinimized"),
                {},
                patchbay::AppSettings::Current().StartMinimized(),
                [](bool value) { patchbay::AppSettings::Current().StartMinimized(value); });

            addToggle(
                resources::GetString(L"SettingActivateAtStartup"),
                {},
                patchbay::AppSettings::Current().ActivateSavedPatchesAtStartup(),
                [](bool value) { patchbay::AppSettings::Current().ActivateSavedPatchesAtStartup(value); });

            addToggle(
                resources::GetString(L"SettingWarnAboutLoops"),
                {},
                patchbay::AppSettings::Current().WarnAboutLoops(),
                [](bool value) { patchbay::AppSettings::Current().WarnAboutLoops(value); });

            return panel;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the app settings panel.")

        return nullptr;
    }

    _Use_decl_annotations_
    void MainWindow::OnAlwaysOnTopToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto const isChecked = AlwaysOnTopToggle().IsChecked();

            patchbay::AppSettings::Current().AlwaysOnTop(isChecked && isChecked.Value());
            m_chrome.ApplyAlwaysOnTop();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to change the always on top setting.")
    }

    // ------------------------------------------------------------------ patches

    _Use_decl_annotations_
    std::wstring MainWindow::PatchKey(patchbay::PatchDocument const& patch) noexcept
    {
        if (!patch.FilePath.empty())
        {
            return patch.FilePath;
        }

        return L"\u0002unsaved:" + patch.Name;
    }

    patchbay::PatchDocument* MainWindow::CurrentPatch() noexcept
    {
        if (m_currentPatchKey.empty())
        {
            return nullptr;
        }

        auto it = std::find_if(m_patches.begin(), m_patches.end(),
            [this](patchbay::PatchDocument const& p) { return PatchKey(p) == m_currentPatchKey; });

        return it == m_patches.end() ? nullptr : &(*it);
    }

    void MainWindow::LoadPatches() noexcept
    {
        try
        {
            auto& store = patchbay::PatchStore::Current();

            if (!store.LoadAll(m_patches))
            {
                ShowStatus(store.LastErrorMessage(), controls::InfoBarSeverity::Error);
            }

            if (patchbay::AppSettings::Current().ActivateSavedPatchesAtStartup())
            {
                for (auto const& patch : m_patches)
                {
                    if (patch.ActivateAtStartup)
                    {
                        m_routingPatchKeys.insert(PatchKey(patch));
                    }
                }
            }

            RebuildNavigation();

            auto const& options = App::StartupOptions();

            if (!options.PatchName.empty())
            {
                auto it = std::find_if(m_patches.begin(), m_patches.end(),
                    [&options](patchbay::PatchDocument const& p)
                    {
                        return ::CompareStringOrdinal(
                            p.Name.c_str(), -1, options.PatchName.c_str(), -1, TRUE) == CSTR_EQUAL;
                    });

                if (it != m_patches.end())
                {
                    SelectPatch(PatchKey(*it));
                    return;
                }
            }

            if (!m_patches.empty())
            {
                SelectPatch(PatchKey(m_patches.front()));
            }
            else
            {
                // Landing on nothing is not useful, so a first run opens an unsaved patch. It is
                // temporary until the customer names it, so nothing is written.
                CreateNewPatch();
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to load the patches.")
    }

    // Endpoints come and go while the window is open, so the badge that says a patch is waiting
    // for one has to be re-evaluated. Rebuilding the list on every catalog change would be far too
    // much churn, so only a change in which patches are affected is acted on.
    std::wstring MainWindow::NavigationBadgeSignature() const noexcept
    {
        std::wstring signature{};

        for (auto const& patch : m_patches)
        {
            auto const offline = std::any_of(patch.Endpoints.begin(), patch.Endpoints.end(),
                [](patchbay::PatchEndpoint const& e)
                { return !patchbay::ResolveEndpoint(e).has_value(); });

            signature += offline ? L"1" : L"0";
        }

        return signature;
    }

    void MainWindow::RefreshNavigationBadges() noexcept
    {
        try
        {
            if (NavigationBadgeSignature() != m_navigationBadgeSignature)
            {
                RebuildNavigation();
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to refresh the patch list badges.")
    }

    void MainWindow::RebuildNavigation() noexcept
    {
        try
        {
            m_rebuildingNavigation = true;

            auto const previous = m_currentPatchKey;
            m_navigationBadgeSignature = NavigationBadgeSignature();

            MainNavigation().MenuItems().Clear();

            std::vector<patchbay::PatchDocument const*> ordered{};

            for (auto const& patch : m_patches)
            {
                ordered.push_back(&patch);
            }

            if (patchbay::AppSettings::Current().SortOrder() == patchbay::PatchSortOrder::Name)
            {
                std::sort(ordered.begin(), ordered.end(),
                    [](patchbay::PatchDocument const* a, patchbay::PatchDocument const* b)
                    {
                        return ::CompareStringOrdinal(
                            a->Name.c_str(), -1, b->Name.c_str(), -1, TRUE) == CSTR_LESS_THAN;
                    });
            }
            else
            {
                // an unsaved patch has no modified time yet, so it sorts to the top where the
                // customer just created it
                std::sort(ordered.begin(), ordered.end(),
                    [](patchbay::PatchDocument const* a, patchbay::PatchDocument const* b)
                    {
                        if (a->FilePath.empty() != b->FilePath.empty())
                        {
                            return a->FilePath.empty();
                        }

                        return a->ModifiedTimestamp > b->ModifiedTimestamp;
                    });
            }

            controls::NavigationViewItem selectedItem{ nullptr };

            for (auto const* patch : ordered)
            {
                auto const key = PatchKey(*patch);

                controls::NavigationViewItem item{};

                controls::Grid content{};
                content.ColumnSpacing(8);

                controls::ColumnDefinition textColumn{};
                textColumn.Width(xaml::GridLength{ 1, xaml::GridUnitType::Star });
                content.ColumnDefinitions().Append(textColumn);

                controls::ColumnDefinition badgeColumn{};
                badgeColumn.Width(xaml::GridLength{ 0, xaml::GridUnitType::Auto });
                content.ColumnDefinitions().Append(badgeColumn);

                controls::TextBlock label{};
                label.Text(winrt::hstring{ patch->Name });
                label.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
                label.VerticalAlignment(xaml::VerticalAlignment::Center);
                content.Children().Append(label);

                // A badge is only worth showing when it means something the customer can act on.
                std::wstring badgeGlyph{};
                std::wstring_view badgeBrushKey{};

                auto const offline = std::any_of(patch->Endpoints.begin(), patch->Endpoints.end(),
                    [](patchbay::PatchEndpoint const& e)
                    { return !patchbay::ResolveEndpoint(e).has_value(); });

                if (patch->IsTemporary)
                {
                    badgeGlyph = L"\uE823";
                    badgeBrushKey = L"TextFillColorTertiaryBrush";
                }

                if (offline)
                {
                    badgeGlyph = L"\uE7BA";
                    badgeBrushKey = L"SystemFillColorCautionBrush";
                }

                if (!badgeGlyph.empty())
                {
                    controls::FontIcon badge{};
                    badge.Glyph(winrt::hstring{ badgeGlyph });
                    badge.FontSize(13);

                    if (auto const brush = patchbay::ThemeBrushes::Current().Get(badgeBrushKey))
                    {
                        badge.Foreground(brush);
                    }

                    controls::Grid::SetColumn(badge, 1);
                    content.Children().Append(badge);
                }

                item.Content(content);
                item.Tag(winrt::box_value(winrt::hstring{ key }));

                controls::FontIcon glyph{};
                glyph.Glyph(L"\uE71B");
                item.Icon(glyph);

                // hovering a patch shows its description, which is why saving asks for one
                if (!patch->Description.empty())
                {
                    controls::ToolTip tip{};
                    tip.Content(winrt::box_value(winrt::hstring{ patch->Description }));
                    controls::ToolTipService::SetToolTip(item, tip);
                }

                xaml::Automation::AutomationProperties::SetName(item, winrt::hstring{ patch->Name });

                MainNavigation().MenuItems().Append(item);

                if (key == previous)
                {
                    selectedItem = item;
                }
            }

            if (selectedItem != nullptr)
            {
                MainNavigation().SelectedItem(selectedItem);
            }

            m_rebuildingNavigation = false;
        }
        catch (...)
        {
            m_rebuildingNavigation = false;
            MIDI_PATCHBAY_LOG_GENERAL_EXCEPTION(L"Unable to rebuild the patch list.");
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnNewPatchClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        CreateNewPatch();
    }

    _Use_decl_annotations_
    void MainWindow::OnSortClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto const anchor = sender.try_as<xaml::FrameworkElement>();

            if (anchor == nullptr)
            {
                return;
            }

            controls::MenuFlyout menu{};
            auto weak = get_weak();

            auto const addItem = [&menu, weak](winrt::hstring const& text, patchbay::PatchSortOrder order)
                {
                    controls::ToggleMenuFlyoutItem item{};

                    item.Text(text);
                    item.IsChecked(patchbay::AppSettings::Current().SortOrder() == order);

                    item.Click([weak, order](auto&&, auto&&)
                        {
                            if (auto strong = weak.get())
                            {
                                patchbay::AppSettings::Current().SortOrder(order);
                                strong->RebuildNavigation();
                            }
                        });

                    menu.Items().Append(item);
                };

            addItem(resources::GetString(L"NavSortNewest"), patchbay::PatchSortOrder::Newest);
            addItem(resources::GetString(L"NavSortName"), patchbay::PatchSortOrder::Name);

            menu.ShowAt(anchor);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the sort menu.")
    }

    _Use_decl_annotations_
    void MainWindow::OnNavigationSelectionChanged(
        controls::NavigationView const& sender,
        controls::NavigationViewSelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        if (m_rebuildingNavigation)
        {
            return;
        }

        try
        {
            auto const item = args.SelectedItem().try_as<controls::NavigationViewItem>();

            if (item == nullptr)
            {
                return;
            }

            auto const tag = item.Tag();

            if (tag == nullptr)
            {
                return;
            }

            auto const key = winrt::unbox_value_or<winrt::hstring>(tag, L"");

            SelectPatch(std::wstring{ key });
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to change the selected patch.")
    }

    _Use_decl_annotations_
    void MainWindow::SelectPatch(std::wstring const& patchId) noexcept
    {
        try
        {
            m_currentPatchKey = patchId;

            m_canvas.ClearSelection();

            RefreshAnalysis();
            RebuildCanvas();
            RefreshInspector();
            UpdateMessages();
            UpdateStatusStrip();

            auto const* patch = CurrentPatch();

            UpdatePatchHeader();

            if (patch != nullptr)
            {
                m_canvas.FitToContent();
            }

            // keep the nav in step when the selection was made in code
            if (!m_rebuildingNavigation)
            {
                m_rebuildingNavigation = true;

                for (auto const& entry : MainNavigation().MenuItems())
                {
                    auto const item = entry.try_as<controls::NavigationViewItem>();

                    if (item == nullptr || item.Tag() == nullptr)
                    {
                        continue;
                    }

                    if (std::wstring{ winrt::unbox_value_or<winrt::hstring>(item.Tag(), L"") } == patchId)
                    {
                        MainNavigation().SelectedItem(item);
                        break;
                    }
                }

                m_rebuildingNavigation = false;
            }
        }
        catch (...)
        {
            m_rebuildingNavigation = false;
            MIDI_PATCHBAY_LOG_GENERAL_EXCEPTION(L"Unable to select a patch.");
        }
    }

    _Use_decl_annotations_
    void MainWindow::CreateNewPatch(std::wstring const& preferredName) noexcept
    {
        try
        {
            patchbay::PatchDocument patch{};

            patch.Name = preferredName.empty()
                ? std::wstring{ resources::GetString(L"UntitledPatchName") }
                : patchbay::SanitizeStoredString(preferredName);
            patch.IsTemporary = true;
            patch.ActivateAtStartup = true;

            // A new patch is unique by name until it is saved, so a second one does not collide
            // with the first in the key space.
            int suffix = 2;

            while (std::any_of(m_patches.begin(), m_patches.end(),
                [&patch, this](patchbay::PatchDocument const& p) { return PatchKey(p) == PatchKey(patch); }))
            {
                patch.Name = preferredName.empty()
                    ? std::wstring{ resources::FormatString(L"UntitledPatchNameFormat", suffix++) }
                    : std::wstring{ resources::FormatString(L"PatchNameSuffixFormat",
                        patchbay::SanitizeStoredString(preferredName), suffix++) };
            }

            m_patches.push_back(std::move(patch));

            auto const key = PatchKey(m_patches.back());

            // A patch routes as soon as it exists; drawing a connection with nothing happening
            // would be a puzzle rather than a feature.
            m_routingPatchKeys.insert(key);

            RebuildNavigation();
            SelectPatch(key);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to create a patch.")
    }

    _Use_decl_annotations_
    void MainWindow::OnOpenFolderClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        patchbay::PatchStore::Current().ShowFolder();
    }

    // ------------------------------------------------------------------- canvas

    void MainWindow::RebuildCanvas() noexcept
    {
        try
        {
            auto* patch = CurrentPatch();

            m_canvas.Rebuild(patch, m_liveEndpoints, m_analysis);
            m_canvas.RefreshStatus(m_routeStats);

            auto const isEmpty = patch == nullptr || patch->Endpoints.empty();

            EmptyCanvasPanel().Visibility(isEmpty ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            MinimapPanel().Visibility(isEmpty ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

            UpdateStatusStrip();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to rebuild the canvas.")
    }

    void MainWindow::OnCanvasSelectionChanged() noexcept
    {
        RefreshInspector();
    }

    void MainWindow::OnCanvasLayoutChanged() noexcept
    {
        MarkDirty();
        UpdateStatusStrip();
    }

    _Use_decl_annotations_
    void MainWindow::OnConnectionRequested(patchbay::PatchConnection connection) noexcept
    {
        try
        {
            auto* patch = CurrentPatch();

            if (patch == nullptr)
            {
                return;
            }

            if (connection.SourceEndpointId == connection.DestinationEndpointId &&
                connection.SourceGroupIndex == connection.DestinationGroupIndex)
            {
                ShowStatus(resources::GetString(L"ConnectionSelfRejected"), controls::InfoBarSeverity::Warning);
                return;
            }

            if (patch->HasConnection(
                connection.SourceEndpointId, connection.SourceGroupIndex,
                connection.DestinationEndpointId, connection.DestinationGroupIndex))
            {
                ShowStatus(resources::GetString(L"ConnectionDuplicate"), controls::InfoBarSeverity::Informational);
                return;
            }

            if (patch->Connections.size() >= patchbay::MaximumConnectionsPerPatch)
            {
                ShowStatus(resources::GetString(L"ConnectionLimitReached"), controls::InfoBarSeverity::Warning);
                return;
            }

            patch->Connections.push_back(connection);

            MarkDirty();
            RefreshAnalysis();
            RebuildCanvas();
            ApplyRouting();
            UpdateMessages();

            m_canvas.Select(patchbay::CanvasSelectionKind::Connection, connection.Id);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to add the connection.")
    }

    _Use_decl_annotations_
    void MainWindow::OnConnectionRetargetRequested(
        std::wstring const& connectionId,
        patchbay::PatchConnection updated) noexcept
    {
        try
        {
            auto* patch = CurrentPatch();

            if (patch == nullptr)
            {
                return;
            }

            auto* existing = patch->FindConnection(connectionId);

            if (existing == nullptr)
            {
                return;
            }

            if (updated.SourceEndpointId == updated.DestinationEndpointId &&
                updated.SourceGroupIndex == updated.DestinationGroupIndex)
            {
                ShowStatus(resources::GetString(L"ConnectionSelfRejected"), controls::InfoBarSeverity::Warning);
                return;
            }

            // Landing on top of a connection that already exists would leave a duplicate behind.
            for (auto const& other : patch->Connections)
            {
                if (other.Id != connectionId &&
                    other.SourceEndpointId == updated.SourceEndpointId &&
                    other.SourceGroupIndex == updated.SourceGroupIndex &&
                    other.DestinationEndpointId == updated.DestinationEndpointId &&
                    other.DestinationGroupIndex == updated.DestinationGroupIndex)
                {
                    ShowStatus(resources::GetString(L"ConnectionDuplicate"), controls::InfoBarSeverity::Informational);
                    return;
                }
            }

            // The filter and the transform belong to the cord, so they move with it.
            existing->SourceEndpointId = updated.SourceEndpointId;
            existing->SourceGroupIndex = updated.SourceGroupIndex;
            existing->DestinationEndpointId = updated.DestinationEndpointId;
            existing->DestinationGroupIndex = updated.DestinationGroupIndex;

            MarkDirty();
            RefreshAnalysis();
            RebuildCanvas();
            RefreshInspector();
            ApplyRouting();
            UpdateMessages();

            m_canvas.Select(patchbay::CanvasSelectionKind::Connection, connectionId);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to move the connection.")
    }

    _Use_decl_annotations_
    void MainWindow::OnAutoArrangeClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        m_canvas.AutoArrange();
    }

    _Use_decl_annotations_
    void MainWindow::OnFitClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        m_canvas.FitToContent();
    }

    _Use_decl_annotations_
    void MainWindow::OnZoomInClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyZoom(CanvasScroller().ZoomFactor() + 0.1f);
    }

    _Use_decl_annotations_
    void MainWindow::OnZoomOutClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyZoom(CanvasScroller().ZoomFactor() - 0.1f);
    }

    _Use_decl_annotations_
    void MainWindow::ApplyZoom(float zoom) noexcept
    {
        try
        {
            // Snapped to whole steps. Adding 0.1f repeatedly drifts, and the drift shows up as
            // 101% where the customer expects to land back on 100.
            auto const snapped = std::clamp(std::round(zoom * 20.0f) / 20.0f, 0.4f, 1.5f);

            CanvasScroller().ChangeView(nullptr, nullptr,
                winrt::box_value(snapped).as<foundation::IReference<float>>());

            UpdateZoomText(snapped);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to change the zoom.")
    }

    _Use_decl_annotations_
    void MainWindow::UpdateZoomText(float zoom) noexcept
    {
        try
        {
            ZoomText().Text(resources::FormatString(L"ZoomPercentFormat",
                static_cast<int>(std::lround(zoom * 100))));
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the zoom level.")
    }

    _Use_decl_annotations_
    void MainWindow::OnZoomFlyoutOpening(foundation::IInspectable const& sender, foundation::IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            m_settingZoomBox = true;
            ZoomInputBox().Value(std::lround(CanvasScroller().ZoomFactor() * 100));
            m_settingZoomBox = false;
        }
        catch (...)
        {
            m_settingZoomBox = false;
            MIDI_PATCHBAY_LOG_GENERAL_EXCEPTION(L"Unable to show the zoom box.");
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnZoomValueChanged(
        controls::NumberBox const& sender,
        controls::NumberBoxValueChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        if (m_settingZoomBox)
        {
            return;
        }

        // NumberBox raises this on commit, not per keystroke, so Enter and moving focus away
        // both land here and a half typed number never takes effect.
        auto const value = args.NewValue();

        if (!std::isnan(value))
        {
            ApplyZoom(static_cast<float>(value / 100.0));
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnZoomApplyClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            // Taking focus is what makes the number box commit what was typed.
            ZoomApplyButton().Focus(xaml::FocusState::Programmatic);

            ZoomFlyout().Hide();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to apply the zoom.")
    }

    _Use_decl_annotations_
    void MainWindow::OnInspectorCloseClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        m_canvas.ClearSelection();
    }

    _Use_decl_annotations_
    void MainWindow::OnShowLoopClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            if (m_analysis.Loops.empty() || m_analysis.Loops.front().ConnectionIds.empty())
            {
                return;
            }

            m_canvas.Select(patchbay::CanvasSelectionKind::Connection, m_analysis.Loops.front().ConnectionIds.front());
            m_canvas.FitToContent();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the loop.")
    }
}
