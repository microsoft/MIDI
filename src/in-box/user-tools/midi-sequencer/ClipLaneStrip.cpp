// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ClipLaneStrip.h"
#include "ArrangeRenderer.h"
#include "SequenceEdits.h"

using winrt::Windows::Foundation::Point;
using winrt::Windows::Foundation::Rect;

namespace midisequencer
{
    namespace
    {
        constexpr wchar_t TextFont[] = L"Segoe UI Variable Text";

        // How near a point the pointer has to be to pick it up, in pixels, and how far it has to
        // move before a press is a drag.
        constexpr float PointReach = 6.0f;
        constexpr double DragThreshold = 4.0;

        // With snap off, the pencil leaves a point about this many pixels apart.
        constexpr double PencilPixels = 6.0;

        // No more handles than this are drawn in view: drawn points come thick and fast.
        constexpr size_t MaximumHandles = 2000;

        canvasText::CanvasTextFormat MakeFormat(float size, uint16_t weight, canvasText::CanvasHorizontalAlignment horizontal = canvasText::CanvasHorizontalAlignment::Left)
        {
            canvasText::CanvasTextFormat format{};
            format.FontFamily(TextFont);
            format.FontSize(size);
            format.FontWeight(winrt::Windows::UI::Text::FontWeight{ weight });
            format.WordWrapping(canvasText::CanvasWordWrapping::NoWrap);
            format.TrimmingGranularity(canvasText::CanvasTextTrimmingGranularity::Character);
            format.TrimmingSign(canvasText::CanvasTrimmingSign::Ellipsis);
            format.HorizontalAlignment(horizontal);
            format.VerticalAlignment(canvasText::CanvasVerticalAlignment::Center);
            return format;
        }

        uint32_t ProgramStep(uint32_t value) noexcept
        {
            return ScaleUp(ScaleDown(value, 32, 7), 7, 32);
        }
    }

    _Use_decl_annotations_
    void ClipLaneStrip::SetClip(Clip const* clip, Color color)
    {
        // Compared by id, like the piano roll: the old pointer can be left dangling.
        auto const changed = clip == nullptr || clip->Id != m_clipId;

        m_clip = clip;
        m_clipId = clip != nullptr ? clip->Id : std::wstring{};
        m_color = color;

        auto const laneCount = clip != nullptr ? clip->Lanes.size() : 0;

        // Kept until a drawing takes it: the strip is pointed at the clip again from more places
        // than the drawing, and a lane added in between still fits the roll again.
        m_laneCountChanged = !changed && (m_laneCountChanged || laneCount != m_laneCount);
        m_laneCount = laneCount;

        if (changed)
        {
            m_gesture = Gesture::None;
            m_currentLane = 0;
            m_keyboardFocus = false;
            ClearSelection();
        }

        // A lane can go away under an undo. During a gesture the selection is in the copy.
        if (m_gesture == Gesture::None && m_clip != nullptr && m_selectedLane != SIZE_MAX &&
            (m_selectedLane >= m_clip->Lanes.size() || m_selectedPoint >= m_clip->Lanes[m_selectedLane].Points.size()))
        {
            ClearSelection();
        }
    }

    float ClipLaneStrip::AddRowTop() const noexcept
    {
        auto const lanes = m_clip != nullptr ? ShownLanes().size() : 0;
        return m_top + RowHeight * static_cast<float>(lanes);
    }

    Rect ClipLaneStrip::AddButtonBounds() const noexcept
    {
        return Rect{ 4.0f, AddRowTop() + 3.0f, PianoRoll::KeysWidth - 8.0f, AddRowHeight - 6.0f };
    }

    std::vector<AutomationLane> const& ClipLaneStrip::ShownLanes() const noexcept
    {
        // While a gesture changes the lanes, they're drawn as it has them.
        return m_gesture != Gesture::None ? m_working : m_clip->Lanes;
    }

