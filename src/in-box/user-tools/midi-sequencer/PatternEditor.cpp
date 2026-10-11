// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "PatternEditor.h"

using winrt::Windows::Foundation::Point;
using winrt::Windows::Foundation::Rect;

namespace midisequencer
{
    namespace
    {
        constexpr wchar_t TextFont[] = L"Segoe UI Variable Text";

        // The accent and slide rows, as the comp draws them.
        constexpr Color AccentColor = FromRgb(0xE0B23C);
        constexpr Color SlideColor = FromRgb(0x9C8CFF);

        // A drag this far up or down goes from silent to full velocity.
        constexpr float VelocityDragPixels = 120.0f;

        canvasText::CanvasTextFormat MakeFormat(
            float size,
            uint16_t weight,
            canvasText::CanvasHorizontalAlignment horizontal = canvasText::CanvasHorizontalAlignment::Left,
            canvasText::CanvasVerticalAlignment vertical = canvasText::CanvasVerticalAlignment::Center)
        {
            canvasText::CanvasTextFormat format{};
            format.FontFamily(TextFont);
            format.FontSize(size);
            format.FontWeight(winrt::Windows::UI::Text::FontWeight{ weight });
            format.WordWrapping(canvasText::CanvasWordWrapping::NoWrap);
            format.TrimmingGranularity(canvasText::CanvasTextTrimmingGranularity::Character);
            format.TrimmingSign(canvasText::CanvasTrimmingSign::Ellipsis);
            format.HorizontalAlignment(horizontal);
            format.VerticalAlignment(vertical);
            return format;
        }

        // "E1", with C3 = 60 like the rest of the family. The number is in the inspector.
        std::wstring ShortNoteName(uint8_t number)
        {
            static constexpr std::array<wchar_t const*, 12> Names{ L"C", L"C#", L"D", L"D#", L"E", L"F", L"F#", L"G", L"G#", L"A", L"A#", L"B" };
            return std::format(L"{}{}", Names[number % 12], static_cast<int32_t>(number / 12) - 2);
        }
    }

    void PatternEditor::EnsureFormats()
    {
        if (m_titleText != nullptr)
        {
            return;
        }

        using canvasText::CanvasHorizontalAlignment;
        using canvasText::CanvasVerticalAlignment;

        m_titleText = MakeFormat(12.0f, 600);
        m_numberText = MakeFormat(10.0f, 400, CanvasHorizontalAlignment::Center);
        m_laneTitle = MakeFormat(12.0f, 600);
        m_laneSub = MakeFormat(10.5f, 400);
        m_badgeText = MakeFormat(9.0f, 600, CanvasHorizontalAlignment::Left, CanvasVerticalAlignment::Top);
        m_badgeRight = MakeFormat(9.0f, 600, CanvasHorizontalAlignment::Right, CanvasVerticalAlignment::Top);
        m_letterText = MakeFormat(10.0f, 600, CanvasHorizontalAlignment::Center);
        m_footerText = MakeFormat(11.0f, 400);
        m_footerCenter = MakeFormat(11.0f, 400, CanvasHorizontalAlignment::Center);
        m_footerRightText = MakeFormat(11.0f, 400, CanvasHorizontalAlignment::Right);
        m_centerText = MakeFormat(12.0f, 400, CanvasHorizontalAlignment::Center);
    }

    _Use_decl_annotations_
    void PatternEditor::SetClip(Clip const* clip, Color color)
    {
        // Compared by id: the pointer can be left dangling when the clip list grows.
        auto const changed = clip == nullptr || clip->Id != m_clipId;

        m_clip = clip != nullptr && clip->Pattern.has_value() ? clip : nullptr;
        m_clipId = m_clip != nullptr ? m_clip->Id : std::wstring{};
        m_color = color;

        if (changed)
        {
            m_selectedLane = SIZE_MAX;
            m_gesture = Gesture::None;
            m_scrollY = 0;
            m_playTick = -1;
        }

        if (m_clip != nullptr && m_selectedLane != SIZE_MAX && m_selectedLane >= m_clip->Pattern->Lanes.size())
        {
            m_selectedLane = SIZE_MAX;
        }
    }

