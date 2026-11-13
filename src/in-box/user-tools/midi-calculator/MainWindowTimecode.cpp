// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Timecode and samples: the timecode calculator, converting a timecode, and samples and buffers.
// Laid out as src\prototypes\midi-calculator\design\4-timecode-and-samples.html.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "CalcTempo.h"
#include "CalcText.h"
#include "CalcTimecode.h"
#include "StringResources.h"

namespace native = ::midicalculator;
namespace res = ::midicalculator::resources;
namespace ui = ::midicalculator::ui;
namespace text = ::midicalculator::text;

namespace winrt::midicalculator::implementation
{
    namespace
    {
        enum class Operator : int32_t
        {
            Add = 0,
            Subtract = 1,
            Multiply = 2,
            Divide = 3
        };

        constexpr uint32_t BufferSizes[6]{ 32, 64, 128, 256, 512, 1024 };

        // the tape keeps this many answers, newest first
        constexpr uint32_t TapeLength = 4;

        constexpr wchar_t MonospaceFont[] = L"Cascadia Mono, Consolas";

        media::Brush Brush(wchar_t const* key)
        {
            return xaml::Application::Current().Resources().Lookup(winrt::box_value(key)).as<media::Brush>();
        }

        ::midicalc::FrameRate RateOf(controls::ComboBox const& box) noexcept
        {
            auto const index = box.SelectedIndex();
            return index >= 0 && index < ::midicalc::FrameRateCount
                ? static_cast<::midicalc::FrameRate>(index)
                : native::AppSettings::Current().FrameRate();
        }

        controls::ComboBox MakeRateBox(winrt::hstring const& label)
        {
            std::vector<winrt::hstring> names{};

            for (int32_t rate = 0; rate < ::midicalc::FrameRateCount; ++rate)
            {
                names.push_back(res::GetString(std::format(L"FrameRateName{}", rate)));
            }

            return ui::MakeComboBox(label, names, static_cast<int32_t>(native::AppSettings::Current().FrameRate()), 170);
        }

        winrt::hstring RateShortName(::midicalc::FrameRate rate)
        {
            return res::GetString(std::format(L"FrameRateShort{}", static_cast<int32_t>(rate)));
        }

        std::wstring_view Trim(std::wstring_view value) noexcept
        {
            while (!value.empty() && std::iswspace(value.front())) value.remove_prefix(1);
            while (!value.empty() && std::iswspace(value.back())) value.remove_suffix(1);
            return value;
        }

        // A frame count, with a minus sign allowed.
        std::optional<int64_t> ParseFrames(std::wstring_view value) noexcept
        {
            value = Trim(value);
            bool negative{ false };

            if (!value.empty() && (value.front() == L'-' || value.front() == L'\u2212'))
            {
                negative = true;
                value.remove_prefix(1);
            }

            auto const parsed = text::ParseUnsigned(value);

            if (!parsed.has_value() || *parsed > static_cast<uint64_t>(INT64_MAX))
            {
                return std::nullopt;
            }

            return negative ? -static_cast<int64_t>(*parsed) : static_cast<int64_t>(*parsed);
        }

        // A plain number to multiply or divide by: "3", "1.5".
        std::optional<double> ParseNumber(std::wstring_view value)
        {
            std::wstring const trimmed{ Trim(value) };

            if (trimmed.empty())
            {
                return std::nullopt;
            }

            wchar_t* end{ nullptr };
            auto const number = std::wcstod(trimmed.c_str(), &end);

            if (end != trimmed.c_str() + trimmed.size() || !std::isfinite(number))
            {
                return std::nullopt;
            }

            return number;
        }

        // A timecode or a frame count, whichever the calculator is set to, as frames. skipped
        // says the timecode was a drop frame number that isn't counted.
        std::optional<int64_t> ParseValue(std::wstring_view value, ::midicalc::FrameRate rate, bool frames, bool& skipped) noexcept
        {
            skipped = false;

            if (frames)
            {
                return ParseFrames(value);
            }

            if (auto const timecode = ::midicalc::ParseTimecode(value, rate); timecode.has_value())
            {
                return ::midicalc::TimecodeToFrames(*timecode, rate);
            }

            // read it again at the same rate without drop frame, to say why it was refused
            auto const plain = rate == ::midicalc::FrameRate::Fps2997Drop ? ::midicalc::FrameRate::Fps2997
                : rate == ::midicalc::FrameRate::Fps5994Drop ? ::midicalc::FrameRate::Fps5994
                : rate;

            if (auto const timecode = ::midicalc::ParseTimecode(value, plain); timecode.has_value())
            {
                skipped = ::midicalc::IsSkippedFrameNumber(*timecode, rate);
            }

            return std::nullopt;
        }

