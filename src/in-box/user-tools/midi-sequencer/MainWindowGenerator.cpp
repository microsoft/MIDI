// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "SequenceGenerator.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;

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

        void Place(controls::Grid const& grid, xaml::FrameworkElement const& element, int32_t row, int32_t column)
        {
            controls::Grid::SetRow(element, row);
            controls::Grid::SetColumn(element, column);
            grid.Children().Append(element);
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

            for (auto const key : TypeKeys)
            {
                GeneratorTypeCombo().Items().Append(winrt::box_value(res::GetString(key)));
            }

            for (auto const ticks : seq::PatternStepSizes)
            {
                GeneratorStepSizeCombo().Items().Append(winrt::box_value(res::GetString(L"PatternStepSize" + std::to_wstring(ticks))));
            }

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

            m_updatingPatternUi = true;

            GeneratorTypeCombo().SelectedIndex(static_cast<int32_t>(generator.Type));
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
            GeneratorChangesToggle().IsChecked(generator.ChangesEachPass);

            m_updatingPatternUi = false;
        }
        catch (...)
        {
            m_updatingPatternUi = false;
        }
    }

    void MainWindow::UpdateGeneratorInspector() noexcept
    {
        try
        {
            auto const body = NoteInspectorBody();
            body.Children().Clear();

            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (clip == nullptr || !clip->Generator.has_value())
            {
                return;
            }

            auto const type = std::min<size_t>(static_cast<size_t>(clip->Generator->Type), TypeKeys.size() - 1);
            NoteInspectorTitle().Text(res::GetString(TypeKeys[type]));
            body.Children().Append(Text(res::GetString(AboutKeys[type]), BrushFor(m_palette.Text2)));

            // An arpeggiator that plays what's held can't be played back from its seed.
            auto const repeatKey = clip->Generator->Type != seq::GeneratorType::Arpeggiator ? L"GeneratorRepeatable" :
                clip->Generator->ArpSource == seq::ArpeggiatorSource::Held ? L"ArpHeldNote" : L"ArpTrackNote";

            auto repeat = Text(res::GetString(repeatKey), BrushFor(m_palette.Text3));
            repeat.Margin(xaml::Thickness{ 0, 10, 0, 0 });
            body.Children().Append(repeat);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to describe the generator.")
    }

    void MainWindow::UpdateGeneratorPanel() noexcept
    {
        try
        {
            auto const body = GeneratorBody();
            body.Children().Clear();

            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (clip == nullptr || !clip->Generator.has_value())
            {
                return;
            }

            auto const& generator = *clip->Generator;
            auto const secondary = BrushFor(m_palette.Text3);
            auto const strong = BrushFor(m_palette.Text2);
            std::wstring const undoName{ res::GetString(L"UndoGeneratorSettings") };

            // An edit rebuilds this panel, so it runs after the control's event rather than inside it.
            auto const edit = [queue = DispatcherQueue(), weak = get_weak(), undoName](std::function<void(seq::GeneratorSettings&)> change)
            {
                queue.TryEnqueue([weak, undoName, change]()
                {
                    if (auto window = weak.get())
                    {
                        window->EditGenerator(undoName, change);
                    }
                });
            };

            auto const numberBox = [&](winrt::hstring const& name, bool header, double minimum, double maximum, double value, std::function<void(seq::GeneratorSettings&, double)> apply)
            {
                controls::NumberBox box{};

                if (header)
                {
                    box.Header(winrt::box_value(name));
                }
                else
                {
                    automation::AutomationProperties::SetName(box, name);
                }

                box.Minimum(minimum);
                box.Maximum(maximum);
                box.SmallChange(1);
                box.LargeChange(10);
                box.Value(value);
                box.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);

                // A box with a description is taller. The others in its row keep their own height.
                box.VerticalAlignment(xaml::VerticalAlignment::Top);

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

            switch (generator.Type)
            {
            case seq::GeneratorType::Euclidean:
            {
                body.Children().Append(Text(res::GetString(L"GeneratorRings"), strong, 13));

                // One row for each ring: note, pulses, steps, rotate, velocity, chance.
                controls::Grid table{};
                table.ColumnSpacing(10);
                table.RowSpacing(6);

                for (auto const width : { 40.0, 150.0, 100.0, 100.0, 100.0, 120.0, 120.0, 40.0 })
                {
                    controls::ColumnDefinition column{};
                    column.Width(xaml::GridLength{ width, xaml::GridUnitType::Pixel });
                    table.ColumnDefinitions().Append(column);
                }

                for (size_t row = 0; row <= generator.Rings.size(); ++row)
                {
                    controls::RowDefinition definition{};
                    definition.Height(xaml::GridLengthHelper::Auto());
                    table.RowDefinitions().Append(definition);
                }

                int32_t column{ 0 };

                for (auto const key : { L"GeneratorRingHeader", L"GeneratorNoteHeader", L"GeneratorPulsesHeader", L"GeneratorStepsHeader",
                    L"GeneratorRotateHeader", L"GeneratorVelocityHeader", L"GeneratorChanceHeader" })
                {
                    Place(table, Text(res::GetString(key), secondary), 0, column++);
                }

                for (size_t i = 0; i < generator.Rings.size(); ++i)
                {
                    auto const& ring = generator.Rings[i];
                    auto const row = static_cast<int32_t>(i) + 1;
                    auto const number = static_cast<uint32_t>(i) + 1;

                    auto const ringBox = [&](wchar_t const* nameKey, double minimum, double maximum, double value, std::function<void(seq::EuclidRing&, double)> apply)
                    {
                        return numberBox(res::FormatString(nameKey, number), false, minimum, maximum, value, [i, apply](seq::GeneratorSettings& edited, double newValue)
                        {
                            if (i < edited.Rings.size())
                            {
                                apply(edited.Rings[i], newValue);
                            }
                        });
                    };

                    Place(table, Text(winrt::hstring{ std::to_wstring(number) }, strong), row, 0);

                    auto note = ringBox(L"GeneratorRingNoteFormat", 0, 127, ring.Note, [](seq::EuclidRing& edited, double value) { edited.Note = static_cast<uint8_t>(std::lround(value)); });
                    note.Description(winrt::box_value(winrt::hstring{ seq::NoteLabel(ring.Note) }));
                    Place(table, note, row, 1);

                    Place(table, ringBox(L"GeneratorRingPulsesFormat", 0, ring.Steps, ring.Pulses, [](seq::EuclidRing& edited, double value) { edited.Pulses = static_cast<uint8_t>(std::lround(value)); }), row, 2);
                    Place(table, ringBox(L"GeneratorRingStepsFormat", 1, seq::MaximumRingSteps, ring.Steps, [](seq::EuclidRing& edited, double value) { edited.Steps = static_cast<uint8_t>(std::lround(value)); }), row, 3);
                    Place(table, ringBox(L"GeneratorRingRotateFormat", 0, std::max(0, ring.Steps - 1), ring.Rotate, [](seq::EuclidRing& edited, double value) { edited.Rotate = static_cast<uint8_t>(std::lround(value)); }), row, 4);
                    Place(table, ringBox(L"GeneratorRingVelocityFormat", 0, 100, Percent(ring.Velocity), [](seq::EuclidRing& edited, double value) { edited.Velocity = FromPercent(value); }), row, 5);
                    Place(table, ringBox(L"GeneratorRingChanceFormat", 0, 100, ring.Chance, [](seq::EuclidRing& edited, double value) { edited.Chance = static_cast<uint8_t>(std::clamp(std::lround(value), 0L, 100L)); }), row, 6);

                    controls::Button remove{};
                    remove.Content(winrt::box_value(L"\uE74D"));
                    remove.FontFamily(media::FontFamily{ L"Segoe Fluent Icons" });
                    remove.Width(32);
                    remove.Height(32);
                    remove.Padding(xaml::Thickness{ 0 });
                    remove.VerticalAlignment(xaml::VerticalAlignment::Top);
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
                    Place(table, remove, row, 7);
                }

                body.Children().Append(table);

                if (generator.Rings.size() < seq::MaximumEuclidRings)
                {
                    controls::Button add{};
                    add.Content(winrt::box_value(res::GetString(L"GeneratorAddRing")));
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
                    body.Children().Append(add);
                }

                body.Children().Append(Text(res::GetString(generator.ChangesEachPass ? L"GeneratorChangesHint" : L"GeneratorSameHint"), secondary));
                break;
            }

            case seq::GeneratorType::ChanceMelody:
            {
                controls::Grid form{};
                form.ColumnSpacing(18);
                form.RowSpacing(12);

                for (int32_t i = 0; i < 4; ++i)
                {
                    controls::ColumnDefinition column{};
                    column.Width(xaml::GridLength{ 170, xaml::GridUnitType::Pixel });
                    form.ColumnDefinitions().Append(column);
                }

                for (int32_t i = 0; i < 3; ++i)
                {
                    controls::RowDefinition definition{};
                    definition.Height(xaml::GridLengthHelper::Auto());
                    form.RowDefinitions().Append(definition);
                }

                // The key and the scale.
                controls::ComboBox key{};
                key.Header(winrt::box_value(res::GetString(L"GeneratorKey")));
                key.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

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
                Place(form, key, 0, 0);

                controls::ComboBox scale{};
                scale.Header(winrt::box_value(res::GetString(L"GeneratorScale")));
                scale.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

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
                Place(form, scale, 0, 1);

                // The range.
                auto low = numberBox(res::GetString(L"GeneratorLowNote"), true, 0, 127, generator.LowNote, [](seq::GeneratorSettings& edited, double value) { edited.LowNote = static_cast<uint8_t>(std::lround(value)); });
                low.Description(winrt::box_value(winrt::hstring{ seq::NoteLabel(generator.LowNote) }));
                Place(form, low, 0, 2);

                auto high = numberBox(res::GetString(L"GeneratorHighNote"), true, 0, 127, generator.HighNote, [](seq::GeneratorSettings& edited, double value) { edited.HighNote = static_cast<uint8_t>(std::lround(value)); });
                high.Description(winrt::box_value(winrt::hstring{ seq::NoteLabel(generator.HighNote) }));
                Place(form, high, 0, 3);

                // How busy, and how much it changes.
                Place(form, numberBox(res::GetString(L"GeneratorBusy"), true, 0, 100, generator.Busy,
                    [](seq::GeneratorSettings& edited, double value) { edited.Busy = static_cast<uint8_t>(std::clamp(std::lround(value), 0L, 100L)); }), 1, 0);

                auto keep = numberBox(res::GetString(L"GeneratorKeep"), true, 0, 100, generator.Keep,
                    [](seq::GeneratorSettings& edited, double value) { edited.Keep = static_cast<uint8_t>(std::clamp(std::lround(value), 0L, 100L)); });
                keep.IsEnabled(generator.ChangesEachPass);

                // A text description doesn't wrap, and this one is longer than the box is wide.
                controls::TextBlock keepHelp{};
                keepHelp.Text(res::GetString(generator.ChangesEachPass ? L"GeneratorKeepHelp" : L"GeneratorKeepOff"));
                keepHelp.TextWrapping(xaml::TextWrapping::Wrap);
                keep.Description(keepHelp);
                Place(form, keep, 1, 1);

                // Velocity and note length.
                Place(form, numberBox(res::GetString(L"GeneratorVelocityFrom"), true, 0, 100, Percent(generator.VelocityLow),
                    [](seq::GeneratorSettings& edited, double value) { edited.VelocityLow = FromPercent(value); }), 1, 2);

                Place(form, numberBox(res::GetString(L"GeneratorVelocityTo"), true, 0, 100, Percent(generator.VelocityHigh),
                    [](seq::GeneratorSettings& edited, double value) { edited.VelocityHigh = FromPercent(value); }), 1, 3);

                Place(form, numberBox(res::GetString(L"GeneratorNoteLength"), true, 1, 1600, generator.NoteLength,
                    [](seq::GeneratorSettings& edited, double value) { edited.NoteLength = static_cast<uint16_t>(std::clamp(std::lround(value), 1L, 1600L)); }), 2, 0);

                body.Children().Append(form);
                break;
            }

            case seq::GeneratorType::Arpeggiator:
                AddArpeggiatorSettings(body, generator, edit);
                break;

            default:
                break;
            }

            // Where it plays.
            if (auto const track = EditorTrack(); track != nullptr)
            {
                std::wstring where{ res::FormatString(L"PatternPlaysToFormat", DescribeDestination(*track)) };

                if (track->Destination.Channel < 0)
                {
                    where += L" \u00B7 " + std::wstring{ res::FormatString(L"ChannelShortFormat", static_cast<int32_t>(generator.Channel) + 1) };
                }

                auto plays = Text(winrt::hstring{ where }, secondary);
                plays.Margin(xaml::Thickness{ 0, 8, 0, 0 });
                body.Children().Append(plays);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the generator.")
    }

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

    // ---------------------------------------------------------------- the header

    _Use_decl_annotations_
    void MainWindow::OnGeneratorTypeChanged(foundation::IInspectable const&, controls::SelectionChangedEventArgs const&)
    {
        if (m_updatingPatternUi)
        {
            return;
        }

        auto const index = GeneratorTypeCombo().SelectedIndex();

        if (index < 0 || static_cast<size_t>(index) >= TypeKeys.size())
        {
            return;
        }

        EditGenerator(std::wstring{ res::GetString(L"UndoGeneratorSettings") }, [index](seq::GeneratorSettings& generator)
        {
            generator.Type = static_cast<seq::GeneratorType>(index);

            // A Euclidean rhythm starts with the comp's rings rather than none.
            if (generator.Type == seq::GeneratorType::Euclidean && generator.Rings.empty())
            {
                generator.Rings = seq::MakeEuclideanGenerator().Rings;
            }
        });
    }

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
    void MainWindow::OnGeneratorChangesClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto const changes = GeneratorChangesToggle().IsChecked().GetBoolean();
        EditGenerator(std::wstring{ res::GetString(L"UndoGeneratorSettings") }, [changes](seq::GeneratorSettings& generator) { generator.ChangesEachPass = changes; });
    }
}