    _Use_decl_annotations_
    void PatternEditor::SetFooter(std::wstring left, std::wstring middle, std::wstring right)
    {
        m_footerLeft = std::move(left);
        m_footerMiddle = std::move(middle);
        m_footerRight = std::move(right);
    }

    _Use_decl_annotations_
    void PatternEditor::SetPlayhead(int64_t tick, uint64_t pass) noexcept
    {
        m_playTick = tick;
        m_playPass = pass;
    }

    bool PatternEditor::Melodic() const noexcept
    {
        return m_clip != nullptr && m_clip->Pattern->Mode == PatternMode::Melodic;
    }

    uint16_t PatternEditor::Columns() const noexcept
    {
        if (m_clip == nullptr)
        {
            return 16;
        }

        auto const& pattern = *m_clip->Pattern;
        auto columns = std::clamp<uint16_t>(pattern.StepCount, 1, MaximumPatternSteps);

        for (auto const& lane : pattern.Lanes)
        {
            columns = std::max(columns, LaneStepCount(pattern, lane));
        }

        return columns;
    }

    size_t PatternEditor::RowCount() const noexcept
    {
        if (m_clip == nullptr)
        {
            return 0;
        }

        // Drums: a row for each lane, and the row that adds one. Melodic: note, accent, slide.
        return Melodic() ? 3 : m_clip->Pattern->Lanes.size() + 1;
    }

    _Use_decl_annotations_
    float PatternEditor::RowHeight(size_t row) const noexcept
    {
        if (m_clip == nullptr)
        {
            return LaneHeight;
        }

        if (Melodic())
        {
            return row == 0 ? LaneHeight : SmallRowHeight;
        }

        return row < m_clip->Pattern->Lanes.size() ? LaneHeight : SmallRowHeight;
    }

    _Use_decl_annotations_
    float PatternEditor::RowTop(size_t row) const noexcept
    {
        auto top = GridTop() + 8.0f - m_scrollY;

        for (size_t i = 0; i < row; ++i)
        {
            top += RowHeight(i) + RowGap;
        }

        return top;
    }

    float PatternEditor::ContentHeight() const noexcept
    {
        auto height = 16.0f;

        for (size_t i = 0; i < RowCount(); ++i)
        {
            height += RowHeight(i) + RowGap;
        }

        return height;
    }

    void PatternEditor::ComputeColumns() noexcept
    {
        m_columns = Columns();

        auto const count = static_cast<float>(m_columns);
        auto const groups = static_cast<float>((m_columns + 3) / 4);
        auto const gaps = (count - 1.0f) * CellGap + std::max(0.0f, groups - 1.0f) * (GroupGap - CellGap);
        auto const available = m_width - LaneHeaderWidth - 14.0f;

        m_cellWidth = std::clamp((available - gaps) / count, 10.0f, 56.0f);
    }

    _Use_decl_annotations_
    float PatternEditor::ColumnX(uint16_t column) const noexcept
    {
        return LaneHeaderWidth + static_cast<float>(column) * (m_cellWidth + CellGap) + static_cast<float>(column / 4) * (GroupGap - CellGap);
    }

    void PatternEditor::ClampScroll() noexcept
    {
        auto const visible = GridBottom() - GridTop();
        m_scrollY = std::clamp(m_scrollY, 0.0f, std::max(0.0f, ContentHeight() - visible));
    }

    _Use_decl_annotations_
    void PatternEditor::EnsureVisible(size_t row) noexcept
    {
        auto const top = RowTop(row);
        auto const bottom = top + RowHeight(row);

        if (top < GridTop())
        {
            m_scrollY -= GridTop() - top + 4.0f;
        }
        else if (bottom > GridBottom())
        {
            m_scrollY += bottom - GridBottom() + 4.0f;
        }

        ClampScroll();
    }

