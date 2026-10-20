// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "GeneratorDrawings.h"

#include "SequenceGenerator.h"

namespace midisequencer
{
    namespace
    {
        constexpr double Pi = 3.14159265358979323846;

        // Ringers draw the treble's path in red and a working bell's in blue.
        constexpr Color TrebleRed{ 255, 0xE5, 0x48, 0x4D };
        constexpr Color WorkingBlue{ 255, 0x3E, 0x8E, 0xF7 };

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

        canvasText::CanvasTextFormat Format(float size, canvasText::CanvasHorizontalAlignment alignment = canvasText::CanvasHorizontalAlignment::Center)
        {
            canvasText::CanvasTextFormat format{};
            format.FontFamily(L"Segoe UI Variable Text");
            format.FontSize(std::max(6.0f, size));
            format.HorizontalAlignment(alignment);
            format.VerticalAlignment(canvasText::CanvasVerticalAlignment::Center);
            format.WordWrapping(canvasText::CanvasWordWrapping::NoWrap);
            format.TrimmingGranularity(canvasText::CanvasTextTrimmingGranularity::Character);
            return format;
        }

        int32_t WhiteKeysBefore(KeyboardSpan span, int32_t note) noexcept
        {
            int32_t count{ 0 };

            for (auto n = static_cast<int32_t>(span.First); n < note; ++n)
            {
                count += IsBlackKey(n) ? 0 : 1;
            }

            return count;
        }

        // A small roll of notes in a box, for Evolve's copies.
        void DrawMiniRoll(
            canvas::CanvasDrawingSession const& ds,
            float x,
            float y,
            float width,
            float height,
            std::vector<StepNote> const& notes,
            int64_t steps,
            uint8_t low,
            uint8_t high,
            Color color,
            Palette const& colors)
        {
            ds.FillRoundedRectangle(x, y, width, height, 3.0f, 3.0f, colors.RollBackground);

            auto const rows = std::max(1, static_cast<int32_t>(high) - static_cast<int32_t>(low) + 1);
            auto const rowHeight = std::max(1.5f, (height - 4.0f) / static_cast<float>(rows));
            auto const stepWidth = width / static_cast<float>(std::max<int64_t>(1, steps));

            for (auto const& note : notes)
            {
                auto const top = y + 2.0f + static_cast<float>(static_cast<int32_t>(high) - static_cast<int32_t>(note.Number)) * rowHeight;
                ds.FillRoundedRectangle(x + static_cast<float>(note.Step) * stepWidth + 0.5f, top,
                    std::max(2.0f, static_cast<float>(note.Steps) * stepWidth - 1.0f), std::max(1.5f, rowHeight), 1.0f, 1.0f, WithAlpha(color, 0.9));
            }
        }

        // Notes as outlines over a small roll: what Evolve aims to sound like.
        void DrawGhostNotes(
            canvas::CanvasDrawingSession const& ds,
            float x,
            float y,
            float width,
            float height,
            std::vector<StepNote> const& notes,
            int64_t steps,
            uint8_t low,
            uint8_t high,
            Color color)
        {
            auto const rows = std::max(1, static_cast<int32_t>(high) - static_cast<int32_t>(low) + 1);
            auto const rowHeight = std::max(1.5f, (height - 4.0f) / static_cast<float>(rows));
            auto const stepWidth = width / static_cast<float>(std::max<int64_t>(1, steps));

            for (auto const& note : notes)
            {
                auto const row = std::clamp(static_cast<int32_t>(high) - static_cast<int32_t>(note.Number), 0, rows - 1);
                ds.DrawRoundedRectangle(x + static_cast<float>(note.Step) * stepWidth + 0.5f, y + 2.0f + static_cast<float>(row) * rowHeight,
                    std::max(2.0f, static_cast<float>(note.Steps) * stepWidth - 1.0f), std::max(1.5f, rowHeight), 1.0f, 1.0f, color, 1.0f);
            }
        }
    }

    // ---------------------------------------------------------------- the key and range

    _Use_decl_annotations_
    KeyboardSpan MeasureKeyboard(uint8_t low, uint8_t high) noexcept
    {
        if (high < low)
        {
            std::swap(low, high);
        }

        auto const first = std::clamp((std::min<int32_t>(low, 48) - 12) / 12 * 12, 0, 108);
        auto const last = std::clamp((std::max<int32_t>(high, 84) + 12) / 12 * 12 + 11, 11, 127);
        return KeyboardSpan{ static_cast<uint8_t>(first), static_cast<uint8_t>(last) };
    }

    _Use_decl_annotations_
    int32_t KeyAtPoint(KeyboardSpan span, float width, float height, float x, float y) noexcept
    {
        auto const whites = WhiteKeysBefore(span, static_cast<int32_t>(span.Last) + 1);

        if (whites <= 0 || width <= 0)
        {
            return -1;
        }

        auto const whiteWidth = width / static_cast<float>(whites);
        auto const keys = std::max(8.0f, height - 12.0f);

        // The black keys sit on top of the white ones.
        if (y < keys * 0.62f)
        {
            for (auto n = static_cast<int32_t>(span.First); n <= static_cast<int32_t>(span.Last); ++n)
            {
                if (IsBlackKey(n))
                {
                    auto const middle = static_cast<float>(WhiteKeysBefore(span, n)) * whiteWidth;

                    if (std::abs(x - middle) <= whiteWidth * 0.31f)
                    {
                        return n;
                    }
                }
            }
        }

        auto const index = std::clamp(static_cast<int32_t>(x / whiteWidth), 0, whites - 1);
        int32_t seen{ 0 };

        for (auto n = static_cast<int32_t>(span.First); n <= static_cast<int32_t>(span.Last); ++n)
        {
            if (!IsBlackKey(n))
            {
                if (seen == index)
                {
                    return n;
                }

                ++seen;
            }
        }

        return -1;
    }

