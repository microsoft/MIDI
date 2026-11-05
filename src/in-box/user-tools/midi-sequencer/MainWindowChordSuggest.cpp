// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Help picking chords (Key and chords proposal, revision 2, section 2, phase 2): the chord
// picker's Next chord, More chords and Progression sections, and Suggest on the row header. The
// music theory is in HarmonySuggest.cpp, which the tests cover.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "Harmony.h"
#include "HarmonySuggest.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        // In ProgressionId order.
        constexpr wchar_t const* ProgressionNameKeys[]{
            L"ProgressionPopOneFiveSixFour", L"ProgressionPopSixFourOneFive", L"ProgressionPopFifties",
            L"ProgressionJazzTwoFiveOne", L"ProgressionJazzTurnaround",
            L"ProgressionTwelveBarBlues", L"ProgressionTwelveBarBluesSevenths",
            L"ProgressionMinorFall", L"ProgressionMinorOneFourFive", L"ProgressionAndalusian",
            L"ProgressionCanon", L"ProgressionCircleOfFifths",
        };

        static_assert(std::size(ProgressionNameKeys) == seq::ProgressionCount);

        // In ProgressionRate order.
        constexpr wchar_t const* RateNameKeys[]{ L"ProgressionRateTwoPerBar", L"ProgressionRateOnePerBar", L"ProgressionRateOnePerTwoBars" };

        constexpr wchar_t BorrowedStar[] = L" \u2605";
    }

    _Use_decl_annotations_
    winrt::hstring MainWindow::KeyDisplayName(seq::KeyChange const& key)
    {
        auto const& scales = KeyScaleNames();
        auto const scaleIndex = static_cast<size_t>(key.Scale);

        return winrt::hstring{ seq::NoteName(key.Root, seq::KeyUsesFlats(key.Root, key.Scale)) + L" " +
            std::wstring{ scaleIndex < scales.size() ? scales[scaleIndex] : winrt::hstring{} } };
    }

    int64_t MainWindow::SuggestStartTick() const noexcept
    {
        if (m_doc.Harmony && m_selectedChord < m_doc.Harmony->Chords.size())
        {
            return m_doc.Harmony->Chords[m_selectedChord].Tick;
        }

        return m_position;
    }

    _Use_decl_annotations_
    void MainWindow::SuggestChords(int64_t from) noexcept
    {
        try
        {
            if (!m_doc.Harmony)
            {
                return;
            }

            if (m_doc.Harmony->Keys.empty())
            {
                ShowMessage(res::GetString(L"SuggestNeedsKeyMessage"));
                return;
            }

            auto const fill = seq::SuggestChordsToFill(*m_doc.Harmony, m_doc.Meter, from);

            if (fill.empty())
            {
                ShowMessage(res::GetString(L"SuggestNoRoomMessage"));
                return;
            }

            auto const lastTick = fill.back().Tick;

            EditHarmony(res::GetString(L"UndoSuggestChords"), [&fill](seq::HarmonyTrack& harmony)
            {
                for (auto const& chord : fill)
                {
                    seq::PlaceChord(harmony, chord);
                }
            });

            // Select the last one, so Suggest again carries on from there.
            if (m_doc.Harmony)
            {
                for (size_t i = 0; i < m_doc.Harmony->Chords.size(); ++i)
                {
                    if (m_doc.Harmony->Chords[i].Tick == lastTick)
                    {
                        m_selectedChord = i;
                        m_selectedKey = SIZE_MAX;
                    }
                }
            }

            InvalidateArrange();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to suggest chords.")
    }

    _Use_decl_annotations_
    void MainWindow::FillProgression(int64_t tick, size_t progression, size_t rate) noexcept
    {
        try
        {
            if (!m_doc.Harmony || progression >= seq::ProgressionCount || rate >= std::size(RateNameKeys))
            {
                return;
            }

            auto const key = seq::KeyAt(*m_doc.Harmony, tick);

            if (key == nullptr)
            {
                ShowMessage(res::GetString(L"SuggestNeedsKeyMessage"));
                return;
            }

            auto const chords = seq::LayOutProgression(
                static_cast<seq::ProgressionId>(progression), *key, m_doc.Meter, tick, static_cast<seq::ProgressionRate>(rate));

            EditHarmony(res::GetString(L"UndoFillProgression"), [&chords](seq::HarmonyTrack& harmony)
            {
                for (auto const& chord : chords)
                {
                    seq::PlaceChord(harmony, chord);
                }
            });

            m_lastProgression = progression;
            m_lastProgressionRate = rate;
            m_selectedChord = SIZE_MAX;
            m_selectedKey = SIZE_MAX;
            InvalidateArrange();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to fill a progression.")
    }

    _Use_decl_annotations_
    void MainWindow::AddChordSuggestionSections(
        controls::StackPanel const& panel,
        int64_t tick,
        std::function<void(uint8_t root, seq::ChordType type)> const& choose,
        std::function<void()> const& close)
    {
        if (!m_doc.Harmony)
        {
            return;
        }

        auto const heading = [&panel](winrt::hstring const& text)
        {
            controls::TextBlock block{};
            block.Text(text);
            block.FontSize(12);
            block.Opacity(0.8);
            panel.Children().Append(block);
        };

        auto const key = seq::KeyAt(*m_doc.Harmony, tick);

        if (key == nullptr)
        {
            heading(res::GetString(L"ChordSuggestNoKey"));
            return;
        }

        // The chord before this one, if any.
        seq::ChordEvent const* previous{ nullptr };

        for (auto const& chord : m_doc.Harmony->Chords)
        {
            if (chord.Tick < tick)
            {
                previous = &chord;
            }
        }

        auto const phraseBar = seq::BarPositionAtTick(m_doc.Meter, tick).Bar - 1;
        auto const showNumerals = seq::AppSettings::Current().ShowRomanNumerals();

        auto const nameOf = [key](seq::ChordSuggestion const& s)
        {
            seq::ChordEvent chord{};
            chord.Root = s.Root;
            chord.Type = s.Type;
            return std::pair{ seq::ChordName(chord, key), seq::RomanNumeral(chord, *key) };
        };

        auto const makeButton = [&nameOf, &choose, showNumerals](seq::ChordSuggestion const& s, bool large)
        {
            auto const [name, numeral] = nameOf(s);

            controls::StackPanel content{};
            content.HorizontalAlignment(xaml::HorizontalAlignment::Center);

            controls::TextBlock nameBlock{};
            nameBlock.Text(winrt::hstring{ name + (s.Borrowed ? BorrowedStar : L"") });
            nameBlock.FontSize(large ? 15 : 12);
            nameBlock.FontWeight(large ? winrt::Microsoft::UI::Text::FontWeights::SemiBold() : winrt::Microsoft::UI::Text::FontWeights::Normal());
            nameBlock.HorizontalAlignment(xaml::HorizontalAlignment::Center);
            content.Children().Append(nameBlock);

            if (showNumerals && large)
            {
                controls::TextBlock numeralBlock{};
                numeralBlock.Text(winrt::hstring{ numeral });
                numeralBlock.FontSize(11);
                numeralBlock.Opacity(0.7);
                numeralBlock.HorizontalAlignment(xaml::HorizontalAlignment::Center);
                content.Children().Append(numeralBlock);
            }

            controls::Button button{};
            button.Content(content);
            button.MinWidth(large ? 60 : 44);
            button.Padding(large ? xaml::Thickness{ 8, 4, 8, 4 } : xaml::Thickness{ 6, 3, 6, 3 });
            button.Margin(xaml::Thickness{ 0, 0, 4, 4 });

            auto const accessible = showNumerals ? name + L" (" + numeral + L")" : name;
            automation::AutomationProperties::SetName(button, winrt::hstring{ accessible });

            if (s.Borrowed)
            {
                controls::ToolTipService::SetToolTip(button, winrt::box_value(res::GetString(L"ChordSuggestBorrowedHelp")));
                automation::AutomationProperties::SetHelpText(button, res::GetString(L"ChordSuggestBorrowedHelp"));
            }

            button.Click([choose, root = s.Root, type = s.Type](auto&&, auto&&) { choose(root, type); });
            return button;
        };

        // ---- Next chord
        heading(res::GetString(L"ChordSuggestNextHeader"));

        controls::TextBlock caption{};
        caption.FontSize(12);
        caption.TextWrapping(xaml::TextWrapping::Wrap);

        if (previous != nullptr)
        {
            auto const previousName = seq::ChordName(*previous, key);
            auto const previousText = showNumerals ? previousName + L" (" + seq::RomanNumeral(*previous, *key) + L")" : previousName;
            caption.Text(res::FormatString(L"ChordSuggestAfterFormat", previousText, std::wstring{ KeyDisplayName(*key) }));
        }
        else
        {
            caption.Text(res::FormatString(L"ChordSuggestStartFormat", std::wstring{ KeyDisplayName(*key) }));
        }

        panel.Children().Append(caption);

        controls::StackPanel next{};
        next.Orientation(controls::Orientation::Horizontal);

        for (auto const& s : seq::SuggestNextChords(previous, *key, phraseBar))
        {
            next.Children().Append(makeButton(s, true));
        }

        panel.Children().Append(next);

        // ---- More chords
        heading(res::GetString(L"ChordSuggestMoreHeader"));

        controls::StackPanel more{};
        more.Orientation(controls::Orientation::Horizontal);

        for (auto const& s : seq::MoreChordSuggestions(previous, *key, phraseBar))
        {
            more.Children().Append(makeButton(s, false));
        }

        panel.Children().Append(more);

        // ---- Progression
        heading(res::GetString(L"ChordSuggestProgressionHeader"));

        controls::Grid row{};
        row.ColumnSpacing(6);

        for (auto const width : { xaml::GridLength{ 1, xaml::GridUnitType::Star }, xaml::GridLength{ 0, xaml::GridUnitType::Auto }, xaml::GridLength{ 0, xaml::GridUnitType::Auto } })
        {
            controls::ColumnDefinition column{};
            column.Width(width);
            row.ColumnDefinitions().Append(column);
        }

        controls::ComboBox progressions{};
        progressions.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
        automation::AutomationProperties::SetName(progressions, res::GetString(L"ChordSuggestProgressionHeader"));

        for (auto const nameKey : ProgressionNameKeys)
        {
            progressions.Items().Append(winrt::box_value(res::GetString(nameKey)));
        }

        progressions.SelectedIndex(static_cast<int32_t>(std::min(m_lastProgression, seq::ProgressionCount - 1)));

        controls::ComboBox rate{};
        rate.MinWidth(110);
        automation::AutomationProperties::SetName(rate, res::GetString(L"ChordSuggestRateHeader"));
        controls::ToolTipService::SetToolTip(rate, winrt::box_value(res::GetString(L"ChordSuggestRateHeader")));

        for (auto const nameKey : RateNameKeys)
        {
            rate.Items().Append(winrt::box_value(res::GetString(nameKey)));
        }

        rate.SelectedIndex(static_cast<int32_t>(std::min(m_lastProgressionRate, std::size(RateNameKeys) - 1)));

        controls::Button fill{};
        fill.Content(winrt::box_value(res::GetString(L"ChordSuggestFill")));
        controls::ToolTipService::SetToolTip(fill, winrt::box_value(res::GetString(L"ChordSuggestFillHelp")));
        automation::AutomationProperties::SetHelpText(fill, res::GetString(L"ChordSuggestFillHelp"));

        fill.Click([weak = get_weak(), tick, progressions, rate, close](auto&&, auto&&)
        {
            auto const p = progressions.SelectedIndex();
            auto const r = rate.SelectedIndex();

            if (auto s = weak.get(); s && p >= 0 && r >= 0)
            {
                close();
                s->FillProgression(tick, static_cast<size_t>(p), static_cast<size_t>(r));
            }
        });

        controls::Grid::SetColumn(progressions, 0);
        controls::Grid::SetColumn(rate, 1);
        controls::Grid::SetColumn(fill, 2);
        row.Children().Append(progressions);
        row.Children().Append(rate);
        row.Children().Append(fill);
        panel.Children().Append(row);

        // A line between the help and building a chord by hand.
        shapes::Rectangle divider{};
        divider.Height(1);
        divider.Fill(BrushFor(m_palette.Divider));
        divider.Margin(xaml::Thickness{ 0, 4, 0, 0 });
        panel.Children().Append(divider);
    }
}
