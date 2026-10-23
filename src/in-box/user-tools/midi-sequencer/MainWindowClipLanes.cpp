// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// A notes clip's own automation (design section 7): lanes under the piano roll, on its time scale,
// edited like a track's lanes on the timeline. The menus and the add-lane dialog are the
// timeline's, given the clip's id instead of a track's. From the keyboard (design section 17),
// Alt and the arrow keys pick a point, the arrow keys move it, and the menu key opens the menu.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        using winrt::Windows::System::VirtualKey;

        // The piano roll keeps at least this much when a clip has many lanes; the editor can be
        // made taller to see them all.
        constexpr float MinimumRollHeight = 120.0f;

        // An arrow key changes a value by 1%, or 10% with Shift, and a program by 1 or 10.
        constexpr double PercentStep = 4294967295.0 / 100.0;
        constexpr double BendPercentStep = 2147483648.0 / 100.0;

        bool ShowsLanes(seq::Clip const* clip) noexcept
        {
            return clip != nullptr && clip->Kind == seq::ClipKind::Notes;
        }

        // Shift held while a point is dragged keeps its value.
        bool ShiftIsDown() noexcept
        {
            try
            {
                auto const state = winrt::Microsoft::UI::Input::InputKeyboardSource::GetKeyStateForCurrentThread(VirtualKey::Shift);
                return (state & winrt::Windows::UI::Core::CoreVirtualKeyStates::Down) == winrt::Windows::UI::Core::CoreVirtualKeyStates::Down;
            }
            catch (...)
            {
                return false;
            }
        }

        // What the roll leaves for the lanes and the add button, given the editor's height.
        float RollHeightFor(seq::Clip const* clip, float height) noexcept
        {
            if (!ShowsLanes(clip))
            {
                return height;
            }

            auto const strip = seq::ClipLaneStrip::AddRowHeight + seq::ClipLaneStrip::RowHeight * static_cast<float>(clip->Lanes.size());
            return std::max(std::min(height, MinimumRollHeight), height - strip);
        }

        // What a lane changes, short enough for the column under the keys.
        winrt::hstring ShortTarget(seq::AutomationLane const& lane)
        {
            auto const bank = lane.Number >> 7;
            auto const index = lane.Number & 0x7F;

            switch (lane.Target)
            {
            case seq::LaneTarget::Controller: return res::FormatString(L"LaneShortControllerFormat", lane.Number);
            case seq::LaneTarget::PitchBend: return res::GetString(L"LaneShortBend");
            case seq::LaneTarget::ChannelPressure: return res::GetString(L"LaneShortPressure");
            case seq::LaneTarget::Program: return res::GetString(L"LaneShortProgram");
            case seq::LaneTarget::Registered: return res::FormatString(L"LaneRegisteredFormat", bank, index);
            case seq::LaneTarget::Assignable: return res::FormatString(L"LaneAssignableFormat", bank, index);
            default: return {};
            }
        }

        seq::ClipLaneStrings const& StripStrings()
        {
            static seq::ClipLaneStrings const strings = []()
            {
                seq::ClipLaneStrings made{};
                made.AddLane = res::GetString(L"ClipLaneAdd");
                made.EmptyLane = res::GetString(L"EmptyLaneHint");
                made.NoLanes = res::GetString(L"ClipLanesNone");
                return made;
            }();

            return strings;
        }

        // The point at or after a tick, going right, or at or before it, going left.
        size_t PointNear(seq::AutomationLane const& lane, int64_t tick, bool right) noexcept
        {
            auto const& points = lane.Points;

            if (points.empty())
            {
                return SIZE_MAX;
            }

            auto const after = std::lower_bound(points.begin(), points.end(), tick, [](seq::LanePoint const& point, int64_t at) { return point.Tick < at; });
            auto const index = static_cast<size_t>(after - points.begin());

            if (right)
            {
                return std::min(index, points.size() - 1);
            }

            return index < points.size() && points[index].Tick == tick ? index : (index > 0 ? index - 1 : 0);
        }

        // A value one step up or down, kept in range.
        uint32_t StepValue(seq::LaneTarget target, uint32_t value, bool up, bool large) noexcept
        {
            auto const steps = large ? 10 : 1;

            if (target == seq::LaneTarget::Program)
            {
                auto const program = static_cast<int32_t>(seq::ScaleDown(value, 32, 7)) + (up ? steps : -steps);
                return seq::ScaleUp(static_cast<uint32_t>(std::clamp(program, 0, 127)), 7, 32);
            }

            auto const step = (target == seq::LaneTarget::PitchBend ? BendPercentStep : PercentStep) * steps;
            auto const changed = static_cast<double>(value) + (up ? step : -step);
            return static_cast<uint32_t>(std::clamp(std::llround(changed), 0LL, 0xFFFFFFFFLL));
        }
    }

    float MainWindow::NotesEditorRollHeight() const noexcept
    {
        auto const height = m_editorCanvas != nullptr ? static_cast<float>(m_editorCanvas.ActualHeight()) : 0.0f;
        return RollHeightFor(m_editorClipId.empty() ? nullptr : seq::FindClip(m_doc, m_editorClipId), height);
    }

    _Use_decl_annotations_
    void MainWindow::DrawNotesEditor(canvas::CanvasDrawingSession const& ds, float width, float height)
    {
        UpdateGhostNotes();

        auto const clip = m_editorClipId.empty() ? nullptr : seq::FindClip(m_doc, m_editorClipId);

        if (!ShowsLanes(clip))
        {
            m_clipLanes.SetClip(nullptr, seq::Color{});
            m_roll.Draw(ds, width, height);
            return;
        }

        auto const track = EditorTrack();
        auto const color = clip->Color.has_value() ? seq::FromRgb(*clip->Color) : seq::FromRgb(track != nullptr ? track->Color : 0x60CDFF);

        std::vector<seq::ClipLaneLabel> labels{};
        labels.reserve(clip->Lanes.size());

        for (auto const& lane : clip->Lanes)
        {
            labels.push_back(seq::ClipLaneLabel{ winrt::hstring{ LaneName(lane) }, ShortTarget(lane) });
        }

        m_clipLanes.SetClip(clip, color);
        m_clipLanes.SetPalette(&m_palette);
        m_clipLanes.SetStrings(StripStrings());
        m_clipLanes.SetLabels(std::move(labels));

        auto const rollHeight = RollHeightFor(clip, height);

        // A lane added or taken away changes the roll's height, so it shows all the notes again.
        if (m_clipLanes.TakeLaneCountChanged())
        {
            m_roll.FitToClip(width, rollHeight);
        }

        m_roll.Draw(ds, width, rollHeight);
        m_clipLanes.Draw(ds, rollHeight, width, m_roll);
    }

    // ---------------------------------------------------------------- pointer

    _Use_decl_annotations_
    bool MainWindow::OnClipLanesPressed(input::PointerRoutedEventArgs const& args, foundation::Point position, bool right, bool control, bool doubleClick)
    {
        auto const clip = seq::FindClip(m_doc, m_editorClipId);

        if (!ShowsLanes(clip) || position.Y < NotesEditorRollHeight())
        {
            // A click in the roll leaves the lanes' selection behind.
            if (m_clipLanes.HasSelection())
            {
                m_clipLanes.ClearSelection();
            }

            m_clipLanes.HideKeyboardFocus();
            return false;
        }

        args.Handled(true);

        auto const hit = m_clipLanes.HitTest(position, m_roll);
        auto const clipId = m_editorClipId;

        if (hit.What == seq::ClipLaneHit::Kind::AddButton)
        {
            if (!right)
            {
                ShowAddLaneDialog(clipId);
            }

            return true;
        }

        if (hit.Lane >= clip->Lanes.size())
        {
            return true;
        }

        auto const laneId = clip->Lanes[hit.Lane].Id;

        if (hit.What == seq::ClipLaneHit::Kind::Header)
        {
            if (right)
            {
                ShowLaneRowMenu(clipId, laneId, std::nullopt, m_editorCanvas, position);
            }
            else if (doubleClick)
            {
                RenameLane(clipId, laneId);
            }

            return true;
        }

        // A right click, or a double click on a point, opens its menu: type a value, hold, delete.
        if (hit.What == seq::ClipLaneHit::Kind::Point && (right || doubleClick))
        {
            m_clipLanes.Select(hit.Lane, hit.Point);
            InvalidateEditor();
            ShowLanePointMenu(clipId, laneId, hit.Point, m_editorCanvas, position);
            return true;
        }

        if (right)
        {
            seq::LanePoint at{};
            at.Tick = hit.Tick;
            at.Value = hit.Value;
            at.Hold = clip->Lanes[hit.Lane].Target == seq::LaneTarget::Program;
            ShowLaneRowMenu(clipId, laneId, at, m_editorCanvas, position);
            return true;
        }

        // Delete now means this point, not the notes.
        m_roll.ClearSelection();

        if (m_clipLanes.PointerPressed(position, control, m_roll))
        {
            m_clipLaneGesture = true;
            m_editorCanvas.CapturePointer(args.Pointer());
        }

        InvalidateEditor();
        return true;
    }

    _Use_decl_annotations_
    bool MainWindow::OnClipLanesMoved(foundation::Point position)
    {
        if (!m_clipLaneGesture)
        {
            return false;
        }

        if (m_clipLanes.PointerMoved(position, m_roll, ShiftIsDown()))
        {
            InvalidateEditor();
        }

        return true;
    }

    _Use_decl_annotations_
    bool MainWindow::OnClipLanesReleased(foundation::Point position)
    {
        if (!m_clipLaneGesture)
        {
            return false;
        }

        m_clipLaneGesture = false;
        m_editorCanvas.ReleasePointerCaptures();

        try
        {
            auto edit = m_clipLanes.PointerReleased(position, m_roll, ShiftIsDown());
            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (!edit.has_value() || clip == nullptr)
            {
                InvalidateEditor();
                return true;
            }

            if (m_readOnly)
            {
                ShowMessage(res::GetString(L"ReadOnlyEdit"));
            }

            clip->Lanes = edit->After;

            seq::ChangeList changes{};
            changes.push_back(seq::MakeClipLanesChange(m_editorClipId, std::move(edit->Before), clip->Lanes));
            Commit(std::wstring{ res::GetString(edit->Drawn ? L"UndoDrawAutomation" : L"UndoEditAutomation") }, std::move(changes));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to finish an edit on a clip's lane.")

        return true;
    }

    bool MainWindow::DeleteSelectedClipLanePoint() noexcept
    {
        try
        {
            if (!m_clipLanes.HasSelection())
            {
                return false;
            }

            auto edit = m_clipLanes.DeleteSelection();
            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (edit.has_value() && clip != nullptr)
            {
                clip->Lanes = edit->After;

                seq::ChangeList changes{};
                changes.push_back(seq::MakeClipLanesChange(m_editorClipId, std::move(edit->Before), clip->Lanes));
                Commit(std::wstring{ res::GetString(L"UndoDeletePoint") }, std::move(changes));
                ShowMessage(res::GetString(L"ClipLanePointDeleted"));
            }

            return true;
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to delete a point on a clip's lane.")

        return false;
    }

    _Use_decl_annotations_
    void MainWindow::OnEditorAutomationClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (!m_editorClipId.empty())
            {
                ShowAddLaneDialog(m_editorClipId);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to add automation to a clip.")
    }

    // ---------------------------------------------------------------- keyboard

    _Use_decl_annotations_
    void MainWindow::AnnounceClipLanePoint(seq::Clip const& clip, size_t lane, size_t point)
    {
        if (lane >= clip.Lanes.size())
        {
            return;
        }

        auto const& found = clip.Lanes[lane];

        if (point >= found.Points.size())
        {
            ShowMessage(res::FormatString(L"ClipLaneNoPointsFormat", LaneName(found)));
            return;
        }

        auto const& at = found.Points[point];
        auto const position = seq::BarPositionAtTick(m_doc.Meter, at.Tick);

        if (position.TicksIntoBeat > 0)
        {
            ShowMessage(res::FormatString(L"ClipLanePointTicksFormat", LaneName(found), position.Bar, position.Beat, position.TicksIntoBeat, LaneValueText(found, at.Value)));
            return;
        }

        ShowMessage(res::FormatString(L"ClipLanePointFormat", LaneName(found), position.Bar, position.Beat, LaneValueText(found, at.Value)));
    }

    _Use_decl_annotations_
    bool MainWindow::OnClipLanesKeyDown(VirtualKey key, bool shift, bool alt)
    {
        try
        {
            auto const clip = m_editorClipId.empty() ? nullptr : seq::FindClip(m_doc, m_editorClipId);
            auto const arrow = key == VirtualKey::Left || key == VirtualKey::Right || key == VirtualKey::Up || key == VirtualKey::Down;

            if (!ShowsLanes(clip) || m_clipLanes.InGesture() || !arrow || (!alt && !m_clipLanes.HasSelection()))
            {
                return false;
            }

            if (clip->Lanes.empty())
            {
                ShowMessage(res::GetString(L"ClipLanesNoneKeyboard"));
                return true;
            }

            auto const laneIndex = m_clipLanes.CurrentLane();
            auto const& lane = clip->Lanes[laneIndex];
            auto const selected = m_clipLanes.HasSelection() && m_clipLanes.SelectedPoint() < lane.Points.size();
            auto const playhead = m_roll.Playhead();
            auto const from = selected ? lane.Points[m_clipLanes.SelectedPoint()].Tick : std::clamp<int64_t>(playhead, 0, clip->Length);

            // Alt and an arrow: the next or previous point, or the nearest one on the lane above
            // or below. The arrow keys then work on that point instead of the notes.
            if (alt)
            {
                auto pickLane = laneIndex;
                auto pickPoint = SIZE_MAX;

                if (key == VirtualKey::Up || key == VirtualKey::Down)
                {
                    pickLane = key == VirtualKey::Up ? (laneIndex > 0 ? laneIndex - 1 : 0) : std::min(laneIndex + 1, clip->Lanes.size() - 1);
                    pickPoint = PointNear(clip->Lanes[pickLane], from, true);
                }
                else if (!selected)
                {
                    pickPoint = PointNear(lane, from, key == VirtualKey::Right);
                }
                else
                {
                    auto const current = m_clipLanes.SelectedPoint();
                    pickPoint = key == VirtualKey::Right ? std::min(current + 1, lane.Points.size() - 1) : (current > 0 ? current - 1 : 0);
                }

                m_roll.ClearSelection();
                m_clipLanes.SetCurrentLane(pickLane);

                if (pickPoint != SIZE_MAX)
                {
                    m_clipLanes.Select(pickLane, pickPoint);
                }
                else
                {
                    m_clipLanes.ClearSelection();
                }

                AnnounceClipLanePoint(*clip, pickLane, pickPoint);
                UpdateNoteInspector();
                InvalidateEditor();
                return true;
            }

            // An arrow on its own moves the picked point a step of the grid, or its value a step.
            if (!selected)
            {
                m_clipLanes.ClearSelection();
                return false;
            }

            auto const index = m_clipLanes.SelectedPoint();
            auto lanes = clip->Lanes;
            auto& point = lanes[laneIndex].Points[index];

            if (key == VirtualKey::Left || key == VirtualKey::Right)
            {
                auto const grid = SnapGrid() > 0 ? SnapGrid() : seq::TicksPerQuarterNote / 4;
                auto const tick = std::clamp<int64_t>(point.Tick + (key == VirtualKey::Right ? grid : -grid), 0, clip->Length);
                seq::MoveLanePoint(lanes[laneIndex], index, tick, point.Value);
            }
            else
            {
                point.Value = StepValue(lane.Target, point.Value, key == VirtualKey::Up, shift);
            }

            m_clipLanes.SetCurrentLane(laneIndex);

            if (lanes[laneIndex].Points != clip->Lanes[laneIndex].Points)
            {
                auto before = clip->Lanes;
                clip->Lanes = std::move(lanes);

                seq::ChangeList changes{};
                changes.push_back(seq::MakeClipLanesChange(m_editorClipId, std::move(before), clip->Lanes));
                Commit(std::wstring{ res::GetString(L"UndoEditAutomation") }, std::move(changes));
            }

            if (auto const now = seq::FindClip(m_doc, m_editorClipId); now != nullptr)
            {
                AnnounceClipLanePoint(*now, laneIndex, index);
            }

            InvalidateEditor();
            return true;
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to use the keyboard on a clip's lane.")

        return true;
    }

    _Use_decl_annotations_
    void MainWindow::OnEditorContextRequested(input::ContextRequestedEventArgs const& args)
    {
        try
        {
            // A right click opened its menu when it was pressed; this is for the menu key and
            // Shift+F10.
            foundation::Point pointer{};

            if (args.TryGetPosition(m_editorCanvas, pointer) || EditorShowsPattern() || !m_roll.HasClip())
            {
                return;
            }

            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (!ShowsLanes(clip))
            {
                return;
            }

            args.Handled(true);

            if (clip->Lanes.empty())
            {
                ShowAddLaneDialog(m_editorClipId);
                return;
            }

            auto const laneIndex = m_clipLanes.CurrentLane();
            auto const& lane = clip->Lanes[laneIndex];
            m_clipLanes.SetCurrentLane(laneIndex);
            InvalidateEditor();

            if (m_clipLanes.HasSelection() && m_clipLanes.SelectedPoint() < lane.Points.size())
            {
                ShowLanePointMenu(m_editorClipId, lane.Id, m_clipLanes.SelectedPoint(), m_editorCanvas, m_clipLanes.PointPosition(laneIndex, m_clipLanes.SelectedPoint(), m_roll));
                return;
            }

            // "Add a point here" adds it at the playhead, or at the start of the clip.
            auto const playhead = m_roll.Playhead();

            seq::LanePoint at{};
            at.Tick = playhead >= 0 && playhead < clip->Length ? playhead : 0;
            at.Value = seq::LaneValueAt(lane, at.Tick).value_or(lane.Target == seq::LaneTarget::PitchBend ? 0x80000000u : 0);
            at.Hold = lane.Target == seq::LaneTarget::Program;

            ShowLaneRowMenu(m_editorClipId, lane.Id, at, m_editorCanvas, m_clipLanes.LaneAnchor(laneIndex));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to open a lane's menu from the keyboard.")
    }
}