    _Use_decl_annotations_
    void DrawKeyboard(
        canvas::CanvasDrawingSession const& ds,
        float width,
        float height,
        uint8_t root,
        ScaleKind scale,
        uint8_t low,
        uint8_t high,
        Color color,
        Palette const& colors,
        KeyboardSpan const* layout)
    {
        if (high < low)
        {
            std::swap(low, high);
        }

        auto const span = layout != nullptr ? *layout : MeasureKeyboard(low, high);
        auto const whites = WhiteKeysBefore(span, static_cast<int32_t>(span.Last) + 1);

        if (whites <= 0)
        {
            return;
        }

        auto const whiteWidth = width / static_cast<float>(whites);
        auto const keys = std::max(8.0f, height - 12.0f);
        auto const blackWidth = whiteWidth * 0.62f;
        auto const blackHeight = keys * 0.6f;
        auto const scaleNotes = ScaleNotes(root, scale, 0, 127);
        auto const inScale = [&](int32_t n) { return std::binary_search(scaleNotes.begin(), scaleNotes.end(), static_cast<uint8_t>(n)); };

        // The octave names sit under the keys, on the panel, so they take the panel's text color
        // rather than the color for writing on a white key.
        auto const label = Format(9.0f);

        auto const fill = [&](int32_t n, bool black)
        {
            auto const inRange = n >= low && n <= high;

            if (inRange && inScale(n))
            {
                return black ? Mix(color, 0.72, Rgba(0, 0, 0)) : WithAlpha(color, 0.9);
            }

            if (inRange)
            {
                return black ? colors.KeyBlack : Mix(colors.KeyWhite, 0.82, color);
            }

            return black ? WithAlpha(colors.KeyBlack, 0.55) : WithAlpha(colors.KeyWhite, 0.45);
        };

        for (auto n = static_cast<int32_t>(span.First); n <= static_cast<int32_t>(span.Last); ++n)
        {
            if (IsBlackKey(n))
            {
                continue;
            }

            auto const x = static_cast<float>(WhiteKeysBefore(span, n)) * whiteWidth;
            ds.FillRoundedRectangle(x + 0.5f, 0, whiteWidth - 1.0f, keys, 1.5f, 1.5f, fill(n, false));

            if (n % 12 == root % 12 && n >= low && n <= high)
            {
                ds.FillCircle(x + whiteWidth / 2.0f, keys - 4.0f, std::min(2.2f, whiteWidth / 4.0f), colors.Light ? Rgba(255, 255, 255) : Rgba(20, 20, 20));
            }

            if (n % 12 == 0)
            {
                ds.DrawText(winrt::hstring{ std::format(L"C{}", n / 12 - 2) }, foundation::Rect{ x - 6.0f, keys, whiteWidth + 12.0f, 12.0f }, colors.Text2, label);
            }
        }

        for (auto n = static_cast<int32_t>(span.First); n <= static_cast<int32_t>(span.Last); ++n)
        {
            if (!IsBlackKey(n))
            {
                continue;
            }

            auto const x = static_cast<float>(WhiteKeysBefore(span, n)) * whiteWidth - blackWidth / 2.0f;
            ds.FillRoundedRectangle(x, 0, blackWidth, blackHeight, 1.0f, 1.0f, fill(n, true));

            if (n % 12 == root % 12 && n >= low && n <= high)
            {
                ds.FillCircle(x + blackWidth / 2.0f, blackHeight - 3.5f, std::min(2.0f, blackWidth / 4.0f), Rgba(255, 255, 255));
            }
        }

        // A grip at each end of the range.
        auto const edge = [&](int32_t n, bool left)
        {
            auto const black = IsBlackKey(n);
            auto const middle = static_cast<float>(WhiteKeysBefore(span, n)) * whiteWidth + (black ? 0.0f : whiteWidth / 2.0f);
            auto const half = (black ? blackWidth : whiteWidth) / 2.0f;
            return left ? middle - half : middle + half;
        };

        for (auto const& [note, left] : { std::pair<int32_t, bool>{ low, true }, std::pair<int32_t, bool>{ high, false } })
        {
            auto const x = edge(note, left);
            ds.FillRoundedRectangle(x - 1.5f, -1.0f, 3.0f, keys + 2.0f, 1.5f, 1.5f, colors.Accent);
        }
    }

    // ---------------------------------------------------------------- Life

    _Use_decl_annotations_
    BoardLayout MeasureLifeBoard(float width, float height, int32_t columns, int32_t rows, float labelWidth) noexcept
    {
        BoardLayout layout{};
        layout.Columns = std::max(1, columns);
        layout.Rows = std::max(1, rows);
        layout.Left = labelWidth;

        auto const across = (width - labelWidth) / static_cast<float>(layout.Columns);
        auto const down = height / static_cast<float>(layout.Rows);
        layout.Cell = std::clamp(std::floor(std::min(across, down)), 4.0f, 26.0f);
        return layout;
    }

    _Use_decl_annotations_
    bool CellAtPoint(BoardLayout const& layout, float x, float y, int32_t& column, int32_t& row) noexcept
    {
        column = static_cast<int32_t>(std::floor((x - layout.Left) / layout.Cell));
        auto const fromTop = static_cast<int32_t>(std::floor(y / layout.Cell));
        row = layout.Rows - 1 - fromTop;

        return x >= layout.Left && column >= 0 && column < layout.Columns && y >= 0 && row >= 0 && row < layout.Rows;
    }

