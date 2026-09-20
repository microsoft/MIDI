// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "StringResources.h"
#include "ThemeBrushes.h"

using namespace winrt::Microsoft::UI::Xaml;

namespace patchbay = ::midipatchbay;
namespace resources = ::midipatchbay::resources;

namespace winrt::midipatchbay::implementation
{
    namespace
    {
        // A white key per natural, black keys drawn over the joins. Narrow enough that all ten
        // and a half octaves fit the dialog without scrolling.
        constexpr double WhiteKeyWidth = 9.0;
        constexpr double WhiteKeyHeight = 62.0;
        constexpr double BlackKeyWidth = 6.0;
        constexpr double BlackKeyHeight = 38.0;

        constexpr bool IsBlackKey(_In_ uint8_t note) noexcept
        {
            switch (note % 12)
            {
            case 1: case 3: case 6: case 8: case 10: return true;
            default: return false;
            }
        }

        // A keyboard is white and black whatever the app theme is, and the theme text brushes
        // are translucent white in dark mode, which would paint the sharps white.
        winrt::Windows::UI::Color KeyColor(_In_ bool black) noexcept
        {
            winrt::Windows::UI::Color color{};

            color.A = 255;
            color.R = color.G = color.B = black ? 0x22 : 0xF2;

            return color;
        }

        // The same hue as the accent, dark enough to still read as a sharp.
        winrt::Windows::UI::Color DarkenedAccent(_In_ media::Brush const& accent) noexcept
        {
            winrt::Windows::UI::Color color{};

            color.A = 255;
            color.R = 0x1E;
            color.G = 0x4B;
            color.B = 0x60;

            if (auto const solid = accent.try_as<media::SolidColorBrush>())
            {
                auto const source = solid.Color();

                color.R = static_cast<uint8_t>(source.R * 0.42);
                color.G = static_cast<uint8_t>(source.G * 0.42);
                color.B = static_cast<uint8_t>(source.B * 0.42);
            }

            return color;
        }

        // How many naturals come before this note, which is its x position in white key widths.
        int WhiteKeysBefore(_In_ uint8_t note) noexcept
        {
            int count{ 0 };

            for (uint8_t i = 0; i < note; i++)
            {
                if (!IsBlackKey(i))
                {
                    count++;
                }
            }

            return count;
        }

        controls::TextBlock SectionHeading(_In_ winrt::hstring const& text) noexcept
        {
            controls::TextBlock block{};

            block.Text(text);
            block.FontSize(13);
            block.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());

            return block;
        }

        controls::TextBlock SectionHint(_In_ winrt::hstring const& text) noexcept
        {
            controls::TextBlock block{};

            block.Text(text);
            block.FontSize(11);
            block.TextWrapping(xaml::TextWrapping::Wrap);
            block.Margin(xaml::ThicknessHelper::FromLengths(0, -4, 0, 4));
            block.Foreground(patchbay::ThemeBrushes::Current().Get(L"TextFillColorTertiaryBrush"));

            return block;
        }

        controls::Border FilterCard(_In_ xaml::UIElement const& content) noexcept
        {
            controls::Border card{};

            card.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(6));
            card.BorderThickness(xaml::ThicknessHelper::FromUniformLength(1));
            card.Padding(xaml::ThicknessHelper::FromLengths(12, 10, 12, 12));
            card.BorderBrush(patchbay::ThemeBrushes::Current().Get(L"CardStrokeColorDefaultBrush"));
            card.Background(patchbay::ThemeBrushes::Current().Get(L"CardBackgroundFillColorSecondaryBrush"));
            card.Child(content);

