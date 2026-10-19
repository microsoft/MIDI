// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "GeneratorView.h"

#include "SequenceGenerator.h"

namespace midisequencer
{
    namespace
    {
        constexpr double Pi = 3.14159265358979323846;

        // After the clip's own color, the comps' lime and teal, then more that sit well beside them.
        constexpr std::array<uint32_t, 7> RingColors{ 0xB5E86A, 0x4DD0C4, 0xF2C14E, 0xFF8A65, 0x9C8CFF, 0x60CDFF, 0xF48FB1 };

        bool IsBlackKey(int32_t number) noexcept
        {
            switch (number % 12)
            {
            case 1:
            case 3:
            case 6:
            case 8:
            case 10:
                return true;

            default:
                return false;
            }
        }

        foundation::Numerics::float2 PointOnRing(RingGeometry const& geometry, float radius, double turn) noexcept
        {
            auto const angle = -Pi / 2.0 + turn * 2.0 * Pi;
            return foundation::Numerics::float2{
                geometry.CenterX + radius * static_cast<float>(std::cos(angle)),
                geometry.CenterY + radius * static_cast<float>(std::sin(angle)) };
        }
    }

    _Use_decl_annotations_
    Color RingColor(Color clipColor, size_t ring) noexcept
    {
        return ring == 0 ? clipColor : FromRgb(RingColors[(ring - 1) % RingColors.size()]);
    }

    _Use_decl_annotations_
    RingGeometry MeasureRings(float width, float height, size_t ringCount) noexcept
    {
        RingGeometry geometry{};
        auto const size = std::max(1.0f, std::min(width, height));

        geometry.CenterX = width / 2.0f;
        geometry.CenterY = height / 2.0f;
        geometry.Outer = std::max(8.0f, size / 2.0f - 14.0f);

        // The comps space rings 14% of the size apart. More rings share what's left, leaving the
        // middle open.
        auto const room = geometry.Outer * 0.78f;
        geometry.Spacing = ringCount > 1 ? std::min(size * 0.14f, room / static_cast<float>(ringCount - 1)) : 0.0f;

        return geometry;
    }

    _Use_decl_annotations_
    size_t RingAtPoint(RingGeometry const& geometry, size_t ringCount, float x, float y) noexcept
    {
        auto const distance = std::hypot(x - geometry.CenterX, y - geometry.CenterY);
        auto const reach = std::max(8.0f, geometry.Spacing / 2.0f);

        size_t best{ SIZE_MAX };
        auto bestGap = reach;

        for (size_t i = 0; i < ringCount; ++i)
        {
            auto const gap = std::abs(distance - geometry.Radius(i));

            if (gap <= bestGap)
            {
                best = i;
                bestGap = gap;
            }
        }

        return best;
    }

    _Use_decl_annotations_
    double TurnAtPoint(RingGeometry const& geometry, float x, float y) noexcept
    {
        // 0 at the top, going clockwise the way the steps go.
        auto const angle = std::atan2(static_cast<double>(x - geometry.CenterX), -static_cast<double>(y - geometry.CenterY));
        auto const turn = angle / (2.0 * Pi);
        return turn < 0 ? turn + 1.0 : turn;
    }