    _Use_decl_annotations_
    void DrawLifeBoard(
        canvas::CanvasDrawingSession const& ds,
        LifeBoard const& board,
        BoardLayout const& layout,
        std::vector<std::wstring> const& names,
        std::vector<bool> const& roots,
        int64_t playingStep,
        Color color,
        Palette const& colors)
    {
        auto const cell = layout.Cell;
        auto const left = layout.Left;
        auto const width = cell * static_cast<float>(layout.Columns);
        auto const height = cell * static_cast<float>(layout.Rows);
        auto const y = [&](int32_t row) { return static_cast<float>(layout.Rows - 1 - row) * cell; };

        ds.FillRoundedRectangle(left, 0, width, height, 4.0f, 4.0f, colors.RollBackground);

        for (int32_t row = 0; row < layout.Rows; ++row)
        {
            if (static_cast<size_t>(row) < roots.size() && roots[static_cast<size_t>(row)])
            {
                ds.FillRectangle(left, y(row), width, cell, WithAlpha(color, 0.08));
            }
        }

        // A line each beat, stronger each bar.
        for (int32_t column = 4; column < layout.Columns; column += 4)
        {
            ds.FillRectangle(left + static_cast<float>(column) * cell - 0.5f, 0, 1.0f, height, column % 16 == 0 ? colors.RollBarLine : colors.RollBeatLine);
        }

        if (playingStep >= 0 && playingStep < layout.Columns)
        {
            ds.FillRectangle(left + static_cast<float>(playingStep) * cell, 0, cell, height, WithAlpha(colors.Playhead, 0.22));
        }

        auto const round = std::min(3.0f, cell / 4.0f);
        auto const outline = colors.Light ? Rgba(0, 0, 0, 0.75) : Rgba(255, 255, 255, 0.9);

        for (int32_t row = 0; row < board.Rows && row < layout.Rows; ++row)
        {
            for (int32_t column = 0; column < board.Columns && column < layout.Columns; ++column)
            {
                auto const state = board.At(column, row);
                auto const x = left + static_cast<float>(column) * cell;

                if (state == 0)
                {
                    if (cell >= 10.0f)
                    {
                        ds.FillCircle(x + cell / 2.0f, y(row) + cell / 2.0f, 1.0f, WithAlpha(colors.KeyEdge, 0.6));
                    }

                    continue;
                }

                auto const lit = column == playingStep;
                ds.FillRoundedRectangle(x + 1.0f, y(row) + 1.0f, cell - 2.0f, cell - 2.0f, round, round,
                    state == 2 || lit ? color : WithAlpha(color, 0.62));

                if (state == 2 && cell >= 9.0f)
                {
                    ds.DrawRoundedRectangle(x + 1.5f, y(row) + 1.5f, cell - 3.0f, cell - 3.0f, round, round, outline, 1.0f);
                }
            }
        }

        // The rows' names, when there's room to read them.
        if (left > 8.0f && cell >= 7.0f)
        {
            auto format = Format(std::min(10.0f, cell - 1.0f), canvasText::CanvasHorizontalAlignment::Right);
            format.TrimmingSign(canvasText::CanvasTrimmingSign::Ellipsis);

            for (int32_t row = 0; row < layout.Rows && static_cast<size_t>(row) < names.size(); ++row)
            {
                auto const isRoot = static_cast<size_t>(row) < roots.size() && roots[static_cast<size_t>(row)];
                ds.DrawText(winrt::hstring{ names[static_cast<size_t>(row)] }, foundation::Rect{ 0, y(row), left - 5.0f, cell }, isRoot ? colors.Text : colors.Text2, format);
            }
        }
    }

    _Use_decl_annotations_
    FilmLayout MeasureFilmstrip(float width, float height, int32_t columns, int32_t rows, uint64_t around) noexcept
    {
        FilmLayout layout{};
        auto const mini = std::clamp((height - 14.0f) / static_cast<float>(std::max(1, rows)), 1.0f, 3.0f);
        layout.Frame = mini * static_cast<float>(std::max(1, columns)) + 2.0f;
        layout.Gap = 6.0f;
        layout.Count = static_cast<size_t>(std::max(1.0f, std::floor((width + layout.Gap) / (layout.Frame + layout.Gap))));

        // The pass shown sits near the right, with the next one in view.
        auto const count = static_cast<uint64_t>(layout.Count);
        layout.First = count < 3 || around + 2 < count ? 0 : around + 2 - count;
        return layout;
    }

    _Use_decl_annotations_
    uint64_t FilmPassAtPoint(FilmLayout const& layout, float x) noexcept
    {
        if (x < 0)
        {
            return UINT64_MAX;
        }

        auto const index = static_cast<size_t>(x / (layout.Frame + layout.Gap));
        auto const into = x - static_cast<float>(index) * (layout.Frame + layout.Gap);
        return index < layout.Count && into <= layout.Frame ? layout.First + index : UINT64_MAX;
    }

    _Use_decl_annotations_
    void DrawFilmstrip(
        canvas::CanvasDrawingSession const& ds,
        float height,
        std::vector<LifeBoard> const& boards,
        FilmLayout const& layout,
        uint64_t shown,
        uint64_t playing,
        Color color,
        Palette const& colors)
    {
        auto const number = Format(9.0f);

        for (size_t i = 0; i < boards.size() && i < layout.Count; ++i)
        {
            auto const& board = boards[i];
            auto const pass = layout.First + i;
            auto const x = static_cast<float>(i) * (layout.Frame + layout.Gap);
            auto const mini = (layout.Frame - 2.0f) / static_cast<float>(std::max(1, board.Columns));
            auto const boardHeight = mini * static_cast<float>(board.Rows) + 2.0f;

            ds.FillRoundedRectangle(x, 0, layout.Frame, boardHeight, 2.0f, 2.0f, colors.RollBackground);

            for (int32_t row = 0; row < board.Rows; ++row)
            {
                for (int32_t column = 0; column < board.Columns; ++column)
                {
                    if (auto const state = board.At(column, row); state != 0)
                    {
                        ds.FillRectangle(x + 1.0f + static_cast<float>(column) * mini, 1.0f + static_cast<float>(board.Rows - 1 - row) * mini,
                            mini, mini, state == 2 ? color : WithAlpha(color, 0.55));
                    }
                }
            }

            auto const stroke = pass == playing ? color : pass == shown ? colors.Accent : colors.StrokeStrong;
            ds.DrawRoundedRectangle(x, 0, layout.Frame, boardHeight, 2.0f, 2.0f, stroke, pass == shown || pass == playing ? 1.8f : 0.6f);

            ds.DrawText(winrt::hstring{ std::to_wstring(pass + 1) }, foundation::Rect{ x, boardHeight, layout.Frame, std::max(10.0f, height - boardHeight) },
                pass == shown ? colors.Text : colors.Text2, number);

            if (board.Settled)
            {
                ds.FillCircle(x + layout.Frame - 3.0f, boardHeight + 6.0f, 2.0f, colors.Caution);
            }
        }
    }

    // ---------------------------------------------------------------- Learn

