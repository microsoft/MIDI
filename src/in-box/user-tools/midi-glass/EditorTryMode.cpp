// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Try mode and the monitor rail.
//
// Ctrl+Enter flips the canvas live without leaving the editor, so somebody can feel a fader and
// then nudge it two pixels. Nothing about the drawing changes when it does: the editor already
// paints the page with the same SurfaceRenderer the runtime window uses, so Try mode is a matter
// of who gets the pointer and whether a LivePlayer is running behind it.
//
// The monitor rail below reads what that player actually sent, rather than what it was asked to
// send. Printing the request would agree with the arithmetic that built it even when the
// arithmetic is wrong, which is exactly the question somebody opens the monitor to answer.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "EditorItems.h"
#include "StringResources.h"
#include "MonitorFormat.h"
#include "GlassControl.h"

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        // Enough to read a gesture back, not enough to grow while nobody is watching.
        constexpr size_t MaximumMonitorRows = 200;

        // Checked and Unchecked fire while the XAML is still being built, before the generated
        // x:Name field has been assigned, so the state is read from the sender rather than from
        // the field. Reading the field there returns null and throws.
        bool IsOn(_In_ foundation::IInspectable const& sender) noexcept
        {
            try
            {
                auto const toggle = sender.try_as<controls::Primitives::ToggleButton>();

                if (toggle == nullptr)
                {
                    return false;
                }

                auto const checked = toggle.IsChecked();

                return checked != nullptr && checked.Value();
            }
            catch (...)
            {
                return false;
            }
        }
    }

    // ---------------------------------------------------------------- the mode

    // The player is shared: Try mode sends through it and Learn listens through it, and both
    // need the same connections. Creating two would open the same devices twice.
    void EditorWindow::StartPlayerForLearning()
    {
        if (m_player != nullptr)
        {
            m_player->UpdateDocument(m_editor.Document());
            return;
        }

        m_player = glass::LivePlayer::Create();

        auto weak = get_weak();

        m_player->DevicesChanged = [weak]()
            {
                if (auto strong = weak.get())
                {
                    strong->UpdateStatusBar();
                }
            };

        m_player->FeedbackMoved = [weak](uint32_t controlIndex, double value)
            {
                if (auto strong = weak.get())
                {
                    strong->OnTryFeedbackMoved(controlIndex, value);
                }
            };

        m_player->ActivitySeen = [weak](uint32_t controlIndex, glass::LivePlayer::ListenerState state)
            {
                auto strong = weak.get();

                if (strong == nullptr || !strong->m_tryMode)
                {
                    return;
                }

                size_t itemIndex{ 0 };

                if (!strong->m_renderer.TryFindItem(controlIndex, itemIndex))
                {
                    return;
                }

                switch (state)
                {
                case glass::LivePlayer::ListenerState::On:
                    strong->m_renderer.SetValue(itemIndex, 1.0);
                    break;

                case glass::LivePlayer::ListenerState::Off:
                    strong->m_renderer.SetValue(itemIndex, 0.0);
                    strong->m_renderer.ClearBloom(itemIndex);
                    break;

                default:
                    strong->m_renderer.Bloom(itemIndex);
                    break;
                }
            };

        m_player->TempoChanged = [weak](uint32_t controlIndex, double beatsPerMinute)
            {
                auto strong = weak.get();

                if (strong == nullptr)
                {
                    return;
                }

                size_t itemIndex{ 0 };

                if (strong->m_renderer.TryFindItem(controlIndex, itemIndex))
                {
                    strong->m_renderer.SetClockTempo(itemIndex, beatsPerMinute);
                }
            };

        m_player->BeatMoved = [weak](uint32_t controlIndex, int32_t beatInBar, double phase, bool running)
            {
                auto strong = weak.get();

                if (strong == nullptr)
                {
                    return;
                }

                size_t itemIndex{ 0 };

                if (strong->m_renderer.TryFindItem(controlIndex, itemIndex))
                {
                    strong->m_renderer.SetBeat(itemIndex, beatInBar, phase, running);
                }
            };

        m_player->Sent = [weak](glass::SentMessage const& message)
            {
                if (auto strong = weak.get())
                {
                    strong->AppendMonitorRow(message);
                }
            };

        // A sequence step moving a control looks exactly like a device moving one.
        m_player->ControlValueSet = [weak](uint32_t controlIndex, double value)
            {
                if (auto strong = weak.get())
                {
                    strong->OnTryFeedbackMoved(controlIndex, value);
                }
            };

        m_player->PageRequested = [weak](uint32_t pageIndex)
            {
                if (auto strong = weak.get())
                {
                    strong->ShowEditorPage(pageIndex);
                }
            };

        m_player->Learned = [weak](glass::LearnedBinding const& learned)
            {
                if (auto strong = weak.get())
                {
                    strong->OnLearned(learned);
                }
            };

        // Nothing leaves until Try mode says so. Learning opens the connections and listens;
        // it must not push a layout's worth of startup values at a desk on its own.
        m_player->SetOutputEnabled(false);

        // A layout being edited and the same layout running are two owners on purpose, so
        // closing one does not take the other's connections down.
        m_player->Start(m_editor.Document(), m_dispatcher, m_filePath + L"\x1editor");
    }

    _Use_decl_annotations_
    void EditorWindow::SetTryMode(bool tryMode)
    {
        if (m_tryMode == tryMode || !m_loaded)
        {
            return;
        }

        try
        {
            m_tryMode = tryMode;

            EditModeToggle().IsChecked(!tryMode);
            TryModeToggle().IsChecked(tryMode);

            if (tryMode)
            {
                // Leaving a selection drawn over a live surface makes it look like the editor,
                // so the handles and the rubber band go away for as long as Try mode is on.
                m_dragMode = DragMode::None;
                m_hasArmedKind = false;

                // An edit made while Try mode was off has to reach the engine before a finger
                // does, and the player has to exist before either.
                StartPlayerForLearning();

                m_player->SetOutputEnabled(true);
            }
            else
            {
                // Nothing is left held. A finger lifted by leaving Try mode still has to end
                // its note, and that release has to go out before the gate shuts.
                m_input.ReleaseAll();

                if (m_player != nullptr)
                {
                    m_player->SetOutputEnabled(false);
                }
            }

            ApplySurfaceInputMode();
            UpdateOverlay();
            UpdateStatusBar();
            RebuildMonitorList();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to switch between Edit and Try.")
    }

    void EditorWindow::ApplySurfaceInputMode()
    {
        if (!m_loaded)
        {
            return;
        }

        try
        {
            m_input.Detach();

            for (size_t index = 0; index < m_renderer.ItemCount(); ++index)
            {
                if (auto const element = m_renderer.ElementAt(index))
                {
                    element.IsHitTestVisible(m_tryMode);
                }
            }

            // The overlay sits on top of the surface and is transparent to the eye but not to a
            // pointer, so it has to stand aside for Try mode to reach anything.
            OverlayCanvas().IsHitTestVisible(!m_tryMode);

            if (!m_tryMode)
            {
                // Hit testing is not the whole story: a control is still in the automation tree
                // in Edit mode, so a screen reader could drive it. Nothing sends unless Try mode
                // is on, so the handlers come off with it.
                for (size_t index = 0; index < m_renderer.ItemCount(); ++index)
                {
                    auto element = m_renderer.ElementAt(index);

                    if (element == nullptr)
                    {
                        continue;
                    }

                    auto* control = winrt::get_self<implementation::GlassControl>(element);

                    control->SetValueRequestHandler(nullptr);
                    control->SetInvokeHandler(nullptr);
                }

                return;
            }

            auto weak = get_weak();

            m_input.ValueChanged = [weak](size_t itemIndex, double value, bool isFinal)
                {
                    if (auto strong = weak.get())
                    {
                        strong->OnTryValueChanged(itemIndex, value, isFinal);
                    }
                };

            m_input.ValueYChanged = [weak](size_t itemIndex, double value, bool isFinal)
                {
                    auto strong = weak.get();

                    if (strong == nullptr)
                    {
                        return;
                    }

                    strong->m_renderer.SetValueY(itemIndex, value);

                    if (strong->m_tryMode && strong->m_player != nullptr)
                    {
                        strong->m_player->ValueYChanged(
                            strong->m_renderer.ControlIndexOf(itemIndex), value, isFinal);
                    }
                };

            m_input.KeyChanged = [weak](size_t itemIndex, int32_t key, double velocity, bool isDown)
                {
                    auto strong = weak.get();

                    if (strong == nullptr)
                    {
                        return;
                    }

                    if (isDown)
                    {
                        strong->m_renderer.Bloom(itemIndex);
                    }

                    if (strong->m_tryMode && strong->m_player != nullptr)
                    {
                        strong->m_player->KeyChanged(
                            strong->m_renderer.ControlIndexOf(itemIndex), key, velocity, isDown);
                    }
                };

            m_input.Switched = [weak](size_t itemIndex, bool isOn, double velocity)
                {
                    if (auto strong = weak.get())
                    {
                        strong->OnTrySwitched(itemIndex, isOn, velocity);
                    }
                };

            m_input.Snap = [weak](size_t itemIndex, double position) -> double
                {
                    auto strong = weak.get();

                    if (strong != nullptr && strong->m_player != nullptr)
                    {
                        return strong->m_player->SnapToDetent(
                            strong->m_renderer.ControlIndexOf(itemIndex), position);
                    }

                    return position;
                };

            m_input.TouchChanged = [weak](size_t itemIndex, bool isTouched)
                {
                    auto strong = weak.get();

                    if (strong == nullptr)
                    {
                        return;
                    }

                    if (isTouched)
                    {
                        strong->m_renderer.Bloom(itemIndex);
                    }

                    strong->m_renderer.SetTouched(itemIndex, isTouched);

                    if (strong->m_player != nullptr)
                    {
                        strong->m_player->Touched(
                            strong->m_renderer.ControlIndexOf(itemIndex), isTouched);
                    }
                };

            m_input.Attach(m_renderer);

            // Assistive technology drives the same path a finger does, so a screen reader moving
            // a fader in Try mode sends exactly what a finger would.
            for (size_t index = 0; index < m_renderer.ItemCount(); ++index)
            {
                auto element = m_renderer.ElementAt(index);

                if (element == nullptr)
                {
                    continue;
                }

                auto* control = winrt::get_self<implementation::GlassControl>(element);

                control->SetValueRequestHandler([weak, index](uint32_t, double value)
                    {
                        if (auto strong = weak.get())
                        {
                            // One discrete change is a whole gesture: touched, moved, released.
                            strong->OnTryValueChanged(index, value, false);
                            strong->OnTryValueChanged(index, value, true);
                        }
                    });

                control->SetInvokeHandler([weak, index](uint32_t)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->OnTrySwitched(index, true, 1.0);
                            strong->OnTrySwitched(index, false, 0.0);
                        }
                    });
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to set up the surface for the current mode.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnEditModeChecked(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        SetTryMode(false);
    }

    _Use_decl_annotations_
    void EditorWindow::OnEditModeUnchecked(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        // There is always a mode. Unchecking the one you are in would leave the pair showing
        // neither, so it goes straight back on.
        if (m_loaded && !m_tryMode)
        {
            EditModeToggle().IsChecked(true);
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnTryModeChecked(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        SetTryMode(true);
    }
    _Use_decl_annotations_
    void EditorWindow::OnTryModeUnchecked(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_loaded && m_tryMode)
        {
            TryModeToggle().IsChecked(true);
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnTryAccelerator(
        xaml::Input::KeyboardAccelerator const& sender,
        xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        args.Handled(true);

        SetTryMode(!m_tryMode);
    }

    // ---------------------------------------------------------------- sending

    _Use_decl_annotations_
    void EditorWindow::OnTryValueChanged(size_t itemIndex, double value, bool isFinal)
    {
        m_renderer.SetValue(itemIndex, value);

        if (m_tryMode && m_player != nullptr)
        {
            m_player->ValueChanged(m_renderer.ControlIndexOf(itemIndex), value, isFinal);
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnTrySwitched(size_t itemIndex, bool isOn, double velocity)
    {
        m_renderer.SetValue(itemIndex, isOn ? 1.0 : 0.0);

        // A time display counts again from zero when it is tapped, in Try mode too.
        if (m_renderer.KindAt(itemIndex) == glass::ControlKind::TimeDisplay)
        {
            if (isOn)
            {
                m_renderer.ResetElapsed(itemIndex);
            }

            return;
        }

        if (m_tryMode && m_player != nullptr)
        {
            m_player->Switched(m_renderer.ControlIndexOf(itemIndex), isOn, velocity);
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnTryFeedbackMoved(uint32_t controlIndex, double value)
    {
        if (!m_tryMode)
        {
            return;
        }

        try
        {
            size_t itemIndex{ 0 };

            // A control on another page is still tracked by the engine; it simply has nothing on
            // screen to move.
            if (!m_renderer.TryFindItem(controlIndex, itemIndex))
            {
                return;
            }

            m_renderer.SetValue(itemIndex, value);
            m_renderer.Bloom(itemIndex);

            if (auto element = m_renderer.ElementAt(itemIndex))
            {
                winrt::get_self<implementation::GlassControl>(element)->SetValueDirect(value);
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show incoming feedback.")
    }

    // ---------------------------------------------------------------- the monitor rail

    _Use_decl_annotations_
    void EditorWindow::AppendMonitorRow(glass::SentMessage const& message)
    {
        if (m_monitorPaused)
        {
            return;
        }

        try
        {
            if (!m_monitorHasOrigin)
            {
                m_monitorOrigin = message.TimestampMilliseconds;
                m_monitorHasOrigin = true;
            }

            m_monitorRows.push_back(message);

            if (m_monitorRows.size() > MaximumMonitorRows)
            {
                m_monitorRows.erase(
                    m_monitorRows.begin(),
                    m_monitorRows.begin() + static_cast<ptrdiff_t>(m_monitorRows.size() - MaximumMonitorRows));
            }

            // One row appended, not the whole list rebuilt. A fader dragged across the page
            // produces a message every few milliseconds, and rebuilding two hundred rows each
            // time is what makes the surface stutter in the middle of trying it.
            AppendMonitorItem(message);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to add a line to the monitor.")
    }

    _Use_decl_annotations_
    void EditorWindow::AppendMonitorItem(glass::SentMessage const& message)
    {
        if (!m_loaded || m_monitorItems == nullptr)
        {
            return;
        }

        if (m_monitorSelectedOnly && !IsMonitoredControl(message.ControlIndex))
        {
            return;
        }

        m_monitorItems.Append(*MakeMonitorItem(message));

        while (m_monitorItems.Size() > MaximumMonitorRows)
        {
            m_monitorItems.RemoveAt(0);
        }

        m_monitorVisibleCount = m_monitorItems.Size();

        // A monitor that does not follow is a monitor nobody reads.
        if (m_monitorVisibleCount != 0)
        {
            MonitorList().ScrollIntoView(m_monitorItems.GetAt(m_monitorVisibleCount - 1));
        }

        UpdateMonitorEmptyText();
    }

    _Use_decl_annotations_
    bool EditorWindow::IsMonitoredControl(uint32_t controlIndex) const
    {
        for (auto const& id : m_editor.Selection())
        {
            if (auto const index = m_editor.ControlIndexOf(id);
                index >= 0 && static_cast<uint32_t>(index) == controlIndex)
            {
                return true;
            }
        }

        return false;
    }

    _Use_decl_annotations_
    winrt::com_ptr<MonitorItem> EditorWindow::MakeMonitorItem(glass::SentMessage const& message) const
    {
        auto const& devices = m_editor.Document().Devices;

        auto const formatted = glass::DescribeMessage(message.Words, message.WordCount);

        std::wstring destination{};

        if (message.DestinationIndex >= 0 &&
            static_cast<size_t>(message.DestinationIndex) < devices.size())
        {
            destination = devices[static_cast<size_t>(message.DestinationIndex)].Name;
        }

        if (formatted.Channel > 0)
        {
            destination += resources::FormatString(L"MonitorChannelFormat", formatted.Channel);
        }

        auto item = winrt::make_self<MonitorItem>();

        item->Update(
            glass::FormatElapsed(message.TimestampMilliseconds - m_monitorOrigin),
            formatted.Words,
            formatted.Meaning,
            destination);

        return item;
    }

    void EditorWindow::RebuildMonitorList()
    {
        if (!m_loaded)
        {
            return;
        }

        try
        {
            // The whole list, from scratch. Only the filter, the selection and Clear come
            // through here; a message arriving appends one row instead.
            auto items = winrt::single_threaded_observable_vector<foundation::IInspectable>();

            for (auto const& row : m_monitorRows)
            {
                if (m_monitorSelectedOnly && !IsMonitoredControl(row.ControlIndex))
                {
                    continue;
                }

                items.Append(*MakeMonitorItem(row));
            }

            m_monitorItems = items;

            MonitorList().ItemsSource(items);

            m_monitorVisibleCount = items.Size();

            if (items.Size() != 0)
            {
                MonitorList().ScrollIntoView(items.GetAt(items.Size() - 1));
            }

            UpdateMonitorEmptyText();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to rebuild the monitor.")
    }

    void EditorWindow::UpdateMonitorEmptyText()
    {
        if (!m_loaded)
        {
            return;
        }

        try
        {
            // Counted when the list was built. Asking the ListView throws once ItemsSource is
            // set, which is exactly the case this runs in.
            if (m_monitorVisibleCount != 0)
            {
                MonitorEmptyText().Text(L"");
                return;
            }

            // Three different reasons for an empty list, and telling them apart is the whole
            // value of the line.
            if (!m_tryMode)
            {
                MonitorEmptyText().Text(resources::GetString(L"MonitorEmptyNotTrying"));
            }
            else if (m_monitorSelectedOnly && m_editor.Selection().empty())
            {
                MonitorEmptyText().Text(resources::GetString(L"MonitorEmptyNoSelection"));
            }
            else
            {
                MonitorEmptyText().Text(resources::GetString(L"MonitorEmptyNothingYet"));
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to describe the monitor.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnMonitorSelectedOnlyToggled(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        try
        {
            m_monitorSelectedOnly = IsOn(sender);

            RebuildMonitorList();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change the monitor filter.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnMonitorPauseToggled(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        m_monitorPaused = IsOn(sender);
    }

    _Use_decl_annotations_
    void EditorWindow::OnMonitorClearClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        m_monitorRows.clear();
        m_monitorHasOrigin = false;

        RebuildMonitorList();
    }

    _Use_decl_annotations_
    void EditorWindow::OnMonitorExpandClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            m_monitorExpanded = !m_monitorExpanded;

            MonitorList().Visibility(
                m_monitorExpanded ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            // The row carries the height, so folding the rail away gives the canvas the room
            // back rather than leaving an empty panel behind.
            MonitorRow().Height(xaml::GridLengthHelper::FromPixels(m_monitorExpanded ? m_monitorHeight : 27.0));

            MonitorSplitter().Visibility(
                m_monitorExpanded ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            ApplyCanvasScale();
            RebuildGrid();

            MonitorExpandGlyph().Glyph(m_monitorExpanded ? L"\uE70D" : L"\uE70E");

            xaml::Automation::AutomationProperties::SetName(
                MonitorExpandButton(),
                resources::GetString(m_monitorExpanded ? L"MonitorCollapseName" : L"MonitorExpandName"));
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change the size of the monitor.")
    }
}
