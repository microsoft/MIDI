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
#include "SettingsDialog.xaml.h"

#include "EndpointChoice.h"
#include "MessageRowPanel.h"
#include "MidiMessageViewModel.h"
#include "MonitorColumn.h"
#include "MonitorPalette.h"
#include "NamedChoice.h"
#include "StringResources.h"

namespace native = ::midi2monitor;
namespace res = ::midi2monitor::resources;

namespace winrt::midi2monitor::implementation
{
    namespace
    {
        // accepts "125" and "125%", with surrounding spaces
        bool TryParseZoomText(std::wstring_view text, uint32_t& percent) noexcept
        {
            uint32_t value{ 0 };
            bool sawDigit{ false };

            for (auto const ch : text)
            {
                if (ch >= L'0' && ch <= L'9')
                {
                    value = (value * 10) + static_cast<uint32_t>(ch - L'0');
                    sawDigit = true;

                    if (value > 10000)
                    {
                        return false;
                    }
                }
                else if (ch != L'%' && ch != L' ')
                {
                    return false;
                }
            }

            if (!sawDigit)
            {
                return false;
            }

            percent = value;
            return true;
        }

        bool IsToggleOn(controls::Primitives::ToggleButton const& button) noexcept
        {
            auto const value = button.IsChecked();
            return value != nullptr && value.Value();
        }
    }
    _Use_decl_annotations_
    void MainWindow::OnRootLoaded(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_initialized)
        {
            return;
        }

        m_initialized = true;

        // Each phase guards itself, so one failure degrades the app rather than leaving it
        // half built with no explanation.
        InitializeWindowChrome();
        InitializeCollections();
        InitializeColumns();
        InitializeControlsFromSettings();
        InitializePipeline();
        BuildChannelList();
        StartEndpointWatcher();

        UpdateCommandStates();
        UpdateCaptureButtonLayout();
        UpdateStatusBarLayout();
        UpdateStatusLine();
        ApplyStartupOptions();

        // Programmatic rather than keyboard focus, so the device picker is where typing and
        // tabbing begin without drawing a focus rectangle on launch.
        try
        {
            EndpointComboBox().Focus(xaml::FocusState::Programmatic);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to set the initial focus.")
    }