    _Use_decl_annotations_
    int64_t ClipLaneStrip::SnapTick(double tick, PianoRoll const& roll) const noexcept
    {
        auto value = std::max<int64_t>(0, static_cast<int64_t>(std::floor(tick)));
        auto const snap = roll.SnapTicks();

        if (snap > 0)
        {
            value = (value / snap) * snap;
        }

        return m_clip != nullptr ? std::min(value, m_clip->Length) : value;
    }

    _Use_decl_annotations_
    size_t ClipLaneStrip::HitPoint(AutomationLane const& lane, float top, Point point, PianoRoll const& roll) const noexcept
    {
        size_t best{ SIZE_MAX };
        auto bestDistance = PointReach * PointReach + 0.01f;

        auto const from = roll.TickAtX(static_cast<double>(point.X) - PointReach);
        auto const to = roll.TickAtX(static_cast<double>(point.X) + PointReach);
        auto const& points = lane.Points;

        auto const first = std::lower_bound(points.begin(), points.end(), static_cast<int64_t>(std::floor(from)),
            [](LanePoint const& candidate, int64_t tick) { return candidate.Tick < tick; });

        for (auto it = first; it != points.end() && static_cast<double>(it->Tick) <= to; ++it)
        {
            auto const dx = static_cast<float>(roll.XAtTick(static_cast<double>(it->Tick))) - point.X;
            auto const dy = LaneValueY(it->Value, top, RowHeight) - point.Y;
            auto const distance = dx * dx + dy * dy;

            // At a jump, the later point is the one on top.
            if (distance <= bestDistance)
            {
                best = static_cast<size_t>(std::distance(points.begin(), it));
                bestDistance = distance;
            }
        }

        return best;
    }

    _Use_decl_annotations_
    ClipLaneHit ClipLaneStrip::HitTest(Point point, PianoRoll const& roll) const noexcept
    {
        ClipLaneHit hit{};

        if (m_clip == nullptr || point.Y < m_top)
        {
            return hit;
        }

        auto const& lanes = ShownLanes();
        auto const lane = static_cast<size_t>((point.Y - m_top) / RowHeight);

        if (lane >= lanes.size())
        {
            auto const button = AddButtonBounds();

            if (point.X >= button.X && point.X <= button.X + button.Width && point.Y >= button.Y && point.Y <= button.Y + button.Height)
            {
                hit.What = ClipLaneHit::Kind::AddButton;
            }

            return hit;
        }

        hit.Lane = lane;

        if (point.X < PianoRoll::KeysWidth)
        {
            hit.What = ClipLaneHit::Kind::Header;
            return hit;
        }

        auto const top = LaneTop(lane);
        hit.Tick = SnapTick(roll.TickAtX(point.X), roll);
        hit.Value = LaneValueAtY(point.Y, top, RowHeight);

        if (lanes[lane].Target == LaneTarget::Program)
        {
            hit.Value = ProgramStep(hit.Value);
        }

        hit.Point = HitPoint(lanes[lane], top, point, roll);
        hit.What = hit.Point != SIZE_MAX ? ClipLaneHit::Kind::Point : ClipLaneHit::Kind::Lane;
        return hit;
    }

    // ---------------------------------------------------------------- gestures

    _Use_decl_annotations_
    bool ClipLaneStrip::PointerPressed(Point point, bool pencil, PianoRoll const& roll)
    {
        m_gesture = Gesture::None;
        m_keyboardFocus = false;

        auto const hit = HitTest(point, roll);

        if (m_clip == nullptr || (hit.What != ClipLaneHit::Kind::Lane && hit.What != ClipLaneHit::Kind::Point))
        {
            return false;
        }

        m_before = m_clip->Lanes;
        m_working = m_clip->Lanes;
        m_pressPoint = point;
        m_lane = hit.Lane;
        m_drawn.clear();
        m_lastTick = -1;

        auto& lane = m_working[m_lane];

        if (pencil)
        {
            m_gesture = Gesture::Pencil;
            m_moved = true;
            ClearSelection();
            PointerMoved(point, roll);
            return true;
        }

        m_gesture = Gesture::Move;

        if (hit.What == ClipLaneHit::Kind::Point)
        {
            m_point = hit.Point;
            m_moved = false;
        }
        else
        {
            // Anywhere else on the lane adds a point there, already picked up.
            LanePoint added{};
            added.Tick = hit.Tick;
            added.Value = hit.Value;
            added.Hold = lane.Target == LaneTarget::Program;

            m_point = AddLanePoint(lane, added);
            m_moved = true;
        }

        m_startValue = m_point < lane.Points.size() ? lane.Points[m_point].Value : 0;

        Select(m_lane, m_point);
        return true;
    }