    _Use_decl_annotations_
    void DrawLearnedMoves(
        canvas::CanvasDrawingSession const& ds,
        float width,
        float height,
        std::vector<std::vector<uint32_t>> const& moves,
        std::vector<std::wstring> const& names,
        std::vector<int32_t> const& walk,
        int32_t playing,
        Color color,
        Palette const& colors)
    {
        auto const count = names.size();

        if (count == 0)
        {
            return;
        }

        // Room outside the ring for each note's circle and the loop drawn for a repeat.
        auto const radius = std::max(20.0f, std::min(width, height) / 2.0f - 34.0f);
        auto const node = std::clamp(radius * 0.13f, 10.0f, 16.0f);
        auto const centerX = width / 2.0f;
        auto const centerY = height / 2.0f;

        auto const at = [&](size_t i)
        {
            auto const angle = 2.0 * Pi * static_cast<double>(i) / static_cast<double>(count);
            return foundation::Numerics::float2{ centerX + radius * static_cast<float>(std::sin(angle)), centerY - radius * static_cast<float>(std::cos(angle)) };
        };

        uint32_t most{ 1 };

        for (auto const& row : moves)
        {
            for (auto const value : row)
            {
                most = std::max(most, value);
            }
        }

        std::set<std::pair<int32_t, int32_t>> walked{};

        for (size_t i = 1; i < walk.size(); ++i)
        {
            walked.emplace(walk[i - 1], walk[i]);
        }

        auto const quiet = colors.Light ? Rgba(0, 0, 0) : Rgba(255, 255, 255);

        for (size_t from = 0; from < count && from < moves.size(); ++from)
        {
            for (size_t to = 0; to < count && to < moves[from].size(); ++to)
            {
                auto const value = moves[from][to];

                if (value == 0)
                {
                    continue;
                }

                auto const strength = static_cast<float>(value) / static_cast<float>(most);
                auto const lit = walked.contains({ static_cast<int32_t>(from), static_cast<int32_t>(to) });
                auto const stroke = lit ? WithAlpha(color, 0.95) : WithAlpha(quiet, 0.10 + 0.30 * strength);
                auto const thickness = 1.0f + 3.0f * strength + (lit ? 1.0f : 0.0f);

                auto const p = at(from);
                auto const q = at(to);

                if (from == to)
                {
                    // A repeat: a small loop outside the note.
                    auto const outX = (p.x - centerX) / radius;
                    auto const outY = (p.y - centerY) / radius;
                    ds.DrawCircle(p.x + outX * (node + 6.0f), p.y + outY * (node + 6.0f), 7.0f, stroke, thickness);
                    continue;
                }

                // A curve that bends to its right, so a move and its way back don't overlap.
                auto const dx = q.x - p.x;
                auto const dy = q.y - p.y;
                foundation::Numerics::float2 const control{ (p.x + q.x) / 2.0f - dy * 0.14f, (p.y + q.y) / 2.0f + dx * 0.14f };

                auto const backX = q.x - control.x;
                auto const backY = q.y - control.y;
                auto const back = std::max(1.0f, std::hypot(backX, backY));
                foundation::Numerics::float2 const end{ q.x - backX / back * (node + 2.0f), q.y - backY / back * (node + 2.0f) };

                canvasGeometry::CanvasPathBuilder path{ ds };
                path.BeginFigure(p);
                path.AddQuadraticBezier(control, end);
                path.EndFigure(canvasGeometry::CanvasFigureLoop::Open);
                ds.DrawGeometry(canvasGeometry::CanvasGeometry::CreatePath(path), stroke, thickness);

                // The arrowhead.
                auto const ux = backX / back;
                auto const uy = backY / back;
                auto const head = 5.0f + thickness;

                canvasGeometry::CanvasPathBuilder arrow{ ds };
                arrow.BeginFigure(end);
                arrow.AddLine(foundation::Numerics::float2{ end.x - ux * head - uy * head * 0.55f, end.y - uy * head + ux * head * 0.55f });
                arrow.AddLine(foundation::Numerics::float2{ end.x - ux * head + uy * head * 0.55f, end.y - uy * head - ux * head * 0.55f });
                arrow.EndFigure(canvasGeometry::CanvasFigureLoop::Closed);
                ds.FillGeometry(canvasGeometry::CanvasGeometry::CreatePath(arrow), stroke);
            }
        }

        auto const label = Format(std::min(12.0f, node * 0.8f));

        for (size_t i = 0; i < count; ++i)
        {
            auto const p = at(i);
            auto const visited = std::find(walk.begin(), walk.end(), static_cast<int32_t>(i)) != walk.end();
            auto const isPlaying = playing == static_cast<int32_t>(i);

            ds.FillCircle(p, node, isPlaying ? color : visited ? Mix(color, 0.30, colors.RollBackground) : colors.RollBackground);
            ds.DrawCircle(p, node, isPlaying || visited ? color : colors.StrokeStrong, isPlaying ? 2.5f : 1.2f);
            ds.DrawText(winrt::hstring{ names[i] }, foundation::Rect{ p.x - node * 2.0f, p.y - node, node * 4.0f, node * 2.0f },
                isPlaying ? (colors.Light ? Rgba(255, 255, 255) : Rgba(10, 10, 10)) : colors.Text, label);
        }
    }

    // ---------------------------------------------------------------- Words