    _Use_decl_annotations_
    void DrawRings(
        canvas::CanvasDrawingSession const& ds,
        float width,
        float height,
        std::vector<EuclidRing> const& rings,
        Color clipColor,
        int64_t globalStep,
        double passTurn,
        size_t selected,
        Palette const& colors)
    {
        auto const geometry = MeasureRings(width, height, rings.size());
        auto const faint = colors.Light ? Rgba(0, 0, 0, 0.10) : Rgba(255, 255, 255, 0.10);
        auto const rest = colors.Light ? Rgba(0, 0, 0, 0.22) : Rgba(255, 255, 255, 0.22);
        auto const mark = colors.Light ? Rgba(0, 0, 0, 0.85) : Rgba(255, 255, 255, 1.0);

        // Dots get smaller as rings crowd together.
        auto const pulseRadius = rings.size() > 1 ? std::clamp(geometry.Spacing * 0.2f, 3.0f, 6.0f) : 6.0f;
        auto const restRadius = std::max(1.5f, pulseRadius * 0.43f);

        for (size_t r = 0; r < rings.size(); ++r)
        {
            auto const& ring = rings[r];
            auto const radius = geometry.Radius(r);

            if (radius < 4.0f)
            {
                break;
            }

            auto const color = RingColor(clipColor, r);
            auto const steps = std::max<uint8_t>(1, ring.Steps);
            auto const hits = EuclideanRhythm(ring.Pulses, ring.Steps, ring.Rotate);
            auto const current = globalStep >= 0 ? static_cast<size_t>(globalStep % steps) : SIZE_MAX;

            ds.DrawCircle(geometry.CenterX, geometry.CenterY, radius, r == selected ? WithAlpha(color, 0.65) : faint, r == selected ? 2.0f : 1.0f);

            // The pulses joined up, so a ring's shape shows at a glance.
            std::vector<foundation::Numerics::float2> pulses{};

            for (size_t i = 0; i < steps && i < hits.size(); ++i)
            {
                if (hits[i])
                {
                    pulses.push_back(PointOnRing(geometry, radius, static_cast<double>(i) / static_cast<double>(steps)));
                }
            }

            if (pulses.size() > 2)
            {
                canvasGeometry::CanvasPathBuilder path{ ds };
                path.BeginFigure(pulses.front());

                for (size_t i = 1; i < pulses.size(); ++i)
                {
                    path.AddLine(pulses[i]);
                }

                path.EndFigure(canvasGeometry::CanvasFigureLoop::Closed);

                auto const shape = canvasGeometry::CanvasGeometry::CreatePath(path);
                ds.FillGeometry(shape, WithAlpha(color, 0.07));
                ds.DrawGeometry(shape, WithAlpha(color, 0.35), 1.0f);
            }

            for (size_t i = 0; i < steps && i < hits.size(); ++i)
            {
                auto const at = PointOnRing(geometry, radius, static_cast<double>(i) / static_cast<double>(steps));
                auto const size = hits[i] ? pulseRadius : restRadius;

                ds.FillCircle(at, size, hits[i] ? color : rest);

                if (i == current)
                {
                    ds.DrawCircle(at, size + 2.0f, mark, 2.0f);
                }
            }
        }

        // How far through the pass, as a hand from the middle.
        if (passTurn >= 0)
        {
            auto const tip = PointOnRing(geometry, geometry.Outer + 8.0f, passTurn);
            ds.DrawLine(geometry.CenterX, geometry.CenterY, tip.x, tip.y, colors.Light ? Rgba(0, 0, 0, 0.45) : Rgba(255, 255, 255, 0.55), 1.2f);
        }
    }

