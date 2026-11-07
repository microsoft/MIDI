// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The Strum generator's settings where the piano roll goes, and the strum curve the piano roll's
// Strum tool shares (strum proposal; Pete, November 5 2026: "a user could pick the curve style
// and then when they drag the mouse it changes the top of the curve, but retains the overall
// curve shape"). The curve shows a strum's notes, string by string, along its shape; dragging
// sideways changes how long the strum takes, and the shape stays.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "SequenceStrum.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        constexpr float CurveWidth = 380.0f;
        constexpr float CurveHeight = 104.0f;
        constexpr float CurveLeft = 14.0f;
        constexpr float CurveRight = 70.0f;      // room for the time at the end
        constexpr float CurveTop = 10.0f;
        constexpr float CurveBottom = 10.0f;

        constexpr std::array<wchar_t const*, 5> InstrumentKeys{ L"StrumInstrumentGuitar", L"StrumInstrumentUkulele", L"StrumInstrumentPiano", L"StrumInstrumentAsWritten", L"StrumInstrumentHarp" };
        constexpr std::array<wchar_t const*, 5> ShapeKeys{ L"StrumShapeEven", L"StrumShapeSpeedingUp", L"StrumShapeSlowingDown", L"StrumShapeEase", L"StrumShapeRake" };

        controls::TextBlock Text(winrt::hstring const& text, media::Brush const& brush, double size = 12)
        {
            controls::TextBlock block{};
            block.Text(text);
            block.FontSize(size);
            block.TextWrapping(xaml::TextWrapping::Wrap);
            block.Foreground(brush);
            return block;
        }

        controls::RadioButton Choice(winrt::hstring const& name, controls::TextBlock const& detail)
        {
            controls::TextBlock label{};
            label.Text(name);
            label.FontSize(14);
            label.TextWrapping(xaml::TextWrapping::Wrap);

            controls::StackPanel content{};
            content.Children().Append(label);
            content.Children().Append(detail);

            controls::RadioButton button{};
            button.GroupName(L"StrumSource");
            button.Content(content);
            automation::AutomationProperties::SetName(button, name);
            automation::AutomationProperties::SetHelpText(button, detail.Text());
            return button;
        }

        controls::StackPanel Row(double spacing = 18)
        {
            controls::StackPanel row{};
            row.Orientation(controls::Orientation::Horizontal);
            row.Spacing(spacing);
            return row;
        }

        controls::NumberBox Number(wchar_t const* headerKey, double minimum, double maximum, double value, double width = 150)
        {
            controls::NumberBox box{};
            box.Header(winrt::box_value(res::GetString(headerKey)));
            automation::AutomationProperties::SetName(box, res::GetString(headerKey));
            box.Minimum(minimum);
            box.Maximum(maximum);
            box.SmallChange(1);
            box.LargeChange(10);
            box.Value(value);
            box.Width(width);
            midiapp::UseTouchSpinButtons(box);
            return box;
        }

        controls::ComboBox Combo(wchar_t const* headerKey, std::vector<winrt::hstring> const& items, int32_t selected, double width = 200)
        {
            controls::ComboBox box{};
            box.Header(winrt::box_value(res::GetString(headerKey)));
            automation::AutomationProperties::SetName(box, res::GetString(headerKey));
            box.Width(width);

            for (auto const& item : items)
            {
                box.Items().Append(winrt::box_value(item));
            }

            box.SelectedIndex(selected);
            return box;
        }

        std::wstring SpreadText(double spread, bool ticks)
        {
            return ticks ? std::wstring{ res::FormatString(L"StrumSpreadTicksFormat", std::lround(spread)) }
                         : std::wstring{ res::FormatString(L"StrumSpreadMsFormat", std::lround(spread)) };
        }
    }

    // ---------------------------------------------------------------- the curve

    _Use_decl_annotations_
    canvasXaml::CanvasControl MainWindow::MakeStrumCurve(std::shared_ptr<StrumCurveState> const& state, std::function<void(double)> const& changed, std::function<void(double)> const& committed)
    {
        canvasXaml::CanvasControl canvas{};
        canvas.ClearColor(winrt::Microsoft::UI::Colors::Transparent());
        canvas.Width(CurveWidth);
        canvas.Height(CurveHeight);
        canvas.HorizontalAlignment(xaml::HorizontalAlignment::Left);
        automation::AutomationProperties::SetName(canvas, res::GetString(L"StrumCurveName"));
        automation::AutomationProperties::SetHelpText(canvas, res::GetString(L"StrumCurveHelp"));
        controls::ToolTipService::SetToolTip(canvas, winrt::box_value(res::GetString(L"StrumCurveHelp")));

        auto const weak = get_weak();

        canvas.Draw([weak, state](canvasXaml::CanvasControl const& sender, canvasXaml::CanvasDrawEventArgs const& args)
        {
            try
            {
                auto strong = weak.get();

                if (strong == nullptr)
                {
                    return;
                }

                auto const& colors = strong->m_palette;
                auto const ds = args.DrawingSession();
                auto const width = static_cast<float>(sender.ActualWidth());
                auto const height = static_cast<float>(sender.ActualHeight());
                auto const usable = std::max(1.0f, width - CurveLeft - CurveRight);
                auto const strings = std::max<size_t>(2, state->Strings);
                auto const rowGap = (height - CurveTop - CurveBottom) / static_cast<float>(strings - 1);
                auto const most = std::max(1.0, state->MostSpread);

                ds.FillRoundedRectangle(0, 0, width, height, 6.0f, 6.0f, colors.RollBackground);

                // The strings, lowest at the bottom.
                for (size_t i = 0; i < strings; ++i)
                {
                    auto const y = height - CurveBottom - rowGap * static_cast<float>(i);
                    ds.DrawLine(CurveLeft, y, width - CurveRight + 8.0f, y, seq::WithAlpha(colors.Text3, 0.35), 1.0f);
                }

                // Each string's note where the strum reaches it, joined along the shape.
                std::optional<foundation::Point> previous{};
                foundation::Point last{};

                for (size_t i = 0; i < strings; ++i)
                {
                    auto const x = static_cast<double>(i) / static_cast<double>(strings - 1);
                    auto const at = state->Spread * seq::StrumCurve(state->Shape, x, state->Bend);
                    foundation::Point point{ CurveLeft + static_cast<float>(at / most) * usable, height - CurveBottom - rowGap * static_cast<float>(i) };

                    if (previous.has_value())
                    {
                        ds.DrawLine(previous->X, previous->Y, point.X, point.Y, seq::WithAlpha(colors.SelectionStroke, 0.55), 1.5f);
                    }

                    ds.FillCircle(point.X, point.Y, 3.6f, colors.SelectionStroke);
                    previous = point;
                    last = point;
                }

                // The handle at the top, which a drag moves, and how long the strum takes.
                ds.DrawCircle(last.X, last.Y, state->Dragging ? 7.5f : 6.0f, colors.Text, 1.6f);

                canvasText::CanvasTextFormat format{};
                format.FontFamily(L"Segoe UI Variable Text");
                format.FontSize(12.0f);
                format.WordWrapping(canvasText::CanvasWordWrapping::NoWrap);
                ds.DrawText(winrt::hstring{ SpreadText(state->Spread, state->Ticks) }, last.X + 12.0f, last.Y - 9.0f, colors.Text, format);
            }
            MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to draw the strum curve.")
        });

        auto const spreadAt = [state](canvasXaml::CanvasControl const& sender, input::PointerRoutedEventArgs const& args)
        {
            auto const x = args.GetCurrentPoint(sender).Position().X;
            auto const usable = std::max(1.0, sender.ActualWidth() - CurveLeft - CurveRight);
            return std::clamp(std::round((x - CurveLeft) / usable * state->MostSpread), 0.0, state->MostSpread);
        };

        canvas.PointerPressed([state, spreadAt, changed](foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args)
        {
            try
            {
                auto const control = sender.as<canvasXaml::CanvasControl>();
                state->Dragging = true;
                state->Spread = spreadAt(control, args);
                control.CapturePointer(args.Pointer());
                control.Invalidate();

                if (changed)
                {
                    changed(state->Spread);
                }

                args.Handled(true);
            }
            MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to start changing the strum.")
        });

        canvas.PointerMoved([state, spreadAt, changed](foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args)
        {
            try
            {
                if (!state->Dragging)
                {
                    return;
                }

                auto const control = sender.as<canvasXaml::CanvasControl>();
                auto const spread = spreadAt(control, args);

                if (spread != state->Spread)
                {
                    state->Spread = spread;
                    control.Invalidate();

                    if (changed)
                    {
                        changed(spread);
                    }
                }

                args.Handled(true);
            }
            MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change the strum.")
        });

        // The edit is made once, when the drag ends, so the panel isn't built again under it.
        auto const letGo = [state, committed](foundation::IInspectable const& sender, input::PointerRoutedEventArgs const&)
        {
            try
            {
                if (!state->Dragging)
                {
                    return;
                }

                // Cleared first: letting go of the capture comes straight back here.
                state->Dragging = false;
                auto const control = sender.as<canvasXaml::CanvasControl>();
                control.ReleasePointerCaptures();
                control.Invalidate();

                if (committed)
                {
                    committed(state->Spread);
                }
            }
            MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to finish changing the strum.")
        };

        canvas.PointerReleased(letGo);
        canvas.PointerCaptureLost(letGo);
        return canvas;
    }

    // ---------------------------------------------------------------- the generator's settings

    _Use_decl_annotations_
    void MainWindow::AddStrumSettings(
        controls::StackPanel const& body,
        seq::GeneratorSettings const& generator,
        std::function<void(std::function<void(seq::GeneratorSettings&)>)> const& edit)
    {
        auto const secondary = BrushFor(m_palette.Text3);
        auto const chipStyle = RootGrid().Resources().Lookup(winrt::box_value(L"ChipToggleStyle")).as<xaml::Style>();

        // What it plays on the left, and how it plays it on the right.
        controls::StackPanel form{};
        form.Spacing(12);
        form.Width(400);

        controls::StackPanel play{};
        play.Spacing(12);
        play.MaxWidth(560);

        // ---- where the chords come from

        form.Children().Append(Text(res::GetString(L"StrumChordsFrom"), secondary));

        auto const hasChords = m_doc.Harmony.has_value() && !m_doc.Harmony->Chords.empty();
        auto fromChords = Choice(res::GetString(L"StrumFromChords"), Text(res::GetString(hasChords ? L"StrumFromChordsHelp" : L"ArpFromChordsNone"), secondary));
        fromChords.IsChecked(generator.ArpSource == seq::ArpeggiatorSource::Chords);
        form.Children().Append(fromChords);

        auto fromTrack = Choice(res::GetString(L"StrumFromTrack"), Text(res::GetString(L"StrumFromTrackHelp"), secondary));
        fromTrack.IsChecked(generator.ArpSource == seq::ArpeggiatorSource::Track);
        form.Children().Append(fromTrack);

        std::vector<std::wstring> trackIds{};
        std::vector<winrt::hstring> trackNames{};
        int32_t followed{ -1 };

        seq::ForEachTrack(m_doc, [&](seq::Track const& candidate, size_t)
        {
            if (!candidate.IsFolder && candidate.Id != m_editorTrackId)
            {
                if (candidate.Id == generator.ArpTrackId)
                {
                    followed = static_cast<int32_t>(trackIds.size());
                }

                trackIds.push_back(candidate.Id);
                trackNames.emplace_back(candidate.Name);
            }

            return true;
        });

        auto follow = Combo(L"ArpTrackName", trackNames, followed, 240);
        follow.Margin(xaml::Thickness{ 28, -4, 0, 0 });
        follow.IsEnabled(generator.ArpSource == seq::ArpeggiatorSource::Track && !trackIds.empty());
        form.Children().Append(follow);

        // The keys held on the track's source, as the arpeggiator can play them.
        auto const track = EditorTrack();
        auto const sourceText = track != nullptr && !track->Source.Endpoint.IsEmpty() ? winrt::hstring{ DescribeSource(*track) } : res::GetString(L"ArpNoSource");
        auto fromHeld = Choice(res::GetString(L"StrumFromHeld"), Text(sourceText, secondary));
        fromHeld.IsChecked(generator.ArpSource == seq::ArpeggiatorSource::Held);
        form.Children().Append(fromHeld);

        fromHeld.Checked([edit](auto&&, auto&&)
        {
            edit([](seq::GeneratorSettings& edited) { edited.ArpSource = seq::ArpeggiatorSource::Held; });
        });

        controls::ToggleSwitch latch{};
        latch.Header(winrt::box_value(res::GetString(L"ArpLatch")));
        latch.IsOn(generator.ArpLatch);
        latch.IsEnabled(generator.ArpSource == seq::ArpeggiatorSource::Held);
        latch.Margin(xaml::Thickness{ 28, -4, 0, 0 });
        automation::AutomationProperties::SetName(latch, res::GetString(L"ArpLatch"));
        automation::AutomationProperties::SetHelpText(latch, res::GetString(L"StrumLatchHelp"));
        controls::ToolTipService::SetToolTip(latch, winrt::box_value(res::GetString(L"StrumLatchHelp")));
        latch.Toggled([edit](foundation::IInspectable const& sender, auto&&)
        {
            auto const on = sender.as<controls::ToggleSwitch>().IsOn();
            edit([on](seq::GeneratorSettings& edited) { edited.ArpLatch = on; });
        });
        form.Children().Append(latch);

        fromChords.Checked([edit](auto&&, auto&&)
        {
            edit([](seq::GeneratorSettings& edited) { edited.ArpSource = seq::ArpeggiatorSource::Chords; });
        });

        auto const firstTrack = trackIds.empty() ? std::wstring{} : trackIds.front();

        fromTrack.Checked([edit, firstTrack](auto&&, auto&&)
        {
            edit([firstTrack](seq::GeneratorSettings& edited)
            {
                edited.ArpSource = seq::ArpeggiatorSource::Track;

                if (edited.ArpTrackId.empty())
                {
                    edited.ArpTrackId = firstTrack;
                }
            });
        });

        follow.SelectionChanged([edit, trackIds](foundation::IInspectable const& sender, auto&&)
        {
            auto const index = sender.as<controls::ComboBox>().SelectedIndex();

            if (index >= 0 && static_cast<size_t>(index) < trackIds.size())
            {
                auto const id = trackIds[static_cast<size_t>(index)];
                edit([id](seq::GeneratorSettings& edited) { edited.ArpTrackId = id; });
            }
        });

        // ---- the instrument it voices the chords for

        form.Children().Append(Text(res::GetString(L"StrumInstrument"), secondary));

        auto instruments = Row(6);

        for (size_t i = 0; i < InstrumentKeys.size(); ++i)
        {
            primitives::ToggleButton chip{};
            chip.Style(chipStyle);
            chip.Content(winrt::box_value(res::GetString(InstrumentKeys[i])));
            chip.IsChecked(static_cast<size_t>(generator.Instrument) == i);
            automation::AutomationProperties::SetName(chip, res::GetString(InstrumentKeys[i]));
            automation::AutomationProperties::SetHelpText(chip, res::GetString(std::wstring{ InstrumentKeys[i] } + L"Help"));
            automation::AutomationProperties::SetPositionInSet(chip, static_cast<int32_t>(i) + 1);
            automation::AutomationProperties::SetSizeOfSet(chip, static_cast<int32_t>(InstrumentKeys.size()));
            controls::ToolTipService::SetToolTip(chip, winrt::box_value(res::GetString(std::wstring{ InstrumentKeys[i] } + L"Help")));
            chip.Click([edit, i](auto&&, auto&&)
            {
                edit([i](seq::GeneratorSettings& edited) { edited.Instrument = static_cast<seq::StrumInstrument>(i); });
            });
            instruments.Children().Append(chip);
        }

        form.Children().Append(instruments);
        form.Children().Append(Text(res::GetString(std::wstring{ InstrumentKeys[std::min<size_t>(static_cast<size_t>(generator.Instrument), InstrumentKeys.size() - 1)] } + L"Help"), secondary));

        // A capo, and where on the neck the shapes stay, for the fretted instruments.
        auto const fretted = generator.Instrument == seq::StrumInstrument::Guitar || generator.Instrument == seq::StrumInstrument::Ukulele;
        auto neck = Row();
        auto capo = Number(L"StrumCapo", 0, 9, generator.StrumCapo, 120);
        capo.IsEnabled(fretted);
        controls::ToolTipService::SetToolTip(capo, winrt::box_value(res::GetString(L"StrumCapoHelp")));
        automation::AutomationProperties::SetHelpText(capo, res::GetString(L"StrumCapoHelp"));
        neck.Children().Append(capo);

        auto position = Number(L"StrumPosition", 0, 9, generator.StrumPosition, 150);
        position.IsEnabled(fretted);
        controls::ToolTipService::SetToolTip(position, winrt::box_value(res::GetString(L"StrumPositionHelp")));
        automation::AutomationProperties::SetHelpText(position, res::GetString(L"StrumPositionHelp"));
        neck.Children().Append(position);
        form.Children().Append(neck);

        auto const neckEdit = [&edit](controls::NumberBox const& box, bool isCapo)
        {
            box.ValueChanged([edit, isCapo](controls::NumberBox const&, controls::NumberBoxValueChangedEventArgs const& args)
            {
                auto const value = args.NewValue();

                if (std::isfinite(value) && value != args.OldValue())
                {
                    auto const frets = static_cast<uint8_t>(std::clamp(std::lround(value), 0L, 9L));
                    edit([frets, isCapo](seq::GeneratorSettings& edited) { (isCapo ? edited.StrumCapo : edited.StrumPosition) = frets; });
                }
            });
        };

        neckEdit(capo, true);
        neckEdit(position, false);

        // ---- the pattern, from a style or typed

        auto const& styles = seq::StrumStyles();
        std::vector<winrt::hstring> styleNames{};
        int32_t styleIndex{ -1 };

        for (size_t i = 0; i < styles.size(); ++i)
        {
            styleNames.push_back(res::GetString(styles[i].NameKey));

            if (generator.StrumPattern == styles[i].Pattern && generator.StepTicks == styles[i].StepTicks)
            {
                styleIndex = static_cast<int32_t>(i);
            }
        }

        auto style = Combo(L"StrumStyle", styleNames, styleIndex, 240);
        style.PlaceholderText(res::GetString(L"StrumStyleOwn"));
        style.SelectionChanged([edit](foundation::IInspectable const& sender, auto&&)
        {
            auto const index = sender.as<controls::ComboBox>().SelectedIndex();
            auto const& all = seq::StrumStyles();

            if (index >= 0 && static_cast<size_t>(index) < all.size())
            {
                auto const chosen = all[static_cast<size_t>(index)];
                edit([chosen](seq::GeneratorSettings& edited) { seq::UseStrumStyle(edited, chosen); });
            }
        });
        form.Children().Append(style);

        controls::TextBox pattern{};
        pattern.Header(winrt::box_value(res::GetString(L"StrumPattern")));
        automation::AutomationProperties::SetName(pattern, res::GetString(L"StrumPattern"));
        automation::AutomationProperties::SetHelpText(pattern, res::GetString(L"StrumPatternHelp"));
        pattern.Text(winrt::hstring{ generator.StrumPattern });
        pattern.MaxLength(static_cast<int32_t>(seq::MaximumStrumPattern));
        pattern.FontFamily(media::FontFamily{ L"Cascadia Mono,Consolas" });
        pattern.Width(240);
        pattern.HorizontalAlignment(xaml::HorizontalAlignment::Left);

        auto const commitPattern = [edit](controls::TextBox const& box)
        {
            auto const text = seq::CleanStrumPattern(std::wstring_view{ box.Text() });

            if (!text.empty())
            {
                edit([text](seq::GeneratorSettings& edited) { edited.StrumPattern = text; });
            }
        };

        pattern.LostFocus([commitPattern](foundation::IInspectable const& sender, auto&&)
        {
            commitPattern(sender.as<controls::TextBox>());
        });

        pattern.KeyDown([commitPattern](foundation::IInspectable const& sender, input::KeyRoutedEventArgs const& args)
        {
            if (args.Key() == winrt::Windows::System::VirtualKey::Enter)
            {
                commitPattern(sender.as<controls::TextBox>());
                args.Handled(true);
            }
        });

        form.Children().Append(pattern);
        form.Children().Append(Text(res::GetString(L"StrumPatternHelp"), secondary));

        std::vector<winrt::hstring> stepNames{};
        int32_t stepIndex{ -1 };

        for (size_t i = 0; i < std::size(seq::PatternStepSizes); ++i)
        {
            stepNames.push_back(res::GetString(L"PatternStepSize" + std::to_wstring(seq::PatternStepSizes[i])));

            if (seq::PatternStepSizes[i] == generator.StepTicks)
            {
                stepIndex = static_cast<int32_t>(i);
            }
        }

        auto step = Combo(L"StrumStep", stepNames, stepIndex, 240);
        step.SelectionChanged([edit](foundation::IInspectable const& sender, auto&&)
        {
            auto const index = sender.as<controls::ComboBox>().SelectedIndex();

            if (index >= 0 && static_cast<size_t>(index) < std::size(seq::PatternStepSizes))
            {
                auto const ticks = seq::PatternStepSizes[static_cast<size_t>(index)];
                edit([ticks](seq::GeneratorSettings& edited) { edited.StepTicks = ticks; });
            }
        });
        form.Children().Append(step);

        // ---- the strum's shape, and the curve to drag

        play.Children().Append(Text(res::GetString(L"StrumShape"), secondary));

        auto shapes = Row(6);

        for (size_t i = 0; i < ShapeKeys.size(); ++i)
        {
            primitives::ToggleButton chip{};
            chip.Style(chipStyle);
            chip.Content(winrt::box_value(res::GetString(ShapeKeys[i])));
            chip.IsChecked(static_cast<size_t>(generator.StrumCurve) == i);
            automation::AutomationProperties::SetName(chip, res::GetString(ShapeKeys[i]));
            automation::AutomationProperties::SetHelpText(chip, res::GetString(std::wstring{ ShapeKeys[i] } + L"Help"));
            automation::AutomationProperties::SetPositionInSet(chip, static_cast<int32_t>(i) + 1);
            automation::AutomationProperties::SetSizeOfSet(chip, static_cast<int32_t>(ShapeKeys.size()));
            controls::ToolTipService::SetToolTip(chip, winrt::box_value(res::GetString(std::wstring{ ShapeKeys[i] } + L"Help")));
            chip.Click([edit, i](auto&&, auto&&)
            {
                edit([i](seq::GeneratorSettings& edited) { edited.StrumCurve = static_cast<seq::StrumShape>(i); });
            });
            shapes.Children().Append(chip);
        }

        play.Children().Append(shapes);

        auto curve = std::make_shared<StrumCurveState>();
        curve->Shape = generator.StrumCurve;
        curve->Bend = generator.StrumBend / 100.0;
        curve->Ticks = generator.StrumInTicks;
        curve->Spread = generator.StrumInTicks ? generator.StrumSpreadTicks : generator.StrumSpreadMs;
        curve->MostSpread = generator.StrumInTicks ? 240.0 : 150.0;
        curve->Strings = generator.Instrument == seq::StrumInstrument::Harp ? 8 : seq::InstrumentStrings(generator.Instrument) > 0 ? seq::InstrumentStrings(generator.Instrument) : 4;

        auto const inTicks = generator.StrumInTicks;
        play.Children().Append(MakeStrumCurve(curve, nullptr, [edit, inTicks](double spread)
        {
            auto const value = static_cast<uint16_t>(std::lround(spread));
            edit([value, inTicks](seq::GeneratorSettings& edited)
            {
                (inTicks ? edited.StrumSpreadTicks : edited.StrumSpreadMs) = value;
            });
        }));

        auto const numberEdit = [&edit](controls::NumberBox const& box, std::function<void(seq::GeneratorSettings&, double)> apply)
        {
            box.ValueChanged([edit, apply](controls::NumberBox const&, controls::NumberBoxValueChangedEventArgs const& args)
            {
                auto const value = args.NewValue();

                if (std::isfinite(value) && value != args.OldValue())
                {
                    edit([apply, value](seq::GeneratorSettings& edited) { apply(edited, value); });
                }
            });
        };

        auto spreadRow = Row();
        auto spread = inTicks
            ? Number(L"StrumSpreadTicks", 0, seq::MaximumStrumSpreadTicks, generator.StrumSpreadTicks)
            : Number(L"StrumSpreadMs", 0, seq::MaximumStrumSpreadMs, generator.StrumSpreadMs);
        numberEdit(spread, [inTicks](seq::GeneratorSettings& edited, double value)
        {
            (inTicks ? edited.StrumSpreadTicks : edited.StrumSpreadMs) = static_cast<uint16_t>(std::lround(value));
        });
        spreadRow.Children().Append(spread);

        auto measure = Combo(L"StrumMeasure", { res::GetString(L"StrumMeasureMs"), res::GetString(L"StrumMeasureTicks") }, inTicks ? 1 : 0, 220);
        controls::ToolTipService::SetToolTip(measure, winrt::box_value(res::GetString(L"StrumMeasureHelp")));
        automation::AutomationProperties::SetHelpText(measure, res::GetString(L"StrumMeasureHelp"));
        measure.SelectionChanged([edit](foundation::IInspectable const& sender, auto&&)
        {
            auto const ticks = sender.as<controls::ComboBox>().SelectedIndex() == 1;
            edit([ticks](seq::GeneratorSettings& edited) { edited.StrumInTicks = ticks; });
        });
        spreadRow.Children().Append(measure);

        auto bend = Number(L"StrumBend", 0, 100, generator.StrumBend);
        numberEdit(bend, [](seq::GeneratorSettings& edited, double value) { edited.StrumBend = static_cast<uint8_t>(std::clamp(std::lround(value), 0L, 100L)); });
        spreadRow.Children().Append(bend);
        play.Children().Append(spreadRow);

        // ---- where it lands, how it leans, how loose the hand is

        auto feelRow = Row();
        auto lands = Combo(L"StrumLands", { res::GetString(L"StrumLandsFirst"), res::GetString(L"StrumLandsMiddle"), res::GetString(L"StrumLandsLast") },
            static_cast<int32_t>(generator.StrumLand), 220);
        controls::ToolTipService::SetToolTip(lands, winrt::box_value(res::GetString(L"StrumLandsHelp")));
        automation::AutomationProperties::SetHelpText(lands, res::GetString(L"StrumLandsHelp"));
        lands.SelectionChanged([edit](foundation::IInspectable const& sender, auto&&)
        {
            auto const index = sender.as<controls::ComboBox>().SelectedIndex();

            if (index >= 0)
            {
                edit([index](seq::GeneratorSettings& edited) { edited.StrumLand = static_cast<seq::StrumLands>(index); });
            }
        });
        feelRow.Children().Append(lands);

        auto tiltBox = Number(L"StrumTilt", -50, 50, generator.StrumTilt);
        controls::ToolTipService::SetToolTip(tiltBox, winrt::box_value(res::GetString(L"StrumTiltHelp")));
        numberEdit(tiltBox, [](seq::GeneratorSettings& edited, double value) { edited.StrumTilt = static_cast<int8_t>(std::clamp(std::lround(value), -50L, 50L)); });
        feelRow.Children().Append(tiltBox);

        auto handBox = Number(L"StrumHand", 0, 100, generator.StrumHand);
        controls::ToolTipService::SetToolTip(handBox, winrt::box_value(res::GetString(L"StrumHandHelp")));
        numberEdit(handBox, [](seq::GeneratorSettings& edited, double value) { edited.StrumHand = static_cast<uint8_t>(std::clamp(std::lround(value), 0L, 100L)); });
        feelRow.Children().Append(handBox);
        play.Children().Append(feelRow);

        // ---- how long notes sound

        auto ringRow = Row();
        controls::ToggleSwitch ring{};
        ring.Header(winrt::box_value(res::GetString(L"StrumRing")));
        ring.IsOn(generator.StrumRing);
        automation::AutomationProperties::SetName(ring, res::GetString(L"StrumRing"));
        automation::AutomationProperties::SetHelpText(ring, res::GetString(L"StrumRingHelp"));
        controls::ToolTipService::SetToolTip(ring, winrt::box_value(res::GetString(L"StrumRingHelp")));
        ring.Toggled([edit](foundation::IInspectable const& sender, auto&&)
        {
            auto const on = sender.as<controls::ToggleSwitch>().IsOn();
            edit([on](seq::GeneratorSettings& edited) { edited.StrumRing = on; });
        });
        ringRow.Children().Append(ring);

        auto length = Number(L"GeneratorNoteLength", 1, 1600, generator.NoteLength);
        length.IsEnabled(!generator.StrumRing);
        numberEdit(length, [](seq::GeneratorSettings& edited, double value) { edited.NoteLength = static_cast<uint16_t>(std::clamp(std::lround(value), 1L, 1600L)); });
        ringRow.Children().Append(length);

        controls::CheckBox keepBass{};
        keepBass.Content(winrt::box_value(res::GetString(L"StrumKeepBass")));
        keepBass.IsChecked(generator.StrumKeepBass);
        keepBass.VerticalAlignment(xaml::VerticalAlignment::Bottom);
        automation::AutomationProperties::SetHelpText(keepBass, res::GetString(L"StrumKeepBassHelp"));
        controls::ToolTipService::SetToolTip(keepBass, winrt::box_value(res::GetString(L"StrumKeepBassHelp")));
        keepBass.Click([edit](foundation::IInspectable const& sender, auto&&)
        {
            auto const checked = sender.as<controls::CheckBox>().IsChecked();
            auto const on = checked != nullptr && checked.Value();
            edit([on](seq::GeneratorSettings& edited) { edited.StrumKeepBass = on; });
        });
        ringRow.Children().Append(keepBass);
        play.Children().Append(ringRow);

        // ---- how loud: normal strokes and soft ones, in percent

        auto loudRow = Row();
        auto normal = Number(L"StrumVelocityNormal", 1, 100, std::round(generator.VelocityHigh / 655.35));
        numberEdit(normal, [](seq::GeneratorSettings& edited, double value) { edited.VelocityHigh = static_cast<uint16_t>(std::clamp(std::lround(value * 655.35), 512L, 65535L)); });
        loudRow.Children().Append(normal);

        auto soft = Number(L"StrumVelocitySoft", 1, 100, std::round(generator.VelocityLow / 655.35));
        numberEdit(soft, [](seq::GeneratorSettings& edited, double value) { edited.VelocityLow = static_cast<uint16_t>(std::clamp(std::lround(value * 655.35), 512L, 65535L)); });
        loudRow.Children().Append(soft);
        play.Children().Append(loudRow);

        // ---- R and T

        auto burstRow = Row();
        auto burst = Number(L"StrumBurst", 2, 12, generator.StrumBurst);
        controls::ToolTipService::SetToolTip(burst, winrt::box_value(res::GetString(L"StrumBurstHelp")));
        numberEdit(burst, [](seq::GeneratorSettings& edited, double value) { edited.StrumBurst = static_cast<uint8_t>(std::clamp(std::lround(value), 2L, 12L)); });
        burstRow.Children().Append(burst);

        std::vector<winrt::hstring> shapeNames{};

        for (auto const key : ShapeKeys)
        {
            shapeNames.push_back(res::GetString(key));
        }

        auto burstCurve = Combo(L"StrumBurstCurve", shapeNames, static_cast<int32_t>(generator.StrumBurstCurve), 200);
        burstCurve.SelectionChanged([edit](foundation::IInspectable const& sender, auto&&)
        {
            auto const index = sender.as<controls::ComboBox>().SelectedIndex();

            if (index >= 0)
            {
                edit([index](seq::GeneratorSettings& edited) { edited.StrumBurstCurve = static_cast<seq::StrumShape>(index); });
            }
        });
        burstRow.Children().Append(burstCurve);
        play.Children().Append(burstRow);

        // ---- muted strokes

        auto muteRow = Row();
        auto const keySwitch = generator.StrumMute == seq::StrumMuting::KeySwitch;
        auto muting = Combo(L"StrumMuted", { res::GetString(L"StrumMutedShort"), res::GetString(L"StrumMutedKeySwitch") }, keySwitch ? 1 : 0, 200);
        muting.SelectionChanged([edit](foundation::IInspectable const& sender, auto&&)
        {
            auto const on = sender.as<controls::ComboBox>().SelectedIndex() == 1;
            edit([on](seq::GeneratorSettings& edited) { edited.StrumMute = on ? seq::StrumMuting::KeySwitch : seq::StrumMuting::Short; });
        });
        muteRow.Children().Append(muting);

        auto muteKey = Number(L"StrumMuteKey", 0, 127, generator.StrumMuteKey, 120);
        muteKey.IsEnabled(keySwitch);
        muteKey.Description(winrt::box_value(winrt::hstring{ seq::NoteLabel(generator.StrumMuteKey) }));
        numberEdit(muteKey, [](seq::GeneratorSettings& edited, double value) { edited.StrumMuteKey = static_cast<uint8_t>(std::clamp(std::lround(value), 0L, 127L)); });
        muteRow.Children().Append(muteKey);

        auto openKey = Number(L"StrumOpenKey", 0, 127, generator.StrumOpenKey, 120);
        openKey.IsEnabled(keySwitch);
        openKey.Description(winrt::box_value(winrt::hstring{ seq::NoteLabel(generator.StrumOpenKey) }));
        numberEdit(openKey, [](seq::GeneratorSettings& edited, double value) { edited.StrumOpenKey = static_cast<uint8_t>(std::clamp(std::lround(value), 0L, 127L)); });
        muteRow.Children().Append(openKey);
        play.Children().Append(muteRow);
        play.Children().Append(Text(res::GetString(keySwitch ? L"StrumMutedKeySwitchHelp" : L"StrumMutedShortHelp"), secondary, 11));

        controls::StackPanel columns{};
        columns.Orientation(controls::Orientation::Horizontal);
        columns.Spacing(40);
        columns.Children().Append(form);
        columns.Children().Append(play);
        body.Children().Append(columns);
    }
}
