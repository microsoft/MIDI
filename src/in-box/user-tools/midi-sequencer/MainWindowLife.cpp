// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Life and Life drums (design section 28): the board on the left, which you draw the first pass
// on and which grows as it plays, the passes it grows through under it, and chips for the rules.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "GeneratorDrawings.h"
#include "GeneratorKinds.h"
#include "PianoRoll.h"
#include "SequenceGenerator.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        // General MIDI names drums 35 to 81.
        constexpr uint8_t FirstDrumName = 35;
        constexpr uint8_t LastDrumName = 81;

        constexpr std::array<uint16_t, 5> RestartChoices{ 0, 4, 8, 16, 32 };

        constexpr std::array<wchar_t const*, 12> ShortNames{ L"C", L"C#", L"D", L"D#", L"E", L"F", L"F#", L"G", L"G#", L"A", L"A#", L"B" };

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

        std::wstring DrumName(uint8_t note)
        {
            return note >= FirstDrumName && note <= LastDrumName ? std::wstring{ res::GetString(L"DrumName" + std::to_wstring(note)) } : seq::NoteLabel(note);
        }

        std::vector<winrt::hstring> Strings(std::initializer_list<wchar_t const*> keys)
        {
            std::vector<winrt::hstring> strings{};

            for (auto const key : keys)
            {
                strings.push_back(res::GetString(key));
            }

            return strings;
        }

        bool Contains(std::vector<seq::LifeCell> const& cells, seq::LifeCell cell)
        {
            return std::binary_search(cells.begin(), cells.end(), cell);
        }

        void Set(std::vector<seq::LifeCell>& cells, seq::LifeCell cell, bool on)
        {
            auto const at = std::lower_bound(cells.begin(), cells.end(), cell);

            if (on && (at == cells.end() || *at != cell))
            {
                cells.insert(at, cell);
            }
            else if (!on && at != cells.end() && *at == cell)
            {
                cells.erase(at);
            }
        }
    }

    _Use_decl_annotations_
    void MainWindow::AddLifeSettings(controls::StackPanel const& body, seq::Clip const& clip, GeneratorEdit const& edit)
    {
        auto const& generator = *clip.Generator;
        auto const drums = generator.Type == seq::GeneratorType::LifeDrums;
        auto const rowNotes = seq::LifeRowNotes(generator);
        auto const columns = static_cast<int32_t>(seq::GeneratorStepCount(clip));
        auto const rows = static_cast<int32_t>(std::max<size_t>(1, rowNotes.size()));
        auto const labelWidth = drums ? 112.0f : 34.0f;
        auto const color = ClipColor(clip);
        auto const weak = get_weak();
        auto const clipId = clip.Id;
        auto const secondary = BrushFor(m_palette.Text3);

        // Each row's name and whether it's a root, worked out once for the drawing.
        auto names = std::make_shared<std::vector<std::wstring>>();
        auto roots = std::make_shared<std::vector<bool>>();

        for (auto const note : rowNotes)
        {
            // A row's name is short, such as A3, so it fits beside the board.
            names->push_back(drums ? DrumName(note) : std::wstring{ ShortNames[note % 12] } + std::to_wstring(note / 12 - 2));
            roots->push_back(!drums && note % 12 == generator.ScaleRoot % 12);
        }

        // ---- the board

        auto const layoutFor = [labelWidth, columns, rows](float width, float height)
        {
            return seq::MeasureLifeBoard(width, height, columns, rows, labelWidth);
        };

        auto board = MakeGeneratorCanvas([weak, names, roots, layoutFor, color](canvas::CanvasDrawingSession const& ds, float width, float height)
        {
            auto s = weak.get();
            auto const found = s ? seq::FindClip(s->m_doc, s->m_editorClipId) : nullptr;

            if (found == nullptr || !found->Generator.has_value())
            {
                return;
            }

            auto const& settings = *found->Generator;
            auto const steps = seq::GeneratorStepCount(*found);
            auto const pass = s->ShownGeneratorPass(*found);
            auto const hearing = s->ShownPassHearing(*found, pass);
            auto shown = seq::LifeBoardAt(settings, found->Seed, steps, pass, hearing.Before ? &hearing : nullptr);

            // While cells are being drawn, the start as it's being drawn.
            if (s->m_lifePaint.Active)
            {
                std::fill(shown.Cells.begin(), shown.Cells.end(), uint8_t{ 0 });

                for (auto const& cell : s->m_lifePaint.Cells)
                {
                    if (cell.Column < shown.Columns && cell.Row < shown.Rows)
                    {
                        shown.Cells[static_cast<size_t>(cell.Row) * static_cast<size_t>(shown.Columns) + cell.Column] = 2;
                    }
                }
            }

            seq::DrawLifeBoard(ds, shown, layoutFor(width, height), *names, *roots, s->PlayingGeneratorStep(*found), color, s->m_palette);
        });

        automation::AutomationProperties::SetName(board, res::GetString(L"LifeBoardName"));
        controls::ToolTipService::SetToolTip(board, winrt::box_value(res::GetString(L"LifeBoardTip")));

        board.PointerPressed([weak, layoutFor, drums](foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args)
        {
            auto s = weak.get();
            auto const found = s ? seq::FindClip(s->m_doc, s->m_editorClipId) : nullptr;

            if (found == nullptr || !found->Generator.has_value() || s->m_readOnly)
            {
                return;
            }

            auto const canvas = sender.as<canvasXaml::CanvasControl>();
            auto const point = args.GetCurrentPoint(canvas);
            auto const layout = layoutFor(static_cast<float>(canvas.ActualWidth()), static_cast<float>(canvas.ActualHeight()));
            auto const& settings = *found->Generator;

            int32_t column{ 0 };
            int32_t row{ 0 };
            auto const onCell = seq::CellAtPoint(layout, point.Position().X, point.Position().Y, column, row);

            // A drum's name: change it or take the row away.
            if (drums && point.Position().X < layout.Left && row >= 0 && row < layout.Rows)
            {
                args.Handled(true);

                controls::MenuFlyout menu{};
                auto const index = static_cast<size_t>(row);
                auto const current = index < settings.DrumRows.size() ? settings.DrumRows[index] : uint8_t{ 0 };

                if (settings.DrumRows.size() > 1)
                {
                    controls::MenuFlyoutItem remove{};
                    remove.Text(res::GetString(L"LifeRemoveDrum"));

                    controls::FontIcon icon{};
                    icon.Glyph(L"\uE74D");
                    remove.Icon(icon);

                    remove.Click([weak, index](auto&&, auto&&)
                    {
                        if (auto window = weak.get())
                        {
                            window->EditGenerator(std::wstring{ res::GetString(L"UndoGeneratorSettings") }, [index](seq::GeneratorSettings& edited)
                            {
                                if (index < edited.DrumRows.size() && edited.DrumRows.size() > 1)
                                {
                                    edited.DrumRows.erase(edited.DrumRows.begin() + static_cast<ptrdiff_t>(index));
                                }
                            });
                        }
                    });

                    menu.Items().Append(remove);
                    menu.Items().Append(controls::MenuFlyoutSeparator{});
                }

                for (auto note = FirstDrumName; note <= LastDrumName; ++note)
                {
                    controls::ToggleMenuFlyoutItem item{};
                    item.Text(winrt::hstring{ DrumName(note) });
                    item.IsChecked(note == current);
                    item.Click([weak, index, note](auto&&, auto&&)
                    {
                        if (auto window = weak.get())
                        {
                            window->EditGenerator(std::wstring{ res::GetString(L"UndoGeneratorSettings") }, [index, note](seq::GeneratorSettings& edited)
                            {
                                if (index < edited.DrumRows.size())
                                {
                                    edited.DrumRows[index] = note;
                                }
                            });
                        }
                    });

                    menu.Items().Append(item);
                }

                primitives::FlyoutShowOptions options{};
                options.Position(point.Position());
                menu.ShowAt(canvas, options);
                return;
            }

            // Cells are drawn on the first pass, so the board has to be showing it.
            if (!onCell || !point.Properties().IsLeftButtonPressed() || s->ShownGeneratorPass(*found) != 0)
            {
                return;
            }

            auto cells = settings.Scatter
                ? seq::ScatteredCells(settings, found->Seed, layout.Columns, layout.Rows)
                : settings.LifeCells;

            std::sort(cells.begin(), cells.end());

            seq::LifeCell const cell{ static_cast<uint16_t>(column), static_cast<uint16_t>(row) };

            LifePaint paint{};
            paint.Active = true;
            paint.On = !Contains(cells, cell);
            paint.Cells = std::move(cells);
            Set(paint.Cells, cell, paint.On);
            s->m_lifePaint = std::move(paint);

            canvas.CapturePointer(args.Pointer());
            canvas.Invalidate();
            args.Handled(true);
        });

        board.PointerMoved([weak, layoutFor](foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args)
        {
            auto s = weak.get();

            if (!s || !s->m_lifePaint.Active)
            {
                return;
            }

            auto const canvas = sender.as<canvasXaml::CanvasControl>();
            auto const position = args.GetCurrentPoint(canvas).Position();
            auto const layout = layoutFor(static_cast<float>(canvas.ActualWidth()), static_cast<float>(canvas.ActualHeight()));

            int32_t column{ 0 };
            int32_t row{ 0 };

            if (seq::CellAtPoint(layout, position.X, position.Y, column, row))
            {
                seq::LifeCell const cell{ static_cast<uint16_t>(column), static_cast<uint16_t>(row) };

                if (Contains(s->m_lifePaint.Cells, cell) != s->m_lifePaint.On)
                {
                    Set(s->m_lifePaint.Cells, cell, s->m_lifePaint.On);
                    canvas.Invalidate();
                }
            }

            args.Handled(true);
        });

        auto const letGo = [weak, edit](foundation::IInspectable const& sender, input::PointerRoutedEventArgs const&)
        {
            auto s = weak.get();

            if (!s || !s->m_lifePaint.Active)
            {
                return;
            }

            // Cleared first: letting go of the capture comes straight back here.
            auto cells = std::move(s->m_lifePaint.Cells);
            s->m_lifePaint = LifePaint{};
            sender.as<canvasXaml::CanvasControl>().ReleasePointerCaptures();

            // The edit builds the panel again, so it runs after this event: built inside a
            // pointer's own event, the board that replaces this one loses the next press.
            edit([cells = std::move(cells)](seq::GeneratorSettings& edited)
            {
                edited.Scatter = false;
                edited.LifeCells = cells;
            });
        };

        board.PointerReleased(letGo);
        board.PointerCaptureLost(letGo);

        // ---- the passes it grows through

        auto film = std::make_shared<std::pair<uint64_t, std::vector<seq::LifeBoard>>>(UINT64_MAX, std::vector<seq::LifeBoard>{});

        auto strip = MakeGeneratorCanvas([weak, color, film, columns, rows](canvas::CanvasDrawingSession const& ds, float width, float height)
        {
            auto s = weak.get();
            auto const found = s ? seq::FindClip(s->m_doc, s->m_editorClipId) : nullptr;

            if (found == nullptr || !found->Generator.has_value())
            {
                return;
            }

            auto const shown = s->ShownGeneratorPass(*found);
            auto const playing = s->PlayingGeneratorStep(*found) >= 0 ? shown : UINT64_MAX;
            auto const layout = seq::MeasureFilmstrip(width, height, columns, rows, shown);

            // Worked out again only when the strip moves along, or what it heard changes.
            auto const hearing = s->ShownPassHearing(*found, 0);
            auto const mark = layout.First ^ (hearing.Key * 0x9E3779B97F4A7C15ull);

            if (film->first != mark || film->second.size() != layout.Count)
            {
                film->first = mark;
                film->second.clear();

                for (size_t i = 0; i < layout.Count; ++i)
                {
                    film->second.push_back(seq::LifeBoardAt(*found->Generator, found->Seed, seq::GeneratorStepCount(*found), layout.First + i, hearing.Before ? &hearing : nullptr));
                }
            }

            seq::DrawFilmstrip(ds, height, film->second, layout, shown, playing, color, s->m_palette);
        });

        strip.Height(48);
        automation::AutomationProperties::SetName(strip, res::GetString(L"LifeFilmName"));
        controls::ToolTipService::SetToolTip(strip, winrt::box_value(res::GetString(L"LifeFilmTip")));

        strip.PointerPressed([weak, columns, rows](foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args)
        {
            auto s = weak.get();
            auto const found = s ? seq::FindClip(s->m_doc, s->m_editorClipId) : nullptr;

            if (found == nullptr || s->PlayingGeneratorStep(*found) >= 0)
            {
                return;
            }

            auto const canvas = sender.as<canvasXaml::CanvasControl>();
            auto const layout = seq::MeasureFilmstrip(static_cast<float>(canvas.ActualWidth()), static_cast<float>(canvas.ActualHeight()), columns, rows, s->ShownGeneratorPass(*found));
            auto const pass = seq::FilmPassAtPoint(layout, args.GetCurrentPoint(canvas).Position().X);

            if (pass != UINT64_MAX)
            {
                args.Handled(true);
                s->PickGeneratorPass(pass);
            }
        });

        // The pass shown, and a way back to the first, or to start from this one.
        m_passCaption = Text(winrt::hstring{}, secondary, 12);

        controls::StackPanel under{};
        under.Orientation(controls::Orientation::Horizontal);
        under.Spacing(10);
        under.Children().Append(m_passCaption);

        auto const shownPass = ShownGeneratorPass(clip);

        if (shownPass > 0 && PlayingGeneratorStep(clip) < 0)
        {
            controls::Button back{};
            back.Style(RootGrid().Resources().Lookup(winrt::box_value(L"SmallButtonStyle")).as<xaml::Style>());
            back.Content(winrt::box_value(res::GetString(L"LifeBackToStart")));
            back.Click([weak](auto&&, auto&&) { if (auto s = weak.get()) { s->PickGeneratorPass(0); } });
            under.Children().Append(back);

            controls::Button startHere{};
            startHere.Style(RootGrid().Resources().Lookup(winrt::box_value(L"SmallButtonStyle")).as<xaml::Style>());
            startHere.Content(winrt::box_value(res::GetString(L"LifeStartHere")));
            startHere.Click([weak, clipId, shownPass](auto&&, auto&&)
            {
                auto s = weak.get();
                auto const found = s ? seq::FindClip(s->m_doc, clipId) : nullptr;

                if (found == nullptr || !found->Generator.has_value())
                {
                    return;
                }

                auto const hearing = s->ShownPassHearing(*found, shownPass);
                auto const shown = seq::LifeBoardAt(*found->Generator, found->Seed, seq::GeneratorStepCount(*found), shownPass, hearing.Before ? &hearing : nullptr);
                std::vector<seq::LifeCell> cells{};

                for (int32_t row = 0; row < shown.Rows; ++row)
                {
                    for (int32_t column = 0; column < shown.Columns; ++column)
                    {
                        if (shown.At(column, row) != 0)
                        {
                            cells.push_back(seq::LifeCell{ static_cast<uint16_t>(column), static_cast<uint16_t>(row) });
                        }
                    }
                }

                s->m_pickedPass = 0;
                s->EditGenerator(std::wstring{ res::GetString(L"UndoGeneratorSettings") }, [cells](seq::GeneratorSettings& edited)
                {
                    edited.Scatter = false;
                    edited.LifeCells = cells;
                });
            });

            under.Children().Append(startHere);
        }

        controls::StackPanel left{};
        left.Spacing(8);
        left.Children().Append(FrameGeneratorDrawing(board));
        left.Children().Append(strip);
        left.Children().Append(under);

        // The board fills what the panel can give it, a little bigger for a short clip.
        m_fitGenerator = [weak, board, strip, layoutFor]()
        {
            auto s = weak.get();

            if (!s)
            {
                return;
            }

            auto const panelHeight = s->GeneratorPanel().ActualHeight();

            // Room for the settings and the key and range beside it, or for the settings alone
            // when the key and range goes under them.
            auto const width = static_cast<float>(s->GeneratorDrawingWidth(300.0, 760.0));
            auto const height = static_cast<float>(std::clamp(panelHeight - 28.0 - 48.0 - 40.0 - 40.0, 110.0, 440.0));
            auto const layout = layoutFor(width, height);

            board.Width(layout.Width());
            board.Height(layout.Height());
            strip.Width(std::max(220.0f, layout.Width()));
        };

        m_fitGenerator();

        // ---- the rules, as chips

        controls::StackPanel settings{};

        // How it starts: scattered by the seed, a named shape, or drawn on the board. The chip
        // for how it starts now is lit, and Your own when the cells were drawn by hand.
        auto const start = AddGeneratorSection(settings, L"LifeStart");
        winrt::midisequencer::WrapPanel starts{};
        auto named = generator.Scatter;

        starts.Children().Append(MakeChip(res::GetString(L"LifeScatter"), generator.Scatter, [edit](bool)
        {
            edit([](seq::GeneratorSettings& edited)
            {
                edited.Scatter = true;
                edited.LearnedFrom.clear();
            });
        }));

        for (auto const& [key, shape] : { std::pair{ L"LifeGlider", seq::LifeShape::Glider }, std::pair{ L"LifeBlinker", seq::LifeShape::Blinker }, std::pair{ L"LifeRPentomino", seq::LifeShape::RPentomino } })
        {
            auto const isShape = !generator.Scatter && !generator.LifeCells.empty() && seq::LifeShapeCells(shape, columns, rows) == generator.LifeCells;
            named = named || isShape;

            starts.Children().Append(MakeChip(res::GetString(key), isShape, [edit, shape, columns, rows](bool)
            {
                edit([shape, columns, rows](seq::GeneratorSettings& edited)
                {
                    edited.Scatter = false;
                    edited.LifeCells = seq::LifeShapeCells(shape, columns, rows);
                    edited.LearnedFrom.clear();
                });
            }));
        }

        auto const empty = !generator.Scatter && generator.LifeCells.empty();
        named = named || empty;

        starts.Children().Append(MakeChip(res::GetString(L"LifeClear"), empty, [edit](bool)
        {
            edit([](seq::GeneratorSettings& edited)
            {
                edited.Scatter = false;
                edited.LifeCells.clear();
                edited.LearnedFrom.clear();
            });
        }));

        // A chip that's lit when the cells are drawn by hand, there only then.
        if (!named)
        {
            auto own = MakeChip(res::GetString(L"LifeYourOwn"), true, [](bool) {});
            own.IsHitTestVisible(false);
            own.IsTabStop(false);
            starts.Children().Append(own);
        }

        start.Children().Append(starts);

        if (generator.Scatter)
        {
            auto densityValue = Text(winrt::hstring{}, BrushFor(m_palette.Text2));
            auto density = MakeGeneratorSlider(res::GetString(L"LifeDensity"), 90, generator.Density, densityValue, [edit](double value)
            {
                edit([value](seq::GeneratorSettings& edited) { edited.Density = static_cast<uint8_t>(std::clamp(std::lround(value), 1L, 90L)); });
            });

            density.Width(220);

            controls::StackPanel row{};
            row.Orientation(controls::Orientation::Horizontal);
            row.Spacing(10);
            row.Children().Append(Text(res::GetString(L"LifeDensity"), secondary, 11));
            row.Children().Append(density);
            row.Children().Append(densityValue);
            start.Children().Append(row);
        }

        // A clip or a track whose notes land on the board as cells: once, at the start, or every pass.
        AddListenSource(AddGeneratorSection(settings, L"ListenTo"), clip, edit);

        AddGeneratorSection(settings, L"LifeRule").Children().Append(MakeChipRow(
            Strings({ L"LifeRuleLife", L"LifeRuleHighLife", L"LifeRuleSeeds" }), static_cast<size_t>(generator.Rule), res::GetString(L"LifeRule"),
            [edit](size_t index) { edit([index](seq::GeneratorSettings& edited) { edited.Rule = static_cast<seq::LifeRule>(index); }); }));

        if (!drums)
        {
            auto const plays = AddGeneratorSection(settings, L"LifePlays");
            auto voices = MakeChipRow(Strings({ L"LifeLowest", L"LifeHighest", L"LifeAll" }), static_cast<size_t>(generator.Voice), res::GetString(L"LifePlays"),
                [edit](size_t index) { edit([index](seq::GeneratorSettings& edited) { edited.Voice = static_cast<seq::LifeVoice>(index); }); });

            // Join sits in the same row as the voices, set apart, since it's on or off by itself.
            AppendSeparateChip(voices, MakeChip(res::GetString(L"LifeJoin"), generator.Join, [edit](bool on)
            {
                edit([on](seq::GeneratorSettings& edited) { edited.Join = on; });
            }));

            plays.Children().Append(voices);
        }

        // Whether new cells play like the rest or alone. How loud they play is under Velocity.
        AddGeneratorSection(settings, L"LifeBirths").Children().Append(MakeChipRow(
            Strings({ L"LifeBirthsPlain", L"LifeBirthsOnly" }), generator.Births == seq::LifeBirths::Only ? size_t{ 1 } : size_t{ 0 }, res::GetString(L"LifeBirths"),
            [edit](size_t index) { edit([index](seq::GeneratorSettings& edited) { edited.Births = index == 1 ? seq::LifeBirths::Only : seq::LifeBirths::Plain; }); }));

        AddGeneratorSection(settings, L"LifeSettledTitle").Children().Append(MakeChipRow(
            Strings({ L"LifeStartAgain", L"LifeAddCells", L"LifeHold" }), static_cast<size_t>(generator.Settled), res::GetString(L"LifeSettledTitle"),
            [edit](size_t index) { edit([index](seq::GeneratorSettings& edited) { edited.Settled = static_cast<seq::LifeSettled>(index); }); }));

        std::vector<winrt::hstring> restarts{ res::GetString(L"LifeNever") };

        for (size_t i = 1; i < RestartChoices.size(); ++i)
        {
            restarts.push_back(res::FormatString(L"LifePassesFormat", RestartChoices[i]));
        }

        auto const restart = static_cast<size_t>(std::distance(RestartChoices.begin(), std::find(RestartChoices.begin(), RestartChoices.end(), generator.RestartEvery)));

        AddGeneratorSection(settings, L"LifeRestartEvery").Children().Append(MakeChipRow(restarts, restart, res::GetString(L"LifeRestartEvery"),
            [edit](size_t index) { edit([index](seq::GeneratorSettings& edited) { edited.RestartEvery = RestartChoices[std::min(index, RestartChoices.size() - 1)]; }); }));

        // The drums, or the key and range, beside the rules.
        controls::StackPanel side{};

        if (drums)
        {
            // Each drum's row is named on the board; selecting a name changes the drum.
            auto const section = AddGeneratorSection(side, L"LifeDrums");
            section.Children().Append(Text(res::GetString(L"LifeDrumsTip"), secondary, 11));

            if (generator.DrumRows.size() < seq::MaximumDrumRows)
            {
                controls::FontIcon plus{};
                plus.Glyph(L"\uE710");
                plus.FontSize(12);

                controls::StackPanel addContent{};
                addContent.Orientation(controls::Orientation::Horizontal);
                addContent.Spacing(6);
                addContent.Children().Append(plus);
                addContent.Children().Append(Text(res::GetString(L"LifeAddDrum"), BrushFor(m_palette.Text), 12));

                controls::Button add{};
                add.Style(RootGrid().Resources().Lookup(winrt::box_value(L"SmallButtonStyle")).as<xaml::Style>());
                add.Content(addContent);
                add.HorizontalAlignment(xaml::HorizontalAlignment::Left);
                automation::AutomationProperties::SetName(add, res::GetString(L"LifeAddDrum"));
                add.Click([edit](auto&&, auto&&)
                {
                    edit([](seq::GeneratorSettings& edited)
                    {
                        // A crash, then whichever General MIDI drum isn't there yet.
                        uint8_t note{ 49 };

                        while (std::find(edited.DrumRows.begin(), edited.DrumRows.end(), note) != edited.DrumRows.end() && note < LastDrumName)
                        {
                            ++note;
                        }

                        edited.DrumRows.push_back(note);
                    });
                });

                section.Children().Append(add);
            }
        }
        else
        {
            AddKeyAndRange(side, clip, edit, false);
        }

        AddVelocity(side, clip, edit);
        side.Children().Append(MakeGeneratorNotesButton(clipId));

        body.Children().Append(MakeGeneratorLayout(left, settings, side));
    }
}