        std::wstring FormatValue(int64_t frames, ::midicalc::FrameRate rate, bool asFrames)
        {
            return asFrames
                ? text::SignedWhole(frames)
                : ::midicalc::FormatTimecode(::midicalc::FramesToTimecode(frames, rate), rate);
        }

        // "0:59:59.996": always with the hours, as a timecode reads
        std::wstring RealTime(double seconds)
        {
            if (!std::isfinite(seconds))
            {
                return L"\u2014";
            }

            auto const negative = seconds < 0.0;
            auto duration = text::Duration(std::abs(seconds));

            // Duration leaves the hours off under an hour: "59:59.996"
            if (std::count(duration.begin(), duration.end(), L':') == 1)
            {
                if (duration.find(L':') == 1)
                {
                    duration.insert(0, 1, L'0');
                }

                duration.insert(0, L"0:");
            }

            return negative ? L"-" + duration : duration;
        }

        std::wstring KiloHertz(double sampleRate)
        {
            return text::Number(sampleRate / 1000.0, 3);
        }

        std::wstring MeterText(::midicalc::Meter const& meter)
        {
            return std::format(L"{}/{}", meter.Numerator, meter.Denominator);
        }

        controls::TextBox MakeTimecodeBox(winrt::hstring const& name, winrt::hstring const& initial)
        {
            controls::TextBox box{};
            box.Text(initial);
            box.FontFamily(media::FontFamily{ MonospaceFont });
            box.FontSize(18);
            box.MinHeight(40);
            box.VerticalContentAlignment(xaml::VerticalAlignment::Center);
            box.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
            box.IsSpellCheckEnabled(false);
            winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(box, name);
            return box;
        }
    }