    uint64_t PatternEditor::PlayingGlobalStep() const noexcept
    {
        if (m_clip == nullptr || m_playTick < 0)
        {
            return UINT64_MAX;
        }

        auto const& pattern = *m_clip->Pattern;
        auto const stepCount = static_cast<uint64_t>(std::clamp<uint16_t>(pattern.StepCount, 1, MaximumPatternSteps));
        auto const local = static_cast<uint64_t>(m_playTick / std::max<int64_t>(1, pattern.StepTicks));

        return m_playPass * stepCount + std::min(local, stepCount - 1);
    }

    bool PatternEditor::HasSelection() const noexcept
    {
        return m_clip != nullptr && m_selectedLane < m_clip->Pattern->Lanes.size();
    }

    _Use_decl_annotations_
    void PatternEditor::Select(size_t lane, uint16_t step) noexcept
    {
        m_selectedLane = lane;
        m_selectedStep = step;

        // A melodic step turned on next gets the note of the one picked.
        if (Melodic() && lane < m_clip->Pattern->Lanes.size())
        {
            if (auto const existing = FindStep(m_clip->Pattern->Lanes[lane], step); existing != nullptr)
            {
                m_nextNote = existing->Note;
            }
        }
    }

    _Use_decl_annotations_
    bool PatternEditor::MoveSelection(int32_t steps, int32_t lanes) noexcept
    {
        if (!HasSelection())
        {
            return false;
        }

        auto const& pattern = *m_clip->Pattern;
        auto const lane = static_cast<size_t>(std::clamp<int64_t>(static_cast<int64_t>(m_selectedLane) + lanes, 0, static_cast<int64_t>(pattern.Lanes.size()) - 1));
        auto const laneSteps = LaneStepCount(pattern, pattern.Lanes[lane]);
        auto const step = static_cast<uint16_t>(std::clamp<int32_t>(static_cast<int32_t>(m_selectedStep) + steps, 0, laneSteps - 1));

        if (lane == m_selectedLane && step == m_selectedStep)
        {
            return false;
        }

        Select(lane, step);
        EnsureVisible(Melodic() ? 0 : lane);
        return true;
    }

    // ---------------------------------------------------------------- drawing

