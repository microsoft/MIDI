// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Notes and pitch: note to frequency, frequency to note, intervals, and a note table in the
// tuning the customer picks. Laid out as src\prototypes\midi-calculator\design\2-notes-and-pitch.html.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "CalcMidi2.h"
#include "CalcPitch.h"
#include "CalcText.h"
#include "StringResources.h"

namespace native = ::midicalculator;
namespace res = ::midicalculator::resources;
namespace ui = ::midicalculator::ui;
namespace text = ::midicalculator::text;

namespace winrt::midicalculator::implementation
{
    namespace
    {
        double NumberOr(controls::NumberBox const& box, double fallback) noexcept
        {
            auto const value = box.Value();
            return std::isnan(value) ? fallback : value;
        }

        media::Brush Brush(wchar_t const* key)
        {
            return xaml::Application::Current().Resources().Lookup(winrt::box_value(key)).as<media::Brush>();
        }

        // The names of the 12 pitch classes, for the tuning's root. Sharps, as in the note boxes.
        std::vector<winrt::hstring> RootNames(uint8_t middleC)
        {
            std::vector<winrt::hstring> names{};

            for (int32_t pc = 0; pc < 12; ++pc)
            {
                auto name = ::midicalc::NoteName(60 + pc, middleC, false);

                // just the letter and accidental, not the octave
                while (!name.empty() && (std::iswdigit(name.back()) || name.back() == L'-'))
                {
                    name.pop_back();
                }

                names.emplace_back(name);
            }

            return names;
        }

        // the pure ratios IntervalBetween compares with, written the way musicians write them
        constexpr wchar_t const* PureRatioNames[12]{ L"1:1", L"16:15", L"9:8", L"6:5", L"5:4", L"4:3", L"45:32", L"3:2", L"8:5", L"5:3", L"9:5", L"15:8" };

        constexpr double GaugeWidth = 300.0;
    }