    _Use_decl_annotations_
    void DrawSpelling(
        canvas::CanvasDrawingSession const& ds,
        float width,
        float height,
        std::vector<SpelledStep> const& spelled,
        int64_t from,
        int64_t count,
        uint8_t low,
        uint8_t high,
        int64_t playing,
        Color color,
        Palette const& colors)
    {
        count = std::max<int64_t>(1, count);

        auto const length = SpelledLength(spelled);
        auto const letters = 22.0f;
        auto const top = letters + 4.0f;
        auto const rollHeight = std::max(10.0f, height - top);
        auto const rows = std::max(1, static_cast<int32_t>(high) - static_cast<int32_t>(low) + 3);
        auto const rowHeight = rollHeight / static_cast<float>(rows);
        auto const stepWidth = width / static_cast<float>(count);

        auto const x = [&](int64_t step) { return static_cast<float>(step - from) * stepWidth; };
        auto const y = [&](int32_t number) { return top + static_cast<float>(static_cast<int32_t>(high) + 1 - number) * rowHeight; };

        ds.FillRoundedRectangle(0, top, width, rollHeight, 4.0f, 4.0f, colors.RollBackground);

        for (auto number = static_cast<int32_t>(low) - 1; number <= static_cast<int32_t>(high) + 1; ++number)
        {
            if (IsBlackKey(number))
            {
                ds.FillRectangle(0, y(number), width, rowHeight, colors.RollBlackKeyRow);
            }
        }

        for (auto step = from - from % 4 + 4; step < from + count; step += 4)
        {
            ds.FillRectangle(x(step) - 0.5f, top, 1.0f, rollHeight, colors.RollBeatLine);
        }

        auto const letterFormat = Format(std::clamp(stepWidth * 0.85f, 9.0f, 16.0f));

        // Long words wrap round, so their start can show after their end.
        for (int64_t wrap = 0; wrap < 2 && length > 0; ++wrap)
        {
            for (auto const& at : spelled)
            {
                auto const step = at.Step + wrap * length;

                if (step + at.Steps <= from || step >= from + count)
                {
                    continue;
                }

                auto const lit = playing >= step && playing < step + at.Steps;

                if (at.Number != 0)
                {
                    ds.FillRoundedRectangle(x(step) + 1.0f, y(at.Number), std::max(3.0f, static_cast<float>(at.Steps) * stepWidth - 2.0f), std::max(2.0f, rowHeight), 2.0f, 2.0f,
                        lit || at.Accent ? color : WithAlpha(color, 0.72));

                    if (lit)
                    {
                        ds.DrawRoundedRectangle(x(step) + 0.5f, y(at.Number) - 0.5f, std::max(3.0f, static_cast<float>(at.Steps) * stepWidth - 1.0f), std::max(2.0f, rowHeight) + 1.0f, 2.5f, 2.5f,
                            colors.Light ? Rgba(0, 0, 0, 0.8) : Rgba(255, 255, 255, 0.95), 1.5f);
                    }
                }

                if (at.Letter != 0)
                {
                    // A letter said twice holds its note, so it shows over each step it holds.
                    for (int64_t held = 0; held < at.Steps; ++held)
                    {
                        if (step + held >= from && step + held < from + count)
                        {
                            ds.DrawText(winrt::hstring{ std::wstring(1, at.Letter) }, foundation::Rect{ x(step + held), 0, stepWidth, letters },
                                lit ? color : at.Accent ? colors.Text : colors.Text2, letterFormat);
                        }
                    }
                }
            }
        }

        if (playing >= from && playing < from + count)
        {
            ds.FillRectangle(x(playing) - 0.75f, top, 1.5f, rollHeight, colors.Playhead);
        }
    }

    // ---------------------------------------------------------------- Evolve

    _Use_decl_annotations_
    void DrawFamily(
        canvas::CanvasDrawingSession const& ds,
        float width,
        float height,
        EvolveFamily const& family,
        int64_t steps,
        uint8_t low,
        uint8_t high,
        Color color,
        Palette const& colors)
    {
        auto const hasChildren = std::any_of(family.Children.begin(), family.Children.end(), [](auto const& child) { return !child.empty(); });
        auto const parentHeight = hasChildren ? std::max(30.0f, height * 0.30f) : height;
        auto const parentWidth = hasChildren ? width * 0.5f : width;
        auto const parentLeft = (width - parentWidth) / 2.0f;

        DrawMiniRoll(ds, parentLeft, 0, parentWidth, parentHeight, family.Parent, steps, low, high, color, colors);

        // What it heard, as outlines the notes can grow into.
        if (!family.Target.empty())
        {
            DrawGhostNotes(ds, parentLeft, 0, parentWidth, parentHeight, family.Target, steps, low, high, WithAlpha(colors.Text, 0.6));
        }

        if (!hasChildren)
        {
            return;
        }

        constexpr size_t Across = 4;
        auto const gap = 8.0f;
        auto const bar = 4.0f;
        auto const barGap = 4.0f;
        auto const top = parentHeight + 22.0f;

        // In from each side a little, so the chosen copy's frame isn't cut at the edge.
        auto const inset = 2.0f;
        auto const childWidth = (width - inset * 2.0f - gap * static_cast<float>(Across - 1)) / static_cast<float>(Across);

        // Two rows, each copy with its score bar under it, so the lower row's bars fit too.
        auto const childHeight = std::max(16.0f, (height - top - gap - (barGap + bar) * 2.0f) / 2.0f);
        auto const quiet = colors.Light ? Rgba(0, 0, 0, 0.18) : Rgba(255, 255, 255, 0.18);

        auto best = 0.0;
        auto worst = 1.0;

        for (auto const score : family.Scores)
        {
            best = std::max(best, score);
            worst = std::min(worst, score);
        }

        for (size_t child = 0; child < EvolveChildren; ++child)
        {
            auto const column = child % Across;
            auto const row = child / Across;
            auto const x = inset + static_cast<float>(column) * (childWidth + gap);
            auto const y = top + static_cast<float>(row) * (childHeight + barGap + bar + gap);
            auto const chosen = child == family.Chosen;

            // Lines from the pass before to each copy, like a family tree.
            ds.DrawLine(width / 2.0f, parentHeight, x + childWidth / 2.0f, y, chosen ? WithAlpha(color, 0.8) : quiet, chosen ? 1.6f : 1.0f);

            DrawMiniRoll(ds, x, y, childWidth, childHeight, family.Children[child], steps, low, high, chosen ? color : WithAlpha(colors.Text3, 0.9), colors);

            if (chosen)
            {
                ds.DrawRoundedRectangle(x - 1.0f, y - 1.0f, childWidth + 2.0f, childHeight + 2.0f, 4.0f, 4.0f, color, 2.0f);
            }
            else
            {
                // A faint edge, so each copy reads as a little roll of its own.
                ds.DrawRoundedRectangle(x, y, childWidth, childHeight, 3.0f, 3.0f, quiet, 1.0f);
            }

            // Its score, from the worst copy's to the best's.
            auto const share = best > worst ? (family.Scores[child] - worst) / (best - worst) : 1.0;
            ds.FillRoundedRectangle(x, y + childHeight + barGap, childWidth, bar, 2.0f, 2.0f, WithAlpha(colors.Text3, 0.2));
            ds.FillRoundedRectangle(x, y + childHeight + barGap, std::max(bar, childWidth * static_cast<float>(0.15 + 0.85 * share)), bar, 2.0f, 2.0f,
                chosen ? color : WithAlpha(colors.Text3, 0.7));
        }
    }

