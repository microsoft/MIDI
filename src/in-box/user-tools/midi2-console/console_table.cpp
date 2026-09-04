// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include <algorithm>

#include "console_output.h"
#include "console_table.h"

namespace midi2console
{
    namespace
    {
        constexpr const char* CornerTopLeft = "\u256D";
        constexpr const char* CornerTopRight = "\u256E";
        constexpr const char* CornerBottomLeft = "\u2570";
        constexpr const char* CornerBottomRight = "\u256F";
        constexpr const char* Horizontal = "\u2500";
        constexpr const char* Vertical = "\u2502";
        constexpr const char* TeeDown = "\u252C";
        constexpr const char* TeeUp = "\u2534";
        constexpr const char* TeeRight = "\u251C";
        constexpr const char* TeeLeft = "\u2524";
        constexpr const char* Cross = "\u253C";
        constexpr const char* Ellipsis = "\u2026";

        constexpr size_t MinimumShrinkableWidth = 12;
        constexpr size_t FieldLabelWidth = 34;

        std::string Repeat(_In_ std::string_view unit, _In_ size_t count)
        {
            std::string result;
            result.reserve(unit.size() * count);

            for (size_t i = 0; i < count; i++)
            {
                result.append(unit);
            }

            return result;
        }

        std::string TruncateToWidth(_In_ std::string_view text, _In_ size_t width)
        {
            if (DisplayWidth(text) <= width || width == 0)
            {
                return std::string{ text };
            }

            // Walk UTF-8 code points so a multi-byte character is never cut in half.
            std::string result;
            size_t currentWidth{ 0 };

            for (size_t i = 0; i < text.size();)
            {
                auto const lead = static_cast<unsigned char>(text[i]);
                size_t length = 1;

                if ((lead & 0xF8) == 0xF0)      length = 4;
                else if ((lead & 0xF0) == 0xE0) length = 3;
                else if ((lead & 0xE0) == 0xC0) length = 2;

                length = std::min(length, text.size() - i);

                auto const piece = text.substr(i, length);
                auto const pieceWidth = DisplayWidth(piece);

                if (currentWidth + pieceWidth > width - 1)
                {
                    break;
                }

                result.append(piece);
                currentWidth += pieceWidth;
                i += length;
            }

            result.append(Ellipsis);

            return result;
        }

        std::string Align(_In_ std::string_view text, _In_ size_t width, _In_ ColumnAlignment alignment)
        {
            auto const current = DisplayWidth(text);

            if (current >= width)
            {
                return std::string{ text };
            }

            auto const padding = std::string(width - current, ' ');

            return alignment == ColumnAlignment::Right
                ? padding + std::string{ text }
                : std::string{ text } + padding;
        }
    }

    size_t ConsoleWidth()
    {
        CONSOLE_SCREEN_BUFFER_INFO info{};

        if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
        {
            auto const width = info.srWindow.Right - info.srWindow.Left + 1;

            if (width > 20)
            {
                return static_cast<size_t>(width);
            }
        }

        // Redirected output has no width. Pick something wide enough that nothing truncates.
        return 200;
    }

    ConsoleTable::ConsoleTable(_In_ std::string title) :
        m_title(std::move(title))
    {
    }

    void ConsoleTable::AddColumn(
        _In_ std::string header,
        _In_ ColumnAlignment alignment,
        _In_ fmt::text_style const& style)
    {
        m_columns.push_back(Column{ std::move(header), alignment, style, false });
    }

    void ConsoleTable::SetLastColumnShrinkable()
    {
        if (!m_columns.empty())
        {
            m_columns.back().Shrinkable = true;
        }
    }

    void ConsoleTable::BeginRow()
    {
        m_rows.emplace_back();
    }

    void ConsoleTable::AddCell(_In_ std::string text)
    {
        if (m_rows.empty())
        {
            BeginRow();
        }

        m_rows.back().push_back(Cell{ std::move(text), {}, false });
    }

    void ConsoleTable::AddCell(_In_ std::string text, _In_ fmt::text_style const& style)
    {
        if (m_rows.empty())
        {
            BeginRow();
        }

        m_rows.back().push_back(Cell{ std::move(text), style, true });
    }

