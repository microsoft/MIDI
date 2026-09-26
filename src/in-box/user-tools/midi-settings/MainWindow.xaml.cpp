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

    // The console and this app can both change the synthesizer, and there is no change notification
    // from the service, so coming back to the window is the moment to re-read it.
    _Use_decl_annotations_
    void MainWindow::OnWindowActivated(
        foundation::IInspectable const&,
        xaml::WindowActivatedEventArgs const& args)
    {
        try
        {
            if (args.WindowActivationState() == xaml::WindowActivationState::Deactivated)
            {
                return;
            }

            RefreshSynthSettings();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to refresh on activation.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRootLoaded(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            m_dispatcherQueue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

            Activated({ this, &MainWindow::OnWindowActivated });

            Title(res::GetString(L"AppTitle"));
            AppTitleTextBlock().Text(res::GetString(L"AppTitle"));

            midiapp::ApplyPreviewBadgeVisibility(PreviewChiclet());

            // The endpoint count is deliberately not a live region: it moves on every watcher
            // tick, and announcing it would talk over everything else.
            midiapp::MakeLiveStatusRegion(StatusText());
            midiapp::MakeLiveStatusRegion(DetailStatusText());
            midiapp::MakeLiveStatusRegion(CustomizeStatusText());
            midiapp::MakeLiveStatusRegion(RelinkStatusText());
            midiapp::MakeLiveStatusRegion(Midi1PortNamesStatusText());
            midiapp::MakeLiveStatusRegion(SynthStatusText());
            midiapp::MakeLiveStatusRegion(GlobalStatusText());
            midiapp::MakeLiveStatusRegion(NotificationsStatusText());

            midiapp::WindowChromeElements elements{};

            elements.Window = *this;
            elements.Root = RootGrid();
            elements.Fill = WindowFill();
            elements.Tint = WindowTint();
            elements.TitleBar = AppTitleBar();
            elements.LeftInset = TitleBarLeftInsetColumn();
            elements.RightInset = TitleBarRightInsetColumn();

            m_chrome.Initialize(elements, native::AppSettings::Current());

            // Now that there is a window, a later launch has something to bring forward.
            ::midiapp::SingleInstance::PublishMainWindow(m_chrome.WindowHandle());
            m_chrome.SetWindowIconFromResource(IDI_APPICON);

            StartListeningForLaunchRequests();

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
            BuildToolbarItems();
            ApplyToolButtons();
            ShowFirstRunInvitation();

            Closed([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_closing = true;
                        strong->StopListeningForLaunchRequests();
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

            // Another tool sent the customer here for one thing. Queued rather than shown from
            // inside Loaded, so the window behind the dialog is laid out first.
            if (App::ShowNotificationsOnLaunch() && m_dispatcherQueue != nullptr)
            {
                m_dispatcherQueue.TryEnqueue(
                    winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority::Low,
                    [weak = get_weak()]()
                    {
                        if (auto strong = weak.get(); strong != nullptr && !strong->m_closing)
                        {
                            strong->OnNotificationsClick(nullptr, xaml::RoutedEventArgs{ nullptr });
                        }
                    });
            }
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

    namespace
    {
        // These mirror the toolbar styles in MainWindow.xaml. Every part of the toolbar is a
        // fixed width, so what fits can be worked out without measuring anything.
        constexpr double ToolButtonWidth = 92.0;
        constexpr double ToolButtonCompactWidth = 40.0;
        constexpr double ToolSeparatorWidth = 13.0;         // 1px rule plus a 6px margin either side
        constexpr double ToolSeparatorCompactWidth = 9.0;   // ... and a 4px margin when compact
        constexpr double ToolButtonSpacing = 2.0;           // StackPanel.Spacing
        constexpr double ToolOverflowSlotWidth = 42.0;      // the overflow button and its spacing
    }

    void MainWindow::BuildToolbarItems() noexcept
    {
        try
        {
            auto const style = [this](wchar_t const* const key)
                {
                    auto const resources = RootGrid().Resources();
                    auto const boxed = winrt::box_value(winrt::hstring{ key });

                    return resources.HasKey(boxed) ?
                        resources.Lookup(boxed).try_as<xaml::Style>() : xaml::Style{ nullptr };
                };

            m_toolButtonStyle = style(L"ToolButtonStyle");
            m_toolButtonCompactStyle = style(L"ToolButtonCompactStyle");
            m_toolSeparatorStyle = style(L"ToolSeparatorStyle");
            m_toolSeparatorCompactStyle = style(L"ToolSeparatorCompactStyle");

            m_toolbarItems.clear();

            m_toolbarItems.push_back({ LoopbackSetupButton(), LoopbackSetupIcon(), LoopbackSetupLabel(), nullptr, native::MidiTool::LoopbackSetup, 0 });
            m_toolbarItems.push_back({ BluetoothSetupButton(), BluetoothSetupIcon(), BluetoothSetupLabel(), nullptr, native::MidiTool::BluetoothSetup, 0 });
            m_toolbarItems.push_back({ NetworkSetupButton(), NetworkSetupIcon(), NetworkSetupLabel(), nullptr, native::MidiTool::NetworkSetup, 0 });

            m_toolbarItems.push_back({ PatchbayButton(), PatchbayIcon(), PatchbayLabel(), SetupToolsSeparator(), native::MidiTool::Patchbay, 1 });
            m_toolbarItems.push_back({ SysExButton(), SysExIcon(), SysExLabel(), nullptr, native::MidiTool::SysEx, 1 });
            m_toolbarItems.push_back({ MonitorButton(), MonitorIcon(), MonitorLabel(), nullptr, native::MidiTool::Monitor, 1 });
            m_toolbarItems.push_back({ ScratchPadButton(), ScratchPadIcon(), ScratchPadLabel(), nullptr, native::MidiTool::ScratchPad, 1 });
            m_toolbarItems.push_back({ KeyboardButton(), KeyboardIcon(), KeyboardLabel(), nullptr, native::MidiTool::Keyboard, 1 });
            m_toolbarItems.push_back({ ClockButton(), ClockIcon(), ClockLabel(), nullptr, native::MidiTool::Clock, 1 });
            m_toolbarItems.push_back({ GlassButton(), GlassIcon(), GlassLabel(), nullptr, native::MidiTool::Glass, 1 });

            m_toolbarItems.push_back({ TroubleshooterButton(), TroubleshooterIcon(), TroubleshooterLabel(), UtilityToolsSeparator(), native::MidiTool::Troubleshooter, 2 });
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to build the toolbar.")
    }

    void MainWindow::ApplyToolButtons() noexcept
    {
        try
        {
            native::RefreshToolLocations();

            for (auto const& item : m_toolbarItems)
            {
                LoadToolIcon(item.Icon, item.Tool);
            }

            // which tools are installed decides how wide the toolbar wants to be
            m_toolbarLayoutDirty = true;

            ApplyToolbarLayout();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to build the toolbar.")
    }

    _Use_decl_annotations_
    void MainWindow::LoadToolIcon(
        controls::Image const& icon,
        native::MidiTool const tool) noexcept
    {
        try
        {
            auto const& location = native::GetToolLocation(tool);

            // The real icon out of the installed executable, so the toolbar matches the Start
            // Menu without this app carrying a second copy of everyone else's artwork.
            if (location.Installed && icon.Source() == nullptr)
            {
                icon.Source(midiapp::WindowChrome::LoadIconImageSourceFromFile(location.FullPath, 48));
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to load a toolbar icon.")
    }

    // Whatever does not fit moves into the overflow menu, from the right hand end. The labels
    // are the customer's choice and are honored at every width, so turning them off is how a
    // narrow window gets all of its tools back onto the bar. The two commands at the right end
    // belong to this app rather than to another tool, and are never given up.
    void MainWindow::ApplyToolbarLayout() noexcept
    {
        try
        {
            if (m_closing || m_toolbarItems.empty())
            {
                return;
            }

            std::vector<size_t> installed{};

            for (size_t index = 0; index < m_toolbarItems.size(); index++)
            {
                if (native::GetToolLocation(m_toolbarItems[index].Tool).Installed)
                {
                    installed.push_back(index);
                }
            }

            auto const available = ToolButtonHost().ActualWidth();

            if (available <= 0.0)
            {
                return;
            }

            auto const widthOf = [this, &installed](size_t const count, bool const compact)
                {
                    auto const buttonWidth = compact ? ToolButtonCompactWidth : ToolButtonWidth;
                    auto const separatorWidth = compact ? ToolSeparatorCompactWidth : ToolSeparatorWidth;

                    double width{ 0.0 };
                    uint32_t previousGroup{ 0 };

                    for (size_t position = 0; position < count; position++)
                    {
                        auto const& item = m_toolbarItems[installed[position]];

                        if (position > 0)
                        {
                            width += ToolButtonSpacing;

                            if (item.Group != previousGroup)
                            {
                                width += separatorWidth + ToolButtonSpacing;
                            }
                        }

                        width += buttonWidth;
                        previousGroup = item.Group;
                    }

                    return width;
                };

            auto const compact = !native::AppSettings::Current().ShowToolbarLabels();

            auto primaryCount = installed.size();

            if (widthOf(primaryCount, compact) > available)
            {
                auto const room = available - ToolOverflowSlotWidth;

                primaryCount = 0;

                while (primaryCount < installed.size() && widthOf(primaryCount + 1, compact) <= room)
                {
                    primaryCount++;
                }
            }

            if (!m_toolbarLayoutDirty && compact == m_toolbarCompact && primaryCount == m_toolbarPrimaryCount)
            {
                return;
            }

            m_toolbarLayoutDirty = false;
            m_toolbarCompact = compact;
            m_toolbarPrimaryCount = primaryCount;

            std::vector<bool> onToolbar(m_toolbarItems.size(), false);
            std::vector<size_t> overflow{};

            for (size_t position = 0; position < installed.size(); position++)
            {
                if (position < primaryCount)
                {
                    onToolbar[installed[position]] = true;
                }
                else
                {
                    overflow.push_back(installed[position]);
                }
            }

            bool seenOnToolbar{ false };

            for (size_t index = 0; index < m_toolbarItems.size(); index++)
            {
                auto const& item = m_toolbarItems[index];

                item.Button.Visibility(onToolbar[index] ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

                ApplyToolButtonMode(item.Button, item.Label, compact);

                if (item.SeparatorBefore != nullptr)
                {
                    // A separator with nothing on one side of it reads as a stray line, so it
                    // only appears when both of the groups it divides still have a button.
                    bool followedOnToolbar{ false };

                    for (size_t later = index; later < onToolbar.size() && !followedOnToolbar; later++)
                    {
                        followedOnToolbar = onToolbar[later];
                    }

                    item.SeparatorBefore.Visibility(seenOnToolbar && followedOnToolbar ?
                        xaml::Visibility::Visible : xaml::Visibility::Collapsed);

                    ApplyToolSeparatorMode(item.SeparatorBefore, compact);
                }

                seenOnToolbar = seenOnToolbar || onToolbar[index];
            }

            ApplyToolButtonMode(GlobalSettingsButton(), GlobalSettingsLabel(), compact);
            ApplyToolButtonMode(NotificationsButton(), NotificationsLabel(), compact);

            ApplyToolSeparatorMode(PinnedCommandsSeparator(), compact);

            PinnedCommandsSeparator().Visibility(installed.empty() ?
                xaml::Visibility::Collapsed : xaml::Visibility::Visible);

            ToolOverflowButton().Visibility(overflow.empty() ?
                xaml::Visibility::Collapsed : xaml::Visibility::Visible);

            ToolbarLabelsMenuItem().IsChecked(native::AppSettings::Current().ShowToolbarLabels());

            BuildToolOverflowMenu(overflow);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to lay out the toolbar.")
    }

    _Use_decl_annotations_
    void MainWindow::ApplyToolButtonMode(
        controls::Button const& button,
        controls::TextBlock const& label,
        bool const compact) noexcept
    {
        try
        {
            label.Visibility(compact ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

            auto const& style = compact ? m_toolButtonCompactStyle : m_toolButtonStyle;

            if (style != nullptr)
            {
                button.Style(style);
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to change a toolbar button.")
    }

    _Use_decl_annotations_
    void MainWindow::ApplyToolSeparatorMode(
        xaml::Shapes::Rectangle const& separator,
        bool const compact) noexcept
    {
        try
        {
            auto const& style = compact ? m_toolSeparatorCompactStyle : m_toolSeparatorStyle;

            if (style != nullptr)
            {
                separator.Style(style);
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to change a toolbar separator.")
    }

    _Use_decl_annotations_
    void MainWindow::BuildToolOverflowMenu(std::vector<size_t> const& overflowItems) noexcept
    {
        try
        {
            ToolOverflowFlyout().Items().Clear();

            uint32_t previousGroup{ 0 };
            bool first{ true };

            for (auto const index : overflowItems)
            {
                auto const& item = m_toolbarItems[index];

                if (!first && item.Group != previousGroup)
                {
                    ToolOverflowFlyout().Items().Append(controls::MenuFlyoutSeparator{});
                }

                controls::MenuFlyoutItem entry{};

                entry.Text(item.Label.Text());

                if (item.Icon.Source() != nullptr)
                {
                    controls::ImageIcon icon{};

                    icon.Source(item.Icon.Source());
                    entry.Icon(icon);
                }

                auto const tool = item.Tool;

                entry.Click([tool](auto&&, auto&&) { native::LaunchTool(tool); });

                ToolOverflowFlyout().Items().Append(entry);

                previousGroup = item.Group;
                first = false;
            }

            // The same choice the toolbar offers on right-click. In a window this narrow, the
            // overflow button can be the only part of the toolbar with room to be clicked.
            if (!first)
            {
                ToolOverflowFlyout().Items().Append(controls::MenuFlyoutSeparator{});
            }

            controls::ToggleMenuFlyoutItem labels{};

            labels.Text(ToolbarLabelsMenuItem().Text());
            labels.IsChecked(native::AppSettings::Current().ShowToolbarLabels());
            labels.Click({ this, &MainWindow::OnToolbarLabelsClick });

            ToolOverflowFlyout().Items().Append(labels);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to build the overflow menu.")
    }

    _Use_decl_annotations_
    void MainWindow::OnToolbarHostSizeChanged(foundation::IInspectable const&, xaml::SizeChangedEventArgs const&)
    {
        ApplyToolbarLayout();
    }

    _Use_decl_annotations_
    void MainWindow::OnToolbarLabelsClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const item = sender.try_as<controls::ToggleMenuFlyoutItem>();

            if (item == nullptr)
            {
                return;
            }

            native::AppSettings::Current().ShowToolbarLabels(item.IsChecked());

            m_toolbarLayoutDirty = true;

            ApplyToolbarLayout();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to change the toolbar labels.")
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
    void MainWindow::OnPatchbayClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        native::LaunchTool(native::MidiTool::Patchbay);
    }

    _Use_decl_annotations_
    void MainWindow::OnSysExClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        native::LaunchTool(native::MidiTool::SysEx);
    }

    _Use_decl_annotations_
    void MainWindow::OnMonitorClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        native::LaunchTool(native::MidiTool::Monitor);
    }

    _Use_decl_annotations_
    void MainWindow::OnScratchPadClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        native::LaunchTool(native::MidiTool::ScratchPad);
    }

    _Use_decl_annotations_
    void MainWindow::OnKeyboardClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        native::LaunchTool(native::MidiTool::Keyboard);
    }

    _Use_decl_annotations_
    void MainWindow::OnClockClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        native::LaunchTool(native::MidiTool::Clock);
    }

    _Use_decl_annotations_
    void MainWindow::OnGlassClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        native::LaunchTool(native::MidiTool::Glass);
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