    _Use_decl_annotations_
    void DrawScores(
        canvas::CanvasDrawingSession const& ds,
        float width,
        float height,
        std::vector<double> const& scores,
        uint64_t shown,
        Color color,
        Palette const& colors)
    {
        ds.FillRoundedRectangle(0, 0, width, height, 4.0f, 4.0f, colors.RollBackground);

        if (scores.size() < 2)
        {
            return;
        }

        auto const [lowest, highest] = std::minmax_element(scores.begin(), scores.end());
        auto const bottom = *lowest - 0.02;
        auto const top = *highest + 0.02;

        auto const x = [&](size_t i) { return 4.0f + static_cast<float>(i) / static_cast<float>(scores.size() - 1) * (width - 8.0f); };
        auto const y = [&](double score) { return 4.0f + static_cast<float>((top - score) / (top - bottom)) * (height - 8.0f); };

        canvasGeometry::CanvasPathBuilder line{ ds };
        line.BeginFigure(x(0), y(scores[0]));

        for (size_t i = 1; i < scores.size(); ++i)
        {
            line.AddLine(x(i), y(scores[i]));
        }

        line.EndFigure(canvasGeometry::CanvasFigureLoop::Open);
        ds.DrawGeometry(canvasGeometry::CanvasGeometry::CreatePath(line), color, 1.5f);

        if (shown < scores.size())
        {
            ds.FillCircle(x(static_cast<size_t>(shown)), y(scores[static_cast<size_t>(shown)]), 3.5f, color);
            ds.DrawCircle(x(static_cast<size_t>(shown)), y(scores[static_cast<size_t>(shown)]), 5.0f, colors.Text, 1.0f);
        }
    }

    // ---------------------------------------------------------------- the infinity series

    _Use_decl_annotations_
    void DrawSeries(
        canvas::CanvasDrawingSession const& ds,
        float width,
        float height,
        uint64_t first,
        uint64_t count,
        uint64_t passFirst,
        int64_t steps,
        int64_t playing,
        Color color,
        Palette const& colors)
    {
        count = std::max<uint64_t>(1, count);
        ds.FillRoundedRectangle(0, 0, width, height, 4.0f, 4.0f, colors.RollBackground);

        int32_t largest{ 4 };

        for (uint64_t i = 0; i < count; ++i)
        {
            largest = std::max(largest, std::abs(InfinitySeries(first + i)));
        }

        // In from each side a little, so the first and last dots and the playing ring aren't cut.
        auto const margin = 8.0f;
        auto const spacing = std::max(1.0f, width - margin * 2.0f) / static_cast<float>(count);
        auto const x = [&](uint64_t index) { return margin + (static_cast<float>(index - first) + 0.5f) * spacing; };
        auto const y = [&](int32_t value) { return height / 2.0f - static_cast<float>(value) / static_cast<float>(largest) * (height / 2.0f - 8.0f); };

        ds.FillRectangle(0, height / 2.0f - 0.5f, width, 1.0f, colors.RollBeatLine);

        auto const inPass = [&](uint64_t index) { return index >= passFirst && index < passFirst + static_cast<uint64_t>(std::max<int64_t>(0, steps)); };

        if (passFirst + static_cast<uint64_t>(steps) > first && passFirst < first + count)
        {
            auto const left = x(std::max(passFirst, first)) - spacing / 2.0f;
            auto const right = x(std::min(passFirst + static_cast<uint64_t>(steps), first + count) - 1) + spacing / 2.0f;
            ds.FillRectangle(left, 0, right - left, height, WithAlpha(color, 0.10));
        }

        // The pass's values, joined up.
        canvasGeometry::CanvasPathBuilder line{ ds };
        auto started = false;

        for (uint64_t index = std::max(passFirst, first); index < first + count && inPass(index); ++index)
        {
            foundation::Numerics::float2 const point{ x(index), y(InfinitySeries(index)) };

            if (!started)
            {
                line.BeginFigure(point);
                started = true;
            }
            else
            {
                line.AddLine(point);
            }
        }

        if (started)
        {
            line.EndFigure(canvasGeometry::CanvasFigureLoop::Open);
            ds.DrawGeometry(canvasGeometry::CanvasGeometry::CreatePath(line), WithAlpha(color, 0.85), 1.5f);
        }

        auto const quiet = colors.Light ? Rgba(0, 0, 0, 0.35) : Rgba(255, 255, 255, 0.35);
        auto const dot = std::clamp(spacing * 0.32f, 1.5f, 3.5f);

        for (uint64_t i = 0; i < count; ++i)
        {
            auto const index = first + i;

            // Every fourth value is the series again, at a quarter of the speed.
            auto const hidden = index % 4 == 0;
            ds.FillCircle(x(index), y(InfinitySeries(index)), inPass(index) ? dot + 1.0f : dot, inPass(index) || hidden ? color : quiet);
        }

        if (playing >= 0 && inPass(static_cast<uint64_t>(playing)) && static_cast<uint64_t>(playing) >= first && static_cast<uint64_t>(playing) < first + count)
        {
            ds.DrawCircle(x(static_cast<uint64_t>(playing)), y(InfinitySeries(static_cast<uint64_t>(playing))), dot + 4.0f, colors.Text, 1.5f);
        }
    }

    // ---------------------------------------------------------------- L-systems

