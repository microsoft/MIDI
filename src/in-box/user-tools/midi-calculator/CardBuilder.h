// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The pieces every calculator page is built from: cards, labeled fields and result rows, so the
// pages look and read the same. Text always comes in from the caller, who reads it from
// Resources.resw. Every field gets its label as its accessible name.

namespace midicalculator::ui
{
    using CopyHandler = std::function<void(winrt::hstring const& label, winrt::hstring const& value)>;

    struct Card
    {
        controls::Border Root{ nullptr };
        controls::StackPanel Body{ nullptr };
        controls::TextBlock Title{ nullptr };
    };

    // "Copy {0}" from the app's resources, for the copy buttons' accessible names and tooltips.
    void SetCopyNameFormat(_In_ winrt::hstring const& format);

    Card MakeCard(_In_ winrt::hstring const& title, _In_ wchar_t const* glyph);

    // A short paragraph under a card's title.
    controls::TextBlock MakeHelp(_In_ winrt::hstring const& text);

    // Fields side by side. Keep it to what fits half a window; a second row is another call.
    xaml::FrameworkElement MakeFieldRow(_In_ std::vector<xaml::FrameworkElement> const& fields);

    controls::NumberBox MakeNumberBox(
        _In_ winrt::hstring const& label,
        _In_ double value,
        _In_ double minimum,
        _In_ double maximum,
        _In_ double width);

    controls::TextBox MakeTextBox(_In_ winrt::hstring const& label, _In_ winrt::hstring const& text, _In_ double width);

    controls::ComboBox MakeComboBox(
        _In_ winrt::hstring const& label,
        _In_ std::vector<winrt::hstring> const& items,
        _In_ int32_t selectedIndex,
        _In_ double width);

    // The field with its label above it, the way WinUI headers look, for any control.
    xaml::FrameworkElement Labeled(_In_ winrt::hstring const& label, _In_ xaml::FrameworkElement const& control);

    // Text that sits beside a text box in a field row, on the same baseline: a holder with the
    // text box's own height and border, with the text placed on the box's baseline (measured). framed draws
    // the border, for a result that should look like a box.
    controls::Border MakeInlineText(_In_ controls::TextBlock const& text, _In_ bool framed);

    controls::Button MakeButton(_In_ winrt::hstring const& text, _In_opt_ wchar_t const* glyph, _In_ bool accent);

    // A big answer with a line of detail under it and a copy button.
    struct BigResult
    {
        controls::Border Root{ nullptr };
        controls::TextBlock Value{ nullptr };
        controls::TextBlock Detail{ nullptr };
        controls::StackPanel Actions{ nullptr };
    };

    BigResult MakeBigResult(_In_ winrt::hstring const& label, _In_ CopyHandler const& copy);

    // Rows of "label   value   copy". Values can be selected with the mouse as well.
    class ResultList
    {
    public:
        ResultList();

        controls::Grid Root() const noexcept { return m_grid; }

        controls::TextBlock Add(_In_ winrt::hstring const& label, _In_ CopyHandler const& copy, _In_ bool monospace = false);

        // the label of the row added last, for a label that changes
        controls::TextBlock LastLabel() const noexcept { return m_lastLabel; }

    private:
        controls::Grid m_grid{ nullptr };
        controls::TextBlock m_lastLabel{ nullptr };
        int32_t m_rows{ 0 };
    };

    // Cards in the two columns the comps put them in, when there's room; one column, in the order
    // they were added, when there isn't. Full-width cards go under both columns.
    class CardColumns
    {
    public:
        CardColumns();

        controls::Grid Root() const noexcept { return m_grid; }

        void Add(_In_ Card const& card, _In_ int32_t column, _In_ bool fullWidth = false);
        void Layout(_In_ double width);

    private:
        struct Entry
        {
            controls::Border Card{ nullptr };
            int32_t Column{ 0 };
            bool FullWidth{ false };
        };

        controls::Grid m_grid{ nullptr };
        std::vector<Entry> m_cards{};
        int32_t m_columns{ 0 };
    };

    // A row of joined buttons where exactly one is chosen, like the comps' segmented controls.
    // Each is a toggle button, numbered for screen readers ("2 of 4").
    class Segmented
    {
    public:
        Segmented(_In_ std::vector<winrt::hstring> const& items, _In_ int32_t selected, _In_ winrt::hstring const& name);
        Segmented(Segmented const&) = delete;
        Segmented& operator=(Segmented const&) = delete;

        controls::StackPanel Root() const noexcept { return m_panel; }
        int32_t Selected() const noexcept { return m_selected; }
        void Changed(_In_ std::function<void(int32_t)> const& handler) { m_changed = handler; }

    private:
        controls::StackPanel m_panel{ nullptr };
        std::vector<controls::Primitives::ToggleButton> m_buttons{};
        int32_t m_selected{ 0 };
        bool m_updating{ false };
        std::function<void(int32_t)> m_changed{};
    };

    // One field of a bit diagram: a short name over its value, as wide as its bits.
    struct BitField
    {
        winrt::hstring Name{};
        winrt::hstring Value{};
        int32_t Bits{ 1 };
        // 0 blue, 1 orange, 2 purple, 3 gray, the comps' colors
        int32_t Color{ 3 };
    };

    // A Grid that shows the fields side by side. Fill again whenever the fields change.
    controls::Grid MakeBitDiagram();
    void FillBitDiagram(_In_ controls::Grid const& diagram, _In_ std::vector<BitField> const& fields);

    void CopyText(_In_ winrt::hstring const& text) noexcept;
}