    void MainWindow::BuildTimecodePage()
    {
        auto const page = res::GetString(L"PageTimecode");
        auto const copy = CopyFor(page);
        auto columns = std::make_shared<ui::CardColumns>();

        // ---- timecode calculator (left) ----
        {
            auto card = ui::MakeCard(res::GetString(L"TcTitle"), L"\uE823");

            m_tcRate = MakeRateBox(res::GetString(L"TcRateLabel"));
            m_tcMode = std::make_shared<ui::Segmented>(
                std::vector<winrt::hstring>{ res::GetString(L"TcModeTimecode"), res::GetString(L"TcModeFrames") },
                0, res::GetString(L"TcModeLabel"));
            // 32 px here, as tall as the frame rate box beside it, as in the comp
            for (auto const& child : m_tcMode->Root().Children())
            {
                child.as<xaml::FrameworkElement>().MinHeight(32);
            }

            card.Body.Children().Append(ui::MakeFieldRow({ m_tcRate, m_tcMode->Root() }));

            m_tcA = MakeTimecodeBox(res::GetString(L"TcALabel"), L"01:00:00:00");

            // the four operators, one of them chosen, then the second value
            controls::StackPanel operators{};
            operators.Orientation(controls::Orientation::Horizontal);
            operators.Spacing(6);

            wchar_t const* const symbols[4]{ L"TcOpAddText", L"TcOpSubtractText", L"TcOpMultiplyText", L"TcOpDivideText" };
            wchar_t const* const names[4]{ L"TcOpAddName", L"TcOpSubtractName", L"TcOpMultiplyName", L"TcOpDivideName" };

            for (int32_t i = 0; i < 4; ++i)
            {
                controls::Primitives::ToggleButton button{};
                button.Content(winrt::box_value(res::GetString(symbols[i])));
                button.Width(44);
                button.Height(32);
                button.Padding(xaml::Thickness{ 0 });
                button.FontSize(14);
                button.VerticalAlignment(xaml::VerticalAlignment::Center);
                button.IsChecked(i == 0);
                winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(button, res::GetString(names[i]));
                winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetPositionInSet(button, i + 1);
                winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetSizeOfSet(button, 4);
                controls::ToolTipService::SetToolTip(button, winrt::box_value(res::GetString(names[i])));

                // Checked and Unchecked, not Click: Click never fires for a screen reader's toggle
                button.Checked([weak = get_weak(), i](auto&&, auto&&)
                    {
                        if (auto s = weak.get(); s && !s->m_tcUpdatingOperators)
                        {
                            s->m_tcUpdatingOperators = true;
                            s->m_tcOperator = i;

                            for (int32_t j = 0; j < 4; ++j)
                            {
                                s->m_tcOperators[j].IsChecked(j == i);
                            }

                            s->m_tcUpdatingOperators = false;
                            s->UpdateTimecodeCalculator();
                        }
                    });

                // one is always chosen, so turning the chosen one off turns it back on
                button.Unchecked([weak = get_weak(), i](auto&&, auto&&)
                    {
                        if (auto s = weak.get(); s && !s->m_tcUpdatingOperators && s->m_tcOperator == i)
                        {
                            s->m_tcUpdatingOperators = true;
                            s->m_tcOperators[i].IsChecked(true);
                            s->m_tcUpdatingOperators = false;
                        }
                    });

                m_tcOperators[i] = button;
                operators.Children().Append(button);
            }

            m_tcB = MakeTimecodeBox(res::GetString(L"TcBLabelTimecode"), L"00:00:30:00");

            controls::Grid second{};
            second.ColumnSpacing(6);
            {
                controls::ColumnDefinition buttons{};
                buttons.Width(xaml::GridLengthHelper::Auto());
                controls::ColumnDefinition box{};
                box.Width(xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
                second.ColumnDefinitions().Append(buttons);
                second.ColumnDefinitions().Append(box);
            }
            controls::Grid::SetColumn(m_tcB, 1);
            second.Children().Append(operators);
            second.Children().Append(m_tcB);

            controls::StackPanel inputs{};
            inputs.Spacing(6);
            inputs.Children().Append(m_tcA);
            inputs.Children().Append(second);
            card.Body.Children().Append(inputs);

            // copying the answer also puts the sum on the tape
            m_tcResult = ui::MakeBigResult(res::GetString(L"TcResultName"),
                [weak = get_weak(), page](winrt::hstring const&, winrt::hstring const& value)
                {
                    if (auto s = weak.get(); s && !s->m_tcQuestion.empty())
                    {
                        s->CopyAnswer(page, s->m_tcQuestion, value);
                        s->AddToTape(s->m_tcQuestion, s->m_tcAnswerText);
                    }
                });
            m_tcResult.Value.FontFamily(media::FontFamily{ MonospaceFont });

            auto use = ui::MakeSmallButton(res::GetString(L"TcUseAsA"), nullptr, false, false);
            use.Click([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto s = weak.get(); s && s->m_tcAnswer.has_value())
                    {
                        s->AddToTape(s->m_tcQuestion, s->m_tcAnswerText);
                        s->m_tcA.Text(s->m_tcAnswerText);
                    }
                });
            m_tcResult.Actions.Children().Append(use);
            card.Body.Children().Append(m_tcResult.Root);

            m_tcTape = controls::StackPanel{};
            winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(m_tcTape, res::GetString(L"TcTapeName"));
            card.Body.Children().Append(m_tcTape);

            auto update = [weak = get_weak()](auto&&, auto&&) { if (auto s = weak.get()) s->UpdateTimecodeCalculator(); };
            m_tcA.TextChanged(update);
            m_tcB.TextChanged(update);
            m_tcRate.SelectionChanged(update);

            // Enter keeps the sum on the tape
            auto enter = [weak = get_weak()](foundation::IInspectable const&, xaml::Input::KeyRoutedEventArgs const& args)
                {
                    if (auto s = weak.get(); s && args.Key() == winrt::Windows::System::VirtualKey::Enter && s->m_tcAnswer.has_value())
                    {
                        s->AddToTape(s->m_tcQuestion, s->m_tcAnswerText);
                        args.Handled(true);
                    }
                };
            m_tcA.KeyDown(enter);
            m_tcB.KeyDown(enter);

            m_tcMode->Changed([weak = get_weak()](int32_t mode)
                {
                    if (auto s = weak.get())
                    {
                        // keep the same values, written the other way
                        auto const rate = RateOf(s->m_tcRate);
                        auto const toFrames = mode == 1;
                        bool skipped{ false };

                        if (auto const a = ParseValue(s->m_tcA.Text(), rate, !toFrames, skipped); a.has_value())
                        {
                            s->m_tcA.Text(FormatValue(*a, rate, toFrames));
                        }

                        if (s->m_tcOperator < 2)
                        {
                            if (auto const b = ParseValue(s->m_tcB.Text(), rate, !toFrames, skipped); b.has_value())
                            {
                                s->m_tcB.Text(FormatValue(*b, rate, toFrames));
                            }
                        }

                        s->UpdateTimecodeCalculator();
                    }
                });

            columns->Add(card, 0);
        }

        // ---- convert a timecode (right) ----
        {
            auto card = ui::MakeCard(res::GetString(L"CvTitle"), L"\uE8AB");

            m_cvTimecode = ui::MakeTextBox(res::GetString(L"CvTimecodeLabel"), L"01:00:00:00", 160);
            m_cvTimecode.FontFamily(media::FontFamily{ MonospaceFont });
            // the header takes the box's font unless told otherwise
            if (auto header = m_cvTimecode.Header().try_as<controls::TextBlock>())
            {
                header.FontFamily(xaml::Application::Current().Resources().Lookup(winrt::box_value(L"ContentControlThemeFontFamily")).as<media::FontFamily>());
            }
            m_cvRate = MakeRateBox(res::GetString(L"CvRateLabel"));
            card.Body.Children().Append(ui::MakeFieldRow({ m_cvTimecode, m_cvRate }));

            ui::ResultList results{};
            m_cvFrames = results.Add(res::GetString(L"CvFrames"), copy);
            m_cvRealTime = results.Add(res::GetString(L"CvRealTime"), copy);
            m_cvSamples = results.Add(L"", copy);
            m_cvSamplesLabel = results.LastLabel();
            m_cvBars = results.Add(L"", copy);
            m_cvBarsLabel = results.LastLabel();
            card.Body.Children().Append(results.Root());

            m_cvTargetRate = MakeRateBox(res::GetString(L"CvTargetLabel"));
            m_cvTargetRate.SelectedIndex(static_cast<int32_t>(::midicalc::FrameRate::Fps25));
            m_cvTarget = controls::TextBlock{};
            m_cvTarget.IsTextSelectionEnabled(true);
            winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(m_cvTarget, res::GetString(L"CvTargetName"));
            // measured: next to a list box, rather than a text box, the text sits 4 DIP lower
            auto target = ui::MakeInlineText(m_cvTarget, true);
            target.Padding(xaml::Thickness{ 10, 8, 10, 3 });
            card.Body.Children().Append(ui::MakeFieldRow({ m_cvTargetRate, target }));

            auto update = [weak = get_weak()](auto&&, auto&&) { if (auto s = weak.get()) s->UpdateTimecodeConvert(); };
            m_cvTimecode.TextChanged(update);
            m_cvRate.SelectionChanged(update);
            m_cvTargetRate.SelectionChanged(update);

            columns->Add(card, 1);
        }

        // ---- samples and buffers (right) ----
        {
            auto card = ui::MakeCard(L"", L"\uE7F6");
            m_samplesTitle = card.Title;

            m_samples = ui::MakeTextBox(res::GetString(L"SmSamplesLabel"), winrt::hstring{ text::Whole(1536000) }, 160);
            m_samplesResult = controls::TextBlock{};
            m_samplesResult.FontSize(13);
            m_samplesResult.IsTextSelectionEnabled(true);
            card.Body.Children().Append(ui::MakeFieldRow({ m_samples, ui::MakeInlineText(m_samplesResult, true) }));

            controls::Grid table{};

            for (int32_t i = 0; i < 7; ++i)
            {
                controls::ColumnDefinition column{};
                column.Width(xaml::GridLengthHelper::FromValueAndType(i == 0 ? 1.3 : 1.0, xaml::GridUnitType::Star));
                table.ColumnDefinitions().Append(column);
            }

            for (int32_t i = 0; i < 3; ++i)
            {
                controls::RowDefinition row{};
                row.Height(xaml::GridLengthHelper::FromPixels(i == 0 ? 30 : 28));
                table.RowDefinitions().Append(row);
            }

            // the session's sample rate, lit
            controls::Border highlight{};
            {
                auto accent = winrt::unbox_value<winrt::Windows::UI::Color>(xaml::Application::Current().Resources().Lookup(winrt::box_value(L"SystemAccentColorLight2")));
                accent.A = 0x24;
                highlight.Background(media::SolidColorBrush{ accent });
            }
            controls::Grid::SetRow(highlight, 1);
            controls::Grid::SetColumnSpan(highlight, 7);
            table.Children().Append(highlight);

            for (int32_t row = 0; row < 3; ++row)
            {
                controls::Border line{};
                line.BorderBrush(Brush(L"DividerStrokeColorDefaultBrush"));
                line.BorderThickness(xaml::Thickness{ 0, 0, 0, 1 });
                controls::Grid::SetRow(line, row);
                controls::Grid::SetColumnSpan(line, 7);
                table.Children().Append(line);
            }

            for (int32_t column = 0; column < 7; ++column)
            {
                controls::TextBlock heading{};
                heading.Text(column == 0 ? res::GetString(L"SmBufferHeading") : winrt::hstring{ text::Whole(BufferSizes[column - 1]) });
                heading.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
                heading.FontSize(12);
                heading.Foreground(Brush(L"TextFillColorSecondaryBrush"));
                heading.Margin(xaml::Thickness{ 10, 0, 10, 0 });
                heading.VerticalAlignment(xaml::VerticalAlignment::Center);
                heading.HorizontalAlignment(column == 0 ? xaml::HorizontalAlignment::Left : xaml::HorizontalAlignment::Right);
                controls::Grid::SetColumn(heading, column);
                table.Children().Append(heading);
            }

            for (int32_t row = 0; row < 2; ++row)
            {
                controls::TextBlock name{};
                name.FontSize(13);
                name.Margin(xaml::Thickness{ 10, 0, 0, 0 });
                name.VerticalAlignment(xaml::VerticalAlignment::Center);
                name.Foreground(row == 0 ? Brush(L"AccentTextFillColorPrimaryBrush") : Brush(L"TextFillColorSecondaryBrush"));
                controls::Grid::SetRow(name, row + 1);
                table.Children().Append(name);
                m_bufferRates[row] = name;

                for (int32_t column = 0; column < 6; ++column)
                {
                    controls::TextBlock cell{};
                    cell.FontSize(13);
                    cell.Margin(xaml::Thickness{ 0, 0, 10, 0 });
                    cell.IsTextSelectionEnabled(true);
                    cell.VerticalAlignment(xaml::VerticalAlignment::Center);
                    cell.HorizontalAlignment(xaml::HorizontalAlignment::Right);
                    controls::Grid::SetRow(cell, row + 1);
                    controls::Grid::SetColumn(cell, column + 1);
                    table.Children().Append(cell);
                    m_bufferCells[row][column] = cell;
                }
            }

            card.Body.Children().Append(table);
            card.Body.Children().Append(ui::MakeHelp(res::GetString(L"SmHelp")));

            m_samples.TextChanged([weak = get_weak()](auto&&, auto&&) { if (auto s = weak.get()) s->UpdateSamples(); });

            columns->Add(card, 1);
        }

        TimecodePage().Children().Append(columns->Root());
        m_columns.push_back(columns);

        UpdateTimecodePage();
    }