    _Use_decl_annotations_
    void PatternEditor::Draw(canvas::CanvasDrawingSession const& ds, float width, float height)
    {
        m_width = width;
        m_height = height;

        if (m_palette == nullptr)
        {
            return;
        }

        EnsureFormats();

        auto const& colors = *m_palette;
        ds.FillRectangle(0, 0, width, height, colors.RollBackground);

        if (m_clip == nullptr)
        {
            return;
        }

        auto const& pattern = *m_clip->Pattern;
        auto const melodic = Melodic();

        ComputeColumns();
        ClampScroll();

        auto const stepCount = std::clamp<uint16_t>(pattern.StepCount, 1, MaximumPatternSteps);
        auto const global = PlayingGlobalStep();

        // The title, and where it's playing.
        ds.DrawText(melodic ? m_strings.MelodicSteps : m_strings.DrumLanes, 16.0f, 0, LaneHeaderWidth, TitleHeight, colors.Text2, m_titleText);

        if (global != UINT64_MAX && m_strings.Playing)
        {
            ds.DrawText(winrt::hstring{ m_strings.Playing(global / stepCount + 1, global % stepCount + 1) },
                LaneHeaderWidth, 0, std::max(1.0f, width - LaneHeaderWidth - 14.0f), TitleHeight, colors.Text3, m_footerRightText);
        }

        // Step numbers. Steps past the clip's own length (in a longer lane) are dimmer.
        for (uint16_t column = 0; column < m_columns; ++column)
        {
            ds.DrawText(winrt::hstring{ std::to_wstring(column + 1) }, ColumnX(column), TitleHeight, m_cellWidth, NumbersHeight,
                column < stepCount ? colors.Text3 : WithAlpha(colors.Text3, 0.45), m_numberText);
        }

        {
            auto layer = ds.CreateLayer(1.0f, Rect{ 0, GridTop(), width, std::max(0.0f, GridBottom() - GridTop()) });

            if (melodic && !pattern.Lanes.empty())
            {
                auto const& lane = pattern.Lanes.front();
                auto const laneSteps = LaneStepCount(pattern, lane);
                auto const playing = global == UINT64_MAX ? -1 : static_cast<int32_t>(global % laneSteps);
                auto const selectedStep = m_selectedLane == 0 ? static_cast<int32_t>(m_selectedStep) : -1;

                auto top = RowTop(0);
                DrawLaneHeader(ds, top, LaneHeight, m_color, std::wstring{ m_strings.NoteRow }, std::wstring{ m_strings.NoteRowHint }, nullptr);

                for (uint16_t column = 0; column < m_columns; ++column)
                {
                    auto const step = column < laneSteps ? FindStep(lane, column) : nullptr;
                    DrawCell(ds, ColumnX(column), top, m_cellWidth, LaneHeight, step, column >= laneSteps, selectedStep == column, playing == column,
                        step != nullptr ? ShortNoteName(step->Note) : std::wstring{});
                }

                for (size_t row = 1; row <= 2; ++row)
                {
                    auto const color = row == 1 ? AccentColor : SlideColor;
                    top = RowTop(row);
                    DrawLaneHeader(ds, top, SmallRowHeight, color, std::wstring{ row == 1 ? m_strings.AccentRow : m_strings.SlideRow }, {}, nullptr);

                    for (uint16_t column = 0; column < m_columns; ++column)
                    {
                        auto const step = column < laneSteps ? FindStep(lane, column) : nullptr;
                        auto const on = step != nullptr && (row == 1 ? step->Accent : step->Slide);

                        if (column >= laneSteps)
                        {
                            DrawCell(ds, ColumnX(column), top, m_cellWidth, SmallRowHeight, nullptr, true, false, false, {});
                        }
                        else
                        {
                            DrawFlagCell(ds, ColumnX(column), top, m_cellWidth, SmallRowHeight, on, color, false, playing == column);
                        }
                    }
                }
            }
            else
            {
                for (size_t i = 0; i < pattern.Lanes.size(); ++i)
                {
                    auto const top = RowTop(i);

                    if (top > GridBottom() || top + LaneHeight < GridTop())
                    {
                        continue;
                    }

                    auto const& lane = pattern.Lanes[i];
                    auto const laneSteps = LaneStepCount(pattern, lane);
                    auto const name = m_strings.LaneName ? m_strings.LaneName(lane) : lane.Name;
                    auto const detail = m_strings.LaneDetail ? m_strings.LaneDetail(lane, laneSteps) : std::wstring{};

                    DrawLaneHeader(ds, top, LaneHeight, m_color, name, detail, &lane);

                    auto const playing = global == UINT64_MAX ? -1 : static_cast<int32_t>(global % laneSteps);

                    for (uint16_t column = 0; column < m_columns; ++column)
                    {
                        auto const step = column < laneSteps ? FindStep(lane, column) : nullptr;
                        auto const selected = m_selectedLane == i && m_selectedStep == column;
                        DrawCell(ds, ColumnX(column), top, m_cellWidth, LaneHeight, step, column >= laneSteps, selected, playing == column, {});
                    }
                }

                // Add a lane.
                auto const addTop = RowTop(pattern.Lanes.size());

                if (pattern.Lanes.size() < MaximumPatternLanes)
                {
                    canvasGeometry::CanvasStrokeStyle dashed{};
                    dashed.CustomDashStyle({ 3.0f, 2.0f });

                    Rect const box{ 12.0f, addTop, LaneHeaderWidth - 24.0f, SmallRowHeight };
                    ds.DrawRoundedRectangle(box, 4.0f, 4.0f, colors.EmptySlotStroke, 1.0f, dashed);
                    ds.DrawText(winrt::hstring{ L"+  " + std::wstring{ m_strings.AddLane } }, box, colors.Text, m_centerText);
                }
            }

            layer.Close();
        }

        // The status line.
        auto const footerTop = height - FooterHeight;
        ds.FillRectangle(0, footerTop, width, 1.0f, colors.Divider);

        auto const usable = std::max(1.0f, width - 28.0f);
        ds.DrawText(winrt::hstring{ m_footerLeft }, 14.0f, footerTop + 1.0f, usable * 0.27f, FooterHeight - 1.0f, colors.Text3, m_footerText);
        ds.DrawText(winrt::hstring{ m_footerMiddle }, 14.0f + usable * 0.28f, footerTop + 1.0f, usable * 0.38f, FooterHeight - 1.0f, colors.Text3, m_footerCenter);
        ds.DrawText(winrt::hstring{ m_footerRight }, 14.0f + usable * 0.67f, footerTop + 1.0f, usable * 0.33f, FooterHeight - 1.0f, colors.Text3, m_footerRightText);
    }