    _Use_decl_annotations_
    bool ClipLaneStrip::PointerMoved(Point point, PianoRoll const& roll, bool keepValue)
    {
        if (m_gesture == Gesture::None || m_lane >= m_working.size())
        {
            return false;
        }

        auto& lane = m_working[m_lane];
        auto const top = LaneTop(m_lane);
        auto value = LaneValueAtY(point.Y, top, RowHeight);

        if (lane.Target == LaneTarget::Program)
        {
            value = ProgramStep(value);
        }

        if (m_gesture == Gesture::Move)
        {
            if (!m_moved)
            {
                auto const dx = static_cast<double>(point.X) - static_cast<double>(m_pressPoint.X);
                auto const dy = static_cast<double>(point.Y) - static_cast<double>(m_pressPoint.Y);

                if (std::hypot(dx, dy) < DragThreshold)
                {
                    return false;
                }

                m_moved = true;
            }

            MoveLanePoint(lane, m_point, SnapTick(roll.TickAtX(point.X), roll), keepValue ? m_startValue : value);
            return true;
        }

        // The pencil leaves a point on each step of the grid it crosses, filling in any a quick
        // stroke skipped, and replaces what was under the stroke.
        auto const pixelsPerTick = std::max(1.0e-9, roll.PixelsPerTick());
        auto const grid = roll.SnapTicks() > 0 ? roll.SnapTicks() : std::max<int64_t>(1, std::llround(PencilPixels / pixelsPerTick));
        auto const limit = m_clip != nullptr ? m_clip->Length : INT64_MAX;
        auto const tick = std::min((std::max<int64_t>(0, static_cast<int64_t>(std::floor(roll.TickAtX(point.X)))) / grid) * grid, limit);

        if (m_lastTick >= 0 && tick != m_lastTick)
        {
            auto const step = tick > m_lastTick ? grid : -grid;
            auto const span = static_cast<double>(tick - m_lastTick);

            for (auto t = m_lastTick + step; step > 0 ? t < tick : t > tick; t += step)
            {
                auto const fraction = static_cast<double>(t - m_lastTick) / span;
                auto const between = static_cast<double>(m_lastValue) + (static_cast<double>(value) - static_cast<double>(m_lastValue)) * fraction;
                m_drawn[t] = static_cast<uint32_t>(std::clamp(std::llround(between), 0LL, 0xFFFFFFFFLL));
            }
        }

        m_drawn[tick] = value;
        m_lastTick = tick;
        m_lastValue = value;

        std::vector<LanePoint> points{};
        points.reserve(m_drawn.size());

        for (auto const& [at, level] : m_drawn)
        {
            points.push_back(LanePoint{ at, level, lane.Target == LaneTarget::Program });
        }

        lane.Points = m_before[m_lane].Points;
        ReplaceLanePoints(lane, m_drawn.begin()->first, m_drawn.rbegin()->first, points);
        return true;
    }

    _Use_decl_annotations_
    std::optional<ClipLaneEdit> ClipLaneStrip::PointerReleased(Point point, PianoRoll const& roll, bool keepValue)
    {
        if (m_gesture == Gesture::None)
        {
            return std::nullopt;
        }

        PointerMoved(point, roll, keepValue);

        auto const gesture = m_gesture;
        m_gesture = Gesture::None;

        if (!m_moved || m_working.size() != m_before.size())
        {
            return std::nullopt;
        }

        ClipLaneEdit edit{};
        edit.Drawn = gesture == Gesture::Pencil;
        edit.Before = std::move(m_before);
        edit.After = std::move(m_working);
        return edit;
    }

    // ---------------------------------------------------------------- selection