    _Use_decl_annotations_
    void DrawRoll(
        canvas::CanvasDrawingSession const& ds,
        float width,
        float height,
        std::vector<RollNote> const& notes,
        int64_t ticks,
        int64_t ticksPerBar,
        int64_t ticksPerBeat,
        uint8_t low,
        uint8_t high,
        float keyWidth,
        Color noteColor,
        int64_t playhead,
        Palette const& colors)
    {
        if (high < low)
        {
            std::swap(low, high);
        }

        ticks = std::max<int64_t>(1, ticks);

        auto const rows = static_cast<int32_t>(high) - static_cast<int32_t>(low) + 1;
        auto const rowHeight = height / static_cast<float>(rows);
        auto const left = std::max(0.0f, keyWidth);
        auto const span = std::max(1.0f, width - left);

        auto const x = [&](int64_t tick) { return left + static_cast<float>(static_cast<double>(tick) / static_cast<double>(ticks)) * span; };
        auto const y = [&](int32_t number) { return static_cast<float>(high - number) * rowHeight; };

        ds.Antialiasing(canvas::CanvasAntialiasing::Aliased);
        ds.FillRectangle(0, 0, width, height, colors.RollBackground);

        for (auto number = static_cast<int32_t>(low); number <= static_cast<int32_t>(high); ++number)
        {
            if (IsBlackKey(number))
            {
                ds.FillRectangle(left, y(number), span, rowHeight, colors.RollBlackKeyRow);
            }

            if (number % 12 == 0)
            {
                ds.FillRectangle(left, y(number) + rowHeight - 1.0f, span, 1.0f, colors.RollOctaveLine);
            }
        }

        if (ticksPerBeat > 0 && static_cast<double>(ticksPerBeat) / static_cast<double>(ticks) * span >= 3.0)
        {
            for (int64_t tick = 0; tick <= ticks; tick += ticksPerBeat)
            {
                auto const bar = ticksPerBar > 0 && tick % ticksPerBar == 0;
                ds.FillRectangle(std::floor(x(tick)), 0, 1.0f, height, bar ? colors.RollBarLine : colors.RollBeatLine);
            }
        }

        if (left > 0)
        {
            for (auto number = static_cast<int32_t>(low); number <= static_cast<int32_t>(high); ++number)
            {
                ds.FillRectangle(0, y(number), left, rowHeight, IsBlackKey(number) ? colors.KeyBlack : colors.KeyWhite);
                ds.FillRectangle(0, y(number) + rowHeight - 0.5f, left, 0.5f, colors.KeyEdge);
            }

            ds.FillRectangle(left - 1.0f, 0, 1.0f, height, colors.KeyEdge);

            // C on each octave, when the rows are tall enough to read.
            if (rowHeight >= 7.0f)
            {
                canvasText::CanvasTextFormat format{};
                format.FontFamily(L"Segoe UI Variable Text");
                format.FontSize(std::min(9.0f, rowHeight));
                format.VerticalAlignment(canvasText::CanvasVerticalAlignment::Center);
                format.HorizontalAlignment(canvasText::CanvasHorizontalAlignment::Right);
                format.WordWrapping(canvasText::CanvasWordWrapping::NoWrap);

                for (auto number = static_cast<int32_t>(low); number <= static_cast<int32_t>(high); ++number)
                {
                    if (number % 12 == 0)
                    {
                        ds.DrawText(winrt::hstring{ std::format(L"C{}", number / 12 - 2) }, 0, y(number), left - 3.0f, rowHeight, colors.KeyLabel, format);
                    }
                }
            }
        }

        ds.Antialiasing(canvas::CanvasAntialiasing::Antialiased);

        auto const outline = colors.Light ? Rgba(0, 0, 0, 0.85) : Rgba(255, 255, 255, 0.95);

        for (auto const& note : notes)
        {
            if (note.Number < low || note.Number > high || note.Tick >= ticks)
            {
                continue;
            }

            auto const x0 = x(note.Tick) + 1.0f;
            auto const x1 = std::max(x0 + 3.0f, x(std::min(ticks, note.Tick + note.Length)) - 1.0f);
            auto const top = y(note.Number) + std::min(1.0f, rowHeight * 0.15f);
            auto const h = std::max(2.0f, rowHeight - std::min(2.0f, rowHeight * 0.3f));
            auto const alpha = 0.55 + 0.45 * static_cast<double>(note.Velocity) / 65535.0;

            ds.FillRoundedRectangle(x0, top, x1 - x0, h, 2.0f, 2.0f, WithAlpha(noteColor, alpha));

            if (note.Outlined)
            {
                ds.DrawRoundedRectangle(x0 - 0.5f, top - 0.5f, x1 - x0 + 1.0f, h + 1.0f, 2.5f, 2.5f, outline, 1.5f);
            }
        }

        if (playhead >= 0 && playhead <= ticks)
        {
            ds.FillRectangle(x(playhead) - 0.75f, 0, 1.5f, height, colors.Playhead);
        }
    }
}
