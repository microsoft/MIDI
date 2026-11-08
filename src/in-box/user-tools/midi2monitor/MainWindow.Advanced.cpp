// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// Advanced mode: switching modes, which groups are listened to and where that choice is kept,
// listening on several endpoints at once, and the activity lights.
//
// Simple mode is untouched by any of this: its pickers, its single connection and its
// remembered device all work the way they did before advanced mode existed.
//

#include "pch.h"
#include "MainWindow.xaml.h"

#include "App.xaml.h"
#include "MessageRowPanel.h"
#include "SourceTable.h"
#include "StringResources.h"

#include <bit>
#include <sstream>

namespace native = ::midi2monitor;
namespace res = ::midi2monitor::resources;

namespace winrt::midi2monitor::implementation
{
    namespace
    {
        // Below this width the Sources list goes over the messages instead of beside them.
        constexpr double NarrowLayoutWidth = 900.0;

        // The Sources list's width, which the splitter beside it changes. Whatever it's dragged
        // to, the messages keep enough room to read.
        constexpr double DefaultSourcesPaneWidth = 284.0;
        constexpr double MinimumSourcesPaneWidth = 220.0;
        constexpr double MaximumSourcesPaneWidth = 640.0;
        constexpr double MinimumMessagesWidth = 420.0;
        constexpr double SourcesPaneLeft = 12.0;
        constexpr double SourcesPaneGap = 8.0;

        // How often the lights are checked, and how long a light stays on after a message.
        constexpr int64_t ActivityTimerMilliseconds = 250;
        constexpr uint64_t ActivityHoldMilliseconds = 1000;

        std::wstring LowerKey(_In_ std::wstring const& endpointDeviceId)
        {
            std::wstring key{ endpointDeviceId };

            std::transform(key.begin(), key.end(), key.begin(),
                [](wchar_t ch) { return static_cast<wchar_t>(::towlower(ch)); });

            return key;
        }

        uint32_t CountBits(_In_ uint32_t value) noexcept
        {
            return static_cast<uint32_t>(std::popcount(value));
        }
    }

    // ------------------------------------------------------------------------------------
    // Start up and switching modes
    // ------------------------------------------------------------------------------------

    void MainWindow::InitializeAdvancedMode() noexcept
    {
        try
        {
            auto const& settings = native::AppSettings::Current();
            auto const& options = App::StartupOptions();

            LoadMessageFilter();
            LoadSavedSources();

            auto const advanced = options.AdvancedMode.value_or(settings.AdvancedMode());

            // Another app asked for one endpoint: watch that, and leave the saved list alone.
            if (advanced && !options.HasError && !options.ShowHelp && !options.EndpointDeviceId.empty())
            {
                UseSessionSources(options.EndpointDeviceId, options.GroupNumber);
            }

            m_suppressSourceEvents = true;
            OnlySendingToggle().IsOn(settings.OnlySendingEndpoints());
            SourcesPaneToggle().IsChecked(settings.SourcesPaneOpen());
            m_suppressSourceEvents = false;

            m_sourcesPaneWidth = settings.SourcesPaneWidth() == 0
                ? DefaultSourcesPaneWidth
                : std::clamp(static_cast<double>(settings.SourcesPaneWidth()), MinimumSourcesPaneWidth, MaximumSourcesPaneWidth);

            SourcesCard().Width(m_sourcesPaneWidth);

            // ProtectedCursor is not on the projected type, but the interface carrying it is.
            if (auto const element = SourcesSplitter().try_as<xaml::IUIElementProtected>())
            {
                element.ProtectedCursor(winrt::Microsoft::UI::Input::InputSystemCursor::Create(
                    winrt::Microsoft::UI::Input::InputSystemCursorShape::SizeWestEast));
            }

            m_advancedConnections.Start(
                m_pipeline.CaptureObject(),
                res::GetString(L"SessionName"),
                [weak = get_weak(), queue = m_dispatcherQueue](std::wstring const& endpointDeviceId)
                {
                    if (queue == nullptr)
                    {
                        return;
                    }

                    queue.TryEnqueue([weak, endpointDeviceId]()
                        {
                            auto strong = weak.get();

                            if (strong == nullptr)
                            {
                                return;
                            }

                            try
                            {
                                // an endpoint opened only for its light says nothing; one the
                                // customer checked says it couldn't be opened
                                if (strong->GroupMaskFor(endpointDeviceId) == 0)
                                {
                                    return;
                                }

                                auto const slot = native::SourceTable::FindSlot(endpointDeviceId);
                                auto name = native::SourceTable::EndpointName(slot);

                                if (name.empty())
                                {
                                    name = winrt::hstring{ endpointDeviceId };
                                }

                                strong->m_pipeline.PostNotice(res::FormatString(L"SourceOpenFailedNoticeFormat", std::wstring{ name }));
                            }
                            MIDI_MONITOR_CATCH_AND_LOG(L"Unable to report an endpoint that wouldn't open.")
                        });
                });

            if (m_dispatcherQueue != nullptr)
            {
                m_activityTimer = m_dispatcherQueue.CreateTimer();
                m_activityTimer.Interval(std::chrono::milliseconds{ ActivityTimerMilliseconds });
                m_activityTimer.IsRepeating(true);
                m_activityTimer.Tick([weak = get_weak()](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->RefreshActivityLights();
                        }
                    });
            }

            Closed([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        if (strong->m_activityTimer != nullptr)
                        {
                            strong->m_activityTimer.Stop();
                        }

                        strong->m_advancedConnections.Shutdown();
                    }
                });

