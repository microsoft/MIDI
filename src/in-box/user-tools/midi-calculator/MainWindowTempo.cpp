// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Tempo and beats: tap tempo, the beat calculator, note lengths and tempo change. Laid out as
// src\prototypes\midi-calculator\design\1-tempo-and-beats.html.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "CalcTempo.h"
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
        // what the beat calculator works out
        enum class BeatFind : int32_t
        {
            Tempo = 0,
            Length = 1,
            Bars = 2
        };

        constexpr ::midicalc::NoteValue NoteValues[7]{
            ::midicalc::NoteValue::Whole, ::midicalc::NoteValue::Half, ::midicalc::NoteValue::Quarter,
            ::midicalc::NoteValue::Eighth, ::midicalc::NoteValue::Sixteenth, ::midicalc::NoteValue::ThirtySecond,
            ::midicalc::NoteValue::SixtyFourth };

        // the quarter note's row, lit as one beat
        constexpr size_t QuarterRow = 2;

        double NumberOr(controls::NumberBox const& box, double fallback) noexcept
        {
            auto const value = box.Value();
            return std::isnan(value) ? fallback : value;
        }

        media::Brush Brush(wchar_t const* key)
        {
            return xaml::Application::Current().Resources().Lookup(winrt::box_value(key)).as<media::Brush>();
        }

        std::wstring MeterText(::midicalc::Meter const& meter)
        {
            return std::format(L"{}/{}", meter.Numerator, meter.Denominator);
        }
    }

    void MainWindow::BuildTempoPage()
    {
        auto const page = res::GetString(L"PageTempo");
        auto const copy = CopyFor(page);
        auto columns = std::make_shared<ui::CardColumns>();
        auto const& settings = native::AppSettings::Current();

        // ---- tap tempo (left) ----
        {
            auto card = ui::MakeCard(res::GetString(L"TapTitle"), L"\uE916");

            // the comp's pad: a faint accent tint inside a dashed outline, 128 px tall
            controls::Button pad{};
            pad.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
            pad.HorizontalContentAlignment(xaml::HorizontalAlignment::Stretch);
            pad.VerticalContentAlignment(xaml::VerticalAlignment::Stretch);
            pad.Height(128);
            pad.Padding(xaml::Thickness{ 0 });
            pad.BorderThickness(xaml::Thickness{ 0 });
            pad.CornerRadius(xaml::CornerRadius{ 8 });
            {
                auto accent = winrt::unbox_value<winrt::Windows::UI::Color>(xaml::Application::Current().Resources().Lookup(winrt::box_value(L"SystemAccentColorLight2")));
                accent.A = 0x10;
                pad.Background(media::SolidColorBrush{ accent });
            }
            winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(pad, res::GetString(L"TapPadName"));

            controls::Grid padGrid{};

            xaml::Shapes::Rectangle outline{};
            outline.RadiusX(8);
            outline.RadiusY(8);
            outline.StrokeThickness(1);
            outline.Stroke(Brush(L"ControlStrongStrokeColorDisabledBrush"));
            outline.StrokeDashArray().Append(4);
            outline.StrokeDashArray().Append(3);
            padGrid.Children().Append(outline);

            controls::StackPanel content{};
            content.Spacing(4);
            content.HorizontalAlignment(xaml::HorizontalAlignment::Center);
            content.VerticalAlignment(xaml::VerticalAlignment::Center);

            m_tapValue = controls::TextBlock{};
            m_tapValue.HorizontalAlignment(xaml::HorizontalAlignment::Center);
            ui::SetBigText(m_tapValue, res::GetString(L"TapNoTempo"), 40.0, 16.0);
            winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetLiveSetting(m_tapValue, winrt::Microsoft::UI::Xaml::Automation::Peers::AutomationLiveSetting::Polite);

            // one dot for each tap that counts, as in the comp
            m_tapDots = controls::StackPanel{};
            m_tapDots.Orientation(controls::Orientation::Horizontal);
            m_tapDots.Spacing(6);
            m_tapDots.HorizontalAlignment(xaml::HorizontalAlignment::Center);
            winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetAccessibilityView(m_tapDots, winrt::Microsoft::UI::Xaml::Automation::Peers::AccessibilityView::Raw);

            for (size_t i = 0; i < midiapp::TapTempo::MaximumTapsConsidered; ++i)
            {
                xaml::Shapes::Ellipse dot{};
                dot.Width(8);
                dot.Height(8);
                dot.Fill(Brush(L"ControlStrongFillColorDisabledBrush"));
                m_tapDots.Children().Append(dot);
            }

            m_tapDetail = ui::MakeHelp(res::GetString(L"TapHint"));
            m_tapDetail.HorizontalAlignment(xaml::HorizontalAlignment::Center);
            m_tapDetail.TextAlignment(xaml::TextAlignment::Center);

            content.Children().Append(m_tapValue);
            content.Children().Append(m_tapDots);
            content.Children().Append(m_tapDetail);
            padGrid.Children().Append(content);
            pad.Content(padGrid);
            pad.Click([weak = get_weak()](auto&&, auto&&) { if (auto s = weak.get()) s->Tap(); });

            auto use = ui::MakeSmallButton(res::GetString(L"UseAsSessionTempo"), nullptr, true, false);
            use.Click([weak = get_weak(), page](auto&&, auto&&)
                {
                    if (auto s = weak.get(); s && s->m_tappedTempo.has_value())
                    {
                        s->UseAsSessionTempo(*s->m_tappedTempo, page, res::GetString(L"TapTitle"));
                    }
                });

            auto reset = ui::MakeSmallButton(res::GetString(L"TapReset"), L"\uE777", false, true);
            reset.Click([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto s = weak.get())
                    {
                        s->m_tapTempo.Reset();
                        s->m_tappedTempo.reset();
                        ui::SetBigText(s->m_tapValue, res::GetString(L"TapNoTempo"), 40.0, 16.0);
                        s->m_tapDetail.Text(res::GetString(L"TapHint"));

                        auto const off = xaml::Application::Current().Resources().Lookup(winrt::box_value(L"ControlStrongFillColorDisabledBrush")).as<media::Brush>();

                        for (auto const& dot : s->m_tapDots.Children())
                        {
                            dot.as<xaml::Shapes::Ellipse>().Fill(off);
                        }
                    }
                });

            controls::StackPanel buttons{};
            buttons.Orientation(controls::Orientation::Horizontal);
            buttons.Spacing(8);
            buttons.HorizontalAlignment(xaml::HorizontalAlignment::Right);
            buttons.Children().Append(use);
            buttons.Children().Append(reset);

            card.Body.Children().Append(pad);
            card.Body.Children().Append(buttons);
            columns->Add(card, 0);
        }

        // ---- beat calculator (left) ----
        {
            auto card = ui::MakeCard(res::GetString(L"BeatTitle"), L"\uE8EF");

            m_beatFind = std::make_shared<ui::Segmented>(
                std::vector<winrt::hstring>{ res::GetString(L"BeatFindTempo"), res::GetString(L"BeatFindLength"), res::GetString(L"BeatFindBars") },
                0, res::GetString(L"BeatFindLabel"));
            card.Body.Children().Append(m_beatFind->Root());
            card.Body.Children().Append(ui::MakeHelp(res::GetString(L"BeatHelp")));

            m_beatBars = ui::MakeNumberBox(res::GetString(L"BeatBarsLabel"), 8, 0, 100000, 84);
            m_beatBeats = ui::MakeNumberBox(res::GetString(L"BeatBeatsLabel"), 0, 0, 1000, 84);

            // spin arrows on the two counts, as in the comp
            m_beatBars.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Inline);
            m_beatBeats.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Inline);
            m_beatBars.Width(96);
            m_beatBeats.Width(96);

            m_beatMeter = ui::MakeComboBox(res::GetString(L"BeatMeterLabel"), { L"2/4", L"3/4", L"4/4", L"5/4", L"6/8", L"7/8", L"12/8" }, -1, 90);
            m_beatMeter.IsEditable(true);
            // the session meter, chosen in the list when it's there, typed in when it isn't
            {
                winrt::hstring const sessionMeter{ MeterText(settings.Meter()) };
                bool found{ false };

                for (uint32_t i = 0; i < m_beatMeter.Items().Size(); ++i)
                {
                    if (winrt::unbox_value<winrt::hstring>(m_beatMeter.Items().GetAt(i)) == sessionMeter)
                    {
                        m_beatMeter.SelectedIndex(static_cast<int32_t>(i));
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    m_beatMeter.Text(sessionMeter);
                }
            }

            m_beatLength = ui::MakeTextBox(res::GetString(L"BeatLengthLabel"), L"0:15.600", 130);
            m_beatLengthUnit = ui::MakeComboBox(res::GetString(L"BeatLengthUnitLabel"),
                { res::GetString(L"BeatUnitTime"), res::GetString(L"BeatUnitSamples") }, 0, 130);
            m_beatTempo = ui::MakeNumberBox(res::GetString(L"BeatTempoLabel"), settings.Tempo(), 1, 1000, 100);

            card.Body.Children().Append(ui::MakeFieldRow({ m_beatBars, m_beatBeats, m_beatMeter, m_beatLength }));
            card.Body.Children().Append(ui::MakeFieldRow({ m_beatLengthUnit, m_beatTempo }));

            m_beatResult = ui::MakeBigResult(res::GetString(L"BeatResultName"), copy);

            auto use = ui::MakeSmallButton(res::GetString(L"UseAsSessionTempo"), nullptr, false, false);
            use.Click([weak = get_weak(), page](auto&&, auto&&)
                {
                    if (auto s = weak.get(); s && s->m_beatAnswerTempo.has_value())
                    {
                        s->UseAsSessionTempo(*s->m_beatAnswerTempo, page, res::GetString(L"BeatTitle"));
                    }
                });
            m_beatResult.Actions.Children().Append(use);
            card.Body.Children().Append(m_beatResult.Root);

            auto update = [weak = get_weak()](auto&&, auto&&) { if (auto s = weak.get()) s->UpdateBeatCalculator(); };
            m_beatFind->Changed([weak = get_weak()](int32_t) { if (auto s = weak.get()) s->UpdateBeatCalculator(); });
            m_beatBars.ValueChanged(update);
            m_beatBeats.ValueChanged(update);
            m_beatTempo.ValueChanged(update);
            m_beatLength.TextChanged(update);
            m_beatMeter.SelectionChanged(update);
            m_beatMeter.TextSubmitted([weak = get_weak()](auto&&, controls::ComboBoxTextSubmittedEventArgs const& args)
                {
                    if (auto s = weak.get())
                    {
                        // a meter that isn't in the list is fine; the box keeps what was typed
                        args.Handled(true);
                        s->m_beatMeter.Text(args.Text());
                        s->UpdateBeatCalculator();
                    }
                });
            m_beatLengthUnit.SelectionChanged([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto s = weak.get())
                    {
                        // keep the same length, in the other unit
                        auto const rate = native::AppSettings::Current().SampleRate();
                        auto const textValue = s->m_beatLength.Text();

                        if (s->m_beatLengthUnit.SelectedIndex() == 1)
                        {
                            if (auto const seconds = text::ParseDuration(textValue); seconds.has_value())
                            {
                                s->m_beatLength.Text(std::to_wstring(std::llround(*seconds * rate)));
                            }
                        }
                        else if (auto const samples = text::ParseUnsigned(textValue); samples.has_value())
                        {
                            s->m_beatLength.Text(text::Duration(static_cast<double>(*samples) / rate));
                        }

                        s->UpdateBeatCalculator();
                    }
                });

            columns->Add(card, 0);
        }

        // ---- tempo change (left) ----
        {
            auto card = ui::MakeCard(res::GetString(L"ChangeTitle"), L"\uE8AB");

            auto const tempo = settings.Tempo();
            m_changeFrom = ui::MakeNumberBox(res::GetString(L"ChangeFromLabel"), tempo, 1, 1000, 110);
            m_changeTo = ui::MakeNumberBox(res::GetString(L"ChangeToLabel"), tempo, 1, 1000, 110);
            m_changePercent = ui::MakeNumberBox(res::GetString(L"ChangePercentLabel"), 0, -99, 1000, 120);

            card.Body.Children().Append(ui::MakeFieldRow({ m_changeFrom, m_changeTo, m_changePercent }));

            ui::ResultList results{};
            m_changeSpeed = results.Add(res::GetString(L"ChangeSpeed"), copy);
            m_changeLength = results.Add(res::GetString(L"ChangeLength"), copy);
            m_changePitch = results.Add(res::GetString(L"ChangePitch"), copy);
            card.Body.Children().Append(results.Root());

            m_changeFrom.ValueChanged([weak = get_weak()](auto&&, auto&&) { if (auto s = weak.get()) s->UpdateTempoChange(false); });
            m_changeTo.ValueChanged([weak = get_weak()](auto&&, auto&&) { if (auto s = weak.get()) s->UpdateTempoChange(false); });
            m_changePercent.ValueChanged([weak = get_weak()](auto&&, auto&&) { if (auto s = weak.get()) s->UpdateTempoChange(true); });

            columns->Add(card, 0);
        }

        // ---- note lengths (right) ----
        {
            auto card = ui::MakeCard(L"", L"\uE8FD");
            m_noteLengthsTitle = card.Title;

            m_noteUnit = std::make_shared<ui::Segmented>(
                std::vector<winrt::hstring>{ res::GetString(L"NoteUnitMilliseconds"), res::GetString(L"NoteUnitHertz"), res::GetString(L"NoteUnitSamples"), res::GetString(L"NoteUnitTicks") },
                0, res::GetString(L"NoteUnitLabel"));
            m_noteUnit->Changed([weak = get_weak()](int32_t) { if (auto s = weak.get()) s->UpdateNoteLengths(); });
            card.Body.Children().Append(m_noteUnit->Root());

            controls::Grid table{};
            table.ColumnSpacing(4);

            for (int i = 0; i < 4; ++i)
            {
                controls::ColumnDefinition column{};
                column.Width(xaml::GridLengthHelper::FromValueAndType(i == 0 ? 1.4 : 1.0, xaml::GridUnitType::Star));
                table.ColumnDefinitions().Append(column);
            }

            for (int i = 0; i < 8; ++i)
            {
                controls::RowDefinition row{};
                row.Height(xaml::GridLengthHelper::Auto());
                table.RowDefinitions().Append(row);
            }

            // the quarter note's row, lit behind its cells
            controls::Border highlight{};
            {
                auto accent = winrt::unbox_value<winrt::Windows::UI::Color>(xaml::Application::Current().Resources().Lookup(winrt::box_value(L"SystemAccentColorLight2")));
                accent.A = 0x24;
                highlight.Background(media::SolidColorBrush{ accent });
            }
            highlight.CornerRadius(xaml::CornerRadius{ 4 });
            controls::Grid::SetRow(highlight, static_cast<int32_t>(QuarterRow) + 1);
            controls::Grid::SetColumnSpan(highlight, 4);
            table.Children().Append(highlight);

            winrt::hstring const headings[4]{ res::GetString(L"NoteHeadingNote"), res::GetString(L"NoteHeadingStraight"), res::GetString(L"NoteHeadingDotted"), res::GetString(L"NoteHeadingTriplet") };

            for (int column = 0; column < 4; ++column)
            {
                controls::TextBlock heading{};
                heading.Text(headings[column]);
                heading.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
                heading.FontSize(12);
                heading.Foreground(Brush(L"TextFillColorSecondaryBrush"));
                heading.Margin(xaml::Thickness{ 8, 6, 8, 7 });
                heading.HorizontalAlignment(column == 0 ? xaml::HorizontalAlignment::Left : xaml::HorizontalAlignment::Right);
                controls::Grid::SetColumn(heading, column);
                table.Children().Append(heading);
            }

            winrt::hstring const names[7]{ L"1/1", L"1/2", res::GetString(L"NoteQuarterOneBeat"), L"1/8", L"1/16", L"1/32", L"1/64" };
            winrt::hstring const shortNames[7]{ L"1/1", L"1/2", L"1/4", L"1/8", L"1/16", L"1/32", L"1/64" };
            winrt::hstring const feels[3]{ res::GetString(L"NoteHeadingStraight"), res::GetString(L"NoteHeadingDotted"), res::GetString(L"NoteHeadingTriplet") };

            // a line under the headings and under each row
            for (int row = 0; row < 8; ++row)
            {
                controls::Border line{};
                line.BorderBrush(Brush(L"DividerStrokeColorDefaultBrush"));
                line.BorderThickness(xaml::Thickness{ 0, 0, 0, 1 });
                controls::Grid::SetRow(line, row);
                controls::Grid::SetColumnSpan(line, 4);
                table.Children().Append(line);
            }

            for (int row = 0; row < 7; ++row)
            {
                controls::TextBlock name{};
                name.FontSize(13);
                name.Text(names[row]);
                name.Margin(xaml::Thickness{ 8, 0, 0, 0 });
                name.Foreground(row == static_cast<int>(QuarterRow) ? Brush(L"AccentTextFillColorPrimaryBrush") : Brush(L"TextFillColorSecondaryBrush"));
                name.VerticalAlignment(xaml::VerticalAlignment::Center);
                controls::Grid::SetRow(name, row + 1);
                table.Children().Append(name);

                for (int column = 0; column < 3; ++column)
                {
                    // a button, so each value can be copied with one click or the keyboard
                    controls::HyperlinkButton cell{};
                    cell.HorizontalAlignment(xaml::HorizontalAlignment::Right);
                    cell.Padding(xaml::Thickness{ 8, 4, 8, 4 });
                    cell.FontSize(13);
                    cell.MinHeight(28);
                    cell.Foreground(Brush(L"TextFillColorPrimaryBrush"));

                    controls::TextBlock value{};
                    cell.Content(value);
                    m_noteCells[row][column] = value;

                    auto const label = res::FormatString(L"NoteCellName", std::wstring_view{ shortNames[row] }, std::wstring_view{ feels[column] });
                    cell.Click([weak = get_weak(), value, label, page](auto&&, auto&&)
                        {
                            if (auto s = weak.get())
                            {
                                s->CopyAnswer(page, label, value.Text());
                            }
                        });

                    controls::Grid::SetRow(cell, row + 1);
                    controls::Grid::SetColumn(cell, column + 1);
                    table.Children().Append(cell);
                }
            }

            card.Body.Children().Append(table);
            card.Body.Children().Append(ui::MakeHelp(res::GetString(L"NoteHelp")));

            ui::ResultList bars{};
            m_oneBar = bars.Add(res::GetString(L"NoteOneBar"), copy);
            m_oneBarLabel = bars.LastLabel();
            m_sixteenBars = bars.Add(res::GetString(L"NoteSixteenBars"), copy);
            m_preDelay = bars.Add(res::GetString(L"NotePreDelay"), copy);
            card.Body.Children().Append(bars.Root());

            columns->Add(card, 1);
        }

        TempoPage().Children().Append(columns->Root());
        m_columns.push_back(columns);

        UpdateTempoPage();
    }

    void MainWindow::UpdateTempoPage() noexcept
    {
        if (!m_beatFind)
        {
            return;
        }

        UpdateBeatCalculator();
        UpdateNoteLengths();
        UpdateTempoChange(false);
    }

    void MainWindow::Tap() noexcept
    {
        try
        {
            m_tappedTempo = m_tapTempo.Tap();

            // light a dot for each tap that counts
            auto const count = static_cast<uint32_t>(m_tapTempo.TapCount());
            auto const on = xaml::Application::Current().Resources().Lookup(winrt::box_value(L"AccentFillColorDefaultBrush")).as<media::Brush>();
            auto const off = xaml::Application::Current().Resources().Lookup(winrt::box_value(L"ControlStrongFillColorDisabledBrush")).as<media::Brush>();

            for (uint32_t i = 0; i < m_tapDots.Children().Size(); ++i)
            {
                m_tapDots.Children().GetAt(i).as<xaml::Shapes::Ellipse>().Fill(i < count ? on : off);
            }

            if (!m_tappedTempo.has_value())
            {
                ui::SetBigText(m_tapValue, res::GetString(L"TapKeepTapping"), 40.0, 16.0);
                m_tapDetail.Text(res::GetString(L"TapHint"));
                return;
            }

            ui::SetBigText(m_tapValue, res::FormatString(L"TempoValueFormat", text::Number(*m_tappedTempo, 1, true)), 40.0, 16.0);

            auto const spread = m_tapTempo.SpreadMilliseconds();

            if (spread.has_value())
            {
                m_tapDetail.Text(res::FormatString(L"TapDetailFormat", m_tapTempo.TapCount(), text::Number(*spread, 1, true)));
            }
            else
            {
                m_tapDetail.Text(res::FormatString(L"TapCountFormat", m_tapTempo.TapCount()));
            }
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to tap the tempo.")
    }

    void MainWindow::UpdateBeatCalculator() noexcept
    {
        try
        {
            if (!m_beatFind)
            {
                return;
            }

            auto const& settings = native::AppSettings::Current();
            auto const find = static_cast<BeatFind>(m_beatFind->Selected());
            auto const rate = settings.SampleRate();
            auto const inSamples = m_beatLengthUnit.SelectedIndex() == 1;

            // the card's own meter; the session's when what's typed isn't one
            auto meter = settings.Meter();
            auto const typed = m_beatMeter.SelectedItem()
                ? winrt::unbox_value<winrt::hstring>(m_beatMeter.SelectedItem())
                : m_beatMeter.Text();

            if (auto const parsed = text::ParseMeter(typed); parsed.has_value() && ::midicalc::IsValidMeter({ parsed->first, parsed->second }))
            {
                meter = { parsed->first, parsed->second };
            }

            // the field being worked out is the answer, so it isn't shown as something to type
            m_beatTempo.Visibility(find == BeatFind::Tempo ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
            m_beatLength.IsEnabled(find != BeatFind::Length);
            m_beatLengthUnit.IsEnabled(find != BeatFind::Length);
            m_beatBars.IsEnabled(find != BeatFind::Bars);
            m_beatBeats.IsEnabled(find != BeatFind::Bars);

            std::optional<double> seconds{};

            if (inSamples)
            {
                if (auto const samples = text::ParseUnsigned(m_beatLength.Text()); samples.has_value())
                {
                    seconds = static_cast<double>(*samples) / rate;
                }
            }
            else
            {
                seconds = text::ParseDuration(m_beatLength.Text());
            }

            auto const quarters = ::midicalc::QuarterNotesIn(NumberOr(m_beatBars, 0), NumberOr(m_beatBeats, 0), meter);
            auto const tempo = NumberOr(m_beatTempo, settings.Tempo());
            auto const beatsInMeter = quarters * meter.Denominator / 4.0;

            m_beatAnswerTempo.reset();
            m_beatResult.Actions.Children().GetAt(1).Visibility(find == BeatFind::Tempo ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            switch (find)
            {
            case BeatFind::Tempo:
            {
                auto const answer = seconds.has_value() ? ::midicalc::TempoForLength(quarters, *seconds) : std::nullopt;

                if (!answer.has_value())
                {
                    ::midicalculator::ui::SetBigText(m_beatResult.Value, std::wstring_view{ winrt::hstring{ L"\u2014" } });
                    m_beatResult.Detail.Text(res::GetString(L"BeatNeedLengthAndBars"));
                    return;
                }

                m_beatAnswerTempo = *answer;
                ::midicalculator::ui::SetBigText(m_beatResult.Value, std::wstring_view{ winrt::hstring{ res::FormatString(L"TempoValueFormat", text::Number(*answer, 3)) } });
                m_beatResult.Detail.Text(res::FormatString(L"BeatTempoDetail",
                    text::Number(beatsInMeter, 2), text::Number(*seconds, 3), text::Number(*seconds * 1000.0 / beatsInMeter, 2)));
                break;
            }
            case BeatFind::Length:
            {
                auto const answer = ::midicalc::SecondsForLength(quarters, tempo);

                if (!answer.has_value())
                {
                    ::midicalculator::ui::SetBigText(m_beatResult.Value, std::wstring_view{ winrt::hstring{ L"\u2014" } });
                    m_beatResult.Detail.Text(L"");
                    return;
                }

                ::midicalculator::ui::SetBigText(m_beatResult.Value, std::wstring_view{ winrt::hstring{ winrt::hstring{ text::Duration(*answer) } } });
                m_beatResult.Detail.Text(res::FormatString(L"BeatLengthDetail",
                    text::Number(*answer, 3), text::Whole(static_cast<uint64_t>(std::llround(*answer * rate))), text::Number(rate, 0)));
                break;
            }
            case BeatFind::Bars:
            {
                auto const answer = seconds.has_value() ? ::midicalc::BarsInLength(*seconds, tempo, meter) : std::nullopt;

                if (!answer.has_value())
                {
                    ::midicalculator::ui::SetBigText(m_beatResult.Value, std::wstring_view{ winrt::hstring{ L"\u2014" } });
                    m_beatResult.Detail.Text(res::GetString(L"BeatNeedLength"));
                    return;
                }

                auto const wholeBars = std::floor(*answer + 1e-9);
                auto const beats = (*answer - wholeBars) * meter.Numerator;

                ::midicalculator::ui::SetBigText(m_beatResult.Value, std::wstring_view{ winrt::hstring{ res::FormatString(L"BeatBarsValue", text::Number(*answer, 3)) } });
                m_beatResult.Detail.Text(res::FormatString(L"BeatBarsDetail", text::Number(wholeBars, 0), text::Number(beats, 2), MeterText(meter)));
                break;
            }
            }
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to work out the beat calculator.")
    }

    void MainWindow::UpdateNoteLengths() noexcept
    {
        try
        {
            if (!m_noteUnit)
            {
                return;
            }

            auto const& settings = native::AppSettings::Current();
            auto const tempo = settings.Tempo();
            auto const rate = settings.SampleRate();
            auto const unit = m_noteUnit->Selected();

            m_noteLengthsTitle.Text(res::FormatString(L"NoteTitleFormat", text::Number(tempo, 3)));

            constexpr ::midicalc::NoteFeel Feels[3]{ ::midicalc::NoteFeel::Straight, ::midicalc::NoteFeel::Dotted, ::midicalc::NoteFeel::Triplet };

            for (size_t row = 0; row < 7; ++row)
            {
                for (size_t column = 0; column < 3; ++column)
                {
                    auto const milliseconds = ::midicalc::NoteMilliseconds(NoteValues[row], Feels[column], tempo);
                    std::wstring value{};

                    switch (unit)
                    {
                    case 1: value = text::Number(::midicalc::MillisecondsToHertz(milliseconds), 4); break;
                    case 2: value = text::Number(::midicalc::MillisecondsToSamples(milliseconds, rate), 1); break;
                    case 3: value = text::Number(::midicalc::NoteQuarterNotes(NoteValues[row], Feels[column]) * settings.TicksPerQuarterNote(), 2); break;
                    default: value = text::Number(milliseconds, 2); break;
                    }

                    m_noteCells[row][column].Text(value);
                }
            }

            auto const meter = settings.Meter();
            auto const bar = ::midicalc::SecondsForLength(::midicalc::QuarterNotesPerBar(meter), tempo).value_or(0.0);

            m_oneBarLabel.Text(res::FormatString(L"NoteOneBarName", MeterText(meter)));
            m_oneBar.Text(res::FormatString(L"NoteBarValue", text::Number(bar * 1000.0, 2), text::Whole(static_cast<uint64_t>(std::llround(bar * rate)))));
            m_sixteenBars.Text(res::FormatString(L"NoteSixteenValue", text::Duration(bar * 16.0), text::Whole(static_cast<uint64_t>(std::llround(bar * 16.0 * rate)))));
            m_preDelay.Text(res::FormatString(L"MillisecondsValue", text::Number(::midicalc::NoteMilliseconds(::midicalc::NoteValue::SixtyFourth, ::midicalc::NoteFeel::Straight, tempo), 2)));
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to work out the note lengths.")
    }

    _Use_decl_annotations_
    void MainWindow::UpdateTempoChange(bool fromPercent) noexcept
    {
        try
        {
            if (!m_changeFrom || m_suppressChange)
            {
                return;
            }

            auto const from = NumberOr(m_changeFrom, native::AppSettings::Current().Tempo());

            m_suppressChange = true;

            if (fromPercent)
            {
                if (auto const to = ::midicalc::TempoAfterPercent(from, NumberOr(m_changePercent, 0)); to.has_value())
                {
                    m_changeTo.Value(std::round(*to * 1000.0) / 1000.0);
                }
            }

            auto const change = ::midicalc::ChangeTempo(from, NumberOr(m_changeTo, from));

            if (change.has_value() && !fromPercent)
            {
                m_changePercent.Value(std::round(change->Percent * 1000.0) / 1000.0);
            }

            m_suppressChange = false;

            if (!change.has_value())
            {
                m_changeSpeed.Text(L"\u2014");
                m_changeLength.Text(L"\u2014");
                m_changePitch.Text(L"\u2014");
                return;
            }

            m_changeSpeed.Text(res::FormatString(L"ChangeSpeedValue", text::Signed(change->Percent, 3), text::Number(change->SpeedRatio, 4)));
            m_changeLength.Text(res::FormatString(L"ChangeLengthValue", text::Number(change->LengthRatio, 4), text::Number(16.0 * change->LengthRatio, 2)));
            m_changePitch.Text(res::FormatString(L"ChangePitchValue", text::Signed(change->Semitones, 3), text::Signed(change->Semitones * 100.0, 1)));
        }
        catch (...)
        {
            m_suppressChange = false;
            LOG_CAUGHT_EXCEPTION();
        }
    }
}