    _Use_decl_annotations_
    void PatternEditor::DrawLaneHeader(
        canvas::CanvasDrawingSession const& ds,
        float top,
        float height,
        Color bar,
        std::wstring const& name,
        std::wstring const& detail,
        PatternLane const* lane)
    {
        auto const& colors = *m_palette;
        auto const left = 12.0f;
        auto const right = LaneHeaderWidth - 12.0f;

        ds.FillRoundedRectangle(left, top, right - left, height, 4.0f, 4.0f, colors.LauncherBackground);
        ds.DrawRoundedRectangle(left + 0.5f, top + 0.5f, right - left - 1.0f, height - 1.0f, 4.0f, 4.0f, colors.Divider, 1.0f);
        ds.FillRoundedRectangle(left + 5.0f, top + 6.0f, 3.0f, std::max(2.0f, height - 12.0f), 1.5f, 1.5f, bar);

        auto const textLeft = left + 15.0f;
        auto const textRight = lane != nullptr ? right - 52.0f : right - 8.0f;
        auto const textWidth = std::max(1.0f, textRight - textLeft);

        if (detail.empty())
        {
            ds.DrawText(winrt::hstring{ name }, textLeft, top, textWidth, height, colors.Text, m_laneTitle);
        }
        else
        {
            ds.DrawText(winrt::hstring{ name }, textLeft, top + 4.0f, textWidth, 18.0f, colors.Text, m_laneTitle);
            ds.DrawText(winrt::hstring{ detail }, textLeft, top + 22.0f, textWidth, 16.0f, colors.Text3, m_laneSub);
        }

        if (lane == nullptr)
        {
            return;
        }

        // M and S, the way the track headers draw them.
        for (int32_t i = 0; i < 2; ++i)
        {
            auto const x = right - 46.0f + static_cast<float>(i) * 22.0f;
            auto const y = top + (height - 18.0f) / 2.0f;
            auto const on = i == 0 ? lane->Muted : lane->Soloed;
            auto const fill = on ? (i == 0 ? colors.Mute : colors.Solo) : colors.EmptySlotFill;

            ds.FillRoundedRectangle(x, y, 18.0f, 18.0f, 3.0f, 3.0f, fill);
            ds.DrawRoundedRectangle(x + 0.5f, y + 0.5f, 17.0f, 17.0f, 3.0f, 3.0f, colors.StrokeStrong, 1.0f);
            ds.DrawText(i == 0 ? m_strings.MuteLetter : m_strings.SoloLetter, x, y, 18.0f, 18.0f, on ? FromRgb(0x1A1A1A) : colors.Text2, m_letterText);
        }
    }

