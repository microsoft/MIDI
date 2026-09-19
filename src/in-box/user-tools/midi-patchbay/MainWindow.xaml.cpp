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
                    [this](std::wstring endpointId) { ShowEndpointMenu(endpointId); },
                    [this]() { ZoomText().Text(resources::FormatString(L"ZoomPercentFormat",
                        static_cast<int>(std::lround(CanvasScroller().ZoomFactor() * 100)))); }
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

                        strong->m_chrome.SavePlacement();
                        strong->m_chrome.Shutdown();

                        patchbay::EndpointCatalog::Current().SetChangedHandler(nullptr);
                        patchbay::EndpointCatalog::Current().Stop();

                        // the engine holds service connections, so it is torn down off the UI
                        // thread and the window waits for nothing
                        std::thread([]() { patchbay::RouteEngine::Current().Shutdown(); }).detach();
                    }
                });

            // Starting the watcher blocks on the service, so it never happens on this thread.
            patchbay::RunOnBackgroundAsync([]()
                {
                    patchbay::EndpointCatalog::Current().Start();
                });

            if (patchbay::AppSettings::Current().MinimizeToNotificationArea())
            {
                m_tray.SetOpenHandler([weak]()
                    {
                        if (auto strong = weak.get())
                        {
                            if (auto const handle = strong->m_chrome.WindowHandle())
                            {
                                ::ShowWindow(handle, SW_RESTORE);
                                ::SetForegroundWindow(handle);
                            }
                        }
                    });

                m_tray.SetExitHandler([weak]()
                    {
                        if (auto strong = weak.get())
                        {
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

                m_tray.Show();
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to finish loading the window.")
    }

    void MainWindow::InitializeWindowChrome() noexcept
    {
        try
        {
            midiapp::WindowChromeElements elements{};

            elements.Window = *this;
            elements.Root = RootGrid();
            elements.Fill = WindowFill();
            elements.Tint = WindowTint();
            elements.TitleBar = AppTitleBar();
            elements.LeftInset = TitleBarLeftInsetColumn();
            elements.RightInset = TitleBarRightInsetColumn();

            m_chrome.Initialize(elements, patchbay::AppSettings::Current());
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

    void MainWindow::RebuildNavigation() noexcept
    {
        try
        {
            m_rebuildingNavigation = true;

            auto const previous = m_currentPatchKey;

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
                    { return !patchbay::EndpointCatalog::Current().Resolve(e).has_value(); });

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

        try
        {
            auto const zoom = std::min(1.5f, CanvasScroller().ZoomFactor() + 0.1f);

            CanvasScroller().ChangeView(nullptr, nullptr,
                winrt::box_value(zoom).as<foundation::IReference<float>>());
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to zoom in.")
    }

    _Use_decl_annotations_
    void MainWindow::OnZoomOutClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto const zoom = std::max(0.4f, CanvasScroller().ZoomFactor() - 0.1f);

            CanvasScroller().ChangeView(nullptr, nullptr,
                winrt::box_value(zoom).as<foundation::IReference<float>>());
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to zoom out.")
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
