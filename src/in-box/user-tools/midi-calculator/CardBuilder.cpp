// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "CardBuilder.h"

namespace automation = ::winrt::Microsoft::UI::Xaml::Automation;

namespace midicalculator::ui
{
    namespace
    {
        constexpr double CardSpacing = 12.0;
        constexpr double TwoColumnWidth = 860.0;

        winrt::hstring s_copyNameFormat{ L"{0}" };

        xaml::Style ThemeStyle(_In_ wchar_t const* key)
        {
            return xaml::Application::Current().Resources().Lookup(winrt::box_value(key)).as<xaml::Style>();
        }

        media::Brush ThemeBrush(_In_ wchar_t const* key)
        {
            return xaml::Application::Current().Resources().Lookup(winrt::box_value(key)).as<media::Brush>();
        }

        controls::Button MakeCopyButton(_In_ winrt::hstring const& label, _In_ std::function<winrt::hstring()> const& value, _In_ CopyHandler const& copy)
        {
            controls::Button button{};
            button.Padding(xaml::Thickness{ 6, 4, 6, 4 });
            button.Background(media::SolidColorBrush{ winrt::Microsoft::UI::Colors::Transparent() });
            button.BorderThickness(xaml::Thickness{ 0 });
            button.VerticalAlignment(xaml::VerticalAlignment::Center);

            controls::FontIcon icon{};
            icon.Glyph(L"\uE8C8");
            icon.FontSize(12);
            icon.Foreground(ThemeBrush(L"TextFillColorTertiaryBrush"));
            button.Content(icon);

            winrt::hstring name{ label };

            try
            {
                name = winrt::hstring{ std::vformat(std::wstring_view{ s_copyNameFormat }, std::make_wformat_args(label)) };
            }
            catch (...)
            {
            }

            automation::AutomationProperties::SetName(button, name);
            controls::ToolTipService::SetToolTip(button, winrt::box_value(name));

            button.Click([value, copy, label](auto&&, auto&&)
                {
                    try
                    {
                        auto const text = value();

                        if (!text.empty() && copy)
                        {
                            copy(label, text);
                        }
                    }
                    CATCH_LOG()
                });

            return button;
        }
    }

    _Use_decl_annotations_
    void SetCopyNameFormat(winrt::hstring const& format)
    {
        s_copyNameFormat = format;
    }

    _Use_decl_annotations_
    Card MakeCard(winrt::hstring const& title, wchar_t const* glyph)
    {
        Card card{};

        card.Root = controls::Border{};
        card.Root.Background(ThemeBrush(L"CardBackgroundFillColorDefaultBrush"));
        card.Root.BorderBrush(ThemeBrush(L"CardStrokeColorDefaultBrush"));
        card.Root.BorderThickness(xaml::Thickness{ 1 });
        card.Root.CornerRadius(xaml::CornerRadius{ 8 });
        card.Root.Padding(xaml::Thickness{ 16, 14, 16, 14 });

        card.Body = controls::StackPanel{};
        card.Body.Spacing(10);

        controls::StackPanel heading{};
        heading.Orientation(controls::Orientation::Horizontal);
        heading.Spacing(8);

        if (glyph != nullptr)
        {
            controls::FontIcon icon{};
            icon.Glyph(glyph);
            icon.FontSize(14);
            heading.Children().Append(icon);
        }

        controls::TextBlock text{};
        text.Text(title);
        text.Style(ThemeStyle(L"BodyStrongTextBlockStyle"));
        heading.Children().Append(text);
        card.Title = text;

        automation::AutomationProperties::SetHeadingLevel(text, automation::Peers::AutomationHeadingLevel::Level2);

        card.Body.Children().Append(heading);
        card.Root.Child(card.Body);

        return card;
    }

    _Use_decl_annotations_
    controls::TextBlock MakeHelp(winrt::hstring const& text)
    {
        controls::TextBlock block{};
        block.Text(text);
        block.TextWrapping(xaml::TextWrapping::Wrap);
        block.Style(ThemeStyle(L"CaptionTextBlockStyle"));
        block.Foreground(ThemeBrush(L"TextFillColorTertiaryBrush"));
        return block;
    }

