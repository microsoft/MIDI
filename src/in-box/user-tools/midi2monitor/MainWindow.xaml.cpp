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
#include "resource.h"

namespace native = ::midi2monitor;
namespace res = ::midi2monitor::resources;
namespace backdrops = ::winrt::Microsoft::UI::Composition::SystemBackdrops;

namespace winrt::midi2monitor::implementation
{
    namespace
    {
        winrt::Windows::UI::Color ColorFromArgb(uint32_t argb) noexcept
        {
            winrt::Windows::UI::Color color{};

            color.A = static_cast<uint8_t>((argb >> 24) & 0xFF);
            color.R = static_cast<uint8_t>((argb >> 16) & 0xFF);
            color.G = static_cast<uint8_t>((argb >> 8) & 0xFF);
            color.B = static_cast<uint8_t>(argb & 0xFF);

            return color;
        }

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

            m_endpoints = winrt::single_threaded_observable_vector<appshared::EndpointChoice>();
            m_groups = winrt::single_threaded_observable_vector<appshared::NamedChoice>();
            m_channels = winrt::single_threaded_observable_vector<appshared::NamedChoice>();
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
                        strong->m_chrome.Shutdown();
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

            // the shared chrome does not know about the palette or the list rows
            native::MonitorPalette::Invalidate();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to set up the window chrome.")
    }

    void MainWindow::RestoreWindowPlacement() noexcept
    {
        // static, because this runs before the chrome is initialized
        midiapp::WindowChrome::RestorePlacement(*this, native::AppSettings::Current(), 1280, 800);
    }

    void MainWindow::SaveWindowPlacement() noexcept
    {
        m_chrome.SavePlacement();
    }

    void MainWindow::ApplyBackdrop() noexcept
    {
        m_chrome.ApplyBackdrop();
    }

    void MainWindow::ApplyBackgroundColor() noexcept
    {
        m_chrome.ApplyBackgroundColor();
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

            // the export button is centered on the whole bar, so each side only gets half the
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
        m_chrome.UpdateTitleBarInsets();
    }

    void MainWindow::ApplyTheme() noexcept
    {
        try
        {
            m_chrome.ApplyTheme();

            // the palette and the already built rows are the app's own, not the chrome's
            native::MonitorPalette::Invalidate();

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
            m_channels.Append(winrt::make<appshared::implementation::NamedChoice>(res::GetString(L"ChannelChoiceAll"), 0));

            for (int32_t channel = 1; channel <= 16; channel++)
            {
                m_channels.Append(winrt::make<appshared::implementation::NamedChoice>(
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

            // read first: a rebuild moves the viewport before there is a chance to look
            auto const wasAtEnd = IsMessageListAtEnd();

            m_listSource->Refresh();

            if (wasAtEnd)
            {
                ScrollMessageListToEnd();
            }

            auto const hasRows = m_listSource->Size() > 0;

            EmptyStatePanel().Visibility(hasRows ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to refresh the message list.")
    }

    controls::ScrollViewer MainWindow::MessageListScrollViewer() noexcept
    {
        try
        {
            if (m_messagesScrollViewer != nullptr)
            {
                return m_messagesScrollViewer;
            }

            // the ScrollViewer only exists once the ListView template has been applied
            std::function<controls::ScrollViewer(xaml::DependencyObject const&)> find =
                [&find](xaml::DependencyObject const& node) -> controls::ScrollViewer
                {
                    auto const count = media::VisualTreeHelper::GetChildrenCount(node);

                    for (int32_t i = 0; i < count; i++)
                    {
                        auto const child = media::VisualTreeHelper::GetChild(node, i);

                        if (auto const viewer = child.try_as<controls::ScrollViewer>())
                        {
                            return viewer;
                        }

                        if (auto const found = find(child))
                        {
                            return found;
                        }
                    }

                    return nullptr;
                };

            m_messagesScrollViewer = find(MessagesListView());

            return m_messagesScrollViewer;
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to find the message list scroll viewer.")

        return nullptr;
    }

    bool MainWindow::IsMessageListAtEnd() noexcept
    {
        try
        {
            auto const viewer = MessageListScrollViewer();

            // before the template is applied, following the newest message is the right default
            if (viewer == nullptr || viewer.ScrollableHeight() <= 0.0)
            {
                return true;
            }

            return viewer.VerticalOffset() >= viewer.ScrollableHeight() - 4.0;
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to read the message list scroll position.")

        return false;
    }

    void MainWindow::ScrollMessageListToEnd() noexcept
    {
        try
        {
            if (m_dispatcherQueue == nullptr)
            {
                return;
            }

            // the extent is stale until layout has run, so the scroll is queued behind it
            m_dispatcherQueue.TryEnqueue(
                winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority::Low,
                [weak = get_weak()]()
                {
                    if (auto strong = weak.get())
                    {
                        if (auto const viewer = strong->MessageListScrollViewer())
                        {
                            viewer.ChangeView(nullptr, viewer.ScrollableHeight(), nullptr, true);
                        }
                    }
                });
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to scroll the message list to the end.")
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

            // the stock presenter caps out narrower than the color picker needs
            auto const resources = RootGrid().Resources();
            auto const presenterStyleKey = winrt::box_value(L"SettingsFlyoutPresenterStyle");

            if (resources.HasKey(presenterStyleKey))
            {
                flyout.FlyoutPresenterStyle(resources.Lookup(presenterStyleKey).as<xaml::Style>());
            }

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