            ApplyMode(advanced, false);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to set up advanced mode.")
    }

    _Use_decl_annotations_
    void MainWindow::ApplyMode(bool advanced, bool remember) noexcept
    {
        try
        {
            // the two modes listen in different ways, so whatever is running, or still
            // connecting, stops first
            if ((m_monitoring || m_connecting) && advanced != m_advancedMode)
            {
                StopMonitoring(true);
            }

            m_advancedMode = advanced;

            if (remember)
            {
                native::AppSettings::Current().AdvancedMode(advanced);
            }

            m_suppressModeToggle = true;
            AdvancedModeToggle().IsChecked(advanced);
            m_suppressModeToggle = false;

            auto const visibleWhen = [](bool show) { return show ? xaml::Visibility::Visible : xaml::Visibility::Collapsed; };

            HeaderCard().Visibility(visibleWhen(!advanced));
            AdvancedHeaderPanel().Visibility(visibleWhen(advanced));

            m_pipeline.Advanced(advanced);
            m_pipeline.PostMessageFilter(advanced ? m_messageFilter : ::midiapp::MessageFilter{});
            m_pipeline.PostFoldRepeats(advanced && native::AppSettings::Current().FoldRepeats());

            native::ColumnLayoutState::AdvancedMode(advanced);
            RebuildColumnFlyoutItems();
            InvalidateRowLayout();

            EmptyStateBody().Text(res::GetString(advanced ? L"EmptyStateBodyAdvanced" : L"EmptyStateBodySimple"));

            if (advanced)
            {
                m_monitoredEndpointName = {};
                RefreshSourcesList();
            }
            else
            {
                // closes what advanced mode had open for its lights
                UpdateDesiredConnections();
            }

            UpdateAdvancedLayout();
            RebuildChips();
            UpdateCommandStates();
            UpdateStatusLine();
            UpdateWindowTitle();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to switch modes.")
    }

    _Use_decl_annotations_
    void MainWindow::OnAdvancedModeToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_suppressModeToggle || !m_initialized)
        {
            return;
        }

        try
        {
            auto const isChecked = AdvancedModeToggle().IsChecked();

            ApplyMode(isChecked != nullptr && isChecked.Value(), true);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to change the mode.")
    }

    _Use_decl_annotations_
    void MainWindow::OnAdvancedModeAcceleratorInvoked(xaml::Input::KeyboardAccelerator const&, xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        try
        {
            args.Handled(true);
            ApplyMode(!m_advancedMode, true);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to handle the mode shortcut.")
    }

    void MainWindow::UpdateAdvancedLayout() noexcept
    {
        try
        {
            auto const width = RootGrid().ActualWidth();
            auto const narrow = width > 0 && width < NarrowLayoutWidth;
            auto const narrowChanged = narrow != m_narrowLayout;

            m_narrowLayout = narrow;

            auto messagesMargin = MessagesCard().Margin();
            messagesMargin.Left = 12;

            if (!m_advancedMode)
            {
                SourcesCard().Visibility(xaml::Visibility::Collapsed);
                SourcesSplitter().Visibility(xaml::Visibility::Collapsed);
                MessagesCard().Margin(messagesMargin);
                return;
            }

            // A narrow window starts with the list closed and opens it over the messages; the
            // saved choice is for a wide window, so changes made here aren't kept.
            if (narrowChanged)
            {
                m_suppressSourceEvents = true;
                SourcesPaneToggle().IsChecked(narrow ? false : native::AppSettings::Current().SourcesPaneOpen());
                m_suppressSourceEvents = false;

                RebuildChips();
            }

            auto const toggled = SourcesPaneToggle().IsChecked();
            auto const open = toggled != nullptr && toggled.Value();

            // a window made narrower than the list was dragged to keeps room for the messages,
            // and a window made wide again gets the dragged width back
            auto paneWidth = m_sourcesPaneWidth;

            if (width > 0)
            {
                auto const room = (std::max)(MinimumSourcesPaneWidth, width - SourcesPaneLeft - SourcesPaneGap - MinimumMessagesWidth);
                paneWidth = (std::min)(paneWidth, room);
            }

            SourcesCard().Width(paneWidth);

            SourcesCard().Visibility(open ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            // the splitter fills the gap on the list's right, over the messages when it's open
            // over them
            SourcesSplitter().Visibility(open ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            SourcesSplitter().Margin(xaml::ThicknessHelper::FromLengths(SourcesPaneLeft + paneWidth, 0, 0, 8));

            if (open && !narrow)
            {
                messagesMargin.Left = SourcesPaneLeft + paneWidth + SourcesPaneGap;
            }

            MessagesCard().Margin(messagesMargin);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to lay out advanced mode.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSourcesPaneToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_suppressSourceEvents || !m_initialized)
        {
            return;
        }

        try
        {
            if (!m_narrowLayout)
            {
                auto const isChecked = SourcesPaneToggle().IsChecked();
                native::AppSettings::Current().SourcesPaneOpen(isChecked != nullptr && isChecked.Value());
            }

            UpdateAdvancedLayout();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to show or hide the sources.")
    }

    // ------------------------------------------------------------------------------------
    // Which groups are listened to
    // ------------------------------------------------------------------------------------

    void MainWindow::LoadSavedSources() noexcept
    {
        try
        {
            m_sourceChoices.clear();

            std::wstringstream stream{ native::AppSettings::Current().AdvancedSources() };
            std::wstring line{};

            while (std::getline(stream, line))
            {
                auto const tab = line.find(L'\t');

                if (tab == std::wstring::npos || tab == 0)
                {
                    continue;
                }

                auto const id = line.substr(0, tab);
                uint32_t mask{ 0 };

                try
                {
                    mask = static_cast<uint32_t>(std::stoul(line.substr(tab + 1), nullptr, 16)) & 0xFFFF;
                }
                catch (...)
                {
                    continue;
                }

                if (mask != 0)
                {
                    m_sourceChoices[LowerKey(id)] = SourceChoice{ id, mask };
                }
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to read the saved sources.")
    }

    void MainWindow::PersistSources() noexcept
    {
        try
        {
            // a window watching what another app asked for never writes over the saved list
            if (!m_usingSavedSources)
            {
                return;
            }

            std::wstring text{};

            for (auto const& entry : m_sourceChoices)
            {
                if (entry.second.GroupMask == 0)
                {
                    continue;
                }

                text += std::format(L"{}\t{:X}\n", entry.second.EndpointDeviceId, entry.second.GroupMask);
            }

            native::AppSettings::Current().AdvancedSources(text);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to save the sources.")
    }

    _Use_decl_annotations_
    void MainWindow::UseSessionSources(std::wstring const& endpointDeviceId, std::optional<uint8_t> groupNumber) noexcept
    {
        try
        {
            m_usingSavedSources = false;
            m_sourceChoices.clear();

            uint32_t const mask = groupNumber.has_value() && groupNumber.value() >= 1 && groupNumber.value() <= 16
                ? (1u << (groupNumber.value() - 1))
                : 0xFFFFu;

            m_sourceChoices[LowerKey(endpointDeviceId)] = SourceChoice{ endpointDeviceId, mask };

            SessionSourcesInfoBar().IsOpen(true);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to watch the endpoint from the command line.")
    }

    _Use_decl_annotations_
    void MainWindow::OnAddToSavedSourcesClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const session = m_sourceChoices;

            LoadSavedSources();

            for (auto const& entry : session)
            {
                auto& saved = m_sourceChoices[entry.first];

                saved.EndpointDeviceId = entry.second.EndpointDeviceId;
                saved.GroupMask |= entry.second.GroupMask;
            }

            m_usingSavedSources = true;
            PersistSources();

            SessionSourcesInfoBar().IsOpen(false);

            PushSlotMasks();
            UpdateDesiredConnections();
            UpdateSourceRowStates();
            ApplySourcesSearchAndActivity();
            UpdateSourcesSummary();
            RebuildChips();
            UpdateCommandStates();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to add to the saved sources.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSwitchToSavedSourcesClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            LoadSavedSources();
            m_usingSavedSources = true;

            SessionSourcesInfoBar().IsOpen(false);

            PushSlotMasks();
            UpdateDesiredConnections();
            UpdateSourceRowStates();
            ApplySourcesSearchAndActivity();
            UpdateSourcesSummary();
            RebuildChips();
            UpdateCommandStates();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to switch to the saved sources.")
    }

    _Use_decl_annotations_
    uint32_t MainWindow::AllGroupsMask(SourceRow const& row) const noexcept
    {
        uint32_t mask{ 0 };

        for (uint32_t i = 0; i < 16; i++)
        {
            if (row.Declared[i])
            {
                mask |= (1u << i);
            }
        }

        // an endpoint that says nothing about its groups gets them all
        return mask == 0 ? 0xFFFFu : mask;
    }

    _Use_decl_annotations_
    uint32_t MainWindow::GroupMaskFor(std::wstring const& endpointDeviceId) const noexcept
    {
        try
        {
            auto const found = m_sourceChoices.find(LowerKey(endpointDeviceId));

            return found == m_sourceChoices.end() ? 0u : found->second.GroupMask;
        }
        catch (...)
        {
            return 0;
        }
    }

    _Use_decl_annotations_
    void MainWindow::SetGroupMask(std::wstring const& endpointDeviceId, uint32_t mask) noexcept
    {
        try
        {
            auto const key = LowerKey(endpointDeviceId);

            if (mask == 0)
            {
                m_sourceChoices.erase(key);
            }
            else
            {
                m_sourceChoices[key] = SourceChoice{ endpointDeviceId, mask };
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to change the groups for an endpoint.")
    }

    size_t MainWindow::CheckedGroupCount() const noexcept
    {
        size_t count{ 0 };

        // only endpoints that are here now can be listened to
        for (auto const& row : m_sourceRows)
        {
            count += CountBits(GroupMaskFor(row.EndpointDeviceId) & AllGroupsMask(row));
        }

        return count;
    }

    // Tells the capture which groups to keep for each endpoint here now. Endpoints that are open
    // only for their light keep nothing.
    void MainWindow::PushSlotMasks() noexcept
    {
        try
        {
            for (auto& row : m_sourceRows)
            {
                row.Slot = native::SourceTable::SlotFor(row.EndpointDeviceId, row.Name);

                if (row.Slot == native::NoSourceSlot)
                {
                    continue;
                }

                auto const mask = GroupMaskFor(row.EndpointDeviceId);

                m_pipeline.SlotGroups(row.Slot, mask);

                // colors are handed out in the order groups are checked
                for (uint8_t group = 1; group <= 16; group++)
                {
                    if ((mask & (1u << (group - 1))) != 0)
                    {
                        native::SourceTable::ColorIndex(row.Slot, group);
                    }
                }
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to update the groups to listen to.")
    }

    // What is open: with Activity lights on, every endpoint for as long as advanced mode shows,
    // so the lights work before listening starts; with it off, only checked endpoints, and only
    // while listening. The lights' timer runs exactly when there's activity to show.
    void MainWindow::UpdateDesiredConnections() noexcept
    {
        try
        {
            std::vector<native::AdvancedConnections::Wanted> wanted{};

            auto const everyEndpoint = native::AppSettings::Current().ShowEndpointActivity();

            if (m_advancedMode && (everyEndpoint || m_advancedListening))
            {
                for (auto const& row : m_sourceRows)
                {
                    if (row.Slot == native::NoSourceSlot)
                    {
                        continue;
                    }

                    if (everyEndpoint || GroupMaskFor(row.EndpointDeviceId) != 0)
                    {
                        wanted.push_back(native::AdvancedConnections::Wanted{ row.EndpointDeviceId, row.Slot });
                    }
                }
            }

            m_advancedConnections.Desired(std::move(wanted));

            if (m_activityTimer != nullptr)
            {
                if (m_advancedMode && everyEndpoint)
                {
                    m_activityTimer.Start();
                }
                else
                {
                    m_activityTimer.Stop();
                    RefreshActivityLights();
                }
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to update the advanced mode connections.")
    }

    // ------------------------------------------------------------------------------------
    // Listening
    // ------------------------------------------------------------------------------------

    void MainWindow::StartAdvancedListening() noexcept
    {
        try
        {
            if (m_advancedListening || CheckedGroupCount() == 0)
            {
                return;
            }

            size_t endpointCount{ 0 };

            for (auto const& row : m_sourceRows)
            {
                if (GroupMaskFor(row.EndpointDeviceId) != 0)
                {
                    endpointCount++;
                }
            }

            m_pipeline.Advanced(true);
            PushSlotMasks();

            m_pipeline.CaptureTimebase(midi2::MidiClock::Now());

            // posted before capture is enabled so it lands ahead of any message
            m_pipeline.PostNotice(res::FormatString(
                L"AdvancedListeningNoticeFormat", CheckedGroupCount(), endpointCount));

            m_pipeline.CaptureEnabled(true);

            m_advancedListening = true;
            m_monitoring = true;

            UpdateDesiredConnections();

            MIDI_MONITOR_LOG_INFO(L"Advanced mode listening started.");

            UpdateSourceRowStates();
            ApplySourcesSearchAndActivity();
            UpdateCommandStates();
            UpdateWindowTitle();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to start listening.")
    }

    _Use_decl_annotations_
    void MainWindow::StopAdvancedListening(bool addNotice) noexcept
    {
        try
        {
            if (!m_advancedListening)
            {
                return;
            }

            m_pipeline.CaptureEnabled(false);

            m_advancedListening = false;
            m_monitoring = false;

            UpdateDesiredConnections();

            if (addNotice)
            {
                m_pipeline.PostNotice(res::GetString(L"MonitoringStoppedNotice"));
            }

            UpdateSourceRowStates();
            ApplySourcesSearchAndActivity();
            UpdateCommandStates();
            UpdateWindowTitle();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to stop listening.")
    }

    void MainWindow::ApplyActivitySetting() noexcept
    {
        try
        {
            UpdateDesiredConnections();
            UpdateSourceRowStates();
            ApplySourcesSearchAndActivity();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to apply the activity setting.")
    }

    void MainWindow::RefreshActivityLights() noexcept
    {
        try
        {
            auto const now = midi2::MidiClock::Now();
            auto const hold = midi2::MidiClock::TimestampFrequency() * ActivityHoldMilliseconds / 1000;
            auto const showLights = m_advancedMode && native::AppSettings::Current().ShowEndpointActivity();

            auto const resources = xaml::Application::Current().Resources();
            auto const onBrush = resources.Lookup(winrt::box_value(L"SystemFillColorSuccessBrush")).try_as<media::Brush>();
            auto const offBrush = resources.Lookup(winrt::box_value(L"ControlStrongFillColorDisabledBrush")).try_as<media::Brush>();

            auto const isRecent = [now, hold](uint64_t last) noexcept
                {
                    // a message scheduled ahead of time is stamped in the future
                    return last != 0 && (last >= now || now - last <= hold);
                };

            bool sendingChanged{ false };

            for (auto& row : m_sourceRows)
            {
                auto const valid = showLights && row.Slot != native::NoSourceSlot;
                auto const sending = valid && isRecent(m_pipeline.LastActivity(row.Slot, 16));

                if (sending != row.Sending)
                {
                    row.Sending = sending;
                    sendingChanged = true;
                }

                if (row.Light != nullptr)
                {
                    row.Light.Fill(sending ? onBrush : offBrush);
                }

                for (auto& group : row.Groups)
                {
                    if (group.Light == nullptr)
                    {
                        continue;
                    }

                    auto const groupSending = valid && isRecent(m_pipeline.LastActivity(row.Slot, static_cast<uint8_t>(group.GroupNumber - 1)));
                    group.Light.Fill(groupSending ? onBrush : offBrush);
                }
            }

            if (sendingChanged && OnlySendingToggle().IsOn())
            {
                ApplySourcesSearchAndActivity();
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to update the activity lights.")
    }

    _Use_decl_annotations_
    void MainWindow::OnOnlySendingToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_suppressSourceEvents || !m_initialized)
        {
            return;
        }

        try
        {
            native::AppSettings::Current().OnlySendingEndpoints(OnlySendingToggle().IsOn());
            ApplySourcesSearchAndActivity();
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to change which endpoints are shown.")
    }

    // ------------------------------------------------------------------------------------
    // The splitter beside the Sources list
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    void MainWindow::OnSourcesSplitterPressed(foundation::IInspectable const& sender, xaml::Input::PointerRoutedEventArgs const& args)
    {
        try
        {
            auto const element = sender.try_as<xaml::UIElement>();

            if (element == nullptr)
            {
                return;
            }

            // against the window, not the splitter: the splitter moves under the pointer
            auto const point = args.GetCurrentPoint(RootGrid());

            m_sourcesSplitterStartX = point.Position().X;
            m_sourcesSplitterStartWidth = SourcesCard().ActualWidth();
            m_sourcesSplitterPointerId = point.PointerId();
            m_draggingSourcesSplitter = element.CapturePointer(args.Pointer());

            args.Handled(true);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to start resizing the sources.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSourcesSplitterMoved(foundation::IInspectable const&, xaml::Input::PointerRoutedEventArgs const& args)
    {
        if (!m_draggingSourcesSplitter)
        {
            return;
        }

        try
        {
            auto const point = args.GetCurrentPoint(RootGrid());

            if (point.PointerId() != m_sourcesSplitterPointerId)
            {
                return;
            }

            auto const delta = static_cast<double>(point.Position().X) - m_sourcesSplitterStartX;
            auto const room = RootGrid().ActualWidth() - SourcesPaneLeft - SourcesPaneGap - MinimumMessagesWidth;
            auto const maximum = (std::max)(MinimumSourcesPaneWidth, (std::min)(MaximumSourcesPaneWidth, room));

            m_sourcesPaneWidth = std::clamp(m_sourcesSplitterStartWidth + delta, MinimumSourcesPaneWidth, maximum);

            UpdateAdvancedLayout();

            args.Handled(true);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to resize the sources.")
    }

    // Also the capture being lost, such as to another window, which ends the drag the same way.
    _Use_decl_annotations_
    void MainWindow::OnSourcesSplitterReleased(foundation::IInspectable const& sender, xaml::Input::PointerRoutedEventArgs const& args)
    {
        try
        {
            if (auto const element = sender.try_as<xaml::UIElement>())
            {
                element.ReleasePointerCapture(args.Pointer());
            }

            if (!m_draggingSourcesSplitter)
            {
                return;
            }

            m_draggingSourcesSplitter = false;
            SourcesSplitterLine().Opacity(0);

            // every window opens with the width last dragged to
            native::AppSettings::Current().SourcesPaneWidth(static_cast<uint32_t>(std::lround(m_sourcesPaneWidth)));
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to finish resizing the sources.")
    }

    // Hidden until the pointer is over it, then a line shows where it is.
    _Use_decl_annotations_
    void MainWindow::OnSourcesSplitterEntered(foundation::IInspectable const&, xaml::Input::PointerRoutedEventArgs const&)
    {
        try
        {
            SourcesSplitterLine().Opacity(1);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to highlight the sources splitter.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSourcesSplitterExited(foundation::IInspectable const&, xaml::Input::PointerRoutedEventArgs const&)
    {
        try
        {
            if (!m_draggingSourcesSplitter)
            {
                SourcesSplitterLine().Opacity(0);
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to clear the sources splitter highlight.")
    }
}
