// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midi2console
{
    enum class ColumnAlignment
    {
        Left,
        Right
    };

    // Bordered auto-sized table shared by every listing command. Deliberately separate from
    // MidiMessageTable: this one measures all content before drawing, which is the wrong shape
    // for a monitor that has to emit rows as they arrive.
    class ConsoleTable
    {
    public:
        explicit ConsoleTable(_In_ std::string title = {});

        void AddColumn(
            _In_ std::string header,
            _In_ ColumnAlignment alignment = ColumnAlignment::Left,
            _In_ fmt::text_style const& style = {});

        // Marks the most recently added column as the one to shrink when the table is too wide.
        void SetLastColumnShrinkable();

        void BeginRow();
        void AddCell(_In_ std::string text);
        void AddCell(_In_ std::string text, _In_ fmt::text_style const& style);

        // Full-width continuation line drawn under the current row. Any detail line turns on
        // separators between rows, so each entry reads as one block.
        void AddRowDetail(_In_ std::string text, _In_ fmt::text_style const& style);

        void Render() const;

        bool IsEmpty() const noexcept { return m_rows.empty(); }
        size_t RowCount() const noexcept { return m_rows.size(); }

    private:
        struct Column
        {
            std::string Header;
            ColumnAlignment Alignment{ ColumnAlignment::Left };
            fmt::text_style Style;
            bool Shrinkable{ false };
        };

        struct Cell
        {
            std::string Text;
            fmt::text_style Style;
            bool HasStyle{ false };
        };

        struct Row
        {
            std::vector<Cell> Cells;
            std::string Detail;
            fmt::text_style DetailStyle;
            bool HasDetail{ false };
        };

        std::string m_title;
        std::vector<Column> m_columns;
        std::vector<Row> m_rows;
    };

    // "Label   value" pair used throughout the properties and status output. The label column is
    // padded to a shared width so successive calls line up.
    void WriteField(_In_ std::string_view label, _In_ std::string_view value);
    void WriteField(_In_ std::string_view label, _In_ std::string_view value, _In_ fmt::text_style const& valueStyle);
    void WriteSectionHeading(_In_ std::string_view heading);

    size_t ConsoleWidth();
}