    _Use_decl_annotations_
    xaml::FrameworkElement MakeFieldRow(std::vector<xaml::FrameworkElement> const& fields)
    {
        controls::StackPanel row{};
        row.Orientation(controls::Orientation::Horizontal);
        row.Spacing(10);

        for (auto const& field : fields)
        {
            field.VerticalAlignment(xaml::VerticalAlignment::Bottom);
            row.Children().Append(field);
        }

        return row;
    }

    _Use_decl_annotations_
    controls::NumberBox MakeNumberBox(winrt::hstring const& label, double value, double minimum, double maximum, double width)
    {
        controls::NumberBox box{};
        box.Header(MakeFieldHeader(label));
        box.Minimum(minimum);
        box.Maximum(maximum);
        box.Value(value);
        box.Width(width);
        box.HorizontalAlignment(xaml::HorizontalAlignment::Left);
        box.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Hidden);
        box.ValidationMode(controls::NumberBoxValidationMode::InvalidInputOverwritten);
        automation::AutomationProperties::SetName(box, label);
        return box;
    }

    _Use_decl_annotations_
    controls::TextBox MakeTextBox(winrt::hstring const& label, winrt::hstring const& text, double width)
    {
        controls::TextBox box{};
        box.Header(MakeFieldHeader(label));
        box.Text(text);
        box.Width(width);
        box.HorizontalAlignment(xaml::HorizontalAlignment::Left);
        box.IsSpellCheckEnabled(false);
        automation::AutomationProperties::SetName(box, label);
        return box;
    }

    _Use_decl_annotations_
    controls::ComboBox MakeComboBox(winrt::hstring const& label, std::vector<winrt::hstring> const& items, int32_t selectedIndex, double width)
    {
        controls::ComboBox box{};
        box.Header(MakeFieldHeader(label));
        box.Width(width);
        box.HorizontalAlignment(xaml::HorizontalAlignment::Left);

        for (auto const& item : items)
        {
            box.Items().Append(winrt::box_value(item));
        }

        if (selectedIndex >= 0 && selectedIndex < static_cast<int32_t>(items.size()))
        {
            box.SelectedIndex(selectedIndex);
        }

        automation::AutomationProperties::SetName(box, label);
        return box;
    }

    _Use_decl_annotations_
    xaml::FrameworkElement Labeled(winrt::hstring const& label, xaml::FrameworkElement const& control)
    {
        controls::StackPanel panel{};
        panel.Spacing(4);

        controls::TextBlock text{};
        text.Text(label);
        text.Style(ThemeStyle(L"CaptionTextBlockStyle"));
        panel.Children().Append(text);
        panel.Children().Append(control);

        automation::AutomationProperties::SetName(control, label);
        return panel;
    }

    _Use_decl_annotations_
    controls::TextBlock MakeFieldHeader(winrt::hstring const& label)
    {
        controls::TextBlock header{};
        header.Text(label);
        header.FontSize(12);
        header.Foreground(ThemeBrush(L"TextFillColorSecondaryBrush"));
        return header;
    }

    _Use_decl_annotations_
    controls::Button MakeSmallButton(winrt::hstring const& text, wchar_t const* glyph, bool accent, bool subtle)
    {
        auto button = MakeButton(text, glyph, accent);
        button.FontSize(12);
        button.MinHeight(28);
        button.Padding(xaml::Thickness{ 10, 4, 10, 5 });

        if (auto content = button.Content().try_as<controls::StackPanel>())
        {
            content.Spacing(6);

            if (auto icon = content.Children().GetAt(0).try_as<controls::FontIcon>())
            {
                icon.FontSize(13);
            }
        }

        if (subtle)
        {
            button.Background(media::SolidColorBrush{ winrt::Microsoft::UI::Colors::Transparent() });
            button.BorderThickness(xaml::Thickness{ 0 });
        }

        return button;
    }

    _Use_decl_annotations_
    void SetBigText(controls::TextBlock const& block, std::wstring_view text, double size, double unitSize)
    {
        block.Inlines().Clear();
        block.FontSize(size);
        block.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());

        auto const space = text.rfind(L' ');
        auto const unit = space == std::wstring_view::npos ? std::wstring_view{} : text.substr(space + 1);
        auto const isUnit = !unit.empty() && unit.size() <= 5 &&
            std::all_of(unit.begin(), unit.end(), [](wchar_t c) { return std::iswalpha(c) != 0; });

        winrt::Microsoft::UI::Xaml::Documents::Run value{};
        value.Text(winrt::hstring{ isUnit ? text.substr(0, space) : text });
        block.Inlines().Append(value);

        if (isUnit)
        {
            winrt::Microsoft::UI::Xaml::Documents::Run small{};
            small.Text(winrt::hstring{ std::wstring{ L"\u2009" } + std::wstring{ unit } });
            small.FontSize(unitSize);
            small.FontWeight(winrt::Windows::UI::Text::FontWeights::Normal());
            small.Foreground(ThemeBrush(L"TextFillColorSecondaryBrush"));
            block.Inlines().Append(small);
        }
    }
    _Use_decl_annotations_
    controls::Border MakeInlineText(controls::TextBlock const& text, bool framed)
    {
        controls::Border holder{};
        holder.Height(32);
        holder.BorderThickness(xaml::Thickness{ 1 });
        holder.CornerRadius(xaml::CornerRadius{ 4 });
        // measured: 1 DIP higher than a text box's own padding puts the baselines together
        holder.Padding(xaml::Thickness{ framed ? 10.0 : 0.0, 4, framed ? 10.0 : 0.0, 7 });
        holder.BorderBrush(framed ? ThemeBrush(L"CardStrokeColorDefaultBrush") : media::SolidColorBrush{ winrt::Microsoft::UI::Colors::Transparent() });
        holder.Background(framed ? ThemeBrush(L"LayerFillColorDefaultBrush") : media::SolidColorBrush{ winrt::Microsoft::UI::Colors::Transparent() });

        text.VerticalAlignment(xaml::VerticalAlignment::Top);
        text.Margin(xaml::Thickness{ 0 });
        holder.Child(text);
        return holder;
    }

    _Use_decl_annotations_
    controls::Button MakeButton(winrt::hstring const& text, wchar_t const* glyph, bool accent)
    {
        controls::Button button{};

        if (accent)
        {
            button.Style(ThemeStyle(L"AccentButtonStyle"));
        }

        if (glyph == nullptr)
        {
            button.Content(winrt::box_value(text));
        }
        else
        {
            controls::StackPanel content{};
            content.Orientation(controls::Orientation::Horizontal);
            content.Spacing(8);

            controls::FontIcon icon{};
            icon.Glyph(glyph);
            icon.FontSize(14);
            content.Children().Append(icon);

            controls::TextBlock label{};
            label.Text(text);
            content.Children().Append(label);

            button.Content(content);
            automation::AutomationProperties::SetName(button, text);
        }

        return button;
    }

    _Use_decl_annotations_
    BigResult MakeBigResult(winrt::hstring const& label, CopyHandler const& copy)
    {
        BigResult result{};

        result.Root = controls::Border{};
        result.Root.Background(ThemeBrush(L"SolidBackgroundFillColorBaseBrush"));
        result.Root.BorderBrush(ThemeBrush(L"CardStrokeColorDefaultBrush"));
        result.Root.BorderThickness(xaml::Thickness{ 1 });
        result.Root.CornerRadius(xaml::CornerRadius{ 6 });
        result.Root.Padding(xaml::Thickness{ 12, 10, 12, 10 });

        controls::Grid grid{};
        controls::ColumnDefinition text{};
        text.Width(xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
        controls::ColumnDefinition actions{};
        actions.Width(xaml::GridLengthHelper::Auto());
        grid.ColumnDefinitions().Append(text);
        grid.ColumnDefinitions().Append(actions);
        grid.ColumnSpacing(10);

        controls::StackPanel words{};
        result.Value = controls::TextBlock{};
        result.Value.FontSize(28);
        result.Value.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
        result.Value.IsTextSelectionEnabled(true);
        result.Value.TextWrapping(xaml::TextWrapping::Wrap);
        automation::AutomationProperties::SetLiveSetting(result.Value, automation::Peers::AutomationLiveSetting::Polite);

        result.Detail = controls::TextBlock{};
        result.Detail.Style(ThemeStyle(L"CaptionTextBlockStyle"));
        result.Detail.Foreground(ThemeBrush(L"TextFillColorTertiaryBrush"));
        result.Detail.TextWrapping(xaml::TextWrapping::Wrap);
        result.Detail.IsTextSelectionEnabled(true);

        words.Children().Append(result.Value);
        words.Children().Append(result.Detail);
        grid.Children().Append(words);

        result.Actions = controls::StackPanel{};
        result.Actions.Orientation(controls::Orientation::Horizontal);
        result.Actions.Spacing(6);
        result.Actions.VerticalAlignment(xaml::VerticalAlignment::Center);

        auto value = result.Value;
        result.Actions.Children().Append(MakeCopyButton(label, [value]() { return value.Text(); }, copy));

        controls::Grid::SetColumn(result.Actions, 1);
        grid.Children().Append(result.Actions);

        result.Root.Child(grid);
        return result;
    }

    ResultList::ResultList()
    {
        m_grid = controls::Grid{};
        m_grid.ColumnSpacing(12);

        controls::ColumnDefinition label{};
        label.Width(xaml::GridLengthHelper::Auto());
        controls::ColumnDefinition value{};
        value.Width(xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
        controls::ColumnDefinition copy{};
        copy.Width(xaml::GridLengthHelper::Auto());

        m_grid.ColumnDefinitions().Append(label);
        m_grid.ColumnDefinitions().Append(value);
        m_grid.ColumnDefinitions().Append(copy);
    }

    _Use_decl_annotations_
    controls::TextBlock ResultList::Add(winrt::hstring const& label, CopyHandler const& copy, bool monospace)
    {
        controls::RowDefinition row{};
        row.Height(xaml::GridLengthHelper::Auto());
        m_grid.RowDefinitions().Append(row);

        // a divider above every row but the first, behind the row's cells
        controls::Border line{};
        line.MinHeight(30);
        line.BorderBrush(ThemeBrush(L"DividerStrokeColorDefaultBrush"));
        line.BorderThickness(xaml::Thickness{ 0, m_rows > 0 ? 1.0 : 0.0, 0, 0 });
        controls::Grid::SetRow(line, m_rows);
        controls::Grid::SetColumnSpan(line, 3);
        m_grid.Children().Append(line);

        controls::TextBlock name{};
        name.Text(label);
        name.FontSize(13);
        name.Foreground(ThemeBrush(L"TextFillColorSecondaryBrush"));
        name.VerticalAlignment(xaml::VerticalAlignment::Center);
        name.TextWrapping(xaml::TextWrapping::Wrap);
        name.MaxWidth(220);
        controls::Grid::SetRow(name, m_rows);

        controls::TextBlock value{};
        value.HorizontalAlignment(xaml::HorizontalAlignment::Right);
        value.VerticalAlignment(xaml::VerticalAlignment::Center);
        value.FontSize(13);
        value.IsTextSelectionEnabled(true);
        value.TextWrapping(xaml::TextWrapping::Wrap);
        value.TextAlignment(xaml::TextAlignment::Right);


        if (monospace)
        {
            value.FontFamily(media::FontFamily{ L"Cascadia Mono, Consolas" });
        }

        controls::Grid::SetRow(value, m_rows);
        controls::Grid::SetColumn(value, 1);

        auto button = MakeCopyButton(label, [value]() { return value.Text(); }, copy);
        controls::Grid::SetRow(button, m_rows);
        controls::Grid::SetColumn(button, 2);

        m_lastLabel = name;
        m_grid.Children().Append(name);
        m_grid.Children().Append(value);
        m_grid.Children().Append(button);

        ++m_rows;
        return value;
    }

    CardColumns::CardColumns()
    {
        m_grid = controls::Grid{};
        m_grid.ColumnSpacing(CardSpacing);
        m_grid.RowSpacing(CardSpacing);
    }

    _Use_decl_annotations_
    void CardColumns::Add(Card const& card, int32_t column, bool fullWidth)
    {
        card.Root.VerticalAlignment(xaml::VerticalAlignment::Top);
        m_cards.push_back({ card.Root, column == 0 ? 0 : 1, fullWidth });
        m_columns = 0;
        Layout(m_grid.ActualWidth());
    }

    _Use_decl_annotations_
    void CardColumns::Layout(double width)
    {
        auto const columns = width >= TwoColumnWidth || width <= 0.0 ? 2 : 1;

        if (columns == m_columns)
        {
            return;
        }

        m_columns = columns;

        // take every card out of the panel it's in now
        for (auto const& entry : m_cards)
        {
            if (auto parent = entry.Card.Parent().try_as<controls::Panel>())
            {
                uint32_t index{};

                if (parent.Children().IndexOf(entry.Card, index))
                {
                    parent.Children().RemoveAt(index);
                }
            }
        }

        m_grid.Children().Clear();
        m_grid.ColumnDefinitions().Clear();
        m_grid.RowDefinitions().Clear();

        for (int32_t c = 0; c < columns; ++c)
        {
            controls::ColumnDefinition definition{};
            definition.Width(xaml::GridLengthHelper::FromValueAndType(c == 0 ? 1.1 : 1.0, xaml::GridUnitType::Star));
            m_grid.ColumnDefinitions().Append(definition);
        }

        for (int32_t r = 0; r < 2; ++r)
        {
            controls::RowDefinition definition{};
            definition.Height(xaml::GridLengthHelper::Auto());
            m_grid.RowDefinitions().Append(definition);
        }

        std::array<controls::StackPanel, 2> stacks{};

        for (int32_t c = 0; c < columns; ++c)
        {
            stacks[c] = controls::StackPanel{};
            stacks[c].Spacing(CardSpacing);
            controls::Grid::SetColumn(stacks[c], c);
            m_grid.Children().Append(stacks[c]);
        }

        controls::StackPanel below{};
        below.Spacing(CardSpacing);
        controls::Grid::SetRow(below, 1);
        controls::Grid::SetColumnSpan(below, columns);
        m_grid.Children().Append(below);

        for (auto const& entry : m_cards)
        {
            if (entry.FullWidth && columns > 1)
            {
                below.Children().Append(entry.Card);
            }
            else
            {
                stacks[columns > 1 ? entry.Column : 0].Children().Append(entry.Card);
            }
        }
    }

    _Use_decl_annotations_
    Segmented::Segmented(std::vector<winrt::hstring> const& items, int32_t selected, winrt::hstring const& name)
    {
        m_panel = controls::StackPanel{};
        m_panel.Orientation(controls::Orientation::Horizontal);
        m_panel.HorizontalAlignment(xaml::HorizontalAlignment::Left);
        automation::AutomationProperties::SetName(m_panel, name);

        m_selected = std::clamp(selected, 0, static_cast<int32_t>(items.size()) - 1);
        auto const count = static_cast<int32_t>(items.size());

        for (int32_t i = 0; i < count; ++i)
        {
            controls::Primitives::ToggleButton button{};
            button.Content(winrt::box_value(items[i]));
            button.MinHeight(28);
            button.FontSize(12);
            button.Padding(xaml::Thickness{ 12, 4, 12, 5 });
            button.IsChecked(i == m_selected);

            // joined: only the outer corners are round
            auto const left = i == 0 ? 4.0 : 0.0;
            auto const right = i == count - 1 ? 4.0 : 0.0;
            button.CornerRadius(xaml::CornerRadius{ left, right, right, left });

            automation::AutomationProperties::SetName(button, items[i]);
            automation::AutomationProperties::SetPositionInSet(button, i + 1);
            automation::AutomationProperties::SetSizeOfSet(button, count);

            // Checked and Unchecked, not Click: Click never fires for a screen reader's toggle
            button.Checked([this, i](auto&&, auto&&)
                {
                    if (m_updating)
                    {
                        return;
                    }

                    m_updating = true;
                    m_selected = i;

                    for (int32_t other = 0; other < static_cast<int32_t>(m_buttons.size()); ++other)
                    {
                        if (other != i)
                        {
                            m_buttons[other].IsChecked(false);
                        }
                    }

                    m_updating = false;

                    if (m_changed)
                    {
                        m_changed(i);
                    }
                });

            // the chosen one can't be turned off, only replaced
            button.Unchecked([this, i](auto&&, auto&&)
                {
                    if (!m_updating && m_selected == i)
                    {
                        m_updating = true;
                        m_buttons[i].IsChecked(true);
                        m_updating = false;
                    }
                });

            m_buttons.push_back(button);
            m_panel.Children().Append(button);
        }
    }

    controls::Grid MakeBitDiagram()
    {
        controls::Grid grid{};
        grid.Height(44);
        grid.CornerRadius(xaml::CornerRadius{ 4 });
        return grid;
    }

    _Use_decl_annotations_
    void FillBitDiagram(controls::Grid const& diagram, std::vector<BitField> const& fields)
    {
        // the comps' colors: the same on light and dark backgrounds, with black text
        static constexpr winrt::Windows::UI::Color Colors[4]{
            { 0xFF, 0x4C, 0xC2, 0xFF },
            { 0xFF, 0xF7, 0xA7, 0x5C },
            { 0xFF, 0xA7, 0x8B, 0xFA },
            { 0xFF, 0xB8, 0xB8, 0xB8 } };

        diagram.Children().Clear();
        diagram.ColumnDefinitions().Clear();

        for (size_t i = 0; i < fields.size(); ++i)
        {
            auto const& field = fields[i];

            controls::ColumnDefinition column{};
            column.Width(xaml::GridLengthHelper::FromValueAndType((std::max)(1, field.Bits), xaml::GridUnitType::Star));
            diagram.ColumnDefinitions().Append(column);

            controls::Border cell{};
            cell.Background(media::SolidColorBrush{ Colors[std::clamp(field.Color, 0, 3)] });
            cell.BorderBrush(media::SolidColorBrush{ winrt::Windows::UI::Color{ 0x40, 0, 0, 0 } });
            cell.BorderThickness(xaml::Thickness{ 0, 0, i + 1 < fields.size() ? 1.0 : 0.0, 0 });

            controls::StackPanel text{};
            text.VerticalAlignment(xaml::VerticalAlignment::Center);
            text.HorizontalAlignment(xaml::HorizontalAlignment::Center);

            controls::TextBlock name{};
            name.Text(field.Name);
            name.FontSize(10);
            name.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
            name.Foreground(media::SolidColorBrush{ winrt::Windows::UI::Color{ 0xCC, 0, 0, 0 } });
            name.HorizontalAlignment(xaml::HorizontalAlignment::Center);
            name.TextTrimming(xaml::TextTrimming::CharacterEllipsis);

            controls::TextBlock value{};
            value.Text(field.Value);
            value.FontSize(11);
            value.FontFamily(media::FontFamily{ L"Cascadia Mono, Consolas" });
            value.Foreground(media::SolidColorBrush{ winrt::Windows::UI::Color{ 0xFF, 0, 0, 0 } });
            value.HorizontalAlignment(xaml::HorizontalAlignment::Center);
            value.TextTrimming(xaml::TextTrimming::CharacterEllipsis);

            text.Children().Append(name);
            text.Children().Append(value);
            cell.Child(text);

            controls::Grid::SetColumn(cell, static_cast<int32_t>(i));
            diagram.Children().Append(cell);
        }
    }
    _Use_decl_annotations_
    void CopyText(winrt::hstring const& text) noexcept
    {
        try
        {
            winrt::Windows::ApplicationModel::DataTransfer::DataPackage package{};
            package.SetText(text);
            winrt::Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);
        }
        CATCH_LOG()
    }
}
