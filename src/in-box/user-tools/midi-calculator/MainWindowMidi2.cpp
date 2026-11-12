// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// MIDI 2.0 values: note pitch (Pitch 7.9 and 7.25), pitch bend, value scaling, UMP timing,
// a UMP decoder and manufacturer IDs. Laid out as
// src\prototypes\midi-calculator\design\5-midi-2-values.html. The rules and their spec sections
// are in CalcMidi2.h.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "CalcMidi2.h"
#include "CalcPitch.h"
#include "CalcText.h"
#include "StringResources.h"
#include "UmpDescriber.h"

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

        constexpr uint8_t ScaleBits[4]{ 7, 14, 16, 32 };

        std::wstring Bytes(std::initializer_list<uint8_t> bytes)
        {
            std::wstring result{};

            for (auto const b : bytes)
            {
                if (!result.empty())
                {
                    result += L' ';
                }

                result += std::format(L"{:02X}", b);
            }

            return result;
        }

        std::wstring Binary(uint32_t value, int32_t bits)
        {
            std::wstring result{};

            for (int32_t i = bits - 1; i >= 0; --i)
            {
                result.push_back(((value >> i) & 1) ? L'1' : L'0');
            }

            return result;
        }

        media::Brush Brush(wchar_t const* key)
        {
            return xaml::Application::Current().Resources().Lookup(winrt::box_value(key)).as<media::Brush>();
        }

        // The MIDI 1.0 bytes for a MIDI 1.0 or MIDI 2.0 channel voice message, the way the
        // service translates them (M2-115 scaling down). Empty for anything else.
        std::wstring Midi1Equivalent(std::vector<uint32_t> const& words)
        {
            auto const type = (words[0] >> 28) & 0xF;
            auto const status = static_cast<uint8_t>((words[0] >> 16) & 0xFF);
            auto const opcode = (status >> 4) & 0xF;
            auto const index = static_cast<uint8_t>((words[0] >> 8) & 0x7F);

            if (type == 0x2)
            {
                if (opcode == 0xC || opcode == 0xD)
                {
                    return Bytes({ status, index });
                }

                return Bytes({ status, index, static_cast<uint8_t>(words[0] & 0x7F) });
            }

            if (type != 0x4 || words.size() < 2)
            {
                return {};
            }

            auto const data = words[1];

            switch (opcode)
            {
            case 0x8:
            case 0x9:
                return Bytes({ status, index, static_cast<uint8_t>(::midicalc::ScaleDownMinCenterMax(data >> 16, 16, 7)) });
            case 0xA:
            case 0xB:
                return Bytes({ status, index, static_cast<uint8_t>(::midicalc::ScaleDownMinCenterMax(data, 32, 7)) });
            case 0xC:
                return Bytes({ status, static_cast<uint8_t>((data >> 24) & 0x7F) });
            case 0xD:
                return Bytes({ status, static_cast<uint8_t>(::midicalc::ScaleDownMinCenterMax(data, 32, 7)) });
            case 0xE:
            {
                auto const bend = ::midicalc::ScaleDownMinCenterMax(data, 32, 14);
                return Bytes({ status, static_cast<uint8_t>(bend & 0x7F), static_cast<uint8_t>((bend >> 7) & 0x7F) });
            }
            default:
                return {};
            }
        }
    }

    void MainWindow::BuildMidi2Page()
    {
        auto const page = res::GetString(L"PageMidi2");
        auto const copy = CopyFor(page);
        auto columns = std::make_shared<ui::CardColumns>();

        // ---- note pitch (left) ----
        {
            auto card = ui::MakeCard(res::GetString(L"M2PitchTitle"), L"\uE8D6");

            m_m2Note = ui::MakeTextBox(res::GetString(L"M2NoteLabel"), winrt::hstring{ ::midicalc::NoteNameWithNumber(69, native::AppSettings::Current().MiddleCOctave()) }, 120);
            m_m2Cents = ui::MakeNumberBox(res::GetString(L"M2CentsLabel"), 38.906, -100, 100, 120);
            m_m2Hertz = ui::MakeNumberBox(res::GetString(L"M2HertzLabel"), std::numeric_limits<double>::quiet_NaN(), 1, 20000, 120);
            card.Body.Children().Append(ui::MakeFieldRow({ m_m2Note, m_m2Cents, m_m2Hertz }));

            auto diagramLabel = ui::MakeHelp(res::GetString(L"M2BitsLabel"));
            card.Body.Children().Append(diagramLabel);
            m_m2Bits = ui::MakeBitDiagram();
            card.Body.Children().Append(m_m2Bits);

            ui::ResultList results{};
            m_m2Pitch79 = results.Add(res::GetString(L"M2Pitch79"), copy, true);
            m_m2SoundsAs = results.Add(res::GetString(L"M2SoundsAs"), copy);
            m_m2Step = results.Add(res::GetString(L"M2Step"), copy);
            m_m2Pitch725 = results.Add(res::GetString(L"M2Pitch725"), copy, true);
            m_m2Widened = results.Add(res::GetString(L"M2Widened"), copy, true);
            card.Body.Children().Append(results.Root());

            m_m2Read = ui::MakeTextBox(res::GetString(L"M2ReadLabel"), L"", 200);
            m_m2Read.PlaceholderText(res::GetString(L"M2ReadPlaceholder"));
            ui::ResultList read{};
            m_m2ReadResult = read.Add(res::GetString(L"M2ReadResult"), copy);
            card.Body.Children().Append(m_m2Read);
            card.Body.Children().Append(read.Root());

            auto update = [weak = get_weak()](auto&&, auto&&) { if (auto s = weak.get()) s->UpdateNotePitch(); };
            m_m2Note.TextChanged(update);
            m_m2Cents.ValueChanged(update);
            m_m2Read.TextChanged(update);

            // typing a frequency fills in the note and cents
            m_m2Hertz.ValueChanged([weak = get_weak()](controls::NumberBox const& sender, auto&&)
                {
                    auto s = weak.get();

                    if (!s || s->m_suppressNotePitch || std::isnan(sender.Value()))
                    {
                        return;
                    }

                    auto const nearest = ::midicalc::NearestNoteToFrequency(sender.Value(), ::midicalc::Tuning{ native::AppSettings::Current().ReferenceHertz() });

                    if (nearest.has_value())
                    {
                        s->m_suppressNotePitch = true;
                        s->m_m2Note.Text(winrt::hstring{ ::midicalc::NoteNameWithNumber(nearest->Note, native::AppSettings::Current().MiddleCOctave()) });
                        s->m_m2Cents.Value(std::round(nearest->Cents * 1000.0) / 1000.0);
                        s->m_suppressNotePitch = false;
                        s->UpdateNotePitch();
                    }
                });

            columns->Add(card, 0);
        }

        // ---- pitch bend (right) ----
        {
            auto card = ui::MakeCard(res::GetString(L"M2BendTitle"), L"\uE8AB");

            m_bendRange = ui::MakeNumberBox(res::GetString(L"M2BendRangeLabel"), 2, 0, 127, 190);
            m_bendRangeCents = ui::MakeNumberBox(res::GetString(L"M2BendRangeCentsLabel"), 0, 0, 99, 90);
            m_bendCents = ui::MakeNumberBox(res::GetString(L"M2BendByLabel"), 38.906, -12700, 12700, 130);
            card.Body.Children().Append(ui::MakeFieldRow({ m_bendRange, m_bendRangeCents, m_bendCents }));

            ui::ResultList results{};
            m_bend32 = results.Add(res::GetString(L"M2Bend32"), copy, true);
            m_bend14 = results.Add(res::GetString(L"M2Bend14"), copy, true);
            m_bend14In32 = results.Add(res::GetString(L"M2Bend14In32"), copy, true);
            m_bendRpn2 = results.Add(res::GetString(L"M2BendRpn2"), copy, true);
            m_bendRpn1 = results.Add(res::GetString(L"M2BendRpn1"), copy, true);
            card.Body.Children().Append(results.Root());
            card.Body.Children().Append(ui::MakeHelp(res::GetString(L"M2BendHelp")));

            auto update = [weak = get_weak()](auto&&, auto&&) { if (auto s = weak.get()) s->UpdatePitchBend(); };
            m_bendRange.ValueChanged(update);
            m_bendRangeCents.ValueChanged(update);
            m_bendCents.ValueChanged(update);

            columns->Add(card, 1);
        }

        // ---- value scaling (left) ----
        {
            auto card = ui::MakeCard(res::GetString(L"M2ScaleTitle"), L"\uE9E9");

            m_scaleValue = ui::MakeTextBox(res::GetString(L"M2ScaleValueLabel"), L"100", 110);
            m_scaleBits = ui::MakeComboBox(res::GetString(L"M2ScaleBitsLabel"), { L"7", L"14", L"16", L"32" }, 0, 90);
            m_scaleMethod = ui::MakeComboBox(res::GetString(L"M2ScaleMethodLabel"),
                { res::GetString(L"M2ScaleMinCenterMax"), res::GetString(L"M2ScaleZeroExtension") }, 0, 200);
            card.Body.Children().Append(ui::MakeFieldRow({ m_scaleValue, m_scaleBits, m_scaleMethod }));

            // Bits | Decimal | Hex | Of full scale, the bits typed in lit
            controls::Grid table{};
            winrt::hstring const headings[4]{ res::GetString(L"M2ScaleBitsHeading"), res::GetString(L"M2ScaleDecimalHeading"), res::GetString(L"M2ScaleHexHeading"), res::GetString(L"M2ScalePercentHeading") };
            winrt::hstring const rows[4]{ res::GetString(L"M2Scale7"), res::GetString(L"M2Scale14"), res::GetString(L"M2Scale16"), res::GetString(L"M2Scale32") };

            for (int i = 0; i < 4; ++i)
            {
                controls::ColumnDefinition column{};
                column.Width(xaml::GridLengthHelper::FromValueAndType(i == 0 ? 1.7 : 1.0, xaml::GridUnitType::Star));
                table.ColumnDefinitions().Append(column);
            }

            for (int i = 0; i < 5; ++i)
            {
                controls::RowDefinition row{};
                row.Height(xaml::GridLengthHelper::Auto());
                table.RowDefinitions().Append(row);
            }

            for (int row = 0; row < 4; ++row)
            {
                m_scaleHighlights[row] = controls::Border{};
                m_scaleHighlights[row].CornerRadius(xaml::CornerRadius{ 4 });
                controls::Grid::SetRow(m_scaleHighlights[row], row + 1);
                controls::Grid::SetColumnSpan(m_scaleHighlights[row], 4);
                table.Children().Append(m_scaleHighlights[row]);
            }

            for (int column = 0; column < 4; ++column)
            {
                controls::TextBlock heading{};
                heading.Text(headings[column]);
                heading.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
                heading.Margin(xaml::Thickness{ 8, 0, 8, 6 });
                heading.HorizontalAlignment(column == 0 ? xaml::HorizontalAlignment::Left : xaml::HorizontalAlignment::Right);
                controls::Grid::SetColumn(heading, column);
                table.Children().Append(heading);
            }

            for (int row = 0; row < 4; ++row)
            {
                controls::TextBlock name{};
                name.Text(rows[row]);
                name.Margin(xaml::Thickness{ 8, 5, 8, 5 });
                name.TextWrapping(xaml::TextWrapping::Wrap);
                controls::Grid::SetRow(name, row + 1);
                table.Children().Append(name);

                for (int column = 0; column < 3; ++column)
                {
                    controls::TextBlock cell{};
                    cell.Margin(xaml::Thickness{ 8, 5, 8, 5 });
                    cell.HorizontalAlignment(xaml::HorizontalAlignment::Right);
                    cell.VerticalAlignment(xaml::VerticalAlignment::Center);
                    cell.IsTextSelectionEnabled(true);

                    if (column == 1)
                    {
                        cell.FontFamily(media::FontFamily{ L"Cascadia Mono, Consolas" });
                    }

                    controls::Grid::SetRow(cell, row + 1);
                    controls::Grid::SetColumn(cell, column + 1);
                    table.Children().Append(cell);
                    m_scaleCells[row][column] = cell;
                }
            }

            card.Body.Children().Append(table);
            card.Body.Children().Append(ui::MakeHelp(res::GetString(L"M2ScaleHelp")));

            auto update = [weak = get_weak()](auto&&, auto&&) { if (auto s = weak.get()) s->UpdateScaling(); };
            m_scaleValue.TextChanged(update);
            m_scaleBits.SelectionChanged(update);
            m_scaleMethod.SelectionChanged(update);

            columns->Add(card, 0);
        }

        // ---- UMP timing (right) ----
        {
            auto card = ui::MakeCard(res::GetString(L"M2TimingTitle"), L"\uE916");

            m_jrTicks = ui::MakeTextBox(res::GetString(L"M2JrLabel"), L"0x1234", 140);
            m_jrResult = controls::TextBlock{};
            m_jrResult.IsTextSelectionEnabled(true);
            card.Body.Children().Append(ui::MakeFieldRow({ m_jrTicks, ui::MakeInlineText(m_jrResult, true) }));

            ui::ResultList jr{};
            jr.Add(res::GetString(L"M2JrTick"), copy).Text(res::GetString(L"M2JrTickValue"));
            jr.Add(res::GetString(L"M2JrWrap"), copy).Text(res::GetString(L"M2JrWrapValue"));
            card.Body.Children().Append(jr.Root());

            m_dctpq = ui::MakeNumberBox(res::GetString(L"M2DctpqLabel"), 960, 1, 65535, 230);
            m_dcTicks = ui::MakeNumberBox(res::GetString(L"M2DcTicksLabel"), 480, 0, 1048575, 110);
            card.Body.Children().Append(ui::MakeFieldRow({ m_dctpq, m_dcTicks }));

            ui::ResultList dc{};
            m_dcResult = dc.Add(L"", copy);
            m_dcResultLabel = dc.LastLabel();
            card.Body.Children().Append(dc.Root());

            auto update = [weak = get_weak()](auto&&, auto&&) { if (auto s = weak.get()) s->UpdateUmpTiming(); };
            m_jrTicks.TextChanged(update);
            m_dctpq.ValueChanged(update);
            m_dcTicks.ValueChanged(update);

            columns->Add(card, 1);
        }

        // ---- decode UMP words, full width ----
        {
            auto card = ui::MakeCard(res::GetString(L"M2DecodeTitle"), L"\uE943");

            m_umpWords = ui::MakeTextBox(res::GetString(L"M2WordsLabel"), L"40904503 C9248AC7", 520);

            auto paste = ui::MakeButton(res::GetString(L"M2Paste"), L"\uE77F", false);
            paste.Click([weak = get_weak()](auto&&, auto&&) -> winrt::fire_and_forget
                {
                    try
                    {
                        auto const content = winrt::Windows::ApplicationModel::DataTransfer::Clipboard::GetContent();

                        if (content.Contains(winrt::Windows::ApplicationModel::DataTransfer::StandardDataFormats::Text()))
                        {
                            auto const pasted = co_await content.GetTextAsync();

                            if (auto s = weak.get())
                            {
                                s->m_umpWords.Text(pasted);
                            }
                        }
                    }
                    catch (...)
                    {
                        LOG_CAUGHT_EXCEPTION();
                    }
                });

            card.Body.Children().Append(ui::MakeFieldRow({ m_umpWords, paste }));

            m_umpBits = ui::MakeBitDiagram();
            card.Body.Children().Append(m_umpBits);

            m_umpName = controls::TextBlock{};
            m_umpName.IsTextSelectionEnabled(true);
            m_umpName.TextWrapping(xaml::TextWrapping::Wrap);
            winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetLiveSetting(m_umpName, winrt::Microsoft::UI::Xaml::Automation::Peers::AutomationLiveSetting::Polite);
            m_umpDetail = ui::MakeHelp(L"");
            m_umpDetail.IsTextSelectionEnabled(true);
            card.Body.Children().Append(m_umpName);
            card.Body.Children().Append(m_umpDetail);

            ui::ResultList extra{};
            m_umpMidi1 = extra.Add(res::GetString(L"M2AsMidi1"), copy, true);
            card.Body.Children().Append(extra.Root());

            m_mfrId = ui::MakeTextBox(res::GetString(L"M2MfrLabel"), L"00 20 6B", 180);
            m_mfrResult = controls::TextBlock{};
            m_mfrResult.IsTextSelectionEnabled(true);
            card.Body.Children().Append(ui::MakeFieldRow({ m_mfrId, ui::MakeInlineText(m_mfrResult, false) }));

            auto update = [weak = get_weak()](auto&&, auto&&) { if (auto s = weak.get()) s->UpdateDecoder(); };
            m_umpWords.TextChanged(update);
            m_mfrId.TextChanged(update);

            columns->Add(card, 0, true);
        }

        Midi2Page().Children().Append(columns->Root());
        m_columns.push_back(columns);

        UpdateMidi2Page();
    }

    void MainWindow::UpdateMidi2Page() noexcept
    {
        if (!m_m2Note)
        {
            return;
        }

        UpdateNotePitch();
        UpdatePitchBend();
        UpdateScaling();
        UpdateUmpTiming();
        UpdateDecoder();
    }

    void MainWindow::UpdateNotePitch() noexcept
    {
        try
        {
            if (!m_m2Note || m_suppressNotePitch)
            {
                return;
            }

            auto const& settings = native::AppSettings::Current();
            ::midicalc::Tuning const tuning{ settings.ReferenceHertz() };
            auto const note = ::midicalc::ParseNote(m_m2Note.Text(), settings.MiddleCOctave());

            m_m2Step.Text(res::GetString(L"M2StepValue"));

            if (!note.has_value())
            {
                for (auto const& block : { m_m2Pitch79, m_m2SoundsAs, m_m2Pitch725, m_m2Widened })
                {
                    block.Text(L"\u2014");
                }

                ui::FillBitDiagram(m_m2Bits, {});
            }
            else
            {
                auto const fractional = *note + NumberOr(m_m2Cents, 0) / 100.0;
                auto const p79 = ::midicalc::Pitch79FromNote(fractional);
                auto const p725 = ::midicalc::Pitch725FromNote(fractional);
                auto const sounds = ::midicalc::NoteFromPitch79(p79);
                auto const soundsNote = static_cast<int32_t>(std::floor(sounds));

                ui::FillBitDiagram(m_m2Bits, {
                    { res::GetString(L"M2BitsSemitone"), winrt::hstring{ std::format(L"{} = {}", Binary(p79 >> 9, 7), p79 >> 9) }, 7, 0 },
                    { res::GetString(L"M2BitsFraction"), winrt::hstring{ std::format(L"{} = {}/512", Binary(p79 & 0x1FF, 9), p79 & 0x1FF) }, 9, 1 } });

                m_m2Pitch79.Text(std::format(L"{} \u00B7 {}", text::Hex(p79, 4), text::Whole(p79)));
                m_m2SoundsAs.Text(res::FormatString(L"M2SoundsAsValue",
                    ::midicalc::NoteName(soundsNote, settings.MiddleCOctave(), false),
                    text::Signed((sounds - soundsNote) * 100.0, 3),
                    text::Number(::midicalc::NoteFrequency(sounds, tuning), 3, true)));
                m_m2Pitch725.Text(std::format(L"{} \u00B7 {}", text::Hex(p725, 8), text::Whole(p725)));
                m_m2Widened.Text(winrt::hstring{ text::Hex(::midicalc::ScaleUpZeroExtension(p79, 16, 32), 8) });

                // the frequency box follows the note, once someone has used it
                if (!std::isnan(m_m2Hertz.Value()))
                {
                    m_suppressNotePitch = true;
                    m_m2Hertz.Value(std::round(::midicalc::NoteFrequency(fractional, tuning) * 1000.0) / 1000.0);
                    m_suppressNotePitch = false;
                }
            }

            // reading a value back: 16 bits or less is Pitch 7.9, more is Pitch 7.25
            auto const read = text::ParseHexFirst(m_m2Read.Text());

            if (m_m2Read.Text().empty())
            {
                m_m2ReadResult.Text(L"");
            }
            else if (!read.has_value() || *read > 0xFFFFFFFFull)
            {
                m_m2ReadResult.Text(res::GetString(L"M2ReadInvalid"));
            }
            else
            {
                auto const isShort = *read <= 0xFFFF;
                auto const value = isShort
                    ? ::midicalc::NoteFromPitch79(static_cast<uint16_t>(*read))
                    : ::midicalc::NoteFromPitch725(static_cast<uint32_t>(*read));
                auto const whole = static_cast<int32_t>(std::floor(value));

                m_m2ReadResult.Text(res::FormatString(isShort ? L"M2ReadAs79" : L"M2ReadAs725",
                    ::midicalc::NoteName(whole, settings.MiddleCOctave(), false),
                    text::Signed((value - whole) * 100.0, 4),
                    text::Number(::midicalc::NoteFrequency(value, tuning), 3)));
            }
        }
        catch (...)
        {
            m_suppressNotePitch = false;
            LOG_CAUGHT_EXCEPTION();
        }
    }

    void MainWindow::UpdatePitchBend() noexcept
    {
        try
        {
            if (!m_bendRange)
            {
                return;
            }

            auto const semitones = static_cast<uint8_t>(std::clamp(NumberOr(m_bendRange, 2), 0.0, 127.0));
            auto const rangeCentsPart = static_cast<uint8_t>(std::clamp(NumberOr(m_bendRangeCents, 0), 0.0, 99.0));
            auto const range = semitones * 100.0 + rangeCentsPart;
            auto const cents = NumberOr(m_bendCents, 0);

            if (range <= 0.0)
            {
                for (auto const& block : { m_bend32, m_bend14, m_bend14In32 })
                {
                    block.Text(res::GetString(L"M2BendNoRange"));
                }
            }
            else
            {
                auto const b32 = ::midicalc::PitchBend32FromCents(cents, range);
                auto const b14 = ::midicalc::PitchBend14FromCents(cents, range);
                auto const b14In32 = ::midicalc::ScaleUpMinCenterMax(b14, 14, 32);
                auto const difference = ::midicalc::CentsFromPitchBend32(b14In32, range) - ::midicalc::CentsFromPitchBend32(b32, range);

                m_bend32.Text(winrt::hstring{ text::Hex(b32, 8) });
                m_bend14.Text(res::FormatString(L"M2Bend14Value", text::Whole(b14),
                    Bytes({ 0xE0, static_cast<uint8_t>(b14 & 0x7F), static_cast<uint8_t>((b14 >> 7) & 0x7F) })));
                m_bend14In32.Text(res::FormatString(difference > 0.0 ? L"M2Bend14In32Higher" : L"M2Bend14In32Lower",
                    text::Hex(b14In32, 8), text::Number(std::abs(difference), 3)));
            }

            m_bendRpn2.Text(winrt::hstring{ text::Hex(::midicalc::PitchBendRangeRpnData(semitones, rangeCentsPart), 8) });
            m_bendRpn1.Text(winrt::hstring{ Bytes({ 0xB0, 0x65, 0x00, 0xB0, 0x64, 0x00, 0xB0, 0x06, semitones, 0xB0, 0x26, rangeCentsPart }) });
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to work out the pitch bend.")
    }

    void MainWindow::UpdateScaling() noexcept
    {
        try
        {
            if (!m_scaleValue)
            {
                return;
            }

            auto const sourceIndex = (std::max)(0, m_scaleBits.SelectedIndex());
            auto const bits = ScaleBits[sourceIndex];
            auto const zeroExtension = m_scaleMethod.SelectedIndex() == 1;
            auto const value = text::ParseUnsigned(m_scaleValue.Text());
            auto const lit = Brush(L"AccentFillColorDisabledBrush");

            for (int row = 0; row < 4; ++row)
            {
                m_scaleHighlights[row].Background(row == sourceIndex ? lit : media::SolidColorBrush{ winrt::Microsoft::UI::Colors::Transparent() });
            }

            for (size_t i = 0; i < 4; ++i)
            {
                if (!value.has_value() || *value > ::midicalc::MaximumForBits(bits))
                {
                    for (auto const& cell : m_scaleCells[i])
                    {
                        cell.Text(L"\u2014");
                    }

                    continue;
                }

                auto const target = ScaleBits[i];
                uint32_t scaled{};

                if (target == bits)
                {
                    scaled = static_cast<uint32_t>(*value);
                }
                else if (target > bits)
                {
                    scaled = zeroExtension
                        ? ::midicalc::ScaleUpZeroExtension(static_cast<uint32_t>(*value), bits, target)
                        : ::midicalc::ScaleUpMinCenterMax(static_cast<uint32_t>(*value), bits, target);
                }
                else
                {
                    scaled = zeroExtension
                        ? ::midicalc::ScaleDownZeroExtension(static_cast<uint32_t>(*value), bits, target)
                        : ::midicalc::ScaleDownMinCenterMax(static_cast<uint32_t>(*value), bits, target);
                }

                m_scaleCells[i][0].Text(winrt::hstring{ text::Whole(scaled) });
                m_scaleCells[i][1].Text(winrt::hstring{ text::Hex(scaled, (target + 3) / 4) });
                m_scaleCells[i][2].Text(winrt::hstring{ text::Number(100.0 * scaled / ::midicalc::MaximumForBits(target), 2, true) + L"%" });
            }
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to scale the value.")
    }

    void MainWindow::UpdateUmpTiming() noexcept
    {
        try
        {
            if (!m_jrTicks)
            {
                return;
            }

            auto const jr = text::ParseUnsigned(m_jrTicks.Text());

            if (!jr.has_value() || *jr > 0xFFFF)
            {
                m_jrResult.Text(res::GetString(L"M2JrInvalid"));
            }
            else
            {
                m_jrResult.Text(res::FormatString(L"M2JrValue", text::Whole(*jr), text::Number(::midicalc::JrTicksToMilliseconds(static_cast<uint32_t>(*jr)), 3)));
            }

            auto const tempo = native::AppSettings::Current().Tempo();
            auto const tpq = static_cast<uint32_t>(NumberOr(m_dctpq, 960));
            auto const dc = ::midicalc::DeltaClockstampMilliseconds(static_cast<uint32_t>(NumberOr(m_dcTicks, 0)), tpq, tempo);
            auto const perTick = ::midicalc::DeltaClockstampMilliseconds(1, tpq, tempo);

            m_dcResultLabel.Text(res::FormatString(L"M2DcAtTempo", text::Number(tempo, 3)));
            m_dcResult.Text(dc.has_value() && perTick.has_value()
                ? res::FormatString(L"M2DcValue", text::Number(*dc, 3), text::Number(*perTick, 3))
                : winrt::hstring{ L"\u2014" });
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to work out the UMP timing.")
    }

    void MainWindow::UpdateDecoder() noexcept
    {
        try
        {
            if (!m_umpWords)
            {
                return;
            }

            auto const words = text::ParseWords(m_umpWords.Text());
            m_umpMidi1.Text(L"\u2014");

            if (words.empty())
            {
                m_umpName.Text(m_umpWords.Text().empty() ? L"" : res::GetString(L"M2WordsInvalid"));
                m_umpDetail.Text(L"");
                ui::FillBitDiagram(m_umpBits, {});
            }
            else
            {
                // A UMP is as many words as its message type says; the decoder takes one message.
                static constexpr uint8_t WordsForType[16]{ 1, 1, 1, 2, 2, 4, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4 };
                auto const type = (words[0] >> 28) & 0xF;
                auto const needed = WordsForType[type];

                if (words.size() < needed)
                {
                    m_umpName.Text(res::FormatString(L"M2WordsTooFew", needed));
                    m_umpDetail.Text(L"");
                    ui::FillBitDiagram(m_umpBits, {});
                }
                else
                {
                    std::array<uint32_t, 4> message{};
                    std::copy_n(words.begin(), needed, message.begin());

                    midiapp::UmpDescriptionLabels labels{};
                    labels.Note = res::GetString(L"UmpLabelNote");
                    labels.Velocity = res::GetString(L"UmpLabelVelocity");
                    labels.Pressure = res::GetString(L"UmpLabelPressure");
                    labels.Controller = res::GetString(L"UmpLabelController");
                    labels.Value = res::GetString(L"UmpLabelValue");
                    labels.Program = res::GetString(L"UmpLabelProgram");
                    labels.Bank = res::GetString(L"UmpLabelBank");
                    labels.Bend = res::GetString(L"UmpLabelBend");
                    labels.Index = res::GetString(L"UmpLabelIndex");
                    labels.Bytes = res::GetString(L"UmpLabelBytes");
                    labels.Status = res::GetString(L"UmpLabelStatus");
                    labels.Song = res::GetString(L"UmpLabelSong");
                    labels.Position = res::GetString(L"UmpLabelPosition");
                    labels.Frame = res::GetString(L"UmpLabelFrame");
                    labels.Attribute = res::GetString(L"UmpLabelAttribute");
                    labels.Text = res::GetString(L"UmpLabelText");
                    labels.Name = res::GetString(L"UmpLabelName");
                    labels.Ticks = res::GetString(L"UmpLabelTicks");
                    labels.Tempo = res::GetString(L"UmpLabelTempo");
                    labels.StreamId = res::GetString(L"UmpLabelStreamId");
                    labels.FunctionBlock = res::GetString(L"UmpLabelFunctionBlock");
                    labels.Form = res::GetString(L"UmpLabelForm");
                    labels.TimeSignature = res::GetString(L"UmpLabelTimeSignature");
                    labels.KeySignature = res::GetString(L"UmpLabelKeySignature");
                    labels.PerNote = res::GetString(L"UmpLabelPerNote");
                    labels.Options = res::GetString(L"UmpLabelOptions");
                    labels.NoteOffValue = res::GetString(L"UmpLabelNoteOffValue");
                    labels.TimeSignatureNonStandardFormat = res::GetString(L"UmpLabelTimeSignatureNonStandardFormat");

                    auto const description = midiapp::DescribeUmp(message, needed, labels);
                    auto const group = (words[0] >> 24) & 0xF;
                    auto const middleC = native::AppSettings::Current().MiddleCOctave();

                    std::wstring name{ midiapp::UmpDisplayName(words[0]) };

                    // channel voice, MIDI 1.0 and MIDI 2.0: the group and channel as people count them
                    if (type == 0x2 || type == 0x4)
                    {
                        name += res::FormatString(L"M2GroupChannel", group + 1, ((words[0] >> 16) & 0xF) + 1);
                    }
                    else if (type != 0x0 && type != 0xF)
                    {
                        name += res::FormatString(L"M2Group", group + 1);
                    }

                    std::wstring detail{ description.ToDisplayString() };

                    // the bits of the first word, and of the second for MIDI 2.0 channel voice
                    std::vector<ui::BitField> fields{
                        { res::GetString(L"M2FieldType"), winrt::hstring{ std::format(L"{:X}", type) }, 4, 3 },
                        { res::GetString(L"M2FieldGroup"), winrt::hstring{ std::format(L"{} = {}", group, group + 1) }, 4, 3 } };

                    auto const opcode = (words[0] >> 20) & 0xF;
                    auto const channel = (words[0] >> 16) & 0xF;
                    auto const byte3 = (words[0] >> 8) & 0xFF;
                    auto const byte4 = words[0] & 0xFF;

                    if (type == 0x2 || type == 0x4)
                    {
                        fields.push_back({ res::GetString(L"M2FieldStatus"), winrt::hstring{ std::format(L"{:X}", opcode) }, 4, 0 });
                        fields.push_back({ res::GetString(L"M2FieldChannel"), winrt::hstring{ std::format(L"{} = {}", channel, channel + 1) }, 4, 3 });

                        auto const isNote = opcode == 0x8 || opcode == 0x9 || opcode == 0xA;

                        fields.push_back({ isNote ? res::GetString(L"UmpLabelNote") : res::GetString(L"M2FieldIndex"),
                            winrt::hstring{ isNote ? std::format(L"0x{:02X} = {}", byte3, ::midicalc::NoteName(static_cast<int32_t>(byte3 & 0x7F), middleC, false)) : std::format(L"0x{:02X}", byte3) }, 8, 0 });

                        if (type == 0x2)
                        {
                            fields.push_back({ res::GetString(L"M2FieldData"), winrt::hstring{ std::format(L"0x{:02X}", byte4) }, 8, 1 });
                        }
                        else if (opcode == 0x8 || opcode == 0x9)
                        {
                            fields.push_back({ res::GetString(L"M2FieldAttributeType"), winrt::hstring{ byte4 == 3 ? res::FormatString(L"M2AttributePitch", byte4) : winrt::hstring{ std::to_wstring(byte4) } }, 8, 2 });

                            auto const velocity = words[1] >> 16;
                            auto const attribute = words[1] & 0xFFFF;
                            fields.push_back({ res::GetString(L"UmpLabelVelocity"), winrt::hstring{ std::format(L"0x{:04X} = {}%", velocity, text::Number(100.0 * velocity / 65535.0, 1)) }, 16, 1 });

                            if (byte4 == 3)
                            {
                                auto const pitch = ::midicalc::NoteFromPitch79(static_cast<uint16_t>(attribute));
                                auto const whole = static_cast<int32_t>(std::floor(pitch));
                                auto const pitchText = std::format(L"{} {}", ::midicalc::NoteName(whole, middleC, false), text::Signed((pitch - whole) * 100.0, 2));
                                fields.push_back({ res::GetString(L"UmpLabelAttribute"), winrt::hstring{ std::format(L"0x{:04X} = {}", attribute, pitchText) }, 16, 2 });
                                detail += res::FormatString(L"M2DecodedPitch", pitchText);
                            }
                            else
                            {
                                fields.push_back({ res::GetString(L"UmpLabelAttribute"), winrt::hstring{ std::format(L"0x{:04X}", attribute) }, 16, 2 });
                            }
                        }
                        else
                        {
                            fields.push_back({ res::GetString(L"M2FieldData"), winrt::hstring{ std::format(L"0x{:02X}", byte4) }, 8, 1 });
                            fields.push_back({ res::GetString(L"M2FieldData"), winrt::hstring{ std::format(L"0x{:08X}", words[1]) }, 32, 1 });
                        }

                        auto const midi1 = Midi1Equivalent(words);

                        if (!midi1.empty())
                        {
                            m_umpMidi1.Text(winrt::hstring{ midi1 });
                        }
                    }
                    else
                    {
                        fields.push_back({ res::GetString(L"M2FieldRest"), winrt::hstring{ std::format(L"0x{:06X}", words[0] & 0xFFFFFF) }, 24, 1 });
                    }

                    ui::FillBitDiagram(m_umpBits, fields);
                    m_umpName.Text(name);

                    if (words.size() > needed)
                    {
                        detail += res::FormatString(L"M2WordsExtra", words.size() - needed);
                    }

                    m_umpDetail.Text(detail);
                }
            }

            // manufacturer IDs: three MIDI 1.0 bytes starting 00, one MIDI 1.0 byte, or the
            // MIDI 2.0 16-bit form (0x prefix, or four hex digits)
            std::wstring const mfrText{ m_mfrId.Text() };
            auto const bytes = text::ParseWords(mfrText);

            if (bytes.size() == 3 && bytes[0] == 0 && bytes[1] <= 0x7F && bytes[2] <= 0x7F)
            {
                auto const id = ::midicalc::ManufacturerIdFromMidi1(::midicalc::Midi1ManufacturerId{ 0, static_cast<uint8_t>(bytes[1]), static_cast<uint8_t>(bytes[2]), true });
                m_mfrResult.Text(res::FormatString(L"M2MfrTo2", text::Hex(id, 4)));
            }
            else if (bytes.size() == 1 && bytes[0] <= 0xFFFF && (mfrText.find(L"0x") != std::wstring::npos || mfrText.size() >= 4))
            {
                auto const id = ::midicalc::Midi1FromManufacturerId(static_cast<uint16_t>(bytes[0]));
                m_mfrResult.Text(res::FormatString(L"M2MfrTo1", id.IsThreeBytes ? Bytes({ id.Byte1, id.Byte2, id.Byte3 }) : Bytes({ id.Byte1 })));
            }
            else if (bytes.size() == 1 && bytes[0] >= 0x01 && bytes[0] <= 0x7F)
            {
                auto const id = ::midicalc::ManufacturerIdFromMidi1(::midicalc::Midi1ManufacturerId{ static_cast<uint8_t>(bytes[0]), 0, 0, false });
                m_mfrResult.Text(res::FormatString(L"M2MfrTo2", text::Hex(id, 4)));
            }
            else
            {
                m_mfrResult.Text(mfrText.empty() ? winrt::hstring{} : res::GetString(L"M2MfrInvalid"));
            }
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to decode the UMP words.")
    }
}
