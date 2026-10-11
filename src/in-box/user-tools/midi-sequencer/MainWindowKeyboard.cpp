// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The timeline and the clip launcher from the keyboard (design section 17). Each is a tab stop.
// On the timeline, Up and Down pick a track, Left and Right pick a clip on it, Enter opens it,
// Delete removes it, and the menu key opens its menu, or the track's menu at the playhead when no
// clip is picked. Ctrl and plus or minus zoom. In the launcher the arrow keys move between cells,
// Enter plays or stops a cell's clip, and the menu key opens the cell's menu. Each move is read
// out through the status line, which is a live region.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        using winrt::Windows::System::VirtualKey;

        constexpr double TicksPerFourFourBar = 3840.0;
        constexpr double ZoomStep = 1.25;

        // The plus and minus keys on the main part of the keyboard.
        constexpr auto OemPlus = static_cast<VirtualKey>(187);
        constexpr auto OemMinus = static_cast<VirtualKey>(189);

        bool KeyIsDown(VirtualKey key) noexcept
        {
            try
            {
                auto const state = winrt::Microsoft::UI::Input::InputKeyboardSource::GetKeyStateForCurrentThread(key);
                return (state & winrt::Windows::UI::Core::CoreVirtualKeyStates::Down) == winrt::Windows::UI::Core::CoreVirtualKeyStates::Down;
            }
            catch (...)
            {
                return false;
            }
        }

        // A track's clips on the timeline, by where they start.
        std::vector<size_t> PlacementsInOrder(seq::Track const& track)
        {
            std::vector<size_t> order(track.Timeline.size());

            for (size_t i = 0; i < order.size(); ++i)
            {
                order[i] = i;
            }

            std::stable_sort(order.begin(), order.end(), [&track](size_t a, size_t b) { return track.Timeline[a].Tick < track.Timeline[b].Tick; });
            return order;
        }
    }

    void MainWindow::HookArrangeKeyboard()
    {
        auto weak = get_weak();

        for (auto const launcher : { false, true })
        {
            auto const canvas = launcher ? m_scrollLauncherCanvas : m_scrollLaneCanvas;

            canvas.IsTabStop(true);

            // The system's focus outline is hidden under the timeline's overlays, so both draw
            // their own (DrawKeyboardFocus).
            canvas.UseSystemFocusVisuals(false);
            automation::AutomationProperties::SetHelpText(canvas, res::GetString(launcher ? L"LauncherCanvasHelp" : L"TimelineCanvasHelp"));

            canvas.KeyDown([weak, launcher](auto&&, input::KeyRoutedEventArgs const& args)
            {
                if (auto strong = weak.get())
                {
                    if (launcher)
                    {
                        strong->OnLauncherKeyDown(args);
                    }
                    else
                    {
                        strong->OnTimelineKeyDown(args);
                    }
                }
            });

            canvas.ContextRequested([weak, launcher](auto&&, input::ContextRequestedEventArgs const& args)
            {
                if (auto strong = weak.get())
                {
                    // A right click opened its own menu already.
                    foundation::Point pointer{};

                    if (!args.TryGetPosition(nullptr, pointer))
                    {
                        args.Handled(true);
                        strong->ShowKeyboardMenu(launcher);
                    }
                }
            });

            canvas.GotFocus([weak, launcher](foundation::IInspectable const& sender, auto&&)
            {
                // Read out where the keyboard is when it arrives by Tab, not on every click.
                auto const strong = weak.get();
                auto const control = sender.try_as<canvasXaml::CanvasControl>();

                if (control != nullptr)
                {
                    control.Invalidate();
                }

                if (strong && control != nullptr && control.FocusState() == xaml::FocusState::Keyboard)
                {
                    strong->AnnounceKeyboardCursor(launcher);
                }
            });

            canvas.LostFocus([](foundation::IInspectable const& sender, auto&&)
            {
                if (auto const control = sender.try_as<canvasXaml::CanvasControl>())
                {
                    control.Invalidate();
                }
            });

            // A click puts the keyboard where the mouse is, so the arrow keys go on from there.
            canvas.AddHandler(xaml::UIElement::PointerPressedEvent(), winrt::box_value(input::PointerEventHandler(
                [](foundation::IInspectable const& sender, input::PointerRoutedEventArgs const&)
                {
                    if (auto const element = sender.try_as<xaml::UIElement>())
                    {
                        element.Focus(xaml::FocusState::Pointer);
                    }
                })), true);
        }

        m_editorCanvas.UseSystemFocusVisuals(true);
    }

    _Use_decl_annotations_
    void MainWindow::DrawKeyboardFocus(canvas::CanvasDrawingSession const& ds, canvasXaml::CanvasControl const& canvas)
    {
        auto const state = canvas.FocusState();

        if (state != xaml::FocusState::Keyboard && state != xaml::FocusState::Programmatic)
        {
            return;
        }

        auto const width = static_cast<float>(canvas.ActualWidth());
        auto const height = static_cast<float>(canvas.ActualHeight());
        ds.DrawRectangle(foundation::Rect{ 1.0f, 1.0f, std::max(1.0f, width - 2.0f), std::max(1.0f, height - 2.0f) }, m_palette.SelectionStroke, 2.0f);
    }

    _Use_decl_annotations_
    std::vector<seq::ArrangeRow const*> MainWindow::KeyboardRows(bool tracksOnly) const
    {
        std::vector<seq::ArrangeRow const*> rows{};

        for (auto const* area : { &m_layout.Pinned, &m_layout.Scrolling })
        {
            for (auto const& row : *area)
            {
                if (row.Kind == seq::ArrangeRowKind::Track || (!tracksOnly && row.Kind == seq::ArrangeRowKind::Folder))
                {
                    rows.push_back(&row);
                }
            }
        }

        return rows;
    }

    _Use_decl_annotations_
    void MainWindow::ShowRowForKeyboard(seq::ArrangeRow const& row)
    {
        if (!row.Pinned)
        {
            auto const viewport = ScrollViewportHeight();

            if (row.Top < m_scrollY)
            {
                m_scrollY = row.Top;
            }
            else if (row.Top + row.Height > m_scrollY + viewport)
            {
                m_scrollY = row.Top + row.Height - viewport;
            }

            UpdateScrollBars();
            PositionHeaders();
        }

        InvalidateArrange();
        InvalidateLaunchers();
    }

    _Use_decl_annotations_
    foundation::Point MainWindow::RowPoint(seq::ArrangeRow const& row, double x) const noexcept
    {
        auto const top = row.Pinned ? row.Top : row.Top - m_scrollY;
        return foundation::Point{ static_cast<float>(std::max(0.0, x)), static_cast<float>(top + row.Height / 2.0) };
    }

    // ---------------------------------------------------------------- the timeline

    _Use_decl_annotations_
    void MainWindow::OnTimelineKeyDown(input::KeyRoutedEventArgs const& args)
    {
        try
        {
            auto const key = args.Key();
            auto const control = KeyIsDown(VirtualKey::Control);

            // Zoom, around the playhead.
            if (control && (key == VirtualKey::Add || key == OemPlus || key == VirtualKey::Subtract || key == OemMinus))
            {
                auto const in = key == VirtualKey::Add || key == OemPlus;
                auto const anchor = std::clamp(static_cast<double>(m_position) * (m_barWidth / TicksPerFourFourBar) - m_scrollX, 0.0, LaneWidth());
                SetZoom(in ? m_barWidth * ZoomStep : m_barWidth / ZoomStep, anchor);
                args.Handled(true);
                return;
            }

            auto const rows = KeyboardRows(false);

            if (rows.empty())
            {
                return;
            }

            auto current = std::find_if(rows.begin(), rows.end(), [this](seq::ArrangeRow const* row) { return row->TrackId == m_selectedTrackId; });
            auto handled = true;

            switch (key)
            {
            case VirtualKey::Up:
            case VirtualKey::Down:
            {
                auto index = current == rows.end() ? size_t{ 0 } : static_cast<size_t>(current - rows.begin());

                if (current != rows.end())
                {
                    index = key == VirtualKey::Up ? (index > 0 ? index - 1 : 0) : std::min(index + 1, rows.size() - 1);
                }

                SelectTrack(rows[index]->TrackId);
                m_selectedPlacement = SIZE_MAX;
                ShowRowForKeyboard(*rows[index]);
                AnnounceKeyboardCursor(false);
                break;
            }

            case VirtualKey::Left:
            case VirtualKey::Right:
            {
                auto const track = current == rows.end() ? nullptr : seq::FindTrack(m_doc, (*current)->TrackId);

                if (track == nullptr || track->IsFolder || track->Timeline.empty())
                {
                    AnnounceKeyboardCursor(false);
                    break;
                }

                auto const order = PlacementsInOrder(*track);
                auto const at = std::find(order.begin(), order.end(), m_selectedPlacement);
                size_t pick{};

                if (at != order.end())
                {
                    auto const index = static_cast<size_t>(at - order.begin());
                    pick = order[key == VirtualKey::Right ? std::min(index + 1, order.size() - 1) : (index > 0 ? index - 1 : 0)];
                }
                else
                {
                    // From the playhead: the next clip going right, the one before it going left.
                    auto const next = std::find_if(order.begin(), order.end(), [&](size_t i) { return track->Timeline[i].Tick >= m_position; });

                    if (key == VirtualKey::Right)
                    {
                        pick = next != order.end() ? *next : order.back();
                    }
                    else
                    {
                        pick = next != order.begin() ? *std::prev(next) : order.front();
                    }
                }

                m_selectedPlacement = pick;

                auto const x = static_cast<double>(track->Timeline[pick].Tick) * (m_barWidth / TicksPerFourFourBar) - m_scrollX;

                if (x < 0 || x > LaneWidth() - 24.0)
                {
                    ScrollToTick(track->Timeline[pick].Tick);
                }

                ShowRowForKeyboard(**current);
                AnnounceKeyboardCursor(false);
                break;
            }

            case VirtualKey::Enter:
            {
                auto const track = seq::FindTrack(m_doc, m_selectedTrackId);

                if (track != nullptr && m_selectedPlacement < track->Timeline.size())
                {
                    OpenClipInEditor(track->Timeline[m_selectedPlacement].ClipId, track->Id);
                }
                else
                {
                    handled = false;
                }

                break;
            }

            case VirtualKey::Delete:
            {
                auto const track = seq::FindTrack(m_doc, m_selectedTrackId);

                if (track != nullptr && m_selectedPlacement < track->Timeline.size())
                {
                    DeleteSelectedPlacement();
                    AnnounceKeyboardCursor(false);
                }
                else
                {
                    handled = false;
                }

                break;
            }

            case VirtualKey::Escape:
                m_selectedPlacement = SIZE_MAX;
                InvalidateArrange();
                AnnounceKeyboardCursor(false);
                break;

            default:
                handled = false;
                break;
            }

            args.Handled(handled);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to use the keyboard on the timeline.")
    }

    // ---------------------------------------------------------------- the launcher

    _Use_decl_annotations_
    void MainWindow::OnLauncherKeyDown(input::KeyRoutedEventArgs const& args)
    {
        try
        {
            auto const key = args.Key();
            auto const rows = KeyboardRows(true);

            if (rows.empty() || m_doc.Scenes.empty())
            {
                return;
            }

            auto const wanted = m_selectedSlotTrackId.empty() ? m_selectedTrackId : m_selectedSlotTrackId;
            auto current = std::find_if(rows.begin(), rows.end(), [&wanted](seq::ArrangeRow const* row) { return row->TrackId == wanted; });
            auto index = current == rows.end() ? size_t{ 0 } : static_cast<size_t>(current - rows.begin());
            auto scene = std::min(m_selectedSlot == SIZE_MAX ? size_t{ 0 } : m_selectedSlot, m_doc.Scenes.size() - 1);
            auto handled = true;
            auto moved = current == rows.end() || m_selectedSlot == SIZE_MAX;

            switch (key)
            {
            case VirtualKey::Up:
            case VirtualKey::Down:
                if (!moved)
                {
                    index = key == VirtualKey::Up ? (index > 0 ? index - 1 : 0) : std::min(index + 1, rows.size() - 1);
                }

                moved = true;
                break;

            case VirtualKey::Left:
            case VirtualKey::Right:
                if (!moved)
                {
                    scene = key == VirtualKey::Left ? (scene > 0 ? scene - 1 : 0) : std::min(scene + 1, m_doc.Scenes.size() - 1);
                }

                moved = true;
                break;

            case VirtualKey::Enter:
            {
                // A clip plays or stops, as its triangle does. An empty cell shows what can go in it.
                auto const track = seq::FindTrack(m_doc, rows[index]->TrackId);

                if (moved || track == nullptr)
                {
                    break;
                }

                if (scene < track->Slots.size() && !track->Slots[scene].empty())
                {
                    LaunchSlot(track->Id, scene);
                }
                else
                {
                    ShowKeyboardMenu(true);
                }

                break;
            }

            default:
                handled = false;
                break;
            }

            if (handled && moved)
            {
                m_selectedSlotTrackId = rows[index]->TrackId;
                m_selectedSlot = scene;
                SelectTrack(rows[index]->TrackId);
                ShowRowForKeyboard(*rows[index]);
                AnnounceKeyboardCursor(true);
            }

            args.Handled(handled);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to use the keyboard in the clip launcher.")
    }

    // ---------------------------------------------------------------- menus and announcements

    _Use_decl_annotations_
    void MainWindow::ShowKeyboardMenu(bool launcher)
    {
        try
        {
            auto const trackId = launcher ? m_selectedSlotTrackId : m_selectedTrackId;
            auto const row = trackId.empty() ? nullptr : m_layout.FindRow(trackId);

            if (row == nullptr)
            {
                AnnounceKeyboardCursor(launcher);
                return;
            }

            if (launcher)
            {
                if (row->Kind != seq::ArrangeRowKind::Track || m_selectedSlot >= m_doc.Scenes.size())
                {
                    return;
                }

                auto const x = static_cast<double>(m_selectedSlot) * DrawContext().SceneWidth - m_sceneScrollX + 20.0;
                ShowSlotMenu(trackId, m_selectedSlot, row->Pinned ? m_pinnedLauncherCanvas : m_scrollLauncherCanvas, RowPoint(*row, x));
                return;
            }

            auto const canvas = row->Pinned ? m_pinnedLaneCanvas : m_scrollLaneCanvas;
            auto const track = seq::FindTrack(m_doc, trackId);

            if (track == nullptr)
            {
                return;
            }

            if (row->Kind == seq::ArrangeRowKind::Folder)
            {
                ShowTrackMenu(trackId, canvas, RowPoint(*row, 20.0));
                return;
            }

            auto const pixelsPerTick = m_barWidth / TicksPerFourFourBar;

            if (m_selectedPlacement < track->Timeline.size())
            {
                auto const x = static_cast<double>(track->Timeline[m_selectedPlacement].Tick) * pixelsPerTick - m_scrollX + 12.0;
                ShowPlacementMenu(trackId, m_selectedPlacement, canvas, RowPoint(*row, std::min(x, LaneWidth() - 12.0)));
                return;
            }

            // No clip picked: the track's menu at the playhead, which can add one there.
            auto const x = static_cast<double>(m_position) * pixelsPerTick - m_scrollX;
            ShowLaneMenu(*row, m_position, canvas, RowPoint(*row, std::clamp(x, 0.0, LaneWidth() - 12.0)));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to open a menu from the keyboard.")
    }

    _Use_decl_annotations_
    void MainWindow::AnnounceKeyboardCursor(bool launcher)
    {
        try
        {
            auto const track = seq::FindTrack(m_doc, launcher ? m_selectedSlotTrackId : m_selectedTrackId);

            if (track == nullptr)
            {
                ShowMessage(res::GetString(launcher ? L"KeyboardLauncherStart" : L"KeyboardTimelineStart"));
                return;
            }

            if (launcher)
            {
                auto const scene = m_selectedSlot;

                if (scene >= m_doc.Scenes.size())
                {
                    return;
                }

                auto const& sceneName = m_doc.Scenes[scene].Name;
                auto const clip = scene < track->Slots.size() && !track->Slots[scene].empty() ? seq::FindClip(m_doc, track->Slots[scene]) : nullptr;

                if (clip == nullptr)
                {
                    ShowMessage(res::FormatString(L"KeyboardSlotEmptyFormat", track->Name, scene + 1, sceneName));
                    return;
                }

                auto const kind = res::GetString(clip->Pattern.has_value() ? L"ClipKindPattern" : clip->Generator.has_value() ? L"ClipKindGenerator" : L"ClipKindNotes");
                auto state = res::GetString(L"KeyboardSlotStopped");

                if (m_slotTake.has_value() && m_slotTake->TrackId == track->Id && m_slotTake->Scene == scene)
                {
                    state = res::GetString(L"RecordingCaption");
                }
                else if (auto const view = m_launchViews.find(track->Id); view != m_launchViews.end())
                {
                    if (view->second.Plays(clip->Id, scene))
                    {
                        state = res::GetString(L"KeyboardSlotPlaying");
                    }
                    else if (view->second.Queues(clip->Id, scene))
                    {
                        state = res::GetString(L"KeyboardSlotQueued");
                    }
                }

                ShowMessage(res::FormatString(L"KeyboardSlotFormat", track->Name, clip->Name, kind, scene + 1, sceneName, state));
                return;
            }

            if (track->IsFolder)
            {
                ShowMessage(res::FormatString(L"KeyboardFolderFormat", track->Name));
                return;
            }

            if (m_selectedPlacement < track->Timeline.size())
            {
                auto const& placement = track->Timeline[m_selectedPlacement];
                auto const clip = seq::FindClip(m_doc, placement.ClipId);
                auto const position = seq::BarPositionAtTick(m_doc.Meter, placement.Tick);
                ShowMessage(res::FormatString(L"KeyboardPlacementFormat", track->Name, clip != nullptr ? clip->Name : std::wstring{}, position.Bar));
                return;
            }

            ShowMessage(track->Timeline.empty()
                ? res::FormatString(L"KeyboardTrackEmptyFormat", track->Name)
                : res::FormatString(L"KeyboardTrackFormat", track->Name, track->Timeline.size()));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to describe where the keyboard is.")
    }
}
