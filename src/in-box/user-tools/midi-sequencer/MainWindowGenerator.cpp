// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Generator clips in the editor, laid out like comp 4. The header has the type, the length, the
// step, the seed and whether each pass is the same. A Euclidean rhythm draws its rings beside a
// table of them and a roll of what it plays. A chance melody has its settings down the left and
// its first four passes on the right. Settings that used to sit in the inspector live here now,
// so the inspector is hidden for generators.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "GeneratorView.h"
#include "SequenceGenerator.h"
#include "StringResources.h"

#include <cwctype>

#include <winrt/Windows.Globalization.NumberFormatting.h>

namespace res = ::midisequencer::resources;
namespace numbering = ::winrt::Windows::Globalization::NumberFormatting;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        constexpr std::array<wchar_t const*, 3> TypeKeys{ L"GeneratorTypeEuclidean", L"GeneratorTypeChanceMelody", L"GeneratorTypeArpeggiator" };
        constexpr std::array<wchar_t const*, 3> AboutKeys{ L"GeneratorAboutEuclidean", L"GeneratorAboutMelody", L"GeneratorAboutArpeggiator" };

        constexpr std::array<wchar_t const*, 11> ScaleKeys{
            L"ScaleMajor", L"ScaleMinor", L"ScaleDorian", L"ScalePhrygian", L"ScaleLydian", L"ScaleMixolydian",
            L"ScaleHarmonicMinor", L"ScaleMajorPentatonic", L"ScaleMinorPentatonic", L"ScaleBlues", L"ScaleChromatic" };

        // The same letters NoteLabel uses.
        constexpr std::array<wchar_t const*, 12> KeyNames{ L"C", L"C#", L"D", L"D#", L"E", L"F", L"F#", L"G", L"G#", L"A", L"A#", L"B" };

        // The ring drawing fits the editor's height, between these.
        constexpr double MinimumRingSize = 160.0;
        constexpr double MaximumRingSize = 380.0;

        // The rings table's columns: the dot, the number, note, pulses, steps, rotate, velocity,
        // chance and remove. Its rows are a little shorter than a standard number box.
        constexpr std::array<double, 9> RingColumnWidths{ 10, 34, 112, 80, 80, 80, 84, 84, 28 };
        constexpr double RingColumnSpacing = 8.0;
        constexpr double RingRowHeight = 28.0;
        constexpr double RingsTableWidth = []()
        {
            auto width = RingColumnSpacing * static_cast<double>(RingColumnWidths.size() - 1);

            for (auto const column : RingColumnWidths)
            {
                width += column;
            }

            return width;
        }();

        // What a Euclidean rhythm plays goes beside its table when it gets at least this much
        // width, and under it, this tall, when it doesn't.
        constexpr double MinimumBesideRollWidth = 300.0;
        constexpr double MinimumBesideRollHeight = 110.0;
        constexpr double UnderRollHeight = 140.0;

        // What it plays: two bars, and each of a chance melody's passes.
        constexpr double PassRollHeight = 58.0;
        constexpr float RollKeyWidth = 34.0f;

        // A slider's change waits this long for the next before it's kept.
        constexpr auto SliderRest = std::chrono::milliseconds{ 450 };

        double Percent(uint32_t value) noexcept
        {
            return std::round(static_cast<double>(value) * 100.0 / 65535.0);
        }

        uint16_t FromPercent(double value) noexcept
        {
            return static_cast<uint16_t>(std::clamp(std::lround(value * 65535.0 / 100.0), 0L, 65535L));
        }

        bool SameGenerator(seq::GeneratorSettings const& a, seq::GeneratorSettings const& b) noexcept
        {
            return a.Type == b.Type && a.StepTicks == b.StepTicks && a.ChangesEachPass == b.ChangesEachPass && a.Channel == b.Channel &&
                a.NoteLength == b.NoteLength && a.Rings == b.Rings && a.ScaleRoot == b.ScaleRoot && a.Scale == b.Scale &&
                a.LowNote == b.LowNote && a.HighNote == b.HighNote && a.Busy == b.Busy && a.Keep == b.Keep &&
                a.VelocityLow == b.VelocityLow && a.VelocityHigh == b.VelocityHigh &&
                a.ArpSource == b.ArpSource && a.ArpTrackId == b.ArpTrackId && a.ArpOrder == b.ArpOrder &&
                a.ArpOctaves == b.ArpOctaves && a.ArpLatch == b.ArpLatch;
        }

        controls::TextBlock Text(winrt::hstring const& text, media::Brush const& brush, double size = 12)
        {
            controls::TextBlock block{};
            block.Text(text);
            block.FontSize(size);
            block.TextWrapping(xaml::TextWrapping::Wrap);
            block.Foreground(brush);
            block.VerticalAlignment(xaml::VerticalAlignment::Center);
            return block;
        }

        // A section's heading (seq.css .sec-title).
        controls::TextBlock SectionTitle(winrt::hstring const& text, media::Brush const& brush)
        {
            auto block = Text(text, brush, 12);
            block.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
            return block;
        }

        void Place(controls::Grid const& grid, xaml::FrameworkElement const& element, int32_t row, int32_t column)
        {
            controls::Grid::SetRow(element, row);
            controls::Grid::SetColumn(element, column);
            grid.Children().Append(element);
        }

        void AddColumns(controls::Grid const& grid, std::initializer_list<xaml::GridLength> widths)
        {
            for (auto const& width : widths)
            {
                controls::ColumnDefinition column{};
                column.Width(width);
                grid.ColumnDefinitions().Append(column);
            }
        }

        void AddRows(controls::Grid const& grid, size_t count)
        {
            for (size_t i = 0; i < count; ++i)
            {
                controls::RowDefinition row{};
                row.Height(xaml::GridLengthHelper::Auto());
                grid.RowDefinitions().Append(row);
            }
        }

        xaml::GridLength Pixels(double value) noexcept
        {
            return xaml::GridLength{ value, xaml::GridUnitType::Pixel };
        }

        xaml::GridLength Star() noexcept
        {
            return xaml::GridLength{ 1, xaml::GridUnitType::Star };
        }

        // Shows a number box's value as a note, "E2 (52)", or a percent, "90%", and reads either
        // back however it's typed: "52", "E2", "e2 (52)", "C#3", "Bb1", or "75" and "75%".
        struct ValueFormatter : winrt::implements<ValueFormatter, numbering::INumberFormatter2, numbering::INumberParser>
        {
            explicit ValueFormatter(bool note) noexcept : m_note(note)
            {
            }

            winrt::hstring FormatInt(int64_t value) const
            {
                return FormatDouble(static_cast<double>(value));
            }

            winrt::hstring FormatUInt(uint64_t value) const
            {
                return FormatDouble(static_cast<double>(value));
            }

            winrt::hstring FormatDouble(double value) const
            {
                auto const rounded = std::isfinite(value) ? std::lround(value) : 0L;

                if (m_note)
                {
                    return winrt::hstring{ seq::NoteLabel(static_cast<uint8_t>(std::clamp(rounded, 0L, 127L))) };
                }

                return winrt::hstring{ std::to_wstring(rounded) + L"%" };
            }

            foundation::IReference<int64_t> ParseInt(winrt::hstring const& text) const
            {
                auto const value = Parse(text);
                return value.has_value() ? foundation::IReference<int64_t>{ static_cast<int64_t>(std::llround(*value)) } : nullptr;
            }

            foundation::IReference<uint64_t> ParseUInt(winrt::hstring const& text) const
            {
                auto const value = Parse(text);
                return value.has_value() && *value >= 0 ? foundation::IReference<uint64_t>{ static_cast<uint64_t>(std::llround(*value)) } : nullptr;
            }

            foundation::IReference<double> ParseDouble(winrt::hstring const& text) const
            {
                auto const value = Parse(text);
                return value.has_value() ? foundation::IReference<double>{ *value } : nullptr;
            }

        private:
            std::optional<double> Parse(std::wstring_view text) const
            {
                std::wstring value{ text };
                auto const first = value.find_first_not_of(L" \t");

                if (first == std::wstring::npos)
                {
                    return std::nullopt;
                }

                value = value.substr(first, value.find_last_not_of(L" \t") - first + 1);

                try
                {
                    if (!m_note)
                    {
                        if (!value.empty() && value.back() == L'%')
                        {
                            value.pop_back();
                        }

                        size_t used{ 0 };
                        auto const number = std::stod(value, &used);
                        return used > 0 && std::isfinite(number) ? std::optional<double>{ number } : std::nullopt;
                    }

                    // "E2 (52)" gives its number in brackets.
                    if (auto const open = value.find(L'('); open != std::wstring::npos)
                    {
                        auto const close = value.find(L')', open);
                        value = value.substr(open + 1, close == std::wstring::npos ? std::wstring::npos : close - open - 1);
                    }

                    if (value.empty())
                    {
                        return std::nullopt;
                    }

                    if (std::iswdigit(value.front()))
                    {
                        size_t used{ 0 };
                        auto const number = std::stoi(value, &used);
                        return used == value.size() && number >= 0 && number <= 127 ? std::optional<double>{ number } : std::nullopt;
                    }

                    // A letter, then # or b, then the octave, which can be below zero.
                    static constexpr std::array<int32_t, 7> Pitches{ 9, 11, 0, 2, 4, 5, 7 };
                    auto const letter = static_cast<wchar_t>(std::towupper(value.front()));

                    if (letter < L'A' || letter > L'G')
                    {
                        return std::nullopt;
                    }

                    auto pitch = Pitches[static_cast<size_t>(letter - L'A')];
                    size_t at{ 1 };

                    if (at < value.size() && value[at] == L'#')
                    {
                        ++pitch;
                        ++at;
                    }
                    else if (at < value.size() && value[at] == L'b')
                    {
                        --pitch;
                        ++at;
                    }

                    size_t used{ 0 };
                    auto const octave = std::stoi(value.substr(at), &used);

                    if (used != value.size() - at)
                    {
                        return std::nullopt;
                    }

                    auto const number = (octave + 2) * 12 + pitch;
                    return number >= 0 && number <= 127 ? std::optional<double>{ number } : std::nullopt;
                }
                catch (...)
                {
                    return std::nullopt;
                }
            }

            bool m_note{ false };
        };

        controls::NumberBox MakeNumberBox(winrt::hstring const& name, double minimum, double maximum, double value, std::optional<bool> note)
        {
            controls::NumberBox box{};
            automation::AutomationProperties::SetName(box, name);
            box.Minimum(minimum);
            box.Maximum(maximum);
            box.SmallChange(1);
            box.LargeChange(note.value_or(false) ? 12 : 10);
            box.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);
            box.FontSize(13);

            if (note.has_value())
            {
                box.NumberFormatter(winrt::make<ValueFormatter>(*note));
            }

            box.Value(value);
            return box;
        }
    }

    bool MainWindow::EditorShowsGenerator() const noexcept
    {
        if (m_editorClipId.empty())
        {
            return false;
        }

        auto const clip = seq::FindClip(m_doc, m_editorClipId);
        return clip != nullptr && clip->Kind == seq::ClipKind::Generator && clip->Generator.has_value();
    }

    void MainWindow::InitializeGeneratorEditor() noexcept
    {
        try
        {
            m_updatingPatternUi = true;

            for (auto const ticks : seq::PatternStepSizes)
            {
                GeneratorStepSizeCombo().Items().Append(winrt::box_value(res::GetString(L"PatternStepSize" + std::to_wstring(ticks))));
            }

            // The type list: each type with what it does under its name (comp 4).
            for (size_t i = 0; i < TypeKeys.size(); ++i)
            {
                controls::TextBlock name{};
                name.Text(res::GetString(TypeKeys[i]));
                name.FontSize(13);

                controls::TextBlock about{};
                about.Text(res::GetString(AboutKeys[i]));
                about.FontSize(11);
                about.TextWrapping(xaml::TextWrapping::Wrap);
                about.Foreground(BrushFor(m_palette.Text3));

                controls::StackPanel content{};
                content.Spacing(1);
                content.Children().Append(name);
                content.Children().Append(about);

                controls::RadioButton choice{};
                choice.GroupName(L"GeneratorType");
                choice.Content(content);
                choice.Padding(xaml::Thickness{ 8, 4, 4, 6 });
                choice.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                automation::AutomationProperties::SetName(choice, res::GetString(TypeKeys[i]));
                automation::AutomationProperties::SetHelpText(choice, res::GetString(AboutKeys[i]));

                choice.Checked([weak = get_weak(), i](auto&&, auto&&)
                {
                    if (auto strong = weak.get(); strong && !strong->m_updatingPatternUi)
                    {
                        strong->GeneratorTypeFlyout().Hide();
                        strong->SetGeneratorType(static_cast<seq::GeneratorType>(i));
                    }
                });

                GeneratorTypeList().Children().Append(choice);
            }

            // The rings drawing follows the editor's height, and what it plays the editor's width.
            GeneratorPanel().SizeChanged([weak = get_weak()](auto&&, auto&&)
            {
                if (auto strong = weak.get()) { strong->FitGeneratorPanel(); }
            });

            m_updatingPatternUi = false;
        }
        catch (...)
        {
            m_updatingPatternUi = false;
        }
    }

    void MainWindow::UpdateGeneratorToolbar() noexcept
    {
        try
        {
            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (clip == nullptr || !clip->Generator.has_value())
            {
                return;
            }

            auto const& generator = *clip->Generator;
            auto const& meter = m_doc.Meter.empty() ? seq::MeterChange{} : m_doc.Meter.front();
            auto const type = std::min<size_t>(static_cast<size_t>(generator.Type), TypeKeys.size() - 1);

            m_updatingPatternUi = true;

            GeneratorTypeButton().Content(winrt::box_value(res::GetString(TypeKeys[type])));

            for (uint32_t i = 0; i < GeneratorTypeList().Children().Size(); ++i)
            {
                if (auto const choice = GeneratorTypeList().Children().GetAt(i).try_as<controls::RadioButton>())
                {
                    choice.IsChecked(i == type);
                }
            }

            GeneratorBarsBox().Value(std::round(static_cast<double>(clip->Length) * 100.0 / static_cast<double>(seq::TicksPerBar(meter))) / 100.0);

            int32_t sizeIndex{ -1 };

            for (size_t i = 0; i < std::size(seq::PatternStepSizes); ++i)
            {
                if (seq::PatternStepSizes[i] == generator.StepTicks)
                {
                    sizeIndex = static_cast<int32_t>(i);
                }
            }

            GeneratorStepSizeCombo().SelectedIndex(sizeIndex);
            GeneratorSameToggle().IsChecked(!generator.ChangesEachPass);
            GeneratorChangesToggle().IsChecked(generator.ChangesEachPass);

            // An arpeggiator plays what it's given. Its seed only picks a random order's notes, so
            // the header shows it only then.
            GeneratorSeedBox().Text(winrt::hstring{ std::format(L"{:04X}", clip->Seed) });
            GeneratorSeedTools().Visibility(generator.Type != seq::GeneratorType::Arpeggiator || generator.ArpOrder == seq::ArpeggiatorOrder::Random
                ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            m_updatingPatternUi = false;
        }
        catch (...)
        {
            m_updatingPatternUi = false;
        }
    }

    void MainWindow::UpdateGeneratorInspector() noexcept
    {
        // What the inspector said, what each type does and whether it repeats, is in the type list
        // and along the bottom of the generator now, so it isn't shown for generators at all.
        try
        {
            NoteInspectorBody().Children().Clear();
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    canvasXaml::CanvasControl MainWindow::MakeGeneratorCanvas(std::function<void(canvas::CanvasDrawingSession const&, float, float)> draw)
    {
        canvasXaml::CanvasControl canvas{};
        canvas.ClearColor(winrt::Microsoft::UI::Colors::Transparent());

        canvas.Draw([draw = std::move(draw)](canvasXaml::CanvasControl const& sender, canvasXaml::CanvasDrawEventArgs const& args)
        {
            try
            {
                draw(args.DrawingSession(), static_cast<float>(sender.ActualWidth()), static_cast<float>(sender.ActualHeight()));
            }
            MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to draw a generator.")
        });

        m_generatorCanvases.push_back(canvas);
        return canvas;
    }

    void MainWindow::UpdateGeneratorPanel() noexcept
    {
        try
        {
            auto const body = GeneratorBody();
            body.Children().Clear();
            m_ringCanvas = nullptr;
            m_passCaption = nullptr;
            m_generatorFooter = nullptr;
            m_ringsRoll = nullptr;
            m_ringsRollTitle = nullptr;
            m_generatorCanvases.clear();
            m_ringRows.clear();
            m_ringDrag = RingDrag{};

            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (clip == nullptr || !clip->Generator.has_value())
            {
                return;
            }

            auto const& generator = *clip->Generator;
            auto const secondary = BrushFor(m_palette.Text3);
            std::wstring const undoName{ res::GetString(L"UndoGeneratorSettings") };

            // An edit rebuilds this panel, so it runs after the control's event rather than inside
            // it, and only on the clip it was made for.
            auto const edit = [queue = DispatcherQueue(), weak = get_weak(), undoName, clipId = clip->Id](std::function<void(seq::GeneratorSettings&)> change)
            {
                queue.TryEnqueue([weak, undoName, change, clipId]()
                {
                    if (auto window = weak.get(); window && window->m_editorClipId == clipId)
                    {
                        window->EditGenerator(undoName, change);
                    }
                });
            };

            switch (generator.Type)
            {
            case seq::GeneratorType::Euclidean:
                AddEuclidSettings(body, *clip, edit);
                break;

            case seq::GeneratorType::ChanceMelody:
                AddMelodySettings(body, *clip, edit);
                break;

            case seq::GeneratorType::Arpeggiator:
                AddArpeggiatorSettings(body, generator, edit);
                break;

            default:
                break;
            }

            // Where it plays, and what the seed does, along the bottom (comp 4).
            m_generatorFooter = Text(winrt::hstring{}, secondary, 11);
            m_generatorFooter.Margin(xaml::Thickness{ 0, 4, 0, 0 });
            body.Children().Append(m_generatorFooter);
            UpdateGeneratorFooter();

            FitGeneratorPanel();
            UpdatePassCaption();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the generator.")
    }

    // Shown again when devices come and go, so a track that gets its device after the sequence
    // opens says where it plays.
    void MainWindow::UpdateGeneratorFooter() noexcept
    {
        try
        {
            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (m_generatorFooter == nullptr || clip == nullptr || !clip->Generator.has_value())
            {
                return;
            }

            auto const& generator = *clip->Generator;
            std::wstring footer{};

            if (auto const track = EditorTrack(); track != nullptr)
            {
                // "Plays to" only goes in front of a device. A track without one says so on its own.
                footer = track->Destination.Endpoint.IsEmpty() ? DescribeDestination(*track) :
                    std::wstring{ res::FormatString(L"PatternPlaysToFormat", DescribeDestination(*track)) };

                if (track->Destination.Channel < 0 && !track->Destination.Endpoint.IsEmpty())
                {
                    footer += L" \u00B7 " + std::wstring{ res::FormatString(L"ChannelShortFormat", static_cast<int32_t>(generator.Channel) + 1) };
                }
            }

            auto const hintKey =
                generator.Type == seq::GeneratorType::Arpeggiator ? (generator.ArpSource == seq::ArpeggiatorSource::Held ? L"ArpHeldNote" : L"ArpTrackNote") :
                generator.Type == seq::GeneratorType::Euclidean ? (generator.ChangesEachPass ? L"GeneratorChangesHint" : L"GeneratorSameHint") :
                L"GeneratorRepeatable";

            footer += footer.empty() ? L"" : L"     ";
            footer += std::wstring{ res::GetString(hintKey) };

            m_generatorFooter.Text(winrt::hstring{ footer });
        }
        catch (...)
        {
        }
    }

    // ---------------------------------------------------------------- a Euclidean rhythm

    _Use_decl_annotations_
    void MainWindow::AddEuclidSettings(controls::StackPanel const& body, seq::Clip const& clip, std::function<void(std::function<void(seq::GeneratorSettings&)>)> const& edit)
    {
        auto const& generator = *clip.Generator;
        auto const secondary = BrushFor(m_palette.Text3);
        auto const strong = BrushFor(m_palette.Text2);
        auto const weak = get_weak();

        controls::Grid layout{};
        layout.ColumnSpacing(20);
        AddColumns(layout, { xaml::GridLengthHelper::Auto(), Star() });

        // The rings, drawn. Dragging round a ring turns it, and the wheel over it changes its pulses.
        controls::StackPanel left{};
        left.Spacing(6);
        left.VerticalAlignment(xaml::VerticalAlignment::Top);

        m_ringCanvas = MakeGeneratorCanvas([weak](canvas::CanvasDrawingSession const& ds, float width, float height)
        {
            auto s = weak.get();
            auto const clip = s ? seq::FindClip(s->m_doc, s->m_editorClipId) : nullptr;

            if (clip == nullptr || !clip->Generator.has_value() || clip->Generator->Type != seq::GeneratorType::Euclidean)
            {
                return;
            }

            auto const& rings = s->m_ringDrag.Active ? s->m_ringDrag.Rings : clip->Generator->Rings;
            auto const playing = s->m_engine != nullptr && s->m_engine->IsPlaying() && s->m_editorPlayhead >= 0;
            auto const stepTicks = std::max<int64_t>(1, clip->Generator->StepTicks);
            auto const global = playing ? static_cast<int64_t>(s->m_editorPass) * seq::GeneratorStepCount(*clip) + s->m_editorPlayhead / stepTicks : -1;
            auto const turn = playing ? static_cast<double>(s->m_editorPlayhead) / static_cast<double>(std::max<int64_t>(1, clip->Length)) : -1.0;

            seq::DrawRings(ds, width, height, rings, s->ClipColor(*clip), global, turn, s->m_selectedRing, s->m_palette);
        });

        automation::AutomationProperties::SetName(m_ringCanvas, res::GetString(L"GeneratorRingsDrawing"));
        controls::ToolTipService::SetToolTip(m_ringCanvas, winrt::box_value(res::GetString(L"GeneratorRingsTip")));

        m_ringCanvas.PointerPressed([weak](auto&&, input::PointerRoutedEventArgs const& args) { if (auto s = weak.get()) { s->OnRingPressed(args); } });
        m_ringCanvas.PointerMoved([weak](auto&&, input::PointerRoutedEventArgs const& args) { if (auto s = weak.get()) { s->OnRingMoved(args); } });
        m_ringCanvas.PointerReleased([weak](auto&&, auto&&) { if (auto s = weak.get()) { s->OnRingReleased(); } });
        m_ringCanvas.PointerCaptureLost([weak](auto&&, auto&&) { if (auto s = weak.get()) { s->OnRingReleased(); } });
        m_ringCanvas.PointerWheelChanged([weak](auto&&, input::PointerRoutedEventArgs const& args) { if (auto s = weak.get()) { s->OnRingWheel(args); } });

        left.Children().Append(m_ringCanvas);

        m_passCaption = Text(winrt::hstring{}, secondary, 12);
        m_passCaption.HorizontalAlignment(xaml::HorizontalAlignment::Center);
        left.Children().Append(m_passCaption);

        controls::Border leftEdge{};
        leftEdge.BorderThickness(xaml::Thickness{ 0, 0, 1, 0 });
        leftEdge.BorderBrush(BrushFor(m_palette.Divider));
        leftEdge.Padding(xaml::Thickness{ 0, 0, 20, 0 });
        leftEdge.Child(left);
        Place(layout, leftEdge, 0, 0);

        // The rings as a table: note, pulses, steps, rotate, velocity and chance. What they play
        // goes beside the table or under it, whichever fits (FitGeneratorPanel).
        controls::Grid right{};
        right.ColumnSpacing(20);
        right.RowSpacing(8);
        AddColumns(right, { xaml::GridLengthHelper::Auto(), Star() });
        AddRows(right, 4);

        controls::StackPanel ringsTitle{};
        ringsTitle.Orientation(controls::Orientation::Horizontal);
        ringsTitle.Spacing(12);
        ringsTitle.Children().Append(SectionTitle(res::GetString(L"GeneratorRings"), secondary));
        Place(right, ringsTitle, 0, 0);

        controls::Grid table{};
        table.ColumnSpacing(RingColumnSpacing);
        table.RowSpacing(3);
        table.HorizontalAlignment(xaml::HorizontalAlignment::Left);
        table.VerticalAlignment(xaml::VerticalAlignment::Top);

        for (auto const width : RingColumnWidths)
        {
            AddColumns(table, { Pixels(width) });
        }

        AddRows(table, generator.Rings.size() + 1);

        // Shorter number boxes than the standard, so more rings fit before the table scrolls.
        table.Resources().Insert(winrt::box_value(L"TextControlThemeMinHeight"), winrt::box_value(RingRowHeight));
        table.Resources().Insert(winrt::box_value(L"TextControlThemePadding"), winrt::box_value(xaml::Thickness{ 8, 3, 6, 3 }));

        int32_t column{ 1 };

        for (auto const key : { L"GeneratorRingHeader", L"GeneratorNoteHeader", L"GeneratorPulsesHeader", L"GeneratorStepsHeader",
            L"GeneratorRotateHeader", L"GeneratorVelocityHeader", L"GeneratorChanceHeader" })
        {
            Place(table, Text(res::GetString(key), secondary), 0, column++);
        }

        auto const color = ClipColor(clip);

        for (size_t i = 0; i < generator.Rings.size(); ++i)
        {
            auto const& ring = generator.Rings[i];
            auto const row = static_cast<int32_t>(i) + 1;
            auto const number = static_cast<uint32_t>(i) + 1;

            // Behind the row, lit when its ring is selected.
            controls::Grid band{};
            band.CornerRadius(xaml::CornerRadius{ 4, 4, 4, 4 });
            band.Margin(xaml::Thickness{ -4, -2, -4, -2 });
            controls::Grid::SetColumnSpan(band, 9);
            Place(table, band, row, 0);
            m_ringRows.push_back(band);

            shapes::Ellipse dot{};
            dot.Width(8);
            dot.Height(8);
            dot.Fill(BrushFor(seq::RingColor(color, i)));
            Place(table, dot, row, 0);

            Place(table, Text(winrt::hstring{ std::to_wstring(number) }, strong), row, 1);

            auto const ringBox = [&](wchar_t const* nameKey, double minimum, double maximum, double value, std::optional<bool> note, int32_t at, std::function<void(seq::EuclidRing&, double)> apply)
            {
                auto box = MakeNumberBox(res::FormatString(nameKey, number), minimum, maximum, value, note);

                box.ValueChanged([edit, apply, i](controls::NumberBox const&, controls::NumberBoxValueChangedEventArgs const& args)
                {
                    auto const newValue = args.NewValue();

                    if (std::isfinite(newValue) && newValue != args.OldValue())
                    {
                        edit([apply, newValue, i](seq::GeneratorSettings& edited)
                        {
                            if (i < edited.Rings.size())
                            {
                                apply(edited.Rings[i], newValue);
                            }
                        });
                    }
                });

                // Working on a ring's row picks it out in the drawing.
                box.GotFocus([weak, i](auto&&, auto&&)
                {
                    if (auto s = weak.get())
                    {
                        s->m_selectedRing = i;
                        s->RefreshRingRows();
                    }
                });

                Place(table, box, row, at);
            };

            ringBox(L"GeneratorRingNoteFormat", 0, 127, ring.Note, true, 2, [](seq::EuclidRing& edited, double value) { edited.Note = static_cast<uint8_t>(std::lround(value)); });
            ringBox(L"GeneratorRingPulsesFormat", 0, ring.Steps, ring.Pulses, std::nullopt, 3, [](seq::EuclidRing& edited, double value) { edited.Pulses = static_cast<uint8_t>(std::lround(value)); });
            ringBox(L"GeneratorRingStepsFormat", 1, seq::MaximumRingSteps, ring.Steps, std::nullopt, 4, [](seq::EuclidRing& edited, double value) { edited.Steps = static_cast<uint8_t>(std::lround(value)); });
            ringBox(L"GeneratorRingRotateFormat", 0, std::max(0, ring.Steps - 1), ring.Rotate, std::nullopt, 5, [](seq::EuclidRing& edited, double value) { edited.Rotate = static_cast<uint8_t>(std::lround(value)); });
            ringBox(L"GeneratorRingVelocityFormat", 0, 100, Percent(ring.Velocity), false, 6, [](seq::EuclidRing& edited, double value) { edited.Velocity = FromPercent(value); });
            ringBox(L"GeneratorRingChanceFormat", 0, 100, ring.Chance, false, 7, [](seq::EuclidRing& edited, double value) { edited.Chance = static_cast<uint8_t>(std::clamp(std::lround(value), 0L, 100L)); });

            controls::Button remove{};
            remove.Content(winrt::box_value(L"\uE74D"));
            remove.FontFamily(media::FontFamily{ L"Segoe Fluent Icons" });
            remove.Width(RingRowHeight);
            remove.Height(RingRowHeight);
            remove.MinHeight(0);
            remove.Padding(xaml::Thickness{ 0 });
            remove.Background(BrushFor(winrt::Microsoft::UI::Colors::Transparent()));
            remove.BorderThickness(xaml::Thickness{ 0 });
            automation::AutomationProperties::SetName(remove, res::FormatString(L"GeneratorRemoveRingFormat", number));
            controls::ToolTipService::SetToolTip(remove, winrt::box_value(res::FormatString(L"GeneratorRemoveRingFormat", number)));
            remove.Click([edit, i](auto&&, auto&&)
            {
                edit([i](seq::GeneratorSettings& edited)
                {
                    if (i < edited.Rings.size())
                    {
                        edited.Rings.erase(edited.Rings.begin() + static_cast<ptrdiff_t>(i));
                    }
                });
            });
            Place(table, remove, row, 8);
        }

        Place(right, table, 1, 0);
        RefreshRingRows();

        // Adding a ring sits beside the table's title, which keeps the table and the roll in view.
        if (generator.Rings.size() < seq::MaximumEuclidRings)
        {
            controls::StackPanel addContent{};
            addContent.Orientation(controls::Orientation::Horizontal);
            addContent.Spacing(6);

            controls::FontIcon plus{};
            plus.Glyph(L"\uE710");
            plus.FontSize(12);
            addContent.Children().Append(plus);

            controls::TextBlock addText{};
            addText.Text(res::GetString(L"GeneratorAddRing"));
            addContent.Children().Append(addText);

            controls::Button add{};
            add.Style(RootGrid().Resources().Lookup(winrt::box_value(L"SubtleSmallButtonStyle")).as<xaml::Style>());
            add.Content(addContent);
            automation::AutomationProperties::SetName(add, res::GetString(L"GeneratorAddRing"));
            add.Click([edit](auto&&, auto&&)
            {
                edit([](seq::GeneratorSettings& edited)
                {
                    seq::EuclidRing ring{};
                    ring.Note = edited.Rings.empty() ? uint8_t{ 60 } : static_cast<uint8_t>(std::min(127, edited.Rings.back().Note + 5));
                    ring.Pulses = 4;
                    ring.Steps = 16;
                    edited.Rings.push_back(ring);
                });
            });
            ringsTitle.Children().Append(add);
        }

        // What it plays, two bars of it.
        auto playsTitle = SectionTitle(res::GetString(L"GeneratorWhatItPlays"), secondary);
        Place(right, playsTitle, 2, 0);
        m_ringsRollTitle = playsTitle;

        auto const roll = MakeGeneratorCanvas([weak](canvas::CanvasDrawingSession const& ds, float width, float height)
        {
            auto s = weak.get();
            auto const found = s ? seq::FindClip(s->m_doc, s->m_editorClipId) : nullptr;

            if (found == nullptr || !found->Generator.has_value() || found->Length <= 0)
            {
                return;
            }

            // While a ring is being turned, what it would play.
            auto clip = *found;

            if (s->m_ringDrag.Active)
            {
                clip.Generator->Rings = s->m_ringDrag.Rings;
            }

            auto const& meter = s->m_doc.Meter.empty() ? seq::MeterChange{} : s->m_doc.Meter.front();
            auto const bar = seq::TicksPerBar(meter);
            auto const span = bar * 2;

            std::vector<seq::RollNote> notes{};
            uint8_t low{ 127 };
            uint8_t high{ 0 };

            for (uint64_t pass = 0; static_cast<int64_t>(pass) * clip.Length < span && pass < 64; ++pass)
            {
                auto const start = static_cast<int64_t>(pass) * clip.Length;

                seq::ForEachGeneratorNote(clip, start, start + clip.Length, pass, 0, span, true, [&](seq::Note const& note)
                {
                    notes.push_back(seq::RollNote{ note.Tick, note.Length, note.Number, note.Velocity, false });
                    low = std::min(low, note.Number);
                    high = std::max(high, note.Number);
                });
            }

            if (notes.empty())
            {
                low = 48;
                high = 72;
            }

            low = static_cast<uint8_t>(std::max(0, low - 2));
            high = static_cast<uint8_t>(std::min(127, high + 2));

            while (high - low < 12)
            {
                low = static_cast<uint8_t>(std::max(0, low - 1));
                high = static_cast<uint8_t>(std::min(127, high + 1));
            }

            auto const playing = s->m_engine != nullptr && s->m_engine->IsPlaying() && s->m_editorPlayhead >= 0;
            auto const playhead = playing ? (static_cast<int64_t>(s->m_editorPass) * clip.Length + s->m_editorPlayhead) % span : -1;

            seq::DrawRoll(ds, width, height, notes, span, bar, seq::TicksPerBeat(meter), low, high, RollKeyWidth, s->ClipColor(clip), playhead, s->m_palette);
        });

        automation::AutomationProperties::SetName(roll, res::GetString(L"GeneratorWhatItPlays"));

        controls::Border rollFrame{};
        rollFrame.CornerRadius(xaml::CornerRadius{ 4, 4, 4, 4 });
        rollFrame.BorderThickness(xaml::Thickness{ 1 });
        rollFrame.BorderBrush(BrushFor(m_palette.StrokeStrong));
        rollFrame.Height(UnderRollHeight);
        rollFrame.Child(roll);
        controls::Grid::SetColumnSpan(rollFrame, 2);
        Place(right, rollFrame, 3, 0);
        m_ringsRoll = rollFrame;

        Place(layout, right, 0, 1);
        body.Children().Append(layout);
    }

    void MainWindow::RefreshRingRows() noexcept
    {
        try
        {
            for (size_t i = 0; i < m_ringRows.size(); ++i)
            {
                m_ringRows[i].Background(BrushFor(i == m_selectedRing ? seq::WithAlpha(m_palette.Accent, 0.10) : winrt::Microsoft::UI::Colors::Transparent()));
            }

            if (m_ringCanvas != nullptr)
            {
                m_ringCanvas.Invalidate();
            }
        }
        catch (...)
        {
        }
    }

    void MainWindow::FitGeneratorPanel() noexcept
    {
        try
        {
            if (m_ringCanvas == nullptr)
            {
                return;
            }

            // As tall as the panel allows, leaving room for the caption and the line along the bottom.
            auto const available = GeneratorPanel().ActualHeight() - 28.0 - 26.0 - 28.0;
            auto const size = std::clamp(available, MinimumRingSize, MaximumRingSize);

            m_ringCanvas.Width(size);
            m_ringCanvas.Height(size);

            if (m_ringsRoll == nullptr || m_ringsRollTitle == nullptr)
            {
                return;
            }

            // What's left of the width once the panel's padding, the drawing with its edge and the
            // table have theirs. Beside the table the roll is as tall as the table, or a little
            // taller for a ring or two. Under it, it has a height of its own.
            auto const left = GeneratorPanel().ActualWidth() - 40.0 - (size + 21.0) - 20.0 - RingsTableWidth - 20.0;
            auto const beside = left >= MinimumBesideRollWidth;

            controls::Grid::SetRow(m_ringsRollTitle, beside ? 0 : 2);
            controls::Grid::SetColumn(m_ringsRollTitle, beside ? 1 : 0);
            controls::Grid::SetColumnSpan(m_ringsRollTitle, beside ? 1 : 2);

            controls::Grid::SetRow(m_ringsRoll, beside ? 1 : 3);
            controls::Grid::SetColumn(m_ringsRoll, beside ? 1 : 0);
            controls::Grid::SetColumnSpan(m_ringsRoll, beside ? 1 : 2);
            m_ringsRoll.Height(beside ? std::numeric_limits<double>::quiet_NaN() : UnderRollHeight);
            m_ringsRoll.MinHeight(beside ? MinimumBesideRollHeight : 0.0);
        }
        catch (...)
        {
        }
    }

    void MainWindow::UpdatePassCaption() noexcept
    {
        try
        {
            if (m_passCaption == nullptr)
            {
                return;
            }

            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (clip == nullptr || !clip->Generator.has_value())
            {
                return;
            }

            auto const steps = seq::GeneratorStepCount(*clip);
            winrt::hstring text{};

            if (m_engine != nullptr && m_engine->IsPlaying() && m_editorPlayhead >= 0)
            {
                auto const step = m_editorPlayhead / std::max<int64_t>(1, clip->Generator->StepTicks);
                text = res::FormatString(L"GeneratorPassStepFormat", m_editorPass + 1, step + 1, steps);
            }
            else
            {
                text = res::FormatString(L"GeneratorStepsFormat", steps);
            }

            if (m_passCaption.Text() != text)
            {
                m_passCaption.Text(text);
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    seq::Color MainWindow::ClipColor(seq::Clip const& clip) const
    {
        if (clip.Color.has_value())
        {
            return seq::FromRgb(*clip.Color);
        }

        auto const track = EditorTrack();
        return seq::FromRgb(track != nullptr ? track->Color : 0x6CCB5F);
    }

    _Use_decl_annotations_
    void MainWindow::OnRingPressed(input::PointerRoutedEventArgs const& args)
    {
        try
        {
            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (m_ringCanvas == nullptr || clip == nullptr || !clip->Generator.has_value() || m_readOnly)
            {
                return;
            }

            auto const point = args.GetCurrentPoint(m_ringCanvas);

            if (!point.Properties().IsLeftButtonPressed())
            {
                return;
            }

            auto const& rings = clip->Generator->Rings;
            auto const geometry = seq::MeasureRings(static_cast<float>(m_ringCanvas.ActualWidth()), static_cast<float>(m_ringCanvas.ActualHeight()), rings.size());
            auto const ring = seq::RingAtPoint(geometry, rings.size(), point.Position().X, point.Position().Y);

            if (ring == SIZE_MAX)
            {
                return;
            }

            m_selectedRing = ring;

            RingDrag drag{};
            drag.Active = true;
            drag.Ring = ring;
            drag.StartTurn = seq::TurnAtPoint(geometry, point.Position().X, point.Position().Y);
            drag.StartRotate = rings[ring].Rotate;
            drag.Rings = rings;
            m_ringDrag = std::move(drag);

            m_ringCanvas.CapturePointer(args.Pointer());
            RefreshRingRows();
            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to pick up a ring.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRingMoved(input::PointerRoutedEventArgs const& args)
    {
        try
        {
            if (!m_ringDrag.Active || m_ringCanvas == nullptr || m_ringDrag.Ring >= m_ringDrag.Rings.size())
            {
                return;
            }

            auto const point = args.GetCurrentPoint(m_ringCanvas).Position();
            auto const geometry = seq::MeasureRings(static_cast<float>(m_ringCanvas.ActualWidth()), static_cast<float>(m_ringCanvas.ActualHeight()), m_ringDrag.Rings.size());

            // Clockwise turns the hits later, the way Rotate does.
            auto delta = seq::TurnAtPoint(geometry, point.X, point.Y) - m_ringDrag.StartTurn;

            if (delta > 0.5)
            {
                delta -= 1.0;
            }
            else if (delta < -0.5)
            {
                delta += 1.0;
            }

            auto& ring = m_ringDrag.Rings[m_ringDrag.Ring];
            auto const steps = static_cast<int64_t>(std::max<uint8_t>(1, ring.Steps));
            auto const moved = static_cast<int64_t>(std::lround(delta * static_cast<double>(steps)));
            auto const rotate = static_cast<uint8_t>(((static_cast<int64_t>(m_ringDrag.StartRotate) + moved) % steps + steps) % steps);

            if (rotate != ring.Rotate)
            {
                ring.Rotate = rotate;
                m_ringDrag.Moved = true;

                for (auto const& canvas : m_generatorCanvases)
                {
                    canvas.Invalidate();
                }
            }

            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to turn a ring.")
    }

    void MainWindow::OnRingReleased()
    {
        try
        {
            if (!m_ringDrag.Active)
            {
                return;
            }

            // Cleared first: letting go of the capture comes straight back here.
            auto drag = std::move(m_ringDrag);
            m_ringDrag = RingDrag{};

            if (m_ringCanvas != nullptr)
            {
                m_ringCanvas.ReleasePointerCaptures();
                m_ringCanvas.Invalidate();
            }

            if (!drag.Moved)
            {
                return;
            }

            EditGenerator(std::wstring{ res::GetString(L"UndoGeneratorSettings") }, [&drag](seq::GeneratorSettings& generator)
            {
                if (drag.Ring < generator.Rings.size() && drag.Ring < drag.Rings.size())
                {
                    generator.Rings[drag.Ring].Rotate = drag.Rings[drag.Ring].Rotate;
                }
            });
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to turn a ring.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRingWheel(input::PointerRoutedEventArgs const& args)
    {
        try
        {
            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (m_ringCanvas == nullptr || clip == nullptr || !clip->Generator.has_value() || m_readOnly || m_ringDrag.Active)
            {
                return;
            }

            auto const point = args.GetCurrentPoint(m_ringCanvas);
            auto const& rings = clip->Generator->Rings;
            auto const geometry = seq::MeasureRings(static_cast<float>(m_ringCanvas.ActualWidth()), static_cast<float>(m_ringCanvas.ActualHeight()), rings.size());
            auto const ring = seq::RingAtPoint(geometry, rings.size(), point.Position().X, point.Position().Y);

            if (ring == SIZE_MAX)
            {
                return;
            }

            // The wheel adds or takes away a pulse, and with Shift a step.
            auto const up = point.Properties().MouseWheelDelta() > 0;
            auto const shift = (args.KeyModifiers() & winrt::Windows::System::VirtualKeyModifiers::Shift) == winrt::Windows::System::VirtualKeyModifiers::Shift;

            m_selectedRing = ring;
            args.Handled(true);

            EditGenerator(std::wstring{ res::GetString(L"UndoGeneratorSettings") }, [ring, up, shift](seq::GeneratorSettings& generator)
            {
                if (ring >= generator.Rings.size())
                {
                    return;
                }

                auto& edited = generator.Rings[ring];

                if (shift)
                {
                    edited.Steps = static_cast<uint8_t>(std::clamp(edited.Steps + (up ? 1 : -1), 1, static_cast<int32_t>(seq::MaximumRingSteps)));
                    edited.Pulses = std::min(edited.Pulses, edited.Steps);
                }
                else
                {
                    edited.Pulses = static_cast<uint8_t>(std::clamp(edited.Pulses + (up ? 1 : -1), 0, static_cast<int32_t>(edited.Steps)));
                }
            });
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change a ring.")
    }

    // ---------------------------------------------------------------- a chance melody

    _Use_decl_annotations_
    controls::Slider MainWindow::MakeGeneratorSlider(winrt::hstring const& name, double maximum, double value, controls::TextBlock const& readout, std::function<void(double)> commit)
    {
        controls::Slider slider{};
        slider.Minimum(0);
        slider.Maximum(maximum);
        slider.StepFrequency(1);
        slider.SmallChange(1);
        slider.LargeChange(10);
        slider.Value(value);
        slider.VerticalAlignment(xaml::VerticalAlignment::Center);
        automation::AutomationProperties::SetName(slider, name);
        readout.Text(winrt::hstring{ std::format(L"{}%", std::lround(value)) });

        // Kept once it rests and isn't held, so a drag is one edit and the panel isn't rebuilt
        // under the pointer.
        auto const pending = std::make_shared<double>(value);
        auto const held = std::make_shared<bool>(false);
        auto const kept = std::make_shared<double>(value);

        xaml::DispatcherTimer rest{};
        rest.Interval(SliderRest);

        auto const keep = [pending, kept, commit]()
        {
            if (*pending != *kept)
            {
                *kept = *pending;
                commit(*pending);
            }
        };

        rest.Tick([held, keep](foundation::IInspectable const& sender, auto&&)
        {
            if (!*held)
            {
                sender.as<xaml::DispatcherTimer>().Stop();
                keep();
            }
        });

        slider.ValueChanged([readout, pending, rest](auto&&, primitives::RangeBaseValueChangedEventArgs const& args)
        {
            *pending = args.NewValue();
            readout.Text(winrt::hstring{ std::format(L"{}%", std::lround(args.NewValue())) });
            rest.Stop();
            rest.Start();
        });

        slider.AddHandler(xaml::UIElement::PointerPressedEvent(), winrt::box_value(input::PointerEventHandler([held](auto&&, auto&&)
        {
            *held = true;
        })), true);

        auto const letGo = winrt::box_value(input::PointerEventHandler([held, rest, keep](auto&&, auto&&)
        {
            *held = false;
            rest.Stop();
            keep();
        }));

        slider.AddHandler(xaml::UIElement::PointerReleasedEvent(), letGo, true);
        slider.AddHandler(xaml::UIElement::PointerCaptureLostEvent(), letGo, true);

        return slider;
    }

    _Use_decl_annotations_
    void MainWindow::AddMelodySettings(controls::StackPanel const& body, seq::Clip const& clip, std::function<void(std::function<void(seq::GeneratorSettings&)>)> const& edit)
    {
        auto const& generator = *clip.Generator;
        auto const secondary = BrushFor(m_palette.Text3);
        auto const strong = BrushFor(m_palette.Text2);

        controls::Grid layout{};
        layout.ColumnSpacing(20);
        AddColumns(layout, { Pixels(300), Star() });

        // Its settings, down the left (comp 4).
        controls::StackPanel form{};
        form.Spacing(6);

        auto const pair = [&](xaml::FrameworkElement const& first, xaml::FrameworkElement const& second, wchar_t const* joinKey)
        {
            controls::Grid grid{};
            grid.ColumnSpacing(8);

            if (joinKey != nullptr)
            {
                AddColumns(grid, { Star(), xaml::GridLengthHelper::Auto(), Star() });
                Place(grid, first, 0, 0);
                Place(grid, Text(res::GetString(joinKey), secondary), 0, 1);
                Place(grid, second, 0, 2);
            }
            else
            {
                AddColumns(grid, { Star(), Pixels(44) });
                Place(grid, first, 0, 0);
                Place(grid, second, 0, 1);
            }

            first.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
            form.Children().Append(grid);
        };

        auto const heading = [&](wchar_t const* key)
        {
            auto block = Text(res::GetString(key), secondary, 12);
            block.Margin(xaml::Thickness{ 0, 6, 0, 0 });
            form.Children().Append(block);
        };

        // The key and the scale.
        heading(L"GeneratorScale");

        controls::ComboBox key{};
        automation::AutomationProperties::SetName(key, res::GetString(L"GeneratorKey"));

        for (auto const name : KeyNames)
        {
            key.Items().Append(winrt::box_value(winrt::hstring{ name }));
        }

        key.SelectedIndex(generator.ScaleRoot % 12);
        key.SelectionChanged([edit](foundation::IInspectable const& sender, auto&&)
        {
            auto const selected = sender.as<controls::ComboBox>().SelectedIndex();

            if (selected >= 0)
            {
                edit([selected](seq::GeneratorSettings& edited) { edited.ScaleRoot = static_cast<uint8_t>(selected); });
            }
        });

        controls::ComboBox scale{};
        automation::AutomationProperties::SetName(scale, res::GetString(L"GeneratorScale"));

        for (auto const name : ScaleKeys)
        {
            scale.Items().Append(winrt::box_value(res::GetString(name)));
        }

        scale.SelectedIndex(static_cast<int32_t>(generator.Scale));
        scale.SelectionChanged([edit](foundation::IInspectable const& sender, auto&&)
        {
            auto const selected = sender.as<controls::ComboBox>().SelectedIndex();

            if (selected >= 0)
            {
                edit([selected](seq::GeneratorSettings& edited) { edited.Scale = static_cast<seq::ScaleKind>(selected); });
            }
        });

        {
            controls::Grid grid{};
            grid.ColumnSpacing(8);
            AddColumns(grid, { Pixels(76), Star() });
            key.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
            scale.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
            Place(grid, key, 0, 0);
            Place(grid, scale, 0, 1);
            form.Children().Append(grid);
        }

        auto const numberBox = [&](wchar_t const* nameKey, double minimum, double maximum, double value, std::optional<bool> note, std::function<void(seq::GeneratorSettings&, double)> apply)
        {
            auto box = MakeNumberBox(res::GetString(nameKey), minimum, maximum, value, note);

            box.ValueChanged([edit, apply](controls::NumberBox const&, controls::NumberBoxValueChangedEventArgs const& args)
            {
                auto const newValue = args.NewValue();

                if (std::isfinite(newValue) && newValue != args.OldValue())
                {
                    edit([apply, newValue](seq::GeneratorSettings& edited) { apply(edited, newValue); });
                }
            });

            return box;
        };

        // The range.
        heading(L"GeneratorRange");
        pair(numberBox(L"GeneratorLowNote", 0, 127, generator.LowNote, true, [](seq::GeneratorSettings& edited, double value) { edited.LowNote = static_cast<uint8_t>(std::lround(value)); }),
            numberBox(L"GeneratorHighNote", 0, 127, generator.HighNote, true, [](seq::GeneratorSettings& edited, double value) { edited.HighNote = static_cast<uint8_t>(std::lround(value)); }),
            L"GeneratorRangeTo");

        // How busy, and how much it keeps from the pass before, as sliders.
        auto const clipId = clip.Id;
        auto const weak = get_weak();

        heading(L"GeneratorBusy");
        auto busyValue = Text(winrt::hstring{}, strong);
        auto busy = MakeGeneratorSlider(res::GetString(L"GeneratorBusy"), 100, generator.Busy, busyValue, [edit](double value)
        {
            edit([value](seq::GeneratorSettings& edited) { edited.Busy = static_cast<uint8_t>(std::clamp(std::lround(value), 0L, 100L)); });
        });
        pair(busy, busyValue, nullptr);

        heading(L"GeneratorKeep");
        auto keepValue = Text(winrt::hstring{}, strong);
        auto keep = MakeGeneratorSlider(res::GetString(L"GeneratorKeep"), 100, generator.Keep, keepValue, [edit](double value)
        {
            edit([value](seq::GeneratorSettings& edited) { edited.Keep = static_cast<uint8_t>(std::clamp(std::lround(value), 0L, 100L)); });
        });
        keep.IsEnabled(generator.ChangesEachPass);
        pair(keep, keepValue, nullptr);
        form.Children().Append(Text(res::GetString(generator.ChangesEachPass ? L"GeneratorKeepHelp" : L"GeneratorKeepOff"), secondary, 11));

        // Velocity, and how long each note lasts.
        heading(L"GeneratorVelocity");
        pair(numberBox(L"GeneratorVelocityFrom", 0, 100, Percent(generator.VelocityLow), false, [](seq::GeneratorSettings& edited, double value) { edited.VelocityLow = FromPercent(value); }),
            numberBox(L"GeneratorVelocityTo", 0, 100, Percent(generator.VelocityHigh), false, [](seq::GeneratorSettings& edited, double value) { edited.VelocityHigh = FromPercent(value); }),
            L"GeneratorRangeTo");

        heading(L"GeneratorNoteLength");
        {
            auto length = numberBox(L"GeneratorNoteLength", 1, 1600, generator.NoteLength, false,
                [](seq::GeneratorSettings& edited, double value) { edited.NoteLength = static_cast<uint16_t>(std::clamp(std::lround(value), 1L, 1600L)); });
            length.HorizontalAlignment(xaml::HorizontalAlignment::Left);
            length.Width(140);
            form.Children().Append(length);
        }

        controls::Button makeNotes{};
        makeNotes.Style(RootGrid().Resources().Lookup(winrt::box_value(L"SmallButtonStyle")).as<xaml::Style>());
        makeNotes.Content(winrt::box_value(res::GetString(L"GeneratorMakeNotesFromPasses")));
        makeNotes.Margin(xaml::Thickness{ 0, 8, 0, 0 });
        makeNotes.Click([weak, clipId](auto&&, auto&&)
        {
            if (auto s = weak.get(); s && s->m_editorClipId == clipId)
            {
                s->MakeNotesFromPattern();
            }
        });
        form.Children().Append(makeNotes);

        controls::Border formEdge{};
        formEdge.BorderThickness(xaml::Thickness{ 0, 0, 1, 0 });
        formEdge.BorderBrush(BrushFor(m_palette.Divider));
        formEdge.Padding(xaml::Thickness{ 0, 0, 20, 0 });
        formEdge.Child(form);
        Place(layout, formEdge, 0, 0);

        // The first four passes, each with what's new since the one before outlined. One is enough
        // when every pass is the same.
        controls::StackPanel passes{};
        passes.Spacing(8);
        passes.Children().Append(SectionTitle(res::GetString(generator.ChangesEachPass ? L"GeneratorFirstPasses" : L"GeneratorEveryPass"), secondary));

        auto const count = generator.ChangesEachPass ? uint64_t{ 4 } : uint64_t{ 1 };
        auto const low = static_cast<uint8_t>(std::max(0, std::min(generator.LowNote, generator.HighNote) - 1));
        auto const high = static_cast<uint8_t>(std::min(127, std::max(generator.LowNote, generator.HighNote) + 1));
        auto const color = ClipColor(clip);

        std::vector<seq::RollNote> previous{};

        for (uint64_t pass = 0; pass < count; ++pass)
        {
            auto notes = std::make_shared<std::vector<seq::RollNote>>();
            size_t fresh{ 0 };

            seq::ForEachGeneratorNote(clip, 0, clip.Length, pass, 0, clip.Length, true, [&](seq::Note const& note)
            {
                auto const before = std::any_of(previous.begin(), previous.end(), [&note](seq::RollNote const& old)
                {
                    return old.Tick == note.Tick && old.Number == note.Number;
                });

                auto const isNew = pass > 0 && !before;
                fresh += isNew ? 1 : 0;
                notes->push_back(seq::RollNote{ note.Tick, note.Length, note.Number, note.Velocity, isNew });
            });

            previous = *notes;

            controls::Grid row{};
            row.ColumnSpacing(10);
            AddColumns(row, { Pixels(84), Star() });

            controls::StackPanel label{};
            label.VerticalAlignment(xaml::VerticalAlignment::Top);
            label.Margin(xaml::Thickness{ 0, 4, 0, 0 });

            auto passName = Text(res::FormatString(L"GeneratorPassFormat", pass + 1), strong, 12);
            passName.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
            label.Children().Append(passName);

            auto const detail = pass == 0
                ? res::GetString(generator.ChangesEachPass ? L"GeneratorFirstPass" : L"GeneratorEveryPassDetail")
                : res::FormatString(fresh == 1 ? L"GeneratorNewNoteOne" : L"GeneratorNewNotesFormat", fresh);
            label.Children().Append(Text(detail, secondary, 11));
            Place(row, label, 0, 0);

            auto const length = clip.Length;
            auto const canvas = MakeGeneratorCanvas([weak, notes, length, low, high, color, pass](canvas::CanvasDrawingSession const& ds, float width, float height)
            {
                auto s = weak.get();

                if (!s)
                {
                    return;
                }

                auto const& meter = s->m_doc.Meter.empty() ? seq::MeterChange{} : s->m_doc.Meter.front();
                auto const playing = s->m_engine != nullptr && s->m_engine->IsPlaying() && s->m_editorPlayhead >= 0 && s->m_editorPass % 4 == pass;

                seq::DrawRoll(ds, width, height, *notes, length, seq::TicksPerBar(meter), seq::TicksPerBeat(meter), low, high, 0.0f, color, playing ? s->m_editorPlayhead : -1, s->m_palette);
            });

            canvas.Height(PassRollHeight);
            automation::AutomationProperties::SetName(canvas, res::FormatString(L"GeneratorPassFormat", pass + 1));

            controls::Border frame{};
            frame.CornerRadius(xaml::CornerRadius{ 4, 4, 4, 4 });
            frame.BorderThickness(xaml::Thickness{ 1 });
            frame.BorderBrush(BrushFor(m_palette.StrokeStrong));
            frame.Child(canvas);
            Place(row, frame, 0, 1);

            passes.Children().Append(row);
        }

        Place(layout, passes, 0, 1);
        body.Children().Append(layout);
    }

    // ---------------------------------------------------------------- edits

    _Use_decl_annotations_
    void MainWindow::EditGenerator(std::wstring const& name, std::function<void(seq::GeneratorSettings&)> const& edit) noexcept
    {
        try
        {
            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (clip == nullptr || clip->Kind != seq::ClipKind::Generator || !clip->Generator.has_value())
            {
                return;
            }

            auto after = *clip->Generator;
            edit(after);
            seq::NormalizeGenerator(after);

            if (SameGenerator(*clip->Generator, after))
            {
                UpdateGeneratorPanel();
                return;
            }

            if (m_readOnly)
            {
                ShowMessage(res::GetString(L"ReadOnlyEdit"));
            }

            seq::ChangeList changes{};
            changes.push_back(seq::MakeClipGeneratorChange(m_editorClipId, *clip->Generator, std::move(after)));

            m_undo.ApplyAndCommit(m_doc, name, std::move(changes));
            DocumentChanged();
            RefreshEditor();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change the generator.")
    }

    _Use_decl_annotations_
    void MainWindow::SetGeneratorType(seq::GeneratorType type) noexcept
    {
        EditGenerator(std::wstring{ res::GetString(L"UndoGeneratorSettings") }, [type](seq::GeneratorSettings& generator)
        {
            generator.Type = type;

            // A Euclidean rhythm starts with the comp's rings rather than none.
            if (generator.Type == seq::GeneratorType::Euclidean && generator.Rings.empty())
            {
                generator.Rings = seq::MakeEuclideanGenerator().Rings;
            }
        });

        UpdateGeneratorToolbar();
    }

    // ---------------------------------------------------------------- the header

    _Use_decl_annotations_
    void MainWindow::OnGeneratorBarsChanged(controls::NumberBox const&, controls::NumberBoxValueChangedEventArgs const& args)
    {
        try
        {
            if (m_updatingPatternUi)
            {
                return;
            }

            auto const clip = seq::FindClip(m_doc, m_editorClipId);
            auto const value = args.NewValue();

            if (clip == nullptr || !std::isfinite(value))
            {
                UpdateGeneratorToolbar();
                return;
            }

            auto const& meter = m_doc.Meter.empty() ? seq::MeterChange{} : m_doc.Meter.front();
            auto const bars = std::clamp(std::lround(value), 1L, 16L);
            auto const length = static_cast<int64_t>(bars) * seq::TicksPerBar(meter);

            if (length == clip->Length)
            {
                return;
            }

            auto changed = *clip;
            changed.Length = length;

            seq::ChangeList changes{};
            changes.push_back(seq::MakeClipSettingsChange(*clip, changed));

            m_undo.ApplyAndCommit(m_doc, std::wstring{ res::GetString(L"UndoGeneratorSettings") }, std::move(changes));
            DocumentChanged();
            RefreshEditor();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change the generator's length.")
    }

    _Use_decl_annotations_
    void MainWindow::OnGeneratorStepSizeChanged(foundation::IInspectable const&, controls::SelectionChangedEventArgs const&)
    {
        if (m_updatingPatternUi)
        {
            return;
        }

        auto const index = GeneratorStepSizeCombo().SelectedIndex();

        if (index < 0 || static_cast<size_t>(index) >= std::size(seq::PatternStepSizes))
        {
            return;
        }

        auto const ticks = seq::PatternStepSizes[static_cast<size_t>(index)];
        EditGenerator(std::wstring{ res::GetString(L"UndoGeneratorSettings") }, [ticks](seq::GeneratorSettings& generator) { generator.StepTicks = ticks; });
    }

    _Use_decl_annotations_
    void MainWindow::OnGeneratorSameClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        EditGenerator(std::wstring{ res::GetString(L"UndoGeneratorSettings") }, [](seq::GeneratorSettings& generator) { generator.ChangesEachPass = false; });
        UpdateGeneratorToolbar();
    }

    _Use_decl_annotations_
    void MainWindow::OnGeneratorChangesClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        EditGenerator(std::wstring{ res::GetString(L"UndoGeneratorSettings") }, [](seq::GeneratorSettings& generator) { generator.ChangesEachPass = true; });
        UpdateGeneratorToolbar();
    }

    _Use_decl_annotations_
    void MainWindow::OnGeneratorSeedKeyDown(foundation::IInspectable const&, input::KeyRoutedEventArgs const& args)
    {
        if (args.Key() == winrt::Windows::System::VirtualKey::Enter)
        {
            CommitGeneratorSeed();
            args.Handled(true);
        }
        else if (args.Key() == winrt::Windows::System::VirtualKey::Escape)
        {
            UpdateGeneratorToolbar();
            args.Handled(true);
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnGeneratorSeedCommitted(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        CommitGeneratorSeed();
    }

    void MainWindow::CommitGeneratorSeed() noexcept
    {
        try
        {
            if (m_updatingPatternUi)
            {
                return;
            }

            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (clip == nullptr || !clip->Generator.has_value())
            {
                return;
            }

            // The seed as the header shows it: up to eight hex digits.
            std::wstring const text{ GeneratorSeedBox().Text() };
            uint32_t seed{ 0 };
            auto valid = !text.empty() && text.size() <= 8;

            for (auto const ch : text)
            {
                auto const digit = std::iswdigit(ch) ? ch - L'0' : (std::towupper(ch) >= L'A' && std::towupper(ch) <= L'F') ? std::towupper(ch) - L'A' + 10 : -1;

                if (digit < 0)
                {
                    valid = false;
                    break;
                }

                seed = (seed << 4) | static_cast<uint32_t>(digit);
            }

            if (!valid)
            {
                ShowMessage(res::GetString(L"GeneratorSeedInvalid"));
                UpdateGeneratorToolbar();
                return;
            }

            if (seed == clip->Seed)
            {
                return;
            }

            auto changed = *clip;
            changed.Seed = seed;

            seq::ChangeList changes{};
            changes.push_back(seq::MakeClipSettingsChange(*clip, changed));

            m_undo.ApplyAndCommit(m_doc, std::wstring{ res::GetString(L"UndoSeed") }, std::move(changes));
            DocumentChanged();
            RefreshEditor();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change the seed.")
    }
}