    _Use_decl_annotations_
    void DrawTurtle(
        canvas::CanvasDrawingSession const& ds,
        float width,
        float height,
        TurtleDrawing const& drawing,
        int64_t passFirst,
        int64_t steps,
        int64_t playing,
        Color color,
        Palette const& colors)
    {
        ds.FillRoundedRectangle(0, 0, width, height, 4.0f, 4.0f, colors.RollBackground);

        if (drawing.Lines.empty())
        {
            return;
        }

        auto const* lines = &drawing.Lines;

        auto left = lines->front().X0;
        auto right = left;
        auto bottom = lines->front().Y0;
        auto top = bottom;

        auto const measure = [&]()
        {
            left = right = lines->front().X0;
            bottom = top = lines->front().Y0;

            for (auto const& line : *lines)
            {
                left = std::min({ left, line.X0, line.X1 });
                right = std::max({ right, line.X0, line.X1 });
                bottom = std::min({ bottom, line.Y0, line.Y1 });
                top = std::max({ top, line.Y0, line.Y1 });
            }
        };

        measure();

        // A drawing on one line, such as Cantor's, is too long and thin to see whole: it's laid
        // out in rows, as text is, as near square as it goes, so its gaps line up down the rows.
        // Each line is one step, so the rows read in the order it plays.
        std::vector<TurtleLine> wrapped{};

        if (top - bottom < 0.5f && lines->size() > 64)
        {
            auto const count = lines->size();
            auto const perRow = static_cast<size_t>(std::ceil(std::sqrt(static_cast<double>(count))));
            wrapped.reserve(count);

            for (size_t i = 0; i < count; ++i)
            {
                auto const column = static_cast<float>(i % perRow);
                auto const row = -static_cast<float>(i / perRow);
                auto const& line = (*lines)[i];
                wrapped.push_back(TurtleLine{ column, row, column + 1.0f, row, line.Step, line.Plays });
            }

            lines = &wrapped;
            measure();
        }

        auto const margin = 16.0f;
        auto const scale = std::min((width - margin * 2.0f) / std::max(1.0f, right - left), (height - margin * 2.0f) / std::max(1.0f, top - bottom));
        auto const offsetX = (width - (right - left) * scale) / 2.0f;
        auto const offsetY = (height - (top - bottom) * scale) / 2.0f;

        // Up in the drawing is up on the screen, and higher in pitch.
        auto const point = [&](float px, float py)
        {
            return foundation::Numerics::float2{ offsetX + (px - left) * scale, height - offsetY - (py - bottom) * scale };
        };

        auto const quiet = colors.Light ? Rgba(0, 0, 0, 0.36) : Rgba(255, 255, 255, 0.36);
        auto const rest = colors.Light ? Rgba(0, 0, 0, 0.12) : Rgba(255, 255, 255, 0.12);

        canvasGeometry::CanvasPathBuilder all{ ds };

        for (auto const& line : *lines)
        {
            if (line.Plays)
            {
                all.BeginFigure(point(line.X0, line.Y0));
                all.AddLine(point(line.X1, line.Y1));
                all.EndFigure(canvasGeometry::CanvasFigureLoop::Open);
            }
        }

        ds.DrawGeometry(canvasGeometry::CanvasGeometry::CreatePath(all), quiet, 1.0f);

        auto const total = std::max<int64_t>(1, drawing.Steps);

        // The pass's part of the drawing, with a soft glow under it, so a short stretch of a big
        // drawing still stands out.
        canvasGeometry::CanvasPathBuilder glow{ ds };
        auto glowing = false;

        for (int64_t step = 0; step < steps; ++step)
        {
            auto const index = static_cast<size_t>(((passFirst + step) % total + total) % total);

            if (index < lines->size() && (*lines)[index].Plays)
            {
                auto const& line = (*lines)[index];
                glow.BeginFigure(point(line.X0, line.Y0));
                glow.AddLine(point(line.X1, line.Y1));
                glow.EndFigure(canvasGeometry::CanvasFigureLoop::Open);
                glowing = true;
            }
        }

        if (glowing)
        {
            canvasGeometry::CanvasStrokeStyle round{};
            round.StartCap(canvasGeometry::CanvasCapStyle::Round);
            round.EndCap(canvasGeometry::CanvasCapStyle::Round);
            round.LineJoin(canvasGeometry::CanvasLineJoin::Round);
            ds.DrawGeometry(canvasGeometry::CanvasGeometry::CreatePath(glow), WithAlpha(color, 0.3), 10.0f, round);
        }

        for (int64_t step = 0; step < steps; ++step)
        {
            auto const index = static_cast<size_t>(((passFirst + step) % total + total) % total);

            if (index < lines->size())
            {
                auto const& line = (*lines)[index];
                ds.DrawLine(point(line.X0, line.Y0), point(line.X1, line.Y1), line.Plays ? color : rest, line.Plays ? 2.5f : 1.0f);
            }
        }

        if (playing >= 0)
        {
            auto const index = static_cast<size_t>(playing % total);

            if (index < lines->size())
            {
                auto const& line = (*lines)[index];
                ds.FillCircle(point(line.X1, line.Y1), 4.5f, colors.Text);
                ds.FillCircle(point(line.X1, line.Y1), 3.0f, color);
            }
        }
    }

    // ---------------------------------------------------------------- change ringing

