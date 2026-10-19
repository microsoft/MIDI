// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// What design section 28's generator panels share: the type picker's tiles, the New generator
// menu, rows of chips, and the key and range, a keyboard whose lit stretch you drag. Each panel
// puts a drawing on the left that plays along, and its settings on the right, with as few words as
// will do: the point is to try things and hear them.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "ClipIcons.h"
#include "GeneratorDrawings.h"
#include "GeneratorKinds.h"
#include "GeneratorNames.h"
#include "SequenceGenerator.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        constexpr std::array<wchar_t const*, 11> ScaleKeys{
            L"ScaleMajor", L"ScaleMinor", L"ScaleDorian", L"ScalePhrygian", L"ScaleLydian", L"ScaleMixolydian",
            L"ScaleHarmonicMinor", L"ScaleMajorPentatonic", L"ScaleMinorPentatonic", L"ScaleBlues", L"ScaleChromatic" };

        constexpr std::array<wchar_t const*, 12> KeyNames{ L"C", L"C#", L"D", L"D#", L"E", L"F", L"F#", L"G", L"G#", L"A", L"A#", L"B" };

        // The largest jump's choices, in semitones: any, an octave, a fifth, a third, a step.
        constexpr std::array<uint8_t, 5> Jumps{ 0, 12, 7, 4, 2 };
        constexpr std::array<wchar_t const*, 5> JumpKeys{ L"GeneratorJumpAny", L"GeneratorJumpOctave", L"GeneratorJumpFifth", L"GeneratorJumpThird", L"GeneratorJumpStep" };

        constexpr double KeyboardHeight = 46.0;
        constexpr double KeyboardWidth = 400.0;

        controls::TextBlock Label(winrt::hstring const& text, media::Brush const& brush, double size)
        {
            controls::TextBlock block{};
            block.Text(text);
            block.FontSize(size);
            block.Foreground(brush);
            block.TextWrapping(xaml::TextWrapping::Wrap);
            return block;
        }
    }

    // ---------------------------------------------------------------- the type picker

    void MainWindow::BuildGeneratorTypePicker()
    {
        auto const list = GeneratorTypeList();
        list.Children().Clear();

        m_generatorTypeChoices.assign(seq::GeneratorTypeKeys.size(), nullptr);

        auto const secondary = BrushFor(m_palette.Text3);

        auto const group = [&](wchar_t const* titleKey, auto const& types)
        {
            auto title = Label(res::GetString(titleKey), secondary, 11);
            title.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
            title.Margin(xaml::Thickness{ 4, 6, 0, 0 });
            list.Children().Append(title);

            controls::Grid grid{};
            grid.ColumnSpacing(6);
            grid.RowSpacing(4);

            for (int32_t c = 0; c < 2; ++c)
            {
                controls::ColumnDefinition column{};
                column.Width(xaml::GridLength{ 1, xaml::GridUnitType::Star });
                grid.ColumnDefinitions().Append(column);
            }

            for (size_t i = 0; i < types.size(); ++i)
            {
                if (i % 2 == 0)
                {
                    controls::RowDefinition row{};
                    row.Height(xaml::GridLengthHelper::Auto());
                    grid.RowDefinitions().Append(row);
                }

                auto const type = types[i];
                auto const name = res::GetString(seq::GeneratorTypeKey(type));
                auto const about = res::GetString(seq::GeneratorAboutKey(type));

                // Each type's symbol, its name, and what it does under it.
                auto icon = seq::MakeClipIcon(seq::IconFor(type));
                icon.Width(20);
                icon.Height(20);
                icon.VerticalAlignment(xaml::VerticalAlignment::Top);
                icon.Margin(xaml::Thickness{ 0, 2, 0, 0 });

                if (auto const font = icon.try_as<controls::FontIcon>())
                {
                    font.FontSize(18);
                }

                controls::TextBlock nameText{};
                nameText.Text(name);
                nameText.FontSize(13);

                auto aboutText = Label(about, secondary, 11);

                controls::StackPanel words{};
                words.Spacing(1);
                words.Children().Append(nameText);
                words.Children().Append(aboutText);

                controls::Grid content{};
                content.ColumnSpacing(10);

                controls::ColumnDefinition iconColumn{};
                iconColumn.Width(xaml::GridLengthHelper::Auto());
                content.ColumnDefinitions().Append(iconColumn);

                controls::ColumnDefinition wordsColumn{};
                wordsColumn.Width(xaml::GridLength{ 1, xaml::GridUnitType::Star });
                content.ColumnDefinitions().Append(wordsColumn);

                content.Children().Append(icon);
                controls::Grid::SetColumn(words, 1);
                content.Children().Append(words);

                controls::RadioButton choice{};
                choice.GroupName(L"GeneratorType");
                choice.Content(content);
                choice.Padding(xaml::Thickness{ 8, 4, 4, 6 });
                choice.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

                // Names line up across a row even when one tile's words take more lines.
                choice.VerticalAlignment(xaml::VerticalAlignment::Stretch);
                choice.VerticalContentAlignment(xaml::VerticalAlignment::Top);

                automation::AutomationProperties::SetName(choice, name);
                automation::AutomationProperties::SetHelpText(choice, about);

                choice.Checked([weak = get_weak(), type](auto&&, auto&&)
                {
                    if (auto strong = weak.get(); strong && !strong->m_updatingPatternUi)
                    {
                        strong->GeneratorTypeFlyout().Hide();
                        strong->SetGeneratorType(type);
                    }
                });

                controls::Grid::SetRow(choice, static_cast<int32_t>(i / 2));
                controls::Grid::SetColumn(choice, static_cast<int32_t>(i % 2));
                grid.Children().Append(choice);

                m_generatorTypeChoices[static_cast<size_t>(type)] = choice;
            }

            list.Children().Append(grid);
        };

        group(L"GeneratorGroupRhythm", seq::RhythmGenerators);
        group(L"GeneratorGroupMelody", seq::MelodyGenerators);
        group(L"GeneratorGroupPlayAlong", seq::PlayAlongGenerators);

        // The context menus' tinted backdrop.
        midiapp::UseMenuBackdrop(GeneratorTypeFlyout());

        // It opens on the chosen type, so the arrow keys go on from there.
        GeneratorTypeFlyout().Opened([weak = get_weak()](auto&&, auto&&)
        {
            if (auto strong = weak.get())
            {
                for (auto const& choice : strong->m_generatorTypeChoices)
                {
                    if (auto const checked = choice != nullptr ? choice.IsChecked() : nullptr; checked != nullptr && checked.Value())
                    {
                        choice.Focus(xaml::FocusState::Programmatic);
                        break;
                    }
                }
            }
        });
    }

    _Use_decl_annotations_
    controls::MenuFlyoutSubItem MainWindow::MakeNewGeneratorMenu(winrt::hstring const& text, std::function<void(seq::GeneratorType)> const& create)
    {
        controls::MenuFlyoutSubItem menu{};
        menu.Text(text);

        controls::FontIcon icon{};
        icon.Glyph(L"\uE9F5");
        menu.Icon(icon);

        auto const add = [&](auto const& types)
        {
            if (menu.Items().Size() > 0)
            {
                menu.Items().Append(controls::MenuFlyoutSeparator{});
            }

            for (auto const type : types)
            {
                controls::MenuFlyoutItem item{};
                item.Text(res::GetString(seq::GeneratorTypeKey(type)));
                item.Icon(seq::MakeClipIcon(seq::IconFor(type)));
                item.Click([create, type](auto&&, auto&&) { create(type); });
                menu.Items().Append(item);
            }
        };

        add(seq::RhythmGenerators);
        add(seq::MelodyGenerators);
        add(seq::PlayAlongGenerators);
        return menu;
    }

    // ---------------------------------------------------------------- pieces of a panel

    _Use_decl_annotations_
    controls::StackPanel MainWindow::AddGeneratorSection(controls::StackPanel const& panel, wchar_t const* titleKey)
    {
        auto title = Label(res::GetString(titleKey), BrushFor(m_palette.Text3), 11);
        title.Margin(xaml::Thickness{ 0, 4, 0, 0 });
        panel.Children().Append(title);

        controls::StackPanel content{};
        content.Spacing(6);
        panel.Children().Append(content);
        return content;
    }

    _Use_decl_annotations_
    primitives::ToggleButton MainWindow::MakeChip(winrt::hstring const& label, bool on, std::function<void(bool)> const& toggled)
    {
        primitives::ToggleButton chip{};
        chip.Style(RootGrid().Resources().Lookup(winrt::box_value(L"ChipToggleStyle")).as<xaml::Style>());
        chip.Content(winrt::box_value(label));
        chip.IsChecked(on);
        automation::AutomationProperties::SetName(chip, label);

        // The panel is built again after the edit, which sets the chip's state.
        chip.Click([toggled](foundation::IInspectable const& sender, auto&&)
        {
            auto const value = sender.as<primitives::ToggleButton>().IsChecked();
            toggled(value != nullptr && value.Value());
        });

        return chip;
    }

    _Use_decl_annotations_
    controls::StackPanel MainWindow::MakeChipRow(std::vector<winrt::hstring> const& labels, size_t selected, winrt::hstring const& name, std::function<void(size_t)> const& choose)
    {
        // A wrap panel, so a long row goes onto the next line rather than off the edge.
        winrt::midisequencer::WrapPanel wrap{};

        for (size_t i = 0; i < labels.size(); ++i)
        {
            auto chip = MakeChip(labels[i], i == selected, [choose, i](bool) { choose(i); });
            automation::AutomationProperties::SetHelpText(chip, name);
            automation::AutomationProperties::SetPositionInSet(chip, static_cast<int32_t>(i) + 1);
            automation::AutomationProperties::SetSizeOfSet(chip, static_cast<int32_t>(labels.size()));
            wrap.Children().Append(chip);
        }

        controls::StackPanel row{};
        row.Children().Append(wrap);
        return row;
    }

    _Use_decl_annotations_
    void MainWindow::AppendSeparateChip(controls::StackPanel const& row, primitives::ToggleButton const& chip)
    {
        // A chip that's on or off by itself, after a row of choices, with a short line before
        // it so it doesn't read as one more choice.
        controls::Border line{};
        line.Width(1);
        line.Height(14);
        line.Margin(xaml::Thickness{ 4, 0, 4, 0 });
        line.VerticalAlignment(xaml::VerticalAlignment::Center);
        line.Background(BrushFor(m_palette.StrokeStrong));

        controls::StackPanel pair{};
        pair.Orientation(controls::Orientation::Horizontal);
        pair.Spacing(6);
        pair.Children().Append(line);
        pair.Children().Append(chip);

        row.Children().GetAt(0).as<winrt::midisequencer::WrapPanel>().Children().Append(pair);
    }

    _Use_decl_annotations_
    controls::Border MainWindow::FrameGeneratorDrawing(xaml::FrameworkElement const& drawing)
    {
        controls::Border frame{};
        frame.CornerRadius(xaml::CornerRadius{ 6, 6, 6, 6 });
        frame.BorderThickness(xaml::Thickness{ 1 });
        frame.BorderBrush(BrushFor(m_palette.StrokeStrong));
        frame.Padding(xaml::Thickness{ 6 });
        frame.HorizontalAlignment(xaml::HorizontalAlignment::Left);
        frame.Child(drawing);
        return frame;
    }

    _Use_decl_annotations_
    controls::Button MainWindow::MakeGeneratorNotesButton(std::wstring const& clipId)
    {
        controls::Button makeNotes{};
        makeNotes.Style(RootGrid().Resources().Lookup(winrt::box_value(L"SmallButtonStyle")).as<xaml::Style>());
        makeNotes.Content(winrt::box_value(res::GetString(L"GeneratorMakeNotesFromPasses")));
        makeNotes.Margin(xaml::Thickness{ 0, 10, 0, 0 });
        makeNotes.HorizontalAlignment(xaml::HorizontalAlignment::Left);
        makeNotes.Click([weak = get_weak(), clipId](auto&&, auto&&)
        {
            if (auto s = weak.get(); s && s->m_editorClipId == clipId)
            {
                s->MakeNotesFromPattern();
            }
        });

        return makeNotes;
    }

    _Use_decl_annotations_
    controls::Grid MainWindow::MakeGeneratorLayout(xaml::FrameworkElement const& drawing, controls::StackPanel const& settings, controls::StackPanel const& side)
    {
        // Three columns in a wide editor: the drawing, the generator's own settings, then the key
        // and range with Make notes. In a narrower one, the third goes under the second.
        controls::Grid layout{};
        layout.ColumnSpacing(36);
        layout.RowSpacing(18);

        for (auto const width : { xaml::GridLengthHelper::Auto(), xaml::GridLengthHelper::Auto(), xaml::GridLength{ 1, xaml::GridUnitType::Star } })
        {
            controls::ColumnDefinition column{};
            column.Width(width);
            layout.ColumnDefinitions().Append(column);
        }

        for (int32_t i = 0; i < 2; ++i)
        {
            controls::RowDefinition row{};
            row.Height(xaml::GridLengthHelper::Auto());
            layout.RowDefinitions().Append(row);
        }

        drawing.VerticalAlignment(xaml::VerticalAlignment::Top);
        controls::Grid::SetRowSpan(drawing, 2);
        layout.Children().Append(drawing);

        for (auto const& column : { settings, side })
        {
            column.Spacing(6);
            column.MaxWidth(480);
            column.HorizontalAlignment(xaml::HorizontalAlignment::Left);
            column.VerticalAlignment(xaml::VerticalAlignment::Top);
        }

        controls::Grid::SetColumn(settings, 1);
        layout.Children().Append(settings);
        layout.Children().Append(side);

        // The two columns are measured first, so the panel's own sizing knows how much room its
        // drawing has. Then the drawing is measured at the size it'll be.
        auto const own = m_fitGenerator;

        m_fitGenerator = [weak = get_weak(), own, drawing, settings, side]()
        {
            auto s = weak.get();

            if (!s)
            {
                return;
            }

            foundation::Size const unlimited{ std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() };
            settings.Measure(unlimited);
            side.Measure(unlimited);

            auto const settingsWidth = static_cast<double>(std::min(480.0f, settings.DesiredSize().Width));
            auto const sideWidth = static_cast<double>(std::min(480.0f, side.DesiredSize().Width));
            auto const panelWidth = s->GeneratorPanel().ActualWidth();

            // A little short of exact, so rounding can't push the third column under.
            s->m_generatorRoomBeside = panelWidth - 40.0 - 72.0 - settingsWidth - sideWidth - 2.0;
            s->m_generatorRoomAlone = panelWidth - 40.0 - 36.0 - settingsWidth - 2.0;

            if (own)
            {
                own();
            }

            drawing.Measure(unlimited);

            auto const wanted = static_cast<double>(drawing.DesiredSize().Width) + settingsWidth + sideWidth + 72.0;
            auto const wide = panelWidth <= 0 || panelWidth - 40.0 >= wanted;

            controls::Grid::SetRow(side, wide ? 0 : 1);
            controls::Grid::SetColumn(side, wide ? 2 : 1);
        };

        m_fitGenerator();
        return layout;
    }

    _Use_decl_annotations_
    double MainWindow::GeneratorDrawingWidth(double least, double most) const noexcept
    {
        // The drawing's frame adds its padding and border on each side.
        constexpr double Frame = 14.0;

        auto const beside = m_generatorRoomBeside - Frame;
        return beside >= least ? std::min(beside, most) : std::clamp(m_generatorRoomAlone - Frame, least, most);
    }

    _Use_decl_annotations_
    void MainWindow::EditGeneratorInPlace(std::function<void(seq::GeneratorSettings&)> const& change) noexcept
    {
        m_keepGeneratorPanel = true;
        EditGenerator(std::wstring{ res::GetString(L"UndoGeneratorSettings") }, change);
        m_keepGeneratorPanel = false;
    }

    void MainWindow::QueueGeneratorPanelUpdate() noexcept
    {
        try
        {
            if (std::exchange(m_generatorPanelQueued, true))
            {
                return;
            }

            auto const queued = DispatcherQueue().TryEnqueue([weak = get_weak(), clipId = m_editorClipId]()
            {
                if (auto s = weak.get())
                {
                    s->m_generatorPanelQueued = false;

                    if (s->m_editorClipId == clipId)
                    {
                        s->UpdateGeneratorPanel();
                    }
                }
            });

            if (!queued)
            {
                m_generatorPanelQueued = false;
            }
        }
        catch (...)
        {
            m_generatorPanelQueued = false;
        }
    }

    _Use_decl_annotations_
    uint64_t MainWindow::ShownGeneratorPass(seq::Clip const& clip) const noexcept
    {
        if (clip.Generator.has_value() && !clip.Generator->ChangesEachPass)
        {
            return 0;
        }

        if (m_engine != nullptr && m_engine->IsPlaying() && m_editorPlayhead >= 0)
        {
            return m_editorPass;
        }

        return m_pickedPassClip == clip.Id ? m_pickedPass : 0;
    }

    _Use_decl_annotations_
    int64_t MainWindow::PlayingGeneratorStep(seq::Clip const& clip) const noexcept
    {
        if (!clip.Generator.has_value() || m_engine == nullptr || !m_engine->IsPlaying() || m_editorPlayhead < 0)
        {
            return -1;
        }

        return m_editorPlayhead / std::max<int64_t>(1, clip.Generator->StepTicks);
    }

    _Use_decl_annotations_
    void MainWindow::PickGeneratorPass(uint64_t pass) noexcept
    {
        m_pickedPass = std::min(pass, seq::MaximumSteppedPasses);
        m_pickedPassClip = m_editorClipId;

        // Built again, so what depends on the pass shown, such as Start from this pass, follows.
        // That runs after the press or click that picked it: built inside a pointer's own event,
        // what replaces the control under the pointer loses the next press.
        try
        {
            DispatcherQueue().TryEnqueue([weak = get_weak()]()
            {
                if (auto s = weak.get())
                {
                    s->UpdateGeneratorPanel();
                }
            });
        }
        catch (...)
        {
        }
    }

    // ---------------------------------------------------------------- the key and range

    _Use_decl_annotations_
    void MainWindow::AddKeyAndRange(controls::StackPanel const& panel, seq::Clip const& clip, GeneratorEdit const& edit, bool rules)
    {
        auto const& generator = *clip.Generator;
        auto const section = AddGeneratorSection(panel, L"GeneratorKeySection");
        auto const weak = get_weak();

        controls::ComboBox key{};
        key.Style(RootGrid().Resources().Lookup(winrt::box_value(L"SmallComboStyle")).as<xaml::Style>());
        key.Width(72);
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
        scale.Style(RootGrid().Resources().Lookup(winrt::box_value(L"SmallComboStyle")).as<xaml::Style>());
        scale.Width(170);
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

        controls::StackPanel keyRow{};
        keyRow.Orientation(controls::Orientation::Horizontal);
        keyRow.Spacing(8);
        keyRow.Children().Append(key);
        keyRow.Children().Append(scale);
        section.Children().Append(keyRow);

        // The range on a keyboard: drag either end, or the middle to move it all, and scroll to
        // move it an octave.
        auto const color = ClipColor(clip);

        auto keyboard = MakeGeneratorCanvas([weak, color](canvas::CanvasDrawingSession const& ds, float width, float height)
        {
            auto s = weak.get();
            auto const found = s ? seq::FindClip(s->m_doc, s->m_editorClipId) : nullptr;

            if (found == nullptr || !found->Generator.has_value())
            {
                return;
            }

            auto const& settings = *found->Generator;
            auto const low = s->m_rangeDrag.Active ? s->m_rangeDrag.Low : settings.LowNote;
            auto const high = s->m_rangeDrag.Active ? s->m_rangeDrag.High : settings.HighNote;

            // While a grip is dragged the keys stay where they were when it was picked up, so the
            // key under the pointer is the one the range ends on.
            auto const picked = seq::MeasureKeyboard(s->m_rangeDrag.StartLow, s->m_rangeDrag.StartHigh);
            seq::DrawKeyboard(ds, width, height, settings.ScaleRoot, settings.Scale, low, high, color, s->m_palette, s->m_rangeDrag.Active ? &picked : nullptr);
        });

        keyboard.Width(KeyboardWidth);
        keyboard.Height(KeyboardHeight);

        // At the column's left edge, with its title and boxes, however wide a row of chips under it
        // makes the column. Stretched, it would sit in the middle.
        keyboard.HorizontalAlignment(xaml::HorizontalAlignment::Left);
        automation::AutomationProperties::SetName(keyboard, res::GetString(L"GeneratorRangeKeyboard"));
        controls::ToolTipService::SetToolTip(keyboard, winrt::box_value(res::GetString(L"GeneratorRangeKeyboard")));

        keyboard.PointerPressed([weak](foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args)
        {
            auto s = weak.get();
            auto const found = s ? seq::FindClip(s->m_doc, s->m_editorClipId) : nullptr;

            if (found == nullptr || !found->Generator.has_value() || s->m_readOnly)
            {
                return;
            }

            auto const canvas = sender.as<canvasXaml::CanvasControl>();
            auto const point = args.GetCurrentPoint(canvas).Position();
            auto const& settings = *found->Generator;
            auto const span = seq::MeasureKeyboard(settings.LowNote, settings.HighNote);
            auto const note = seq::KeyAtPoint(span, static_cast<float>(canvas.ActualWidth()), static_cast<float>(canvas.ActualHeight()), point.X, point.Y);

            if (note < 0)
            {
                return;
            }

            RangeDrag drag{};
            drag.Active = true;
            drag.From = note;
            drag.StartLow = settings.LowNote;
            drag.StartHigh = settings.HighNote;
            drag.Low = settings.LowNote;
            drag.High = settings.HighNote;

            // Near an end picks up that end; between them, the whole range; outside, the nearer end
            // jumps there.
            if (std::abs(note - settings.LowNote) <= 1)
            {
                drag.Grip = 0;
            }
            else if (std::abs(note - settings.HighNote) <= 1)
            {
                drag.Grip = 1;
            }
            else if (note > settings.LowNote && note < settings.HighNote)
            {
                drag.Grip = 2;
            }
            else
            {
                drag.Grip = note < settings.LowNote ? 0 : 1;
                (drag.Grip == 0 ? drag.Low : drag.High) = static_cast<uint8_t>(note);
            }

            s->m_rangeDrag = drag;
            canvas.CapturePointer(args.Pointer());
            canvas.Invalidate();
            args.Handled(true);
        });

        keyboard.PointerMoved([weak](foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args)
        {
            auto s = weak.get();

            if (!s || !s->m_rangeDrag.Active)
            {
                return;
            }

            auto const canvas = sender.as<canvasXaml::CanvasControl>();
            auto const point = args.GetCurrentPoint(canvas).Position();
            auto& drag = s->m_rangeDrag;
            auto const span = seq::MeasureKeyboard(drag.StartLow, drag.StartHigh);
            auto const note = seq::KeyAtPoint(span, static_cast<float>(canvas.ActualWidth()), static_cast<float>(canvas.ActualHeight()), point.X, std::min(point.Y, static_cast<float>(KeyboardHeight) - 14.0f));

            if (note < 0)
            {
                return;
            }

            switch (drag.Grip)
            {
            case 0:
                drag.Low = static_cast<uint8_t>(std::min<int32_t>(note, drag.High));
                break;

            case 1:
                drag.High = static_cast<uint8_t>(std::max<int32_t>(note, drag.Low));
                break;

            default:
            {
                auto const moved = std::clamp(note - drag.From, -static_cast<int32_t>(drag.StartLow), 127 - static_cast<int32_t>(drag.StartHigh));
                drag.Low = static_cast<uint8_t>(drag.StartLow + moved);
                drag.High = static_cast<uint8_t>(drag.StartHigh + moved);
                break;
            }
            }

            canvas.Invalidate();
            args.Handled(true);
        });

        auto const letGo = [weak, edit](foundation::IInspectable const& sender, input::PointerRoutedEventArgs const&)
        {
            auto s = weak.get();

            if (!s || !s->m_rangeDrag.Active)
            {
                return;
            }

            // Cleared first: letting go of the capture comes straight back here.
            auto const drag = s->m_rangeDrag;
            s->m_rangeDrag = RangeDrag{};
            sender.as<canvasXaml::CanvasControl>().ReleasePointerCaptures();

            // The edit builds the panel again, so it runs after this event: built inside a pointer's
            // own event, the drawing that replaces this one loses the next press.
            if (drag.Low != drag.StartLow || drag.High != drag.StartHigh)
            {
                edit([drag](seq::GeneratorSettings& edited)
                {
                    edited.LowNote = drag.Low;
                    edited.HighNote = drag.High;
                });
            }
            else
            {
                sender.as<canvasXaml::CanvasControl>().Invalidate();
            }
        };

        keyboard.PointerReleased(letGo);
        keyboard.PointerCaptureLost(letGo);

        keyboard.PointerWheelChanged([weak, edit](foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args)
        {
            auto s = weak.get();
            args.Handled(true);

            if (!s)
            {
                return;
            }

            auto const canvas = sender.as<canvasXaml::CanvasControl>();
            auto const point = args.GetCurrentPoint(canvas);
            auto const at = point.Position();
            auto const delta = point.Properties().MouseWheelDelta();

            // One notch moves the range once. A check saw one notch move it twice, the second time
            // when the pointer next moved, after the panel had been built again under it. So a
            // repeat of the last notch is skipped, and so is one from outside the keyboard.
            if (delta == 0 || point.Timestamp() == s->m_lastRangeWheel ||
                at.X < 0 || at.Y < 0 || at.X > canvas.ActualWidth() || at.Y > canvas.ActualHeight())
            {
                return;
            }

            s->m_lastRangeWheel = point.Timestamp();
            auto const up = delta > 0;

            edit([up](seq::GeneratorSettings& edited)
            {
                auto const move = up ? std::min(12, 127 - static_cast<int32_t>(edited.HighNote)) : -std::min(12, static_cast<int32_t>(edited.LowNote));
                edited.LowNote = static_cast<uint8_t>(edited.LowNote + move);
                edited.HighNote = static_cast<uint8_t>(edited.HighNote + move);
            });
        });

        section.Children().Append(keyboard);

        // The same ends as boxes, for the keyboard and a screen reader.
        auto const noteBox = [&](wchar_t const* nameKey, double value, std::function<void(seq::GeneratorSettings&, uint8_t)> apply)
        {
            auto box = MakeGeneratorNumberBox(res::GetString(nameKey), 0, 127, value, true);
            box.Width(110);
            box.ValueChanged([edit, apply](controls::NumberBox const&, controls::NumberBoxValueChangedEventArgs const& args)
            {
                auto const newValue = args.NewValue();

                if (std::isfinite(newValue) && newValue != args.OldValue())
                {
                    auto const note = static_cast<uint8_t>(std::clamp(std::lround(newValue), 0L, 127L));
                    edit([apply, note](seq::GeneratorSettings& edited) { apply(edited, note); });
                }
            });

            return box;
        };

        controls::StackPanel ends{};
        ends.Orientation(controls::Orientation::Horizontal);
        ends.Spacing(8);
        ends.Children().Append(noteBox(L"GeneratorLowNote", generator.LowNote, [](seq::GeneratorSettings& edited, uint8_t note) { edited.LowNote = note; }));

        auto to = Label(res::GetString(L"GeneratorRangeTo"), BrushFor(m_palette.Text3), 12);
        to.VerticalAlignment(xaml::VerticalAlignment::Center);
        ends.Children().Append(to);
        ends.Children().Append(noteBox(L"GeneratorHighNote", generator.HighNote, [](seq::GeneratorSettings& edited, uint8_t note) { edited.HighNote = note; }));
        section.Children().Append(ends);

        if (rules)
        {
            AddKeyRules(panel, clip, edit);
        }
    }

    _Use_decl_annotations_
    void MainWindow::AddKeyRules(controls::StackPanel const& panel, seq::Clip const& clip, GeneratorEdit const& edit)
    {
        auto const& generator = *clip.Generator;

        // How far a line may jump, and whether it starts and ends at home.
        std::vector<winrt::hstring> jumps{};

        for (auto const keyName : JumpKeys)
        {
            jumps.push_back(res::GetString(keyName));
        }

        auto const jumpIndex = static_cast<size_t>(std::distance(Jumps.begin(), std::find(Jumps.begin(), Jumps.end(), generator.LargestJump)));
        auto const jumpSection = AddGeneratorSection(panel, L"GeneratorJump");

        auto const jumpRow = MakeChipRow(jumps, jumpIndex, res::GetString(L"GeneratorJump"), [edit](size_t index)
        {
            edit([index](seq::GeneratorSettings& edited) { edited.LargestJump = Jumps[std::min(index, Jumps.size() - 1)]; });
        });

        // On or off by itself, so it's set apart from the jumps.
        AppendSeparateChip(jumpRow, MakeChip(res::GetString(L"GeneratorRootEnds"), generator.RootEnds, [edit](bool on)
        {
            edit([on](seq::GeneratorSettings& edited) { edited.RootEnds = on; });
        }));

        jumpSection.Children().Append(jumpRow);
    }
}