    _Use_decl_annotations_
    void ClipLaneStrip::Select(size_t lane, size_t point) noexcept
    {
        m_selectedLane = lane;
        m_selectedPoint = point;
        m_currentLane = lane;
    }

    void ClipLaneStrip::ClearSelection() noexcept
    {
        m_selectedLane = SIZE_MAX;
        m_selectedPoint = SIZE_MAX;
    }

    size_t ClipLaneStrip::CurrentLane() const noexcept
    {
        auto const count = m_clip != nullptr ? m_clip->Lanes.size() : 0;
        return count == 0 ? 0 : std::min(m_currentLane, count - 1);
    }

    _Use_decl_annotations_
    void ClipLaneStrip::SetCurrentLane(size_t lane) noexcept
    {
        m_currentLane = lane;
        m_keyboardFocus = true;
    }

    _Use_decl_annotations_
    Point ClipLaneStrip::LaneAnchor(size_t lane) const noexcept
    {
        return Point{ PianoRoll::KeysWidth + 8.0f, LaneTop(lane) + RowHeight / 2.0f };
    }

    _Use_decl_annotations_
    Point ClipLaneStrip::PointPosition(size_t lane, size_t point, PianoRoll const& roll) const noexcept
    {
        if (m_clip == nullptr || lane >= m_clip->Lanes.size() || point >= m_clip->Lanes[lane].Points.size())
        {
            return LaneAnchor(lane);
        }

        auto const& at = m_clip->Lanes[lane].Points[point];
        auto const x = std::clamp(static_cast<float>(roll.XAtTick(static_cast<double>(at.Tick))), PianoRoll::KeysWidth, std::max(PianoRoll::KeysWidth, m_width));
        return Point{ x, LaneValueY(at.Value, LaneTop(lane), RowHeight) };
    }

    std::optional<ClipLaneEdit> ClipLaneStrip::DeleteSelection()
    {
        if (m_clip == nullptr || !HasSelection() || m_selectedLane >= m_clip->Lanes.size() || m_selectedPoint >= m_clip->Lanes[m_selectedLane].Points.size())
        {
            ClearSelection();
            return std::nullopt;
        }

        ClipLaneEdit edit{};
        edit.Before = m_clip->Lanes;
        edit.After = m_clip->Lanes;

        auto& points = edit.After[m_selectedLane].Points;
        points.erase(points.begin() + static_cast<ptrdiff_t>(m_selectedPoint));

        ClearSelection();
        return edit;
    }

    // ---------------------------------------------------------------- drawing

    void ClipLaneStrip::EnsureFormats()
    {
        if (m_titleText != nullptr)
        {
            return;
        }

        m_titleText = MakeFormat(11.0f, 600);
        m_subText = MakeFormat(10.0f, 400);
        m_buttonText = MakeFormat(11.0f, 400, canvasText::CanvasHorizontalAlignment::Center);
        m_hintText = MakeFormat(11.0f, 400);
    }

    _Use_decl_annotations_
    void ClipLaneStrip::Draw(canvas::CanvasDrawingSession const& ds, float top, float width, PianoRoll const& roll)
    {
        m_top = top;
        m_width = width;

        if (m_palette == nullptr || m_clip == nullptr)
        {
            return;
        }

        EnsureFormats();

        auto const& colors = *m_palette;
        auto const& lanes = ShownLanes();
        auto const left = PianoRoll::KeysWidth;

        for (size_t i = 0; i < lanes.size(); ++i)
        {
            DrawLane(ds, lanes[i], i, LaneTop(i), roll);
        }

        // Where the keyboard is: the lane's cell under the keys, outlined.
        if (m_keyboardFocus && !lanes.empty())
        {
            auto const lane = std::min(m_currentLane, lanes.size() - 1);
            ds.DrawRectangle(Rect{ 1.5f, LaneTop(lane) + 1.5f, left - 3.0f, RowHeight - 3.0f }, colors.SelectionStroke, 2.0f);
        }

        // The row with the button that adds a lane.
        auto const rowTop = AddRowTop();

        ds.Antialiasing(canvas::CanvasAntialiasing::Aliased);
        ds.FillRectangle(0, rowTop, width, 1.0f, colors.Light ? Rgba(0, 0, 0, 0.08) : Rgba(255, 255, 255, 0.10));
        ds.FillRectangle(left - 1.0f, rowTop, 1.0f, AddRowHeight, colors.LaneEdge);
        ds.Antialiasing(canvas::CanvasAntialiasing::Antialiased);

        auto const button = AddButtonBounds();
        ds.FillRoundedRectangle(button, 4.0f, 4.0f, colors.Light ? Rgba(0, 0, 0, 0.05) : Rgba(255, 255, 255, 0.06));
        ds.DrawRoundedRectangle(button, 4.0f, 4.0f, colors.StrokeStrong, 1.0f);
        ds.DrawText(m_strings.AddLane, button, colors.Text, m_buttonText);

        if (lanes.empty() && !m_strings.NoLanes.empty())
        {
            ds.DrawText(m_strings.NoLanes, left + 10.0f, rowTop, std::max(1.0f, width - left - 20.0f), AddRowHeight, colors.Text3, m_hintText);
        }
    }