    _Use_decl_annotations_
    void DrawBlueLine(
        canvas::CanvasDrawingSession const& ds,
        float width,
        float height,
        std::vector<std::vector<uint8_t>> const& course,
        size_t firstRow,
        size_t rowCount,
        size_t playingRow,
        int32_t playingPlace,
        bool gaps,
        Color color,
        Palette const& colors)
    {
        if (course.empty() || course.front().empty() || rowCount == 0)
        {
            return;
        }

        auto const bells = course.front().size();
        auto const column = std::min(28.0f, (width - 8.0f) / static_cast<float>(bells));
        auto const row = std::min(18.0f, height / static_cast<float>(rowCount));
        auto const left = (width - column * static_cast<float>(bells)) / 2.0f;
        auto const format = Format(std::min(12.0f, row - 4.0f));

        auto const center = [&](size_t index, size_t place)
        {
            return foundation::Numerics::float2{ left + (static_cast<float>(place) + 0.5f) * column, (static_cast<float>(index) + 0.5f) * row };
        };

        for (size_t i = 0; i < rowCount; ++i)
        {
            auto const at = firstRow + i;

            if (at == playingRow)
            {
                ds.FillRoundedRectangle(left - 6.0f, static_cast<float>(i) * row, column * static_cast<float>(bells) + 12.0f, row, 3.0f, 3.0f, WithAlpha(color, 0.18));
            }

            // Rounds come round again at the end of the course: a line across, above that row.
            if (at % course.size() == 0 && i > 0)
            {
                ds.FillRectangle(left - 6.0f, static_cast<float>(i) * row - 1.0f, column * static_cast<float>(bells) + 12.0f, 2.0f, WithAlpha(color, 0.75));
            }

            if (gaps && at % 2 == 1)
            {
                ds.FillRectangle(left, static_cast<float>(i + 1) * row - 0.5f, column * static_cast<float>(bells), 1.0f, colors.RollBeatLine);
            }
        }

        // Every bell's path faintly, then the treble's and the second's.
        auto const quiet = colors.Light ? Rgba(0, 0, 0, 0.12) : Rgba(255, 255, 255, 0.12);

        for (uint8_t bell = static_cast<uint8_t>(bells); bell >= 1; --bell)
        {
            auto const stroke = bell == 1 ? TrebleRed : bell == 2 ? WorkingBlue : quiet;

            canvasGeometry::CanvasPathBuilder path{ ds };

            for (size_t i = 0; i < rowCount; ++i)
            {
                auto const& order = course[(firstRow + i) % course.size()];
                auto const place = static_cast<size_t>(std::distance(order.begin(), std::find(order.begin(), order.end(), bell)));

                if (i == 0)
                {
                    path.BeginFigure(center(i, place));
                }
                else
                {
                    path.AddLine(center(i, place));
                }
            }

            path.EndFigure(canvasGeometry::CanvasFigureLoop::Open);
            ds.DrawGeometry(canvasGeometry::CanvasGeometry::CreatePath(path), stroke, bell <= 2 ? 2.2f : 1.2f);
        }

        for (size_t i = 0; i < rowCount; ++i)
        {
            auto const& order = course[(firstRow + i) % course.size()];

            for (size_t place = 0; place < order.size(); ++place)
            {
                auto const bell = order[place];
                auto const point = center(i, place);
                auto const isPlaying = firstRow + i == playingRow && static_cast<int32_t>(place) == playingPlace;

                if (isPlaying)
                {
                    ds.FillCircle(point, row * 0.48f, color);
                }

                ds.DrawText(winrt::hstring{ std::to_wstring(bell) }, foundation::Rect{ point.x - column / 2.0f, point.y - row / 2.0f, column, row },
                    isPlaying ? (colors.Light ? Rgba(255, 255, 255) : Rgba(10, 10, 10)) : bell == 1 ? TrebleRed : bell == 2 ? WorkingBlue : colors.Text2, format);
            }
        }
    }

    // ---------------------------------------------------------------- velocity

    namespace
    {
        // Room above the loudest and below the softest.
        constexpr float VelocityMargin = 6.0f;
    }

    _Use_decl_annotations_
    float VelocityY(float height, uint16_t velocity) noexcept
    {
        auto const usable = std::max(1.0f, height - VelocityMargin * 2.0f);
        return height - VelocityMargin - static_cast<float>(velocity) / 65535.0f * usable;
    }

    _Use_decl_annotations_
    uint16_t VelocityAtY(float height, float y) noexcept
    {
        auto const usable = std::max(1.0f, height - VelocityMargin * 2.0f);
        auto const share = std::clamp((height - VelocityMargin - y) / usable, 0.0f, 1.0f);
        return static_cast<uint16_t>(std::lround(share * 65535.0f));
    }

    _Use_decl_annotations_
    void DrawVelocities(
        canvas::CanvasDrawingSession const& ds,
        float width,
        float height,
        std::vector<StepNote> const& notes,
        int64_t steps,
        int64_t stepTicks,
        uint16_t low,
        uint16_t high,
        int64_t playing,
        Color color,
        Palette const& colors)
    {
        if (high < low)
        {
            std::swap(low, high);
        }

        // Framed like the other drawings, so its edges show against the panel.
        ds.FillRoundedRectangle(0, 0, width, height, 4.0f, 4.0f, colors.RollBackground);
        ds.DrawRoundedRectangle(0.5f, 0.5f, width - 1.0f, height - 1.0f, 4.0f, 4.0f, colors.StrokeStrong, 1.0f);

        steps = std::max<int64_t>(1, steps);
        stepTicks = std::max<int64_t>(1, stepTicks);

        auto const left = VelocityMargin;
        auto const stepWidth = std::max(1.0f, width - VelocityGripRoom - left) / static_cast<float>(steps);
        auto const floor = height - VelocityMargin;

        // Each beat.
        for (int64_t step = 1; step < steps; ++step)
        {
            if ((step * stepTicks) % TicksPerQuarterNote == 0)
            {
                ds.FillRectangle(left + static_cast<float>(step) * stepWidth - 0.5f, VelocityMargin, 1.0f, floor - VelocityMargin, colors.RollBeatLine);
            }
        }

        // The range: a band, its edges, and a grip on each edge to drag. Inside the frame.
        auto const top = VelocityY(height, high);
        auto const bottom = VelocityY(height, low);
        auto const edge = WithAlpha(color, 0.6);
        auto const inner = width - 2.0f;

        ds.FillRectangle(1.0f, top, inner, std::max(1.0f, bottom - top), WithAlpha(color, 0.2));
        ds.FillRectangle(1.0f, top - 0.5f, inner, 1.0f, edge);
        ds.FillRectangle(1.0f, bottom - 0.5f, inner, 1.0f, edge);

        auto const grip = width - VelocityGripRoom + 6.0f;
        ds.FillRoundedRectangle(grip, top - 3.0f, 14.0f, 6.0f, 3.0f, 3.0f, color);
        ds.FillRoundedRectangle(grip, bottom - 3.0f, 14.0f, 6.0f, 3.0f, 3.0f, color);

        // A stem for each note where it starts, so a note on the beat stands on the beat's line,
        // with a dot on top as loud as it plays.
        auto const dot = std::clamp(stepWidth * 0.3f, 1.5f, 3.0f);

        for (auto const& note : notes)
        {
            auto const x = left + static_cast<float>(note.Step) * stepWidth;
            auto const y = VelocityY(height, note.Velocity);
            auto const lit = playing >= note.Step && playing < note.Step + std::max<int64_t>(1, note.Steps);

            ds.FillRectangle(x - 0.75f, y, 1.5f, std::max(0.0f, floor - y), lit ? colors.Text : WithAlpha(color, 0.7));
            ds.FillCircle(x, y, lit ? dot + 1.0f : dot, lit ? colors.Text : color);
        }
    }
}