            return card;
        }
    }

    // The dialog edits a copy. Nothing reaches the connection until Apply, so Cancel really does
    // leave a live route alone.
    winrt::fire_and_forget MainWindow::ShowFilterDialogAsync(std::wstring connectionId)
    {
        auto strong = get_strong();

        try
        {
            auto* patch = CurrentPatch();

            if (patch == nullptr)
            {
                co_return;
            }

            auto const* connection = patch->FindConnection(connectionId);

            if (connection == nullptr)
            {
                co_return;
            }

            m_editingFilter = connection->Filter;
            m_editingFilterConnectionId = connectionId;

            BuildFilterDialog();

            FilterDialog().XamlRoot(Content().XamlRoot());

            auto const result = co_await FilterDialog().ShowAsync();

            if (result == controls::ContentDialogResult::None)
            {
                co_return;
            }

            if (result == controls::ContentDialogResult::Secondary)
            {
                m_editingFilter.Reset();
            }

            // Active whenever it excludes something, so the hot path can skip it otherwise.
            m_editingFilter.IsActive = true;
            m_editingFilter.IsActive = !m_editingFilter.PassesEverything();

            patch = CurrentPatch();

            if (patch == nullptr)
            {
                co_return;
            }

            auto* target = patch->FindConnection(connectionId);

            if (target == nullptr)
            {
                co_return;
            }

            target->Filter = m_editingFilter;

            MarkDirty();
            RefreshInspector();
            ApplyRouting();
            UpdateMessages();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to edit the filters.")
    }

    void MainWindow::UpdateFilterSummary() noexcept
    {
        try
        {
            auto preview = m_editingFilter;

            preview.IsActive = true;

            FilterSummaryText().Text(resources::FormatString(L"FilterDialogSummaryFormat",
                patchbay::SummarizeFilter(preview)));
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to summarize the filters.")
    }

    void MainWindow::BuildFilterDialog() noexcept
    {
        try
        {
            FilterContent().Children().Clear();

            m_noteKeyFills.clear();
            m_noteLimitCheck = nullptr;
            m_noteLowBox = nullptr;
            m_noteHighBox = nullptr;
            m_noteRangeText = nullptr;

            auto weak = get_weak();

            // A check box wired straight at one bool in the working copy.
            auto const addCheck = [this, weak](
                controls::Panel const& parent,
                winrt::hstring const& text,
                bool* flag)
                {
                    controls::CheckBox check{};

                    check.Content(winrt::box_value(text));
                    check.IsChecked(*flag);
                    check.MinWidth(0);

                    check.Checked([weak, flag](auto&&, auto&&)
                        {
                            if (auto s = weak.get()) { *flag = true; s->UpdateFilterSummary(); }
                        });

                    check.Unchecked([weak, flag](auto&&, auto&&)
                        {
                            if (auto s = weak.get()) { *flag = false; s->UpdateFilterSummary(); }
                        });

                    parent.Children().Append(check);

                    return check;
                };

            // ------------------------------------------------------ message types
            {
                controls::StackPanel body{};
                body.Spacing(4);

                body.Children().Append(SectionHeading(resources::GetString(L"FilterSectionMessageTypes")));
                body.Children().Append(SectionHint(resources::GetString(L"FilterSectionMessageTypesHint")));

                controls::VariableSizedWrapGrid grid{};
                grid.Orientation(controls::Orientation::Horizontal);
                grid.MaximumRowsOrColumns(3);
                grid.ItemWidth(228);
                grid.ItemHeight(30);

                for (size_t i = 0; i < patchbay::MessageTypeCount; i++)
                {
                    // The reserved types have nothing to say to a customer, and listing eight of
                    // them would bury the six that matter.
                    switch (i)
                    {
                    case 0x0: case 0x1: case 0x2: case 0x3:
                    case 0x4: case 0x5: case 0xD: case 0xF:
                        break;
                    default:
                        continue;
                    }

                    addCheck(grid, patchbay::DescribeMessageType(static_cast<uint8_t>(i)),
                        &m_editingFilter.MessageTypes[i]);
                }

                body.Children().Append(grid);

                FilterContent().Children().Append(FilterCard(body));
            }

            // -------------------------------------------- channel voice messages
            {
                controls::StackPanel body{};
                body.Spacing(4);

                body.Children().Append(SectionHeading(resources::GetString(L"FilterSectionChannelVoice")));
                body.Children().Append(SectionHint(resources::GetString(L"FilterSectionChannelVoiceHint")));

                controls::VariableSizedWrapGrid grid{};
                grid.Orientation(controls::Orientation::Horizontal);
                grid.MaximumRowsOrColumns(3);
                grid.ItemWidth(228);
                grid.ItemHeight(30);

                // Note off and note on together: nobody wants one without the other, and a stuck
                // note is what you get when they are split by accident.
                {
                    controls::CheckBox notes{};

                    notes.Content(winrt::box_value(resources::GetString(L"VoiceNotes")));
                    notes.IsChecked(m_editingFilter.ChannelVoiceStatuses[0x8] &&
                        m_editingFilter.ChannelVoiceStatuses[0x9]);

                    auto const setNotes = [weak](bool value)
                        {
                            if (auto s = weak.get())
                            {
                                s->m_editingFilter.ChannelVoiceStatuses[0x8] = value;
                                s->m_editingFilter.ChannelVoiceStatuses[0x9] = value;
                                s->UpdateFilterSummary();
                            }
                        };

                    notes.Checked([setNotes](auto&&, auto&&) { setNotes(true); });
                    notes.Unchecked([setNotes](auto&&, auto&&) { setNotes(false); });

                    grid.Children().Append(notes);
                }

                for (uint8_t status = 0; status < patchbay::ChannelVoiceStatusCount; status++)
                {
                    if (status == 0x8 || status == 0x9)
                    {
                        continue;
                    }

                    if (status == 0x7)
                    {
                        continue;
                    }

                    addCheck(grid, patchbay::DescribeChannelVoiceStatus(status),
                        &m_editingFilter.ChannelVoiceStatuses[status]);
                }

                body.Children().Append(grid);

                FilterContent().Children().Append(FilterCard(body));
            }

            // -------------------------------------------------- system messages
            {
                controls::StackPanel body{};
                body.Spacing(4);

                body.Children().Append(SectionHeading(resources::GetString(L"FilterSectionSystem")));
                body.Children().Append(SectionHint(resources::GetString(L"FilterSectionSystemHint")));

                controls::VariableSizedWrapGrid grid{};
                grid.Orientation(controls::Orientation::Horizontal);
                grid.MaximumRowsOrColumns(3);
                grid.ItemWidth(228);
                grid.ItemHeight(30);

                for (size_t i = 0; i < patchbay::SystemMessageCount; i++)
                {
                    addCheck(grid, patchbay::DescribeSystemMessage(patchbay::SystemMessageList[i]),
                        &m_editingFilter.SystemMessages[i]);
                }

                body.Children().Append(grid);

                FilterContent().Children().Append(FilterCard(body));
            }

            // --------------------------------------------------------- channels
            {
                controls::StackPanel body{};
                body.Spacing(4);

                body.Children().Append(SectionHeading(resources::GetString(L"FilterSectionChannels")));
                body.Children().Append(SectionHint(resources::GetString(L"FilterSectionChannelsHint")));

                controls::VariableSizedWrapGrid grid{};
                grid.Orientation(controls::Orientation::Horizontal);
                grid.MaximumRowsOrColumns(2);
                grid.ItemWidth(116);
                grid.ItemHeight(32);

                for (size_t i = 0; i < patchbay::ChannelCount; i++)
                {
                    // Channels are numbered from one everywhere a musician sees them.
                    addCheck(grid, resources::FormatString(L"FilterChannelFormat", static_cast<int>(i) + 1),
                        &m_editingFilter.Channels[i]);
                }

                body.Children().Append(grid);

                FilterContent().Children().Append(FilterCard(body));
            }

            // -------------------------------------------------------- note range
            FilterContent().Children().Append(FilterCard(BuildNoteRangeSection()));

            UpdateFilterSummary();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the filter dialog.")
    }

    xaml::UIElement MainWindow::BuildNoteRangeSection() noexcept
    {
        controls::StackPanel body{};

        try
        {
            body.Spacing(4);

            body.Children().Append(SectionHeading(resources::GetString(L"FilterSectionNotes")));
            body.Children().Append(SectionHint(resources::GetString(L"FilterSectionNotesHint")));

            auto weak = get_weak();

            controls::CheckBox limit{};

            limit.Content(winrt::box_value(resources::GetString(L"FilterLimitNoteRange")));
            limit.IsChecked(m_editingFilter.LimitNoteRange);

            auto const setLimit = [weak](bool value)
                {
                    if (auto s = weak.get())
                    {
                        if (s->m_updatingNoteRange)
                        {
                            return;
                        }

                        s->m_editingFilter.LimitNoteRange = value;
                        s->RefreshNoteRangeUi();
                        s->UpdateFilterSummary();
                    }
                };

            limit.Checked([setLimit](auto&&, auto&&) { setLimit(true); });
            limit.Unchecked([setLimit](auto&&, auto&&) { setLimit(false); });

            m_noteLimitCheck = limit;

            body.Children().Append(limit);

            // ---- the numbers
            controls::StackPanel numbers{};
            numbers.Orientation(controls::Orientation::Horizontal);
            numbers.Spacing(12);
            numbers.Margin(xaml::ThicknessHelper::FromLengths(0, 6, 0, 6));

            auto const addNumber = [this, weak, &numbers](winrt::hstring const& header, bool isLow)
                {
                    controls::NumberBox box{};

                    box.Header(winrt::box_value(header));
                    box.Minimum(patchbay::LowestNote);
                    box.Maximum(patchbay::HighestNote);
                    box.SmallChange(1);
                    box.LargeChange(12);
                    box.Width(150);
                    box.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);
                    box.ValidationMode(controls::NumberBoxValidationMode::InvalidInputOverwritten);
                    box.Value(isLow ? m_editingFilter.LowestAllowedNote : m_editingFilter.HighestAllowedNote);

                    box.ValueChanged([weak, isLow](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                        {
                            auto s = weak.get();

                            if (s == nullptr || s->m_updatingNoteRange || std::isnan(args.NewValue()))
                            {
                                return;
                            }

                            s->SetNoteRangeEnd(static_cast<uint8_t>(std::clamp(args.NewValue(),
                                static_cast<double>(patchbay::LowestNote),
                                static_cast<double>(patchbay::HighestNote))), isLow);
                        });

                    numbers.Children().Append(box);

                    return box;
                };

            m_noteLowBox = addNumber(resources::GetString(L"FilterLowestNote"), true);
            m_noteHighBox = addNumber(resources::GetString(L"FilterHighestNote"), false);

            m_noteRangeText = controls::TextBlock{};
            m_noteRangeText.FontSize(13);
            m_noteRangeText.VerticalAlignment(xaml::VerticalAlignment::Bottom);
            m_noteRangeText.Margin(xaml::ThicknessHelper::FromLengths(0, 0, 0, 6));

            numbers.Children().Append(m_noteRangeText);

            body.Children().Append(numbers);

            // ---- the keyboard
            m_noteKeyboard = controls::Canvas{};

            auto const whiteCount = WhiteKeysBefore(patchbay::HighestNote) + 1;

            m_noteKeyboard.Width(whiteCount * WhiteKeyWidth);
            m_noteKeyboard.Height(WhiteKeyHeight);

            // Whites first so the blacks sit on top of them.
            for (int pass = 0; pass < 2; pass++)
            {
                for (uint8_t note = patchbay::LowestNote; note <= patchbay::HighestNote; note++)
                {
                    auto const black = IsBlackKey(note);

                    if ((pass == 0) == black)
                    {
                        continue;
                    }

                    shapes::Rectangle key{};

                    key.Width(black ? BlackKeyWidth : WhiteKeyWidth - 1);
                    key.Height(black ? BlackKeyHeight : WhiteKeyHeight);
                    key.RadiusX(1.5);
                    key.RadiusY(1.5);
                    key.StrokeThickness(0);

                    auto const left = black
                        ? WhiteKeysBefore(note) * WhiteKeyWidth - BlackKeyWidth / 2
                        : WhiteKeysBefore(note) * WhiteKeyWidth;

                    controls::Canvas::SetLeft(key, left);
                    controls::Canvas::SetTop(key, 0);

                    xaml::Automation::AutomationProperties::SetName(key, patchbay::DescribeNote(note));

                    key.PointerPressed([weak, note](auto&&, input::PointerRoutedEventArgs const& args)
                        {
                            args.Handled(true);

                            auto s = weak.get();

                            if (s == nullptr)
                            {
                                return;
                            }

                            // Shift sets the top of the range, a plain click the bottom, which
                            // is quicker than aiming at two separate strips of keys.
                            auto const shift = (args.KeyModifiers() &
                                winrt::Windows::System::VirtualKeyModifiers::Shift) !=
                                winrt::Windows::System::VirtualKeyModifiers::None;

                            s->SetNoteRangeEnd(note, !shift);
                        });

                    m_noteKeyboard.Children().Append(key);
                    m_noteKeyFills.emplace_back(note, key);

                    if (note == patchbay::HighestNote)
                    {
                        break;
                    }
                }
            }

            controls::ScrollViewer keyboardScroller{};

            keyboardScroller.HorizontalScrollBarVisibility(controls::ScrollBarVisibility::Auto);
            keyboardScroller.VerticalScrollBarVisibility(controls::ScrollBarVisibility::Disabled);
            keyboardScroller.HorizontalScrollMode(controls::ScrollMode::Auto);
            keyboardScroller.Content(m_noteKeyboard);

            body.Children().Append(keyboardScroller);

            RefreshNoteRangeUi();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the note range picker.")

        return body;
    }

    _Use_decl_annotations_
    void MainWindow::SetNoteRangeEnd(uint8_t note, bool isLow) noexcept
    {
        try
        {
            if (isLow)
            {
                m_editingFilter.LowestAllowedNote = note;

                if (m_editingFilter.HighestAllowedNote < note)
                {
                    m_editingFilter.HighestAllowedNote = note;
                }
            }
            else
            {
                m_editingFilter.HighestAllowedNote = note;

                if (m_editingFilter.LowestAllowedNote > note)
                {
                    m_editingFilter.LowestAllowedNote = note;
                }
            }

            // Touching the keyboard means they want the range, so turning it on by hand first
            // is one step nobody would thank us for.
            m_editingFilter.LimitNoteRange = true;

            RefreshNoteRangeUi();
            UpdateFilterSummary();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to set the note range.")
    }

    void MainWindow::RefreshNoteRangeUi() noexcept
    {
        try
        {
            m_updatingNoteRange = true;

            if (m_noteLimitCheck != nullptr)
            {
                m_noteLimitCheck.IsChecked(m_editingFilter.LimitNoteRange);
            }

            if (m_noteLowBox != nullptr)
            {
                m_noteLowBox.Value(m_editingFilter.LowestAllowedNote);
            }

            if (m_noteHighBox != nullptr)
            {
                m_noteHighBox.Value(m_editingFilter.HighestAllowedNote);
            }

            if (m_noteRangeText != nullptr)
            {
                m_noteRangeText.Text(m_editingFilter.LimitNoteRange
                    ? resources::FormatString(L"FilterNoteRangeFormat",
                        patchbay::DescribeNote(m_editingFilter.LowestAllowedNote),
                        static_cast<int>(m_editingFilter.LowestAllowedNote),
                        patchbay::DescribeNote(m_editingFilter.HighestAllowedNote),
                        static_cast<int>(m_editingFilter.HighestAllowedNote))
                    : resources::GetString(L"FilterNoteRangeAll"));
            }

            auto const accent = patchbay::ThemeBrushes::Current().Get(L"AccentFillColorDefaultBrush");

            media::SolidColorBrush const naturalKey{ KeyColor(false) };
            media::SolidColorBrush const sharpKey{ KeyColor(true) };
            media::SolidColorBrush const sharpInRange{ DarkenedAccent(accent) };

            for (auto const& [note, key] : m_noteKeyFills)
            {
                auto const black = IsBlackKey(note);

                auto const included = !m_editingFilter.LimitNoteRange ||
                    (note >= m_editingFilter.LowestAllowedNote && note <= m_editingFilter.HighestAllowedNote);

                if (!m_editingFilter.LimitNoteRange)
                {
                    key.Opacity(1.0);
                    key.Fill(black ? sharpKey : naturalKey);
                    continue;
                }

                key.Opacity(included ? 1.0 : 0.26);

                if (!included)
                {
                    key.Fill(black ? sharpKey : naturalKey);
                    continue;
                }

                key.Fill(black ? static_cast<media::Brush>(sharpInRange) : accent);
            }

            m_updatingNoteRange = false;
        }
        catch (...)
        {
            m_updatingNoteRange = false;
            MIDI_PATCHBAY_LOG_GENERAL_EXCEPTION(L"Unable to refresh the note range picker.");
        }
    }
}