    _Use_decl_annotations_
    void ClipLaneStrip::DrawLane(canvas::CanvasDrawingSession const& ds, AutomationLane const& lane, size_t index, float top, PianoRoll const& roll)
    {
        auto const& colors = *m_palette;
        auto const left = PianoRoll::KeysWidth;
        auto const width = m_width;
        auto const height = RowHeight;

        ds.Antialiasing(canvas::CanvasAntialiasing::Aliased);
        ds.FillRectangle(0, top, width, 1.0f, colors.Light ? Rgba(0, 0, 0, 0.08) : Rgba(255, 255, 255, 0.10));
        ds.FillRectangle(left, top + 1.0f, width - left, height - 1.0f, colors.RollBackground);
        ds.FillRectangle(left - 1.0f, top, 1.0f, height, colors.LaneEdge);

        // What it changes under the keys, where there's room for little; its name in the lane.
        ds.Antialiasing(canvas::CanvasAntialiasing::Antialiased);

        if (index < m_labels.size())
        {
            ds.DrawText(m_labels[index].Target, 6.0f, top + 4.0f, left - 9.0f, 16.0f, colors.Text, m_titleText);
        }

        auto layer = ds.CreateLayer(1.0f, Rect{ left, top + 1.0f, std::max(0.0f, width - left), height - 1.0f });

        ds.Antialiasing(canvas::CanvasAntialiasing::Aliased);

        // Beats, and past the end of the clip shaded: nothing there plays.
        auto const beatPixels = static_cast<double>(TicksPerQuarterNote) * roll.PixelsPerTick();

        if (beatPixels >= 5.0)
        {
            auto const startTick = std::max<int64_t>(0, static_cast<int64_t>(roll.TickAtX(left)));
            auto const endTick = static_cast<int64_t>(roll.TickAtX(width)) + 1;
            auto const meter = roll.Meter();

            for (auto tick = (startTick / TicksPerQuarterNote) * TicksPerQuarterNote; tick <= endTick; tick += TicksPerQuarterNote)
            {
                auto const onBar = meter != nullptr && !meter->empty()
                    ? BarPositionAtTick(*meter, tick).Beat == 1 && BarPositionAtTick(*meter, tick).TicksIntoBeat == 0
                    : tick % (TicksPerQuarterNote * 4) == 0;

                ds.FillRectangle(std::floor(static_cast<float>(roll.XAtTick(static_cast<double>(tick)))), top + 1.0f, 1.0f, height - 1.0f,
                    onBar ? colors.RollBarLine : colors.RollBeatLine);
            }
        }

        auto const endX = static_cast<float>(roll.XAtTick(static_cast<double>(m_clip->Length)));

        if (endX < width)
        {
            auto const from = std::max(endX, left);
            ds.FillRectangle(from, top + 1.0f, width - from, height - 1.0f, Rgba(0, 0, 0, colors.Light ? 0.06 : 0.22));
        }

        ds.Antialiasing(canvas::CanvasAntialiasing::Antialiased);

        if (index < m_labels.size())
        {
            ds.DrawText(m_labels[index].Name, left + 6.0f, top + 2.0f, std::max(1.0f, width - left - 12.0f), 14.0f, colors.Text2, m_subText);
        }

        auto const& points = lane.Points;

        if (points.empty())
        {
            ds.DrawText(m_strings.EmptyLane, left + 10.0f, top, std::max(1.0f, width - left - 20.0f), height, colors.Text3, m_hintText);
        }
        else
        {
            // From the left edge to the clip's end, ramping or holding the way the lane plays.
            auto const visibleFrom = std::max<int64_t>(0, static_cast<int64_t>(std::floor(roll.TickAtX(left - 2.0))));
            auto const visibleTo = std::min<int64_t>(m_clip->Length, static_cast<int64_t>(std::ceil(roll.TickAtX(width + 2.0))));

            if (visibleTo > visibleFrom)
            {
                auto const x = [&roll](int64_t tick) { return static_cast<float>(roll.XAtTick(static_cast<double>(tick))); };
                auto const y = [top](uint32_t value) { return LaneValueY(value, top, RowHeight); };

                std::vector<std::pair<float, float>> line{};
                auto current = LaneValueAt(lane, visibleFrom).value_or(0);
                line.emplace_back(x(visibleFrom), y(current));

                auto first = static_cast<size_t>(std::distance(points.begin(), std::lower_bound(points.begin(), points.end(), visibleFrom,
                    [](LanePoint const& point, int64_t tick) { return point.Tick < tick; })));

                auto i = first;

                for (; i < points.size() && points[i].Tick < visibleTo; ++i)
                {
                    auto const px = x(points[i].Tick);

                    if (i == 0 || !LaneRampsAfter(lane, i - 1))
                    {
                        line.emplace_back(px, y(current));
                    }

                    current = points[i].Value;
                    line.emplace_back(px, y(current));
                }

                auto const last = i;
                line.emplace_back(x(visibleTo), y(LaneValueAt(lane, visibleTo - 1).value_or(current)));

                auto const build = [&](bool closed)
                {
                    canvasGeometry::CanvasPathBuilder path{ ds };
                    path.BeginFigure(line.front().first, line.front().second);

                    for (size_t k = 1; k < line.size(); ++k)
                    {
                        path.AddLine(line[k].first, line[k].second);
                    }

                    if (closed)
                    {
                        path.AddLine(line.back().first, top + height - 1.0f);
                        path.AddLine(line.front().first, top + height - 1.0f);
                    }

                    path.EndFigure(closed ? canvasGeometry::CanvasFigureLoop::Closed : canvasGeometry::CanvasFigureLoop::Open);
                    return canvasGeometry::CanvasGeometry::CreatePath(path);
                };

                ds.FillGeometry(build(true), WithAlpha(m_color, 0.12));
                ds.DrawGeometry(build(false), m_color, 1.6f);

                // Handles, when there aren't too many to aim at.
                if (last - first <= MaximumHandles)
                {
                    auto lastX = -1000.0f;
                    auto lastY = -1000.0f;

                    for (auto k = first; k < last; ++k)
                    {
                        auto const selected = m_selectedLane == index && m_selectedPoint == k;
                        auto const radius = selected ? 4.0f : 3.0f;
                        auto const px = x(points[k].Tick);
                        auto const py = y(points[k].Value);

                        if (!selected && (px - lastX) * (px - lastX) + (py - lastY) * (py - lastY) < 49.0f)
                        {
                            continue;
                        }

                        lastX = px;
                        lastY = py;

                        ds.FillCircle(px, py, radius, selected ? colors.SelectionStroke : colors.HandleFill);
                        ds.DrawCircle(px, py, radius, selected ? colors.SelectionStroke : m_color, 1.5f);
                    }
                }
            }
        }

        if (auto const playhead = roll.Playhead(); playhead >= 0)
        {
            ds.FillRectangle(static_cast<float>(roll.XAtTick(static_cast<double>(playhead))), top + 1.0f, 1.5f, height - 1.0f, colors.Playhead);
        }

        layer.Close();
    }
}