    void ConsoleTable::Render() const
    {
        if (m_columns.empty())
        {
            return;
        }

        std::vector<size_t> widths(m_columns.size(), 0);

        for (size_t i = 0; i < m_columns.size(); i++)
        {
            widths[i] = DisplayWidth(m_columns[i].Header);
        }

        for (auto const& row : m_rows)
        {
            for (size_t i = 0; i < row.size() && i < widths.size(); i++)
            {
                widths[i] = std::max(widths[i], DisplayWidth(row[i].Text));
            }
        }

        // Borders and padding cost 3 cells per column plus one for the closing edge.
        auto const decoration = m_columns.size() * 3 + 1;

        size_t total = decoration;

        for (auto const width : widths)
        {
            total += width;
        }

        auto const available = ConsoleWidth();

        if (total > available)
        {
            auto excess = total - available;

            for (size_t i = 0; i < m_columns.size() && excess > 0; i++)
            {
                if (!m_columns[i].Shrinkable || widths[i] <= MinimumShrinkableWidth)
                {
                    continue;
                }

                auto const reducible = std::min(excess, widths[i] - MinimumShrinkableWidth);

                widths[i] -= reducible;
                excess -= reducible;
            }
        }

        if (!m_title.empty())
        {
            WriteLine(fmt::format("{}", Styled(m_title, tableTitleTextStyle)));
        }

        auto buildRule = [&widths](std::string_view left, std::string_view middle, std::string_view right)
        {
            std::string line{ left };

            for (size_t i = 0; i < widths.size(); i++)
            {
                line += Repeat(Horizontal, widths[i] + 2);
                line += (i + 1 < widths.size()) ? middle : right;
            }

            return line;
        };

        WriteLine(fmt::format("{}", Styled(buildRule(CornerTopLeft, TeeDown, CornerTopRight), separatorTextStyle)));

        std::string headerLine{ fmt::format("{}", Styled(Vertical, separatorTextStyle)) };

        for (size_t i = 0; i < m_columns.size(); i++)
        {
            headerLine += " ";
            headerLine += fmt::format("{}",
                Styled(Align(TruncateToWidth(m_columns[i].Header, widths[i]), widths[i], m_columns[i].Alignment),
                    tableHeaderTextStyle));
            headerLine += " ";
            headerLine += fmt::format("{}", Styled(Vertical, separatorTextStyle));
        }

        WriteLine(headerLine);

        WriteLine(fmt::format("{}", Styled(buildRule(TeeRight, Cross, TeeLeft), separatorTextStyle)));

        for (auto const& row : m_rows)
        {
            std::string line{ fmt::format("{}", Styled(Vertical, separatorTextStyle)) };

            for (size_t i = 0; i < m_columns.size(); i++)
            {
                auto const& cell = i < row.size() ? row[i] : Cell{};
                auto const& style = cell.HasStyle ? cell.Style : m_columns[i].Style;

                line += " ";
                line += fmt::format("{}",
                    Styled(Align(TruncateToWidth(cell.Text, widths[i]), widths[i], m_columns[i].Alignment), style));
                line += " ";
                line += fmt::format("{}", Styled(Vertical, separatorTextStyle));
            }

            WriteLine(line);
        }

        WriteLine(fmt::format("{}", Styled(buildRule(CornerBottomLeft, TeeUp, CornerBottomRight), separatorTextStyle)));
    }

    void WriteField(_In_ std::string_view label, _In_ std::string_view value)
    {
        WriteField(label, value, fieldValueTextStyle);
    }

    void WriteField(_In_ std::string_view label, _In_ std::string_view value, _In_ fmt::text_style const& valueStyle)
    {
        auto paddedLabel = std::string{ label };

        auto const width = DisplayWidth(paddedLabel);

        if (width < FieldLabelWidth)
        {
            paddedLabel.append(FieldLabelWidth - width, ' ');
        }

        WriteLine(fmt::format("  {} {}",
            Styled(paddedLabel, fieldLabelTextStyle),
            Styled(value, valueStyle)));
    }

    void WriteSectionHeading(_In_ std::string_view heading)
    {
        WriteBlankLine();
        WriteLine(fmt::format("{}", Styled(heading, sectionHeadingTextStyle)));
        WriteLine(fmt::format("{}",
            Styled(Repeat(Horizontal, std::min(ConsoleWidth() - 1, DisplayWidth(heading) + 20)), separatorTextStyle)));
    }
}