    void MainWindow::InitializeCollections() noexcept
    {
        try
        {
            m_dispatcherQueue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

            m_endpoints = winrt::single_threaded_observable_vector<midi2monitor::EndpointChoice>();
            m_groups = winrt::single_threaded_observable_vector<midi2monitor::NamedChoice>();
            m_channels = winrt::single_threaded_observable_vector<midi2monitor::NamedChoice>();
            m_columns = winrt::single_threaded_observable_vector<midi2monitor::MonitorColumn>();

            EndpointComboBox().ItemsSource(m_endpoints);
            GroupComboBox().ItemsSource(m_groups);
            ChannelComboBox().ItemsSource(m_channels);
            ColumnsItemsControl().ItemsSource(m_columns);

            m_listSource = winrt::make_self<native::MessageListSource>(m_pipeline);
            MessagesListView().ItemsSource(m_listSource.as<foundation::IInspectable>());
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to create the window collections.")
    }

    void MainWindow::InitializePipeline() noexcept
    {
        try
        {
            m_pipeline.PostCapacity(native::AppSettings::Current().RetainedMessageCount());

            m_pipeline.ContentChangedHandler(
                [weak = get_weak(), queue = m_dispatcherQueue]()
                {
                    if (queue == nullptr)
                    {
                        return;
                    }

                    queue.TryEnqueue([weak]()
                        {
                            if (auto strong = weak.get())
                            {
                                strong->OnPipelineContentChanged();
                            }
                        });
                });

            m_pipeline.Start();

            Closed([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->SaveWindowPlacement();
                        strong->StopMonitoring(false);
                        strong->StopEndpointWatcher();
                        strong->m_pipeline.Stop();
                    }
                });
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to start the capture pipeline.")
    }

    void MainWindow::InitializeWindowChrome() noexcept
    {
        try
        {
            UpdateWindowTitle();

            ExtendsContentIntoTitleBar(true);
            SetTitleBar(AppTitleBar());

            ApplyTitleBarColors();
            UpdateTitleBarInsets();
            ApplyTheme();
            ApplyBackdrop();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to set up the window chrome.")
    }

    void MainWindow::RestoreWindowPlacement() noexcept
    {
        try
        {
            auto appWindow = AppWindow();

            if (appWindow == nullptr)
            {
                return;
            }

            auto const& saved = native::AppSettings::Current().WindowPlacement();

            if (!saved.Valid)
            {
                appWindow.Resize(winrt::Windows::Graphics::SizeInt32{ 1280, 800 });
                return;
            }

            winrt::Windows::Graphics::RectInt32 bounds{ saved.X, saved.Y, saved.Width, saved.Height };

            // The saved monitor may be gone or smaller now, so pull the window back onto a
            // display that actually exists before showing it.
            auto const display = winrt::Microsoft::UI::Windowing::DisplayArea::GetFromRect(
                bounds, winrt::Microsoft::UI::Windowing::DisplayAreaFallback::Nearest);

            if (display != nullptr)
            {
                auto const work = display.WorkArea();

                bounds.Width = std::min(bounds.Width, work.Width);
                bounds.Height = std::min(bounds.Height, work.Height);
                bounds.X = std::clamp(bounds.X, work.X, work.X + work.Width - bounds.Width);
                bounds.Y = std::clamp(bounds.Y, work.Y, work.Y + work.Height - bounds.Height);
            }

            appWindow.MoveAndResize(bounds);

            if (saved.Maximized)
            {
                if (auto presenter = appWindow.Presenter().try_as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>())
                {
                    presenter.Maximize();
                }
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to restore the saved window position.")
    }

    void MainWindow::SaveWindowPlacement() noexcept
    {
        try
        {
            auto const handle = WindowHandle();

            if (handle == nullptr)
            {
                return;
            }

            // WINDOWPLACEMENT carries the restore rectangle, so a window closed while maximized
            // still reopens at the size the customer chose.
            WINDOWPLACEMENT placement{};
            placement.length = sizeof(placement);

            if (!::GetWindowPlacement(handle, &placement))
            {
                return;
            }

            native::WindowPlacementInfo info{};

            info.X = placement.rcNormalPosition.left;
            info.Y = placement.rcNormalPosition.top;
            info.Width = placement.rcNormalPosition.right - placement.rcNormalPosition.left;
            info.Height = placement.rcNormalPosition.bottom - placement.rcNormalPosition.top;
            info.Maximized = placement.showCmd == SW_SHOWMAXIMIZED;

            native::AppSettings::Current().WindowPlacement(info);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to save the window position.")
    }

    void MainWindow::ApplyBackdrop() noexcept
    {
        try
        {
            auto const backdrop = native::AppSettings::Current().Backdrop();

            // Assigning a new backdrop object tears the material down and rebuilds it, which
            // flickers. Only do it when the material actually changes.
            if (!m_backdropApplied || m_appliedBackdrop != backdrop)
            {
                switch (backdrop)
                {
                case native::WindowBackdrop::Mica:
                    SystemBackdrop(media::MicaBackdrop{});
                    break;

                case native::WindowBackdrop::Acrylic:
                    SystemBackdrop(media::DesktopAcrylicBackdrop{});
                    break;

                default:
                    SystemBackdrop(nullptr);
                    break;
                }

                m_appliedBackdrop = backdrop;
                m_backdropApplied = true;
            }

            // a system material only shows if the window is not painting over it
            WindowFill().Visibility(backdrop == native::WindowBackdrop::Solid
                ? xaml::Visibility::Visible
                : xaml::Visibility::Collapsed);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to apply the window backdrop.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRootSizeChanged(foundation::IInspectable const&, xaml::SizeChangedEventArgs const&)
    {
        UpdateTitleBarInsets();
        UpdateCaptureButtonLayout();
        UpdateStatusBarLayout();
    }

    void MainWindow::UpdateStatusBarLayout() noexcept
    {
        try
        {
            auto const width = RootGrid().ActualWidth();

            if (width <= 0)
            {
                return;
            }

            // the export button is centred on the whole bar, so each side only gets half the
            // width. Shed the least important controls first rather than letting them collide.
            constexpr double ZoomSliderMinimumWidth = 800.0;
            constexpr double ZoomPresetsMinimumWidth = 730.0;
            constexpr double MessageCountsMinimumWidth = 690.0;

            auto const visible = [](bool show)
                {
                    return show ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
                };

            ZoomSlider().Visibility(visible(width >= ZoomSliderMinimumWidth));
            ZoomButton().Visibility(visible(width >= ZoomPresetsMinimumWidth));

            auto const showCounts = visible(width >= MessageCountsMinimumWidth);

            TotalCountText().Visibility(showCounts);
            RealTimeCountText().Visibility(showCounts);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to lay out the status bar.")
    }

    void MainWindow::UpdateTitleBarInsets() noexcept
    {
        try
        {
            auto appWindow = AppWindow();

            if (appWindow == nullptr)
            {
                return;
            }

            auto const titleBar = appWindow.TitleBar();

            auto scale = 1.0;

            if (auto const xamlRoot = RootGrid().XamlRoot())
            {
                scale = xamlRoot.RasterizationScale();
            }

            if (scale <= 0.0)
            {
                scale = 1.0;
            }

            // insets are physical pixels; XAML columns are in DIPs
            TitleBarLeftInsetColumn().Width(xaml::GridLength{ titleBar.LeftInset() / scale, xaml::GridUnitType::Pixel });
            TitleBarRightInsetColumn().Width(xaml::GridLength{ titleBar.RightInset() / scale, xaml::GridUnitType::Pixel });
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to update the title bar insets.")
    }

    void MainWindow::ApplyTitleBarColors() noexcept
    {
        try
        {
            auto appWindow = AppWindow();

            if (appWindow == nullptr)
            {
                return;
            }

            auto titleBar = appWindow.TitleBar();

            auto const transparent = winrt::Windows::UI::Colors::Transparent();

            titleBar.ButtonBackgroundColor(transparent);
            titleBar.ButtonInactiveBackgroundColor(transparent);

            auto const dark = (RootGrid().ActualTheme() == xaml::ElementTheme::Dark);

            auto const foreground = dark ? winrt::Windows::UI::Colors::White() : winrt::Windows::UI::Colors::Black();

            titleBar.ButtonForegroundColor(foreground);
            titleBar.ButtonHoverForegroundColor(foreground);
            titleBar.ButtonPressedForegroundColor(foreground);
            titleBar.ButtonInactiveForegroundColor(dark ? winrt::Windows::UI::Colors::Gray() : winrt::Windows::UI::Colors::DimGray());
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to apply the title bar colours.")
    }

    void MainWindow::ApplyTheme() noexcept
    {
        try
        {
            auto const theme = native::AppSettings::Current().Theme();

            auto const requested =
                theme == native::AppTheme::Light ? xaml::ElementTheme::Light :
                theme == native::AppTheme::Dark ? xaml::ElementTheme::Dark :
                xaml::ElementTheme::Default;

            RootGrid().RequestedTheme(requested);

            native::MonitorPalette::Invalidate();
            ApplyTitleBarColors();

            // the tinted backgrounds are built from theme colours, so they need rebuilding
            if (m_initialized)
            {
                ApplyBackdrop();
            }

            if (m_listSource != nullptr)
            {
                m_listSource->Reset();
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to apply the theme.")
    }

    void MainWindow::ApplyRetention() noexcept
    {
        m_pipeline.PostCapacity(native::AppSettings::Current().RetainedMessageCount());
    }

    void MainWindow::ApplyMessageNameSetting() noexcept
    {
        if (m_listSource != nullptr)
        {
            auto const& settings = native::AppSettings::Current();

            m_listSource->FormattingOptions(
                settings.TimestampFormat(),
                settings.ShowMessageNameChiclets(),
                native::AppSettings::BaseRowFontSize * settings.TableZoomPercent() / 100.0);
        }
    }

    _Use_decl_annotations_
    void MainWindow::ApplyZoom(uint32_t zoomPercent, bool updateSlider, bool updateChoice) noexcept
    {
        try
        {
            auto& settings = native::AppSettings::Current();

            settings.TableZoomPercent(zoomPercent);

            auto const percent = settings.TableZoomPercent();
            auto const scale = percent / 100.0;

            // rows inherit their size from the list, so one assignment scales every cell
            MessagesListView().FontSize(native::AppSettings::BaseRowFontSize * scale);

            auto const headerFontSize = native::AppSettings::BaseRowFontSize * scale;

            IndexHeaderText().FontSize(headerFontSize);
            TimestampHeaderText().FontSize(headerFontSize);
            DataHeaderText().FontSize(headerFontSize);
            GroupHeaderText().FontSize(headerFontSize);
            ChannelHeaderText().FontSize(headerFontSize);
            DecodedHeaderText().FontSize(headerFontSize);
            DeltaHeaderText().FontSize(headerFontSize);

            m_suppressZoomHandling = true;

            if (updateSlider)
            {
                ZoomSlider().Value(static_cast<double>(percent));
            }

            if (updateChoice)
            {
                ZoomButtonText().Text(winrt::hstring{ std::format(L"{}%", percent) });
            }

            m_suppressZoomHandling = false;

            // the chiclet size lives on the row view models, so they have to be rebuilt
            ApplyMessageNameSetting();
        }
        catch (...)
        {
            m_suppressZoomHandling = false;
            MIDI_MONITOR_LOG_GENERAL_EXCEPTION(L"Unable to apply the table zoom.");
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnZoomSliderValueChanged(foundation::IInspectable const&, controls::Primitives::RangeBaseValueChangedEventArgs const& args)
    {
        if (m_suppressZoomHandling || !m_initialized)
        {
            return;
        }

        ApplyZoom(static_cast<uint32_t>(args.NewValue()), false, true);
    }

    _Use_decl_annotations_
    void MainWindow::OnZoomPresetClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const item = sender.try_as<controls::MenuFlyoutItem>();

            if (item == nullptr)
            {
                return;
            }

            uint32_t percent{ 0 };

            if (TryParseZoomText(winrt::unbox_value_or<winrt::hstring>(item.Tag(), winrt::hstring{}), percent))
            {
                ApplyZoom(percent, true, true);
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to change the zoom level.")
    }

    _Use_decl_annotations_
    void MainWindow::OnZoomOutClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto const current = native::AppSettings::Current().TableZoomPercent();

        ApplyZoom(current > native::AppSettings::MinimumZoomPercent + 10 ? current - 10 : native::AppSettings::MinimumZoomPercent, true, true);
    }

    _Use_decl_annotations_
    void MainWindow::OnZoomInClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto const current = native::AppSettings::Current().TableZoomPercent();

        ApplyZoom(current + 10, true, true);
    }

    void MainWindow::InitializeControlsFromSettings() noexcept
    {
        try
        {
            auto& settings = native::AppSettings::Current();

            m_suppressSelectionHandling = true;

            ShowClockToggle().IsChecked(settings.ShowClockMessages());
            ShowActiveSenseToggle().IsChecked(settings.ShowActiveSenseMessages());
            AlwaysOnTopToggle().IsChecked(settings.AlwaysOnTop());
            TimestampFormatComboBox().SelectedIndex(static_cast<int32_t>(settings.TimestampFormat()));
            AutoHideColumnsCheckBox().IsChecked(settings.AutoHideColumnsWhenNarrow());
            ShowMessageNamesCheckBox().IsChecked(settings.ShowMessageNameChiclets());

            m_suppressSelectionHandling = false;

            native::ColumnLayoutState::AutoHideWhenNarrow(settings.AutoHideColumnsWhenNarrow());

            ApplyHiddenTraits();
            ApplyMessageNameSetting();
            ApplyZoom(settings.TableZoomPercent(), true, true);

            if (settings.AlwaysOnTop())
            {
                OnAlwaysOnTopToggled(nullptr, nullptr);
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to apply saved settings to the window.")
    }

    void MainWindow::InitializeColumns() noexcept
    {
        try
        {
            native::ColumnLayoutState::Deserialize(native::AppSettings::Current().ColumnLayout());
            RebuildColumnFlyoutItems();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to build the column list.")
    }

    void MainWindow::RebuildColumnFlyoutItems() noexcept
    {
        try
        {
            static std::array<std::wstring_view, native::MonitorColumnCount> const headerKeys
            {
                L"ColumnHeaderIndexText",
                L"ColumnHeaderTimestampText",
                L"ColumnHeaderDataText",
                L"ColumnHeaderGroupText",
                L"ColumnHeaderChannelText",
                L"ColumnHeaderDecodedText",
                L"ColumnHeaderDeltaText"
            };

            m_columns.Clear();

            for (auto const& entry : native::ColumnLayoutState::Entries())
            {
                auto const index = static_cast<size_t>(entry.Id);

                m_columns.Append(winrt::make<MonitorColumn>(
                    static_cast<int32_t>(entry.Id),
                    res::GetString(headerKeys[index]),
                    entry.IsVisible));
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to refresh the column list.")
    }

    void MainWindow::PersistColumnLayout() noexcept
    {
        native::AppSettings::Current().ColumnLayout(native::ColumnLayoutState::Serialize());
    }

    void MainWindow::RefreshColumnsAfterReset() noexcept
    {
        RebuildColumnFlyoutItems();
        InvalidateRowLayout();
    }

    void MainWindow::InvalidateRowLayout() noexcept
    {
        try
        {
            HeaderRowPanel().InvalidateMeasure();

            // rows are recycled, so the cheapest way to re-lay-out every one of them is a reset
            if (m_listSource != nullptr)
            {
                m_listSource->Reset();
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to invalidate the message row layout.")
    }

    void MainWindow::BuildChannelList() noexcept
    {
        try
        {
            m_channels.Clear();
            m_channels.Append(winrt::make<NamedChoice>(res::GetString(L"ChannelChoiceAll"), 0));

            for (int32_t channel = 1; channel <= 16; channel++)
            {
                m_channels.Append(winrt::make<NamedChoice>(
                    res::FormatString(L"ChannelChoiceFormat", channel), channel));
            }

            ChannelComboBox().SelectedIndex(0);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to build the channel list.")
    }

    // ------------------------------------------------------------------------------------
    // Message list refresh. Called on the UI thread from a coalesced worker notification.
    // ------------------------------------------------------------------------------------

    void MainWindow::OnPipelineContentChanged() noexcept
    {
        m_pipeline.AcknowledgeContentChanged();

        RefreshMessageList();
        UpdateStatusLine();
    }

    void MainWindow::RefreshMessageList() noexcept
    {
        try
        {
            if (m_listSource == nullptr)
            {
                return;
            }

            m_listSource->Refresh();

            auto const hasRows = m_listSource->Size() > 0;

            EmptyStatePanel().Visibility(hasRows ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to refresh the message list.")
    }

    void MainWindow::UpdateStatusLine() noexcept
    {
        try
        {
            auto const snapshot = m_pipeline.Snapshot();

            TotalCountText().Text(res::FormatString(L"StatusTotalMessagesFormat", snapshot.TotalMessageCount));
            RealTimeCountText().Text(res::FormatString(L"StatusRealTimeMessagesFormat", snapshot.RealTimeMessageCount));

            if (snapshot.DroppedMessageCount > 0)
            {
                DroppedCountText().Text(res::FormatString(L"StatusDroppedMessagesFormat", snapshot.DroppedMessageCount));
                DroppedCountText().Visibility(xaml::Visibility::Visible);
            }
            else
            {
                DroppedCountText().Visibility(xaml::Visibility::Collapsed);
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to update the status line.")
    }

    // ------------------------------------------------------------------------------------
    // Option handlers
    // ------------------------------------------------------------------------------------

    void MainWindow::ApplyHiddenTraits() noexcept
    {
        auto& settings = native::AppSettings::Current();

        auto traits = native::MessageTraits::None;

        if (!settings.ShowClockMessages())
        {
            traits = traits | native::MessageTraits::Clock;
        }

        if (!settings.ShowActiveSenseMessages())
        {
            traits = traits | native::MessageTraits::ActiveSense;
        }

        m_pipeline.PostHiddenTraits(traits);
    }

    _Use_decl_annotations_
    void MainWindow::OnShowClockToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_suppressSelectionHandling)
        {
            return;
        }

        try
        {
            native::AppSettings::Current().ShowClockMessages(IsToggleOn(ShowClockToggle()));
            ApplyHiddenTraits();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to change the clock message filter.")
    }

    _Use_decl_annotations_
    void MainWindow::OnShowActiveSenseToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_suppressSelectionHandling)
        {
            return;
        }

        try
        {
            native::AppSettings::Current().ShowActiveSenseMessages(IsToggleOn(ShowActiveSenseToggle()));
            ApplyHiddenTraits();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to change the active sense message filter.")
    }

    _Use_decl_annotations_
    void MainWindow::OnAlwaysOnTopToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const isOn = IsToggleOn(AlwaysOnTopToggle());

            if (!m_suppressSelectionHandling)
            {
                native::AppSettings::Current().AlwaysOnTop(isOn);
            }

            auto appWindow = AppWindow();

            if (appWindow == nullptr)
            {
                return;
            }

            if (auto presenter = appWindow.Presenter().try_as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>())
            {
                presenter.IsAlwaysOnTop(isOn);
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to change the always on top setting.")
    }

    _Use_decl_annotations_
    void MainWindow::OnTimestampFormatChanged(foundation::IInspectable const&, controls::SelectionChangedEventArgs const&)
    {
        if (m_suppressSelectionHandling)
        {
            return;
        }

        try
        {
            auto const index = TimestampFormatComboBox().SelectedIndex();

            if (index < 0)
            {
                return;
            }

            native::AppSettings::Current().TimestampFormat(static_cast<native::TimestampDisplayFormat>(index));
            ApplyMessageNameSetting();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to change the timestamp format.")
    }

    _Use_decl_annotations_
    void MainWindow::OnColumnVisibilityChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto checkBox = sender.try_as<controls::CheckBox>();

            if (checkBox == nullptr)
            {
                return;
            }

            auto column = checkBox.DataContext().try_as<midi2monitor::MonitorColumn>();

            if (column == nullptr)
            {
                return;
            }

            // read the control, not the view model: this event runs before the two-way binding
            // pushes the new value to the source, so the view model is still one click behind
            auto const isChecked = checkBox.IsChecked();
            auto const visible = isChecked != nullptr && isChecked.Value();

            column.IsVisible(visible);

            for (auto& entry : native::ColumnLayoutState::Entries())
            {
                if (static_cast<int32_t>(entry.Id) == column.ColumnId())
                {
                    entry.IsVisible = visible;
                    break;
                }
            }

            PersistColumnLayout();
            InvalidateRowLayout();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to change column visibility.")
    }

    _Use_decl_annotations_
    void MainWindow::OnMoveColumnUpClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        MoveColumn(sender, -1);
    }

    _Use_decl_annotations_
    void MainWindow::OnMoveColumnDownClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        MoveColumn(sender, 1);
    }

    _Use_decl_annotations_
    void MainWindow::MoveColumn(foundation::IInspectable const& sender, int32_t offset) noexcept
    {
        try
        {
            auto button = sender.try_as<controls::Button>();

            if (button == nullptr)
            {
                return;
            }

            auto const boxedId = button.Tag();

            if (boxedId == nullptr)
            {
                return;
            }

            auto const columnId = winrt::unbox_value<int32_t>(boxedId);

            auto& entries = native::ColumnLayoutState::Entries();

            auto const position = std::find_if(entries.begin(), entries.end(),
                [columnId](native::ColumnDisplayEntry const& entry)
                {
                    return static_cast<int32_t>(entry.Id) == columnId;
                });

            if (position == entries.end())
            {
                return;
            }

            auto const index = static_cast<int32_t>(std::distance(entries.begin(), position));
            auto const target = index + offset;

            if (target < 0 || target >= static_cast<int32_t>(entries.size()))
            {
                return;
            }

            std::swap(entries[static_cast<size_t>(index)], entries[static_cast<size_t>(target)]);

            PersistColumnLayout();
            RebuildColumnFlyoutItems();
            InvalidateRowLayout();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to move the column.")
    }

    _Use_decl_annotations_
    void MainWindow::OnAutoHideColumnsChanged(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_suppressSelectionHandling)
        {
            return;
        }

        try
        {
            auto const isChecked = AutoHideColumnsCheckBox().IsChecked();
            auto const value = isChecked != nullptr && isChecked.Value();

            native::AppSettings::Current().AutoHideColumnsWhenNarrow(value);
            native::ColumnLayoutState::AutoHideWhenNarrow(value);

            InvalidateRowLayout();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to change the automatic column hiding setting.")
    }

    _Use_decl_annotations_
    void MainWindow::OnShowMessageNamesChanged(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_suppressSelectionHandling)
        {
            return;
        }

        try
        {
            auto const isChecked = ShowMessageNamesCheckBox().IsChecked();

            native::AppSettings::Current().ShowMessageNameChiclets(isChecked != nullptr && isChecked.Value());
            ApplyMessageNameSetting();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to change the message name setting.")
    }

    _Use_decl_annotations_
    void MainWindow::OnEditColumnsClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            controls::Primitives::FlyoutBase::ShowAttachedFlyout(HeaderRowBorder());
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to show the column options.")
    }

    _Use_decl_annotations_
    void MainWindow::OnDisplayOptionsClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            controls::Primitives::FlyoutBase::ShowAttachedFlyout(HeaderRowPanel());
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to show the display options.")
    }

    _Use_decl_annotations_
    void MainWindow::OnCommentTextChanged(foundation::IInspectable const& sender, controls::TextChangedEventArgs const&)
    {
        try
        {
            auto textBox = sender.try_as<controls::TextBox>();

            if (textBox == nullptr)
            {
                return;
            }

            auto item = textBox.DataContext().try_as<midi2monitor::MidiMessageViewModel>();

            if (item == nullptr)
            {
                return;
            }

            auto const text = textBox.Text();

            item.Comment(text);
            m_pipeline.TrySetComment(item.Sequence(), text);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to save the message comment.")
    }

    // ------------------------------------------------------------------------------------
    // Commands
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    void MainWindow::OnClearCaptureClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        ConfirmClearCaptureAsync();
    }

    winrt::fire_and_forget MainWindow::ConfirmClearCaptureAsync()
    {
        auto lifetime = get_strong();

        try
        {
            controls::ContentDialog dialog{};

            dialog.XamlRoot(RootGrid().XamlRoot());
            dialog.Title(winrt::box_value(res::GetString(L"ClearConfirmTitle")));
            dialog.Content(winrt::box_value(res::GetString(L"ClearConfirmBody")));
            dialog.PrimaryButtonText(res::GetString(L"ClearConfirmPrimary"));
            dialog.CloseButtonText(res::GetString(L"CommonCancel"));
            dialog.DefaultButton(controls::ContentDialogButton::Close);

            auto const result = co_await dialog.ShowAsync();

            if (result == controls::ContentDialogResult::Primary)
            {
                m_pipeline.PostClear();
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to clear the capture.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSettingsClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto content = winrt::make_self<SettingsDialog>();

            content->Owner(*this);
            content->RequestedTheme(RootGrid().RequestedTheme());

            // a Flyout rather than a ContentDialog: it light dismisses, and picks up the
            // standard overlay corner radius and shadow for free
            controls::Flyout flyout{};

            flyout.Content(content.as<xaml::UIElement>());
            flyout.Placement(controls::Primitives::FlyoutPlacementMode::TopEdgeAlignedLeft);
            flyout.ShowAt(SettingsButton());
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to open the settings.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::ShowMessageAsync(winrt::hstring title, winrt::hstring message)
    {
        auto lifetime = get_strong();

        try
        {
            controls::ContentDialog dialog{};

            dialog.XamlRoot(RootGrid().XamlRoot());
            dialog.Title(winrt::box_value(title));
            dialog.CloseButtonText(res::GetString(L"CommonClose"));

            controls::TextBlock body{};
            body.Text(message);
            body.TextWrapping(xaml::TextWrapping::Wrap);
            body.IsTextSelectionEnabled(true);

            controls::ScrollViewer scroller{};
            scroller.Content(body);
            scroller.MaxHeight(420);
            scroller.VerticalScrollBarVisibility(controls::ScrollBarVisibility::Auto);

            dialog.Content(scroller);

            co_await dialog.ShowAsync();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to show a message dialog.")
    }

    // ------------------------------------------------------------------------------------
    // Command line
    // ------------------------------------------------------------------------------------

    void MainWindow::ApplyStartupOptions() noexcept
    {
        try
        {
            auto const& options = App::StartupOptions();

            if (options.ShowHelp)
            {
                ShowCommandLineHelpAsync();
                return;
            }

            if (options.HasError)
            {
                auto const detail = options.ErrorArgument.empty()
                    ? res::GetString(options.ErrorResourceKey)
                    : res::FormatString(options.ErrorResourceKey, options.ErrorArgument);

                ShowMessageAsync(res::GetString(L"CommandLineErrorTitle"), detail);
                return;
            }

            // the rest is applied once the watcher has enumerated, in RefreshEndpointList
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to apply the command line options.")
    }

    winrt::fire_and_forget MainWindow::ShowCommandLineHelpAsync()
    {
        auto lifetime = get_strong();

        try
        {
            controls::ContentDialog dialog{};

            dialog.XamlRoot(RootGrid().XamlRoot());
            dialog.Title(winrt::box_value(res::GetString(L"CommandLineHelpTitle")));
            dialog.CloseButtonText(res::GetString(L"CommonClose"));

            controls::TextBlock body{};
            body.Text(res::GetString(L"CommandLineHelpBody"));
            body.TextWrapping(xaml::TextWrapping::Wrap);
            body.FontFamily(media::FontFamily{ L"Cascadia Mono, Consolas, Courier New" });
            body.FontSize(13);
            body.IsTextSelectionEnabled(true);

            controls::ScrollViewer scroller{};
            scroller.Content(body);
            scroller.MaxHeight(460);
            scroller.VerticalScrollBarVisibility(controls::ScrollBarVisibility::Auto);

            dialog.Content(scroller);

            co_await dialog.ShowAsync();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to show the command line help.")
    }

    HWND MainWindow::WindowHandle() const noexcept
    {
        try
        {
            HWND handle{ nullptr };

            auto native = this->try_as<::IWindowNative>();

            if (native != nullptr)
            {
                LOG_IF_FAILED(native->get_WindowHandle(&handle));
            }

            return handle;
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to retrieve the window handle.")

        return nullptr;
    }
}