    void MainWindow::BuildPitchPage()
    {
        auto const page = res::GetString(L"PagePitch");
        auto const copy = CopyFor(page);
        auto columns = std::make_shared<ui::CardColumns>();
        auto const middleC = native::AppSettings::Current().MiddleCOctave();

        // ---- note to frequency (left) ----
        {
            auto card = ui::MakeCard(res::GetString(L"PitchNoteTitle"), L"\uE8D6");

            m_pitchNote = ui::MakeTextBox(res::GetString(L"PitchNoteLabel"), winrt::hstring{ ::midicalc::NoteNameWithNumber(69, middleC) }, 150);
            m_pitchCents = ui::MakeNumberBox(res::GetString(L"PitchCentsLabel"), 0, -1200, 1200, 120);
            card.Body.Children().Append(ui::MakeFieldRow({ m_pitchNote, m_pitchCents }));

            m_pitchFrequency = ui::MakeBigResult(res::GetString(L"PitchFrequencyName"), copy);
            card.Body.Children().Append(m_pitchFrequency.Root);

            ui::ResultList results{};
            m_pitchNumber = results.Add(res::GetString(L"PitchNumber"), copy);
            m_pitchPeriod = results.Add(res::GetString(L"PitchPeriod"), copy);
            m_pitchSamples = results.Add(L"", copy);
            m_pitchSamplesLabel = results.LastLabel();
            m_pitchOtherNames = results.Add(res::GetString(L"PitchOtherNames"), copy);
            card.Body.Children().Append(results.Root());

            auto update = [weak = get_weak()](auto&&, auto&&) { if (auto s = weak.get()) s->UpdatePitchPage(); };
            m_pitchNote.TextChanged(update);
            m_pitchCents.ValueChanged(update);

            columns->Add(card, 0);
        }

        // ---- frequency to note (right) ----
        {
            auto card = ui::MakeCard(res::GetString(L"FrequencyTitle"), L"\uE9D9");

            m_frequency = ui::MakeNumberBox(res::GetString(L"FrequencyLabel"), 450, 0.01, 100000, 170);
            card.Body.Children().Append(m_frequency);

            m_frequencyNote = ui::MakeBigResult(res::GetString(L"FrequencyNoteName"), copy);
            card.Body.Children().Append(m_frequencyNote.Root);

            // a tuner's needle, -50 to +50 cents
            m_gauge = controls::Grid{};
            m_gauge.Width(GaugeWidth);
            m_gauge.Height(44);
            m_gauge.HorizontalAlignment(xaml::HorizontalAlignment::Center);
            winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetAccessibilityView(m_gauge, winrt::Microsoft::UI::Xaml::Automation::Peers::AccessibilityView::Raw);

            xaml::Shapes::Rectangle line{};
            line.Height(2);
            line.VerticalAlignment(xaml::VerticalAlignment::Top);
            line.Margin(xaml::Thickness{ 0, 18, 0, 0 });
            line.Fill(Brush(L"ControlStrongStrokeColorDisabledBrush"));
            m_gauge.Children().Append(line);

            for (int i = 0; i <= 4; ++i)
            {
                auto const x = GaugeWidth * i / 4.0;
                auto const middle = i == 2;

                xaml::Shapes::Rectangle tick{};
                tick.Width(1);
                tick.Height(middle ? 26 : 14);
                tick.HorizontalAlignment(xaml::HorizontalAlignment::Left);
                tick.VerticalAlignment(xaml::VerticalAlignment::Top);
                tick.Margin(xaml::Thickness{ x, middle ? 6.0 : 12.0, 0, 0 });
                tick.Fill(middle ? Brush(L"TextFillColorSecondaryBrush") : Brush(L"ControlStrongStrokeColorDisabledBrush"));
                m_gauge.Children().Append(tick);

                controls::TextBlock label{};
                label.Text(winrt::hstring{ text::Signed(-50.0 + 25.0 * i, 0) });
                label.FontSize(10);
                label.Foreground(Brush(L"TextFillColorTertiaryBrush"));
                label.HorizontalAlignment(xaml::HorizontalAlignment::Left);
                label.VerticalAlignment(xaml::VerticalAlignment::Top);
                label.Margin(xaml::Thickness{ x - 8.0, 32, 0, 0 });
                m_gauge.Children().Append(label);
            }

            m_gaugeNeedle = xaml::Shapes::Rectangle{};
            m_gaugeNeedle.Width(3);
            m_gaugeNeedle.Height(30);
            m_gaugeNeedle.RadiusX(1.5);
            m_gaugeNeedle.RadiusY(1.5);
            m_gaugeNeedle.HorizontalAlignment(xaml::HorizontalAlignment::Left);
            m_gaugeNeedle.VerticalAlignment(xaml::VerticalAlignment::Top);
            m_gaugeNeedle.Margin(xaml::Thickness{ GaugeWidth / 2.0 - 1.5, 4, 0, 0 });
            m_gaugeNeedle.Fill(Brush(L"AccentFillColorDefaultBrush"));
            m_gauge.Children().Append(m_gaugeNeedle);

            card.Body.Children().Append(m_gauge);

            ui::ResultList results{};
            m_frequencyFraction = results.Add(res::GetString(L"FrequencyFraction"), copy);
            m_frequencyPitch79 = results.Add(res::GetString(L"FrequencyPitch79"), copy, true);
            card.Body.Children().Append(results.Root());

            // the MIDI 2.0 page, with this pitch already filled in
            m_frequencyLink = controls::HyperlinkButton{};
            m_frequencyLink.Content(winrt::box_value(res::GetString(L"FrequencyOpenMidi2")));
            m_frequencyLink.Click([weak = get_weak()](auto&&, auto&&)
                {
                    auto s = weak.get();

                    if (!s || !s->m_m2Note)
                    {
                        return;
                    }

                    auto const nearest = ::midicalc::NearestNoteToFrequency(NumberOr(s->m_frequency, 0), ::midicalc::Tuning{ native::AppSettings::Current().ReferenceHertz() });

                    if (nearest.has_value())
                    {
                        s->m_m2Note.Text(winrt::hstring{ ::midicalc::NoteNameWithNumber(nearest->Note, native::AppSettings::Current().MiddleCOctave()) });
                        s->m_m2Cents.Value(std::round(nearest->Cents * 1000.0) / 1000.0);
                    }

                    s->ShowPage(native::CalculatorPage::Midi2);
                });
            card.Body.Children().Append(m_frequencyLink);

            m_frequency.ValueChanged([weak = get_weak()](auto&&, auto&&) { if (auto s = weak.get()) s->UpdatePitchPage(); });

            columns->Add(card, 1);
        }

        // ---- interval (left) ----
        {
            auto card = ui::MakeCard(res::GetString(L"IntervalTitle"), L"\uE8AB");

            m_intervalFrom = ui::MakeTextBox(res::GetString(L"IntervalFromLabel"), winrt::hstring{ ::midicalc::NoteNameWithNumber(60, middleC) }, 130);
            m_intervalTo = ui::MakeTextBox(res::GetString(L"IntervalToLabel"), winrt::hstring{ ::midicalc::NoteNameWithNumber(67, middleC) }, 130);
            m_intervalMove = ui::MakeNumberBox(res::GetString(L"IntervalMoveLabel"), 7, -127, 127, 130);
            card.Body.Children().Append(ui::MakeFieldRow({ m_intervalFrom, m_intervalTo, m_intervalMove }));

            ui::ResultList results{};
            m_intervalName = results.Add(res::GetString(L"IntervalName"), copy);
            m_intervalDistance = results.Add(res::GetString(L"IntervalDistance"), copy);
            m_intervalRatio = results.Add(res::GetString(L"IntervalRatio"), copy);
            m_intervalFrequencies = results.Add(res::GetString(L"IntervalFrequencies"), copy);
            card.Body.Children().Append(results.Root());

            auto update = [weak = get_weak()](auto&&, auto&&) { if (auto s = weak.get()) s->UpdatePitchPage(); };
            m_intervalFrom.TextChanged(update);
            m_intervalTo.TextChanged(update);

            // moving by a number of semitones fills in To
            m_intervalMove.ValueChanged([weak = get_weak()](controls::NumberBox const& sender, auto&&)
                {
                    auto s = weak.get();

                    if (!s || s->m_suppressInterval || std::isnan(sender.Value()))
                    {
                        return;
                    }

                    auto const middleC = native::AppSettings::Current().MiddleCOctave();
                    auto const from = ::midicalc::ParseNote(s->m_intervalFrom.Text(), middleC);

                    if (from.has_value())
                    {
                        auto const to = *from + static_cast<int32_t>(sender.Value());

                        if (to >= 0 && to <= 127)
                        {
                            s->m_suppressInterval = true;
                            s->m_intervalTo.Text(winrt::hstring{ ::midicalc::NoteNameWithNumber(to, middleC) });
                            s->m_suppressInterval = false;
                        }
                    }
                });

            columns->Add(card, 0);
        }

        // ---- note table (right) ----
        {
            auto card = ui::MakeCard(res::GetString(L"TableTitle"), L"\uE8FD");

            auto const& settings = native::AppSettings::Current();

            m_tableOctave = ui::MakeNumberBox(res::GetString(L"TableOctaveLabel"), 3, -2, 8, 90);
            m_tableOctave.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);
            m_temperament = ui::MakeComboBox(res::GetString(L"TemperamentLabel"), {
                res::GetString(L"TemperamentEqual"), res::GetString(L"TemperamentJust"), res::GetString(L"TemperamentPythagorean"),
                res::GetString(L"TemperamentMeantone"), res::GetString(L"TemperamentWerckmeister") },
                static_cast<int32_t>(settings.Temperament()), 220);
            m_temperamentRoot = ui::MakeComboBox(res::GetString(L"TemperamentRootLabel"), RootNames(middleC), settings.TemperamentRoot(), 90);
            card.Body.Children().Append(ui::MakeFieldRow({ m_tableOctave, m_temperament, m_temperamentRoot }));

            // two halves side by side: Note | Number | Hz, twice, as in the comp
            controls::Grid table{};
            table.ColumnSpacing(10);

            for (int i = 0; i < 6; ++i)
            {
                controls::ColumnDefinition column{};
                column.Width(xaml::GridLengthHelper::FromValueAndType(i % 3 == 2 ? 1.4 : 1.0, xaml::GridUnitType::Star));
                table.ColumnDefinitions().Append(column);
            }

            for (int row = 0; row <= 6; ++row)
            {
                controls::RowDefinition definition{};
                definition.Height(xaml::GridLengthHelper::Auto());
                table.RowDefinitions().Append(definition);
            }

            winrt::hstring const headings[3]{ res::GetString(L"TableNote"), res::GetString(L"TableNumber"), res::GetString(L"TableHertz") };

            for (int column = 0; column < 6; ++column)
            {
                controls::TextBlock heading{};
                heading.Text(headings[column % 3]);
                heading.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
                heading.FontSize(12);
                heading.Foreground(Brush(L"TextFillColorSecondaryBrush"));
                heading.Margin(xaml::Thickness{ 0, 0, 0, 6 });
                heading.HorizontalAlignment(column % 3 == 0 ? xaml::HorizontalAlignment::Left : xaml::HorizontalAlignment::Right);
                controls::Grid::SetColumn(heading, column);
                table.Children().Append(heading);
            }

            for (int i = 0; i < 12; ++i)
            {
                auto const row = (i % 6) + 1;
                auto const firstColumn = (i / 6) * 3;

                std::array<controls::TextBlock, 3> cells{};

                for (int column = 0; column < 3; ++column)
                {
                    cells[column] = controls::TextBlock{};
                    cells[column].IsTextSelectionEnabled(true);
                    cells[column].Margin(xaml::Thickness{ 0, 4, 0, 4 });
                    cells[column].FontSize(13);
                    cells[column].HorizontalAlignment(column == 0 ? xaml::HorizontalAlignment::Left : xaml::HorizontalAlignment::Right);
                    controls::Grid::SetRow(cells[column], row);
                    controls::Grid::SetColumn(cells[column], firstColumn + column);
                    table.Children().Append(cells[column]);
                }

                m_tableNames[i] = cells[0];
                m_tableNumbers[i] = cells[1];
                m_tableHertz[i] = cells[2];
            }

            card.Body.Children().Append(table);

            auto copyTable = controls::HyperlinkButton{};
            copyTable.Content(winrt::box_value(res::GetString(L"TableCopy")));
            copyTable.Click([weak = get_weak(), page](auto&&, auto&&)
                {
                    if (auto s = weak.get())
                    {
                        std::wstring all{};

                        for (size_t i = 0; i < 12; ++i)
                        {
                            all += std::format(L"{}\t{}\t{}\r\n", std::wstring_view{ s->m_tableNames[i].Text() }, std::wstring_view{ s->m_tableNumbers[i].Text() }, std::wstring_view{ s->m_tableHertz[i].Text() });
                        }

                        s->CopyAnswer(page, res::GetString(L"TableTitle"), winrt::hstring{ all });
                    }
                });
            card.Body.Children().Append(copyTable);
            card.Body.Children().Append(ui::MakeHelp(res::GetString(L"TableHelp")));

            m_temperament.SelectionChanged([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto s = weak.get(); s && s->m_temperament.SelectedIndex() >= 0)
                    {
                        native::AppSettings::Current().Temperament(static_cast<::midicalc::Temperament>(s->m_temperament.SelectedIndex()));
                        s->UpdatePitchPage();
                    }
                });
            m_temperamentRoot.SelectionChanged([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto s = weak.get(); s && s->m_temperamentRoot.SelectedIndex() >= 0)
                    {
                        native::AppSettings::Current().TemperamentRoot(s->m_temperamentRoot.SelectedIndex());
                        s->UpdatePitchPage();
                    }
                });
            m_tableOctave.ValueChanged([weak = get_weak()](auto&&, auto&&) { if (auto s = weak.get()) s->UpdateNoteTable(); });

            columns->Add(card, 1);
        }

        PitchPage().Children().Append(columns->Root());
        m_columns.push_back(columns);

        UpdatePitchPage();
    }

    void MainWindow::UpdatePitchPage() noexcept
    {
        try
        {
            if (!m_pitchNote)
            {
                return;
            }

            auto const& settings = native::AppSettings::Current();
            auto const tuning = settings.Tuning();
            auto const middleC = settings.MiddleCOctave();
            auto const rate = settings.SampleRate();
            auto const referenceName = ::midicalc::NoteName(::midicalc::ReferenceNote, middleC, false);
            std::wstring const temperamentName{ m_temperament ? winrt::unbox_value_or<winrt::hstring>(m_temperament.SelectedItem(), L"") : winrt::hstring{} };

            // ---- note to frequency ----
            m_pitchSamplesLabel.Text(res::FormatString(L"PitchSamplesPerCycle", text::Number(rate / 1000.0, 1)));

            if (auto const note = ::midicalc::ParseNote(m_pitchNote.Text(), middleC); note.has_value())
            {
                auto const hertz = ::midicalc::NoteFrequency(*note + NumberOr(m_pitchCents, 0) / 100.0, tuning);

                ::midicalculator::ui::SetBigText(m_pitchFrequency.Value, std::wstring_view{ winrt::hstring{ res::FormatString(L"HertzValue", text::Number(hertz, 3, true)) } });
                m_pitchFrequency.Detail.Text(res::FormatString(L"PitchFrequencyDetail", temperamentName, referenceName, text::Number(tuning.ReferenceHertz, 3)));
                m_pitchNumber.Text(std::format(L"{} \u00B7 {}", *note, text::Hex(static_cast<uint64_t>(*note), 2)));
                m_pitchPeriod.Text(res::FormatString(L"MillisecondsValue", text::Number(1000.0 / hertz, 4)));
                m_pitchSamples.Text(winrt::hstring{ text::Number(rate / hertz, 2, true) });

                // the same note in the other common numbering: C4 if this PC calls middle C C3, and the other way round
                auto const other = static_cast<uint8_t>(middleC == 4 ? 3 : 4);
                m_pitchOtherNames.Text(res::FormatString(L"PitchOtherNamesValue", ::midicalc::NoteName(*note, other, false), other));
            }
            else
            {
                ::midicalculator::ui::SetBigText(m_pitchFrequency.Value, std::wstring_view{ winrt::hstring{ L"\u2014" } });
                m_pitchFrequency.Detail.Text(res::GetString(L"PitchNoteInvalid"));

                for (auto const& block : { m_pitchNumber, m_pitchPeriod, m_pitchSamples, m_pitchOtherNames })
                {
                    block.Text(L"\u2014");
                }
            }

            // ---- frequency to note ----
            auto const hertz = NumberOr(m_frequency, 0);

            if (auto const nearest = ::midicalc::NearestNoteToFrequency(hertz, tuning); nearest.has_value())
            {
                auto const target = ::midicalc::NoteFrequency(nearest->Note, tuning);
                auto const away = hertz - target;

                ::midicalculator::ui::SetBigText(m_frequencyNote.Value, std::wstring_view{ winrt::hstring{ res::FormatString(L"FrequencyNoteValue", ::midicalc::NoteName(nearest->Note, middleC, false), text::Signed(nearest->Cents, 1)) } });
                m_frequencyNote.Detail.Text(res::FormatString(away >= 0 ? L"FrequencyNoteSharp" : L"FrequencyNoteFlat",
                    nearest->Note, text::Number(std::abs(away), 2), text::Number(target, 3)));

                auto const shown = std::clamp(nearest->Cents, -50.0, 50.0);
                m_gaugeNeedle.Margin(xaml::Thickness{ GaugeWidth / 2.0 + shown / 100.0 * GaugeWidth - 1.5, 4, 0, 0 });

                auto const fractional = ::midicalc::FractionalNote(hertz, tuning.ReferenceHertz);
                m_frequencyFraction.Text(winrt::hstring{ text::Number(fractional, 4) });

                // MIDI 2.0 pitch is always equal-tempered note numbers, whatever the tuning here
                m_frequencyPitch79.Text(fractional >= 0.0 && fractional < 128.0
                    ? res::FormatString(L"FrequencyPitch79Value", text::Hex(::midicalc::Pitch79FromNote(fractional), 4))
                    : winrt::hstring{ L"\u2014" });
                m_frequencyLink.Visibility(fractional >= 0.0 && fractional < 128.0 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            }
            else
            {
                ::midicalculator::ui::SetBigText(m_frequencyNote.Value, std::wstring_view{ winrt::hstring{ L"\u2014" } });
                m_frequencyNote.Detail.Text(res::GetString(L"FrequencyOutOfRange"));
                m_frequencyFraction.Text(L"\u2014");
                m_frequencyPitch79.Text(L"\u2014");
                m_frequencyLink.Visibility(xaml::Visibility::Collapsed);
                m_gaugeNeedle.Margin(xaml::Thickness{ GaugeWidth / 2.0 - 1.5, 4, 0, 0 });
            }

            // ---- interval ----
            auto const from = ::midicalc::ParseNote(m_intervalFrom.Text(), middleC);
            auto const to = ::midicalc::ParseNote(m_intervalTo.Text(), middleC);

            if (from.has_value() && to.has_value())
            {
                auto const interval = ::midicalc::IntervalBetween(*from, *to);
                auto name = res::GetString(winrt::hstring{ std::format(L"IntervalName{}", interval.NameIndex) });

                if (interval.Octaves > 0 && interval.NameIndex != 0)
                {
                    name = res::FormatString(L"IntervalPlusOctaves", std::wstring_view{ name }, interval.Octaves);
                }
                else if (interval.Octaves > 0)
                {
                    name = res::FormatString(L"IntervalOctaves", interval.Octaves);
                }

                if (interval.Semitones != 0)
                {
                    name = res::FormatString(interval.Semitones > 0 ? L"IntervalUp" : L"IntervalDown", std::wstring_view{ name });
                }

                m_intervalName.Text(name);
                m_intervalDistance.Text(res::FormatString(L"IntervalDistanceValue", interval.Semitones, interval.Semitones * 100));
                m_intervalRatio.Text(res::FormatString(interval.PureMinusEqualCents >= 0 ? L"IntervalRatioWider" : L"IntervalRatioNarrower",
                    text::Number(interval.EqualRatio, 4), PureRatioNames[interval.NameIndex], text::Number(std::abs(interval.PureMinusEqualCents), 2)));
                m_intervalFrequencies.Text(res::FormatString(L"IntervalFrequenciesValue",
                    text::Number(::midicalc::NoteFrequency(*from, tuning), 3, true), text::Number(::midicalc::NoteFrequency(*to, tuning), 3, true)));

                if (!m_suppressInterval)
                {
                    m_suppressInterval = true;
                    m_intervalMove.Value(interval.Semitones);
                    m_suppressInterval = false;
                }
            }
            else
            {
                for (auto const& block : { m_intervalName, m_intervalDistance, m_intervalRatio, m_intervalFrequencies })
                {
                    block.Text(L"\u2014");
                }
            }

            UpdateNoteTable();
        }
        catch (...)
        {
            m_suppressInterval = false;
            LOG_CAUGHT_EXCEPTION();
        }
    }

    void MainWindow::UpdateNoteTable() noexcept
    {
        try
        {
            if (!m_tableOctave)
            {
                return;
            }

            auto const& settings = native::AppSettings::Current();
            auto const tuning = settings.Tuning();
            auto const middleC = settings.MiddleCOctave();
            auto const octave = static_cast<int32_t>(NumberOr(m_tableOctave, 3));

            // the note number of C in that octave, with the customer's own octave numbering
            auto const firstNote = (octave + 5 - middleC) * 12;

            for (int32_t i = 0; i < 12; ++i)
            {
                auto const note = firstNote + i;

                if (note < 0 || note > 127)
                {
                    m_tableNames[i].Text(L"");
                    m_tableNumbers[i].Text(L"");
                    m_tableHertz[i].Text(L"");
                    continue;
                }

                auto const reference = note == ::midicalc::ReferenceNote;
                auto const brush = reference ? Brush(L"AccentTextFillColorPrimaryBrush") : Brush(L"TextFillColorPrimaryBrush");

                m_tableNames[i].Text(winrt::hstring{ ::midicalc::NoteName(note, middleC, false) });
                m_tableNumbers[i].Text(winrt::hstring{ std::to_wstring(note) });

                // a tuning other than equal temperament shows how far each note moved
                std::wstring hertz{ text::Number(::midicalc::NoteFrequency(note, tuning), 3, true) };

                if (tuning.Kind != ::midicalc::Temperament::Equal)
                {
                    hertz += std::format(L" ({})", text::Signed(::midicalc::CentsFromEqual(tuning.Kind, tuning.Root, i), 1));
                }

                m_tableHertz[i].Text(winrt::hstring{ hertz });

                for (auto const& cell : { m_tableNames[i], m_tableNumbers[i], m_tableHertz[i] })
                {
                    cell.Foreground(brush);
                }
            }
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to fill in the note table.")
    }
}