    _Use_decl_annotations_
    void PatternEditor::DrawCell(
        canvas::CanvasDrawingSession const& ds,
        float x,
        float y,
        float w,
        float h,
        PatternStep const* step,
        bool pastEnd,
        bool selected,
        bool playing,
        std::wstring const& label)
    {
        auto const& colors = *m_palette;
        constexpr float Radius = 3.0f;

        if (pastEnd)
        {
            // Past this lane's end: hatched.
            ds.FillRoundedRectangle(x, y, w, h, Radius, Radius, WithAlpha(colors.EmptySlotFill, 0.5));

            {
                auto layer = ds.CreateLayer(1.0f, Rect{ x, y, w, h });

                for (auto d = -h; d < w; d += 7.0f)
                {
                    ds.DrawLine(x + d, y + h, x + d + h, y, WithAlpha(colors.EmptySlotStroke, 0.8), 1.0f);
                }

                layer.Close();
            }

            ds.DrawRoundedRectangle(x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f, Radius, Radius, WithAlpha(colors.EmptySlotStroke, 0.6), 1.0f);
            return;
        }

        if (step == nullptr)
        {
            ds.FillRoundedRectangle(x, y, w, h, Radius, Radius, colors.EmptySlotFill);
            ds.DrawRoundedRectangle(x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f, Radius, Radius, colors.EmptySlotStroke, 1.0f);
        }
        else
        {
            // Bar height is velocity. The light theme's roll background is nearly transparent, so
            // the step's color is mixed with white there.
            auto const velocity = m_gesture == Gesture::Velocity && selected ? m_dragVelocity : step->Velocity;
            auto const fraction = std::clamp(static_cast<float>(velocity) / 65535.0f, 0.04f, 1.0f);
            auto const barTop = y + h * (1.0f - fraction);
            auto const base = colors.Light ? FromRgb(0xFFFFFF) : colors.RollBackground;

            ds.FillRoundedRectangle(x, y, w, h, Radius, Radius, Mix(m_color, 0.42, base));

            {
                auto layer = ds.CreateLayer(1.0f, Rect{ x, barTop, w, y + h - barTop });
                ds.FillRoundedRectangle(x, y, w, h, Radius, Radius, Mix(m_color, 0.85, base));
                layer.Close();
            }

            ds.DrawRoundedRectangle(x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f, Radius, Radius, WithAlpha(m_color, 0.9), 1.0f);

            auto const ink = colors.Light ? FromRgb(0x1A1A1A) : Rgba(255, 255, 255);

            // Top left: the note in a melodic pattern, or the chance.
            if (!label.empty())
            {
                ds.DrawText(winrt::hstring{ label }, x + 3.0f, y + 2.0f, std::max(1.0f, w - 5.0f), 14.0f, ink, m_badgeText);
            }
            else if (step->Chance < 100)
            {
                ds.DrawText(winrt::hstring{ std::format(L"{}%", step->Chance) }, x + 3.0f, y + 2.0f, std::max(1.0f, w - 5.0f), 12.0f, ink, m_badgeText);
            }

            // Top right: when it plays.
            if (step->Condition != StepCondition::Always)
            {
                winrt::hstring condition{};

                switch (step->Condition)
                {
                case StepCondition::Every: condition = winrt::hstring{ std::format(L"{}:{}", step->Of, step->Every) }; break;
                case StepCondition::FirstPass: condition = m_strings.BadgeFirst; break;
                default: condition = m_strings.BadgeNotFirst; break;
                }

                ds.DrawText(condition, x + 2.0f, y + (label.empty() && step->Chance < 100 ? 13.0f : 2.0f), std::max(1.0f, w - 5.0f), 12.0f, ink, m_badgeRight);
            }

            // Along the bottom: a dash for each ratchet repeat.
            if (step->Ratchet > 1)
            {
                auto const count = static_cast<float>(step->Ratchet);
                auto const share = std::max(2.0f, (w - 10.0f) / count);

                for (uint8_t r = 0; r < step->Ratchet; ++r)
                {
                    ds.FillRectangle(x + 5.0f + static_cast<float>(r) * share + 1.0f, y + h - 6.0f, std::max(1.0f, share - 2.0f), 2.0f, ink);
                }
            }

            // Bottom left: nudged early or late.
            if (step->Nudge != 0)
            {
                ds.DrawText(step->Nudge < 0 ? L"\u25C2" : L"\u25B8", x + 2.0f, y + h - (step->Ratchet > 1 ? 20.0f : 15.0f), 10.0f, 12.0f, ink, m_badgeText);
            }
        }

        if (playing)
        {
            ds.DrawRoundedRectangle(x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f, Radius + 1.0f, Radius + 1.0f, WithAlpha(colors.Text, 0.85), 1.2f);
        }

        if (selected)
        {
            ds.DrawRoundedRectangle(x - 1.5f, y - 1.5f, w + 3.0f, h + 3.0f, Radius + 1.5f, Radius + 1.5f, colors.Accent, 2.0f);
        }
    }