    void MainWindow::UpdateTimecodePage() noexcept
    {
        if (!m_tcMode)
        {
            return;
        }

        UpdateTimecodeCalculator();
        UpdateTimecodeConvert();
        UpdateSamples();
    }

    void MainWindow::UpdateTimecodeCalculator() noexcept
    {
        try
        {
            if (!m_tcMode)
            {
                return;
            }

            auto const rate = RateOf(m_tcRate);
            auto const asFrames = m_tcMode->Selected() == 1;
            auto const op = static_cast<Operator>(m_tcOperator);
            auto const byNumber = op == Operator::Multiply || op == Operator::Divide;

            m_tcB.PlaceholderText(res::GetString(byNumber ? L"TcPlaceholderNumber" : asFrames ? L"TcPlaceholderFrames" : L"TcPlaceholderTimecode"));
            m_tcA.PlaceholderText(res::GetString(asFrames ? L"TcPlaceholderFrames" : L"TcPlaceholderTimecode"));
            winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(m_tcB, res::GetString(byNumber ? L"TcBLabelNumber" : L"TcBLabelTimecode"));

            m_tcAnswer.reset();
            m_tcQuestion = L"";
            m_tcAnswerText = L"";

            auto fail = [&](winrt::hstring const& why)
                {
                    ui::SetBigText(m_tcResult.Value, L"\u2014", 26.0);
                    m_tcResult.Detail.Text(why);
                };

            auto explain = [&](bool skipped, std::wstring_view typed, wchar_t const* need) -> winrt::hstring
                {
                    if (skipped)
                    {
                        return res::FormatString(L"TcSkipped", typed, std::wstring_view{ RateShortName(rate) }, rate == ::midicalc::FrameRate::Fps5994Drop ? 4 : 2);
                    }

                    return res::GetString(asFrames ? L"TcNeedFrames" : need);
                };

            auto const longest = ::midicalc::TimecodeToFrames(
                { false, ::midicalc::MaximumTimecodeHours, 59, 59, static_cast<uint32_t>(::midicalc::NominalFramesPerSecond(rate) - 1) }, rate);

            bool skipped{ false };
            auto const aHeld = m_tcA.Text();
            auto const aText = Trim(aHeld);
            auto const a = ParseValue(aText, rate, asFrames, skipped);

            if (!a.has_value())
            {
                fail(explain(skipped, aText, L"TcNeedA"));
                return;
            }

            if (*a > longest || *a < -longest)
            {
                fail(res::GetString(L"TcTooLong"));
                return;
            }

            auto const bHeld = m_tcB.Text();
            auto const bText = Trim(bHeld);
            int64_t result{ 0 };
            std::wstring bShown{};

            if (byNumber)
            {
                auto const number = ParseNumber(bText);

                if (!number.has_value())
                {
                    fail(res::GetString(L"TcNeedNumber"));
                    return;
                }

                if (op == Operator::Divide && *number == 0.0)
                {
                    fail(res::GetString(L"TcDivideByZero"));
                    return;
                }

                auto const exact = op == Operator::Multiply ? static_cast<double>(*a) * *number : static_cast<double>(*a) / *number;

                if (!std::isfinite(exact) || std::abs(exact) > 9.0e15)
                {
                    fail(res::GetString(L"TcTooLong"));
                    return;
                }

                result = std::llround(exact);
                bShown = text::Number(*number, 6);
            }
            else
            {
                auto const b = ParseValue(bText, rate, asFrames, skipped);

                if (!b.has_value())
                {
                    fail(explain(skipped, bText, L"TcNeedB"));
                    return;
                }

                if (*b > longest || *b < -longest)
                {
                    fail(res::GetString(L"TcTooLong"));
                    return;
                }

                result = op == Operator::Add ? *a + *b : *a - *b;
                bShown = FormatValue(*b, rate, asFrames);
            }

            if (result > longest || result < -longest)
            {
                fail(res::GetString(L"TcTooLong"));
                return;
            }

            wchar_t const* const symbols[4]{ L"TcOpAddText", L"TcOpSubtractText", L"TcOpMultiplyText", L"TcOpDivideText" };
            auto const timecode = FormatValue(result, rate, false);
            auto const seconds = ::midicalc::FramesToSeconds(result, rate);

            m_tcAnswer = result;
            m_tcAnswerText = FormatValue(result, rate, asFrames);
            m_tcQuestion = res::FormatString(L"TcQuestion",
                FormatValue(*a, rate, asFrames), std::wstring_view{ res::GetString(symbols[m_tcOperator]) }, bShown, std::wstring_view{ RateShortName(rate) });

            if (asFrames)
            {
                // the number large and "frames" small, like a unit
                ui::SetBigText(m_tcResult.Value, text::SignedWhole(result), 26.0);
                winrt::Microsoft::UI::Xaml::Documents::Run unit{};
                unit.Text(L"\u2009" + std::wstring{ res::GetString(L"TcFramesUnit") });
                unit.FontSize(14);
                unit.FontWeight(winrt::Windows::UI::Text::FontWeights::Normal());
                unit.FontFamily(xaml::Application::Current().Resources().Lookup(winrt::box_value(L"ContentControlThemeFontFamily")).as<media::FontFamily>());
                unit.Foreground(Brush(L"TextFillColorSecondaryBrush"));
                m_tcResult.Value.Inlines().Append(unit);
                m_tcResult.Detail.Text(res::FormatString(L"TcDetailFrames", timecode, RealTime(seconds)));
            }
            else
            {
                ui::SetBigText(m_tcResult.Value, timecode, 26.0);
                m_tcResult.Detail.Text(res::FormatString(L"TcDetailTimecode", text::SignedWhole(result), RealTime(seconds)));
            }
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to work out the timecode calculator.")
    }

    _Use_decl_annotations_
    void MainWindow::AddToTape(winrt::hstring const& question, winrt::hstring const& answer) noexcept
    {
        try
        {
            if (question.empty() || !m_tcTape)
            {
                return;
            }

            // the same sum twice in a row is one entry
            if (m_tcTape.Children().Size() > 0)
            {
                if (auto const tag = m_tcTape.Children().GetAt(0).as<xaml::FrameworkElement>().Tag(); winrt::unbox_value_or<winrt::hstring>(tag, L"") == question)
                {
                    return;
                }
            }

            controls::Border entry{};
            entry.Tag(winrt::box_value(question));
            entry.Padding(xaml::Thickness{ 10, 6, 4, 6 });
            entry.BorderBrush(Brush(L"DividerStrokeColorDefaultBrush"));
            entry.BorderThickness(xaml::Thickness{ 0, 0, 0, 1 });

            controls::Grid grid{};
            {
                controls::ColumnDefinition words{};
                words.Width(xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
                controls::ColumnDefinition button{};
                button.Width(xaml::GridLengthHelper::Auto());
                grid.ColumnDefinitions().Append(words);
                grid.ColumnDefinitions().Append(button);
            }

            controls::StackPanel words{};
            words.Spacing(2);

            controls::TextBlock asked{};
            asked.Text(question);
            asked.FontSize(12);
            asked.Foreground(Brush(L"TextFillColorTertiaryBrush"));
            asked.TextWrapping(xaml::TextWrapping::Wrap);

            controls::TextBlock answered{};
            answered.Text(answer);
            answered.FontSize(13);
            answered.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
            answered.IsTextSelectionEnabled(true);

            words.Children().Append(asked);
            words.Children().Append(answered);
            grid.Children().Append(words);

            controls::Button copyButton{};
            copyButton.Background(media::SolidColorBrush{ winrt::Microsoft::UI::Colors::Transparent() });
            copyButton.BorderThickness(xaml::Thickness{ 0 });
            copyButton.Padding(xaml::Thickness{ 8, 6, 8, 6 });
            copyButton.VerticalAlignment(xaml::VerticalAlignment::Center);
            {
                controls::FontIcon icon{};
                icon.Glyph(L"\uE8C8");
                icon.FontSize(12);
                icon.Foreground(Brush(L"TextFillColorTertiaryBrush"));
                copyButton.Content(icon);
            }
            auto const name = res::FormatString(L"CopyNameFormat", std::wstring_view{ question });
            winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(copyButton, name);
            controls::ToolTipService::SetToolTip(copyButton, winrt::box_value(name));

            auto const page = res::GetString(L"PageTimecode");
            copyButton.Click([weak = get_weak(), page, question, answer](auto&&, auto&&)
                {
                    if (auto s = weak.get())
                    {
                        s->CopyAnswer(page, question, answer);
                    }
                });
            controls::Grid::SetColumn(copyButton, 1);
            grid.Children().Append(copyButton);

            entry.Child(grid);
            m_tcTape.Children().InsertAt(0, entry);

            while (m_tcTape.Children().Size() > TapeLength)
            {
                m_tcTape.Children().RemoveAtEnd();
            }
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to add to the timecode tape.")
    }

    void MainWindow::UpdateTimecodeConvert() noexcept
    {
        try
        {
            if (!m_cvTimecode)
            {
                return;
            }

            auto const& settings = native::AppSettings::Current();
            auto const rate = RateOf(m_cvRate);
            auto const target = RateOf(m_cvTargetRate);
            auto const sampleRate = settings.SampleRate();
            auto const tempo = settings.Tempo();
            auto const meter = settings.Meter();

            m_cvSamplesLabel.Text(res::FormatString(L"CvSamplesFormat", KiloHertz(sampleRate)));
            m_cvBarsLabel.Text(res::FormatString(L"CvBarsFormat", text::Number(tempo, 3), MeterText(meter)));

            bool skipped{ false };
            auto const held = m_cvTimecode.Text();
            auto const typed = Trim(held);
            auto const frames = ParseValue(typed, rate, false, skipped);

            m_cvTarget.Inlines().Clear();

            if (!frames.has_value())
            {
                m_cvFrames.Text(L"\u2014");
                m_cvRealTime.Text(L"\u2014");
                m_cvSamples.Text(L"\u2014");
                m_cvBars.Text(L"\u2014");
                m_cvTarget.Text(L"\u2014");
                return;
            }

            auto const seconds = ::midicalc::FramesToSeconds(*frames, rate);
            auto const samples = seconds * sampleRate;

            m_cvFrames.Text(text::SignedWhole(*frames));
            m_cvRealTime.Text(RealTime(seconds));
            // whole samples, counted toward zero, so a sample is never claimed before it starts
            m_cvSamples.Text(text::SignedWhole(static_cast<int64_t>(std::trunc(samples))));

            if (auto const position = ::midicalc::BarPositionAt(seconds, tempo, meter.Numerator, meter.Denominator); position.has_value())
            {
                m_cvBars.Text(res::FormatString(L"CvBarsValue",
                    text::Whole(position->Bar), position->Beat, text::Number(std::floor(position->BeatFraction * 100.0) / 100.0, 2, true)));
            }
            else
            {
                m_cvBars.Text(L"\u2014");
            }

            auto const moment = ::midicalc::ConvertFrames(*frames, rate, target);

            winrt::Microsoft::UI::Xaml::Documents::Run timecode{};
            timecode.Text(FormatValue(moment.Frames, target, false));
            timecode.FontFamily(media::FontFamily{ MonospaceFont });
            timecode.FontSize(14);
            m_cvTarget.Inlines().Append(timecode);

            winrt::Microsoft::UI::Xaml::Documents::Run leftover{};
            leftover.Text(L"  " + (moment.Leftover < 0.005
                ? std::wstring{ res::GetString(L"CvExact") }
                : std::wstring{ res::FormatString(L"CvLeftover", text::Signed(moment.Leftover, 2)) }));
            leftover.FontSize(12);
            leftover.Foreground(Brush(L"TextFillColorTertiaryBrush"));
            m_cvTarget.Inlines().Append(leftover);
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to convert the timecode.")
    }

    void MainWindow::UpdateSamples() noexcept
    {
        try
        {
            if (!m_samples)
            {
                return;
            }

            auto const& settings = native::AppSettings::Current();
            auto const sampleRate = settings.SampleRate();
            auto const tempo = settings.Tempo();

            m_samplesTitle.Text(res::FormatString(L"SmTitleFormat", KiloHertz(sampleRate)));

            if (auto const samples = text::ParseUnsigned(m_samples.Text()); samples.has_value())
            {
                auto const seconds = static_cast<double>(*samples) / sampleRate;
                auto const bars = ::midicalc::BarsInLength(seconds, tempo, settings.Meter());

                m_samplesResult.Text(res::FormatString(L"SmResultFormat",
                    text::Number(seconds, 3, true), text::Number(bars.value_or(0.0), 2), text::Number(tempo, 3)));
            }
            else
            {
                m_samplesResult.Text(res::GetString(L"SmNeed"));
            }

            // the session's rate, then the other common one
            double const rates[2]{ sampleRate, std::abs(sampleRate - 44100.0) < 0.5 ? 48000.0 : 44100.0 };

            for (size_t row = 0; row < 2; ++row)
            {
                m_bufferRates[row].Text(res::FormatString(L"SmRateFormat", KiloHertz(rates[row])));

                for (size_t column = 0; column < 6; ++column)
                {
                    m_bufferCells[row][column].Text(text::Number(::midicalc::BufferMilliseconds(BufferSizes[column], rates[row]), 2, true));
                }
            }
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to work out the samples and buffers.")
    }
}