    _Use_decl_annotations_
    void PatternEditor::DrawFlagCell(
        canvas::CanvasDrawingSession const& ds,
        float x,
        float y,
        float w,
        float h,
        bool on,
        Color color,
        bool selected,
        bool playing)
    {
        auto const& colors = *m_palette;
        constexpr float Radius = 3.0f;

        if (on)
        {
            ds.FillRoundedRectangle(x, y, w, h, Radius, Radius, Mix(color, 0.8, colors.RollBackground));
            ds.DrawRoundedRectangle(x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f, Radius, Radius, WithAlpha(color, 0.9), 1.0f);
        }
        else
        {
            ds.FillRoundedRectangle(x, y, w, h, Radius, Radius, colors.EmptySlotFill);
            ds.DrawRoundedRectangle(x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f, Radius, Radius, colors.EmptySlotStroke, 1.0f);
        }

        if (playing)
        {
            ds.DrawRoundedRectangle(x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f, Radius + 1.0f, Radius + 1.0f, WithAlpha(colors.Text, 0.85), 1.2f);
        }

        if (selected)
        {
            ds.DrawRoundedRectangle(x - 1.5f, y - 1.5f, w + 3.0f, h + 3.0f, Radius + 1.5f, Radius + 1.5f, colors.Accent, 2.0f);
        }
    }

    // ---------------------------------------------------------------- pointing

    _Use_decl_annotations_
    PatternHit PatternEditor::HitTest(Point point) const noexcept
    {
        PatternHit hit{};

        if (m_clip == nullptr || point.Y < GridTop() || point.Y >= GridBottom())
        {
            return hit;
        }

        auto const& pattern = *m_clip->Pattern;
        auto const melodic = Melodic();
        auto const rows = RowCount();

        for (size_t row = 0; row < rows; ++row)
        {
            auto const top = RowTop(row);
            auto const height = RowHeight(row);

            if (point.Y < top || point.Y >= top + height)
            {
                continue;
            }

            if (!melodic && row == pattern.Lanes.size())
            {
                if (point.X >= 12.0f && point.X < LaneHeaderWidth - 12.0f && pattern.Lanes.size() < MaximumPatternLanes)
                {
                    hit.Target = PatternTarget::AddLane;
                }

                return hit;
            }

            auto const lane = melodic ? size_t{ 0 } : row;

            if (lane >= pattern.Lanes.size())
            {
                return hit;
            }

            hit.Lane = lane;

            if (point.X < LaneHeaderWidth - 8.0f)
            {
                if (melodic || point.X < 12.0f)
                {
                    return hit;
                }

                auto const right = LaneHeaderWidth - 12.0f;
                auto const y = top + (height - 18.0f) / 2.0f;

                if (point.Y >= y && point.Y < y + 18.0f)
                {
                    if (point.X >= right - 46.0f && point.X < right - 28.0f)
                    {
                        hit.Target = PatternTarget::LaneMute;
                        return hit;
                    }

                    if (point.X >= right - 24.0f && point.X < right - 6.0f)
                    {
                        hit.Target = PatternTarget::LaneSolo;
                        return hit;
                    }
                }

                hit.Target = PatternTarget::LaneName;
                return hit;
            }

            auto const laneSteps = LaneStepCount(pattern, pattern.Lanes[lane]);

            for (uint16_t column = 0; column < m_columns; ++column)
            {
                auto const x = ColumnX(column);
                auto const next = column + 1 < m_columns ? ColumnX(static_cast<uint16_t>(column + 1)) : x + m_cellWidth + CellGap;

                if (point.X >= x - CellGap / 2.0f && point.X < next - CellGap / 2.0f)
                {
                    // Past the lane's end, nothing plays, so there's nothing to turn on.
                    if (column < laneSteps)
                    {
                        hit.Step = column;
                        hit.Target = !melodic || row == 0 ? PatternTarget::Step : row == 1 ? PatternTarget::Accent : PatternTarget::Slide;
                    }

                    return hit;
                }
            }

            return hit;
        }

        return hit;
    }

    _Use_decl_annotations_
    bool PatternEditor::PointerPressed(Point point, bool rightButton)
    {
        m_gesture = Gesture::None;

        auto const hit = HitTest(point);

        if (hit.Target != PatternTarget::Step && hit.Target != PatternTarget::Accent && hit.Target != PatternTarget::Slide)
        {
            return false;
        }

        Select(hit.Lane, hit.Step);

        if (rightButton)
        {
            return true;
        }

        m_pressHit = hit;
        m_pressPoint = point;

        if (hit.Target == PatternTarget::Step)
        {
            auto const step = FindStep(m_clip->Pattern->Lanes[hit.Lane], hit.Step);
            m_gesture = Gesture::Step;
            m_startVelocity = step != nullptr ? step->Velocity : 0;
            m_dragVelocity = m_startVelocity;
            m_pressedOn = step != nullptr;
        }
        else
        {
            m_gesture = Gesture::Flag;
        }

        return true;
    }

    _Use_decl_annotations_
    bool PatternEditor::PointerMoved(Point point)
    {
        if ((m_gesture != Gesture::Step && m_gesture != Gesture::Velocity) || !m_pressedOn)
        {
            return false;
        }

        auto const rise = m_pressPoint.Y - point.Y;

        if (m_gesture == Gesture::Step && std::abs(rise) < 4.0f)
        {
            return false;
        }

        m_gesture = Gesture::Velocity;

        auto const value = static_cast<double>(m_startVelocity) + static_cast<double>(rise) * 65535.0 / VelocityDragPixels;
        m_dragVelocity = static_cast<uint16_t>(std::clamp(std::lround(value), 1L, 65535L));
        return true;
    }

    _Use_decl_annotations_
    std::optional<PatternEdit> PatternEditor::PointerReleased(Point)
    {
        auto const gesture = m_gesture;
        m_gesture = Gesture::None;

        if (gesture == Gesture::None || m_clip == nullptr)
        {
            return std::nullopt;
        }

        PatternEdit edit{};
        edit.After = *m_clip->Pattern;

        if (m_pressHit.Lane >= edit.After.Lanes.size())
        {
            return std::nullopt;
        }

        auto& lane = edit.After.Lanes[m_pressHit.Lane];
        auto const note = edit.After.Mode == PatternMode::Melodic ? m_nextNote : lane.Note;

        switch (gesture)
        {
        case Gesture::Velocity:
        {
            auto const step = FindStep(lane, m_pressHit.Step);

            if (step == nullptr || step->Velocity == m_dragVelocity)
            {
                return std::nullopt;
            }

            step->Velocity = m_dragVelocity;
            edit.Name = m_strings.UndoVelocity;
            return edit;
        }

        case Gesture::Step:
            SetStepOn(lane, m_pressHit.Step, !m_pressedOn, note);
            edit.Name = m_strings.UndoSteps;
            return edit;

        case Gesture::Flag:
        {
            auto const step = SetStepOn(lane, m_pressHit.Step, true, note);

            if (step == nullptr)
            {
                return std::nullopt;
            }

            if (m_pressHit.Target == PatternTarget::Accent)
            {
                step->Accent = !step->Accent;
            }
            else
            {
                step->Slide = !step->Slide;
            }

            edit.Name = m_strings.UndoSteps;
            return edit;
        }

        default:
            return std::nullopt;
        }
    }

    void PatternEditor::CancelGesture() noexcept
    {
        m_gesture = Gesture::None;
    }

    _Use_decl_annotations_
    bool PatternEditor::Wheel(int32_t delta)
    {
        auto const before = m_scrollY;
        m_scrollY -= static_cast<float>(delta) * 0.5f;
        ClampScroll();
        return m_scrollY != before;
    }
}
