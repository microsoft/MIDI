// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The generators that play a stream, a part each pass (design section 28): the infinity series
// drawn as dots, with the copies it hides in itself lit; an L-system's drawing, with the part this
// pass plays lit; and change ringing's blue line.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "GeneratorDrawings.h"
#include "GeneratorKinds.h"
#include "GeneratorView.h"
#include "SequenceGenerator.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
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

        std::vector<winrt::hstring> Strings(std::initializer_list<wchar_t const*> keys)
        {
            std::vector<winrt::hstring> strings{};

            for (auto const key : keys)
            {
                strings.push_back(res::GetString(key));
            }

            return strings;
        }

        // A rule as it's typed and shown: "X=X+YF+".
        std::wstring RuleText(seq::GrammarRule const& rule)
        {
            return std::wstring(1, rule.Symbol) + L"=" + rule.Replacement;
        }
    }

    // ---------------------------------------------------------------- the infinity series

    _Use_decl_annotations_
    void MainWindow::AddInfinitySettings(controls::StackPanel const& body, seq::Clip const& clip, GeneratorEdit const& edit)
    {
        auto const& generator = *clip.Generator;
        auto const color = ClipColor(clip);
        auto const weak = get_weak();
        auto const secondary = BrushFor(m_palette.Text3);

        auto series = MakeGeneratorCanvas([weak, color](canvas::CanvasDrawingSession const& ds, float width, float height)
        {
            auto s = weak.get();
            auto const found = s ? seq::FindClip(s->m_doc, s->m_editorClipId) : nullptr;

            if (found == nullptr || !found->Generator.has_value())
            {
                return;
            }

            // Where the pass is in the series, with what comes before and after it.
            auto const steps = seq::GeneratorStepCount(*found);
            auto const start = seq::InfinityStart(found->Seed);
            auto const passFirst = start + s->ShownGeneratorPass(*found) * static_cast<uint64_t>(steps);
            auto const count = std::max<uint64_t>(96, static_cast<uint64_t>(steps) * 3);
            auto const first = passFirst >= static_cast<uint64_t>(steps) ? passFirst - static_cast<uint64_t>(steps) : 0;
            auto const playingStep = s->PlayingGeneratorStep(*found);

            seq::DrawSeries(ds, width, height, first, count, passFirst, steps, playingStep >= 0 ? static_cast<int64_t>(passFirst) + playingStep : -1, color, s->m_palette);
        });

        series.Width(560);
        series.Height(200);
        automation::AutomationProperties::SetName(series, res::GetString(L"InfinityDrawingName"));
        controls::ToolTipService::SetToolTip(series, winrt::box_value(res::GetString(L"InfinityDrawingTip")));

        // As wide as the room beside the settings, and as tall as the editor allows.
        m_fitGenerator = [weak, series]()
        {
            if (auto s = weak.get())
            {
                series.Width(s->GeneratorDrawingWidth(400.0, 760.0));
                series.Height(std::clamp(s->GeneratorPanel().ActualHeight() - 28.0 - 70.0, 160.0, 340.0));
            }
        };

        m_fitGenerator();

        m_passCaption = Text(winrt::hstring{}, secondary, 12);

        controls::StackPanel left{};
        left.Spacing(8);
        left.Children().Append(FrameGeneratorDrawing(series));
        left.Children().Append(Text(res::FormatString(L"InfinityStartsFormat", seq::InfinityStart(clip.Seed) + 1), secondary, 11));
        left.Children().Append(m_passCaption);

        controls::StackPanel settings{};

        AddGeneratorSection(settings, L"InfinityInterval").Children().Append(MakeChipRow(
            Strings({ L"InfinitySteps", L"InfinityThirds", L"InfinityFourths" }), static_cast<size_t>(generator.Interval) - 1, res::GetString(L"InfinityInterval"),
            [edit](size_t index) { edit([index](seq::GeneratorSettings& edited) { edited.Interval = static_cast<uint8_t>(index + 1); }); }));

        AddGeneratorSection(settings, L"InfinityVoices").Children().Append(MakeChipRow(
            Strings({ L"InfinityOneVoice", L"InfinityTwoVoices", L"InfinityThreeVoices" }), static_cast<size_t>(generator.Voices) - 1, res::GetString(L"InfinityVoices"),
            [edit](size_t index) { edit([index](seq::GeneratorSettings& edited) { edited.Voices = static_cast<uint8_t>(index + 1); }); }));

        {
            auto const busy = AddGeneratorSection(settings, L"GeneratorBusy");
            auto value = Text(winrt::hstring{}, BrushFor(m_palette.Text2));
            auto slider = MakeGeneratorSlider(res::GetString(L"GeneratorBusy"), 100, generator.Busy, value, [edit](double chosen)
            {
                edit([chosen](seq::GeneratorSettings& edited) { edited.Busy = static_cast<uint8_t>(std::clamp(std::lround(chosen), 0L, 100L)); });
            });

            slider.Width(220);

            controls::StackPanel row{};
            row.Orientation(controls::Orientation::Horizontal);
            row.Spacing(10);
            row.Children().Append(slider);
            row.Children().Append(value);
            busy.Children().Append(row);
        }

        controls::StackPanel side{};
        AddKeyAndRange(side, clip, edit, false);
        AddVelocity(side, clip, edit);
        side.Children().Append(MakeGeneratorNotesButton(clip.Id));

        body.Children().Append(MakeGeneratorLayout(left, settings, side));
    }

    // ---------------------------------------------------------------- L-systems

    _Use_decl_annotations_
    void MainWindow::AddLSystemSettings(controls::StackPanel const& body, seq::Clip const& clip, GeneratorEdit const& edit)
    {
        auto const& generator = *clip.Generator;
        auto const color = ClipColor(clip);
        auto const weak = get_weak();
        auto const secondary = BrushFor(m_palette.Text3);

        // Grown once for the drawing; any change to the rules builds the panel again.
        auto const turtle = std::make_shared<seq::TurtleDrawing>(seq::LSystemTurtle(generator));

        auto drawing = MakeGeneratorCanvas([weak, color, turtle](canvas::CanvasDrawingSession const& ds, float width, float height)
        {
            auto s = weak.get();
            auto const found = s ? seq::FindClip(s->m_doc, s->m_editorClipId) : nullptr;

            if (found == nullptr || !found->Generator.has_value())
            {
                return;
            }

            auto const steps = seq::GeneratorStepCount(*found);
            auto const length = std::max<int64_t>(1, turtle->Steps);
            auto const passFirst = length <= steps ? int64_t{ 0 } : static_cast<int64_t>((s->ShownGeneratorPass(*found) * static_cast<uint64_t>(steps)) % static_cast<uint64_t>(length));
            auto const playingStep = s->PlayingGeneratorStep(*found);

            seq::DrawTurtle(ds, width, height, *turtle, passFirst, steps, playingStep >= 0 ? passFirst + playingStep : -1, color, s->m_palette);
        });

        drawing.Width(360);
        drawing.Height(360);
        automation::AutomationProperties::SetName(drawing, res::GetString(L"LSystemDrawingName"));
        controls::ToolTipService::SetToolTip(drawing, winrt::box_value(res::GetString(L"LSystemDrawingTip")));

        // Square, as big as the editor's height allows.
        m_fitGenerator = [weak, drawing]()
        {
            if (auto s = weak.get())
            {
                auto const size = std::clamp(s->GeneratorPanel().ActualHeight() - 28.0 - 80.0, 240.0, 460.0);
                drawing.Width(size);
                drawing.Height(size);
            }
        };

        m_fitGenerator();

        m_passCaption = Text(winrt::hstring{}, secondary, 12);

        controls::StackPanel left{};
        left.Spacing(8);
        left.Children().Append(FrameGeneratorDrawing(drawing));
        left.Children().Append(Text(res::FormatString(L"LSystemGrownFormat", turtle->Generations, turtle->Steps), secondary, 11));
        left.Children().Append(m_passCaption);

        controls::StackPanel settings{};

        // The shapes, and a chip that's lit when the rules are your own.
        auto const matching = seq::MatchingLSystemPreset(generator);
        std::vector<winrt::hstring> shapes{};

        for (auto const& preset : seq::LSystemPresets())
        {
            shapes.push_back(res::GetString(preset.NameKey));
        }

        auto const shapeRow = MakeChipRow(shapes, matching, res::GetString(L"LSystemShape"), [edit](size_t index)
        {
            edit([index](seq::GeneratorSettings& edited) { seq::UseLSystemPreset(edited, index); });
        });

        // A chip that's lit when the rules are your own, there only then: the rules below are
        // what make them yours.
        if (matching == SIZE_MAX)
        {
            auto own = MakeChip(res::GetString(L"LSystemYourOwn"), true, [](bool) {});
            own.IsHitTestVisible(false);
            own.IsTabStop(false);
            shapeRow.Children().GetAt(0).as<winrt::midisequencer::WrapPanel>().Children().Append(own);
        }

        AddGeneratorSection(settings, L"LSystemShape").Children().Append(shapeRow);

        // The rules themselves, for anyone who wants to grow their own.
        controls::StackPanel rules{};
        rules.Spacing(6);

        auto const commitAxiom = [weak](winrt::hstring const& text)
        {
            if (auto s = weak.get())
            {
                s->EditGenerator(std::wstring{ res::GetString(L"UndoGeneratorSettings") }, [axiom = std::wstring{ text }](seq::GeneratorSettings& edited)
                {
                    edited.Axiom = axiom.substr(0, seq::MaximumGrammarLength);
                });
            }
        };

        controls::TextBox axiom{};
        axiom.Header(winrt::box_value(res::GetString(L"LSystemAxiom")));
        axiom.Text(winrt::hstring{ generator.Axiom });
        axiom.FontFamily(media::FontFamily{ L"Cascadia Mono,Consolas" });
        axiom.Width(300);
        axiom.HorizontalAlignment(xaml::HorizontalAlignment::Left);
        automation::AutomationProperties::SetName(axiom, res::GetString(L"LSystemAxiom"));
        axiom.LostFocus([commitAxiom](foundation::IInspectable const& sender, auto&&) { commitAxiom(sender.as<controls::TextBox>().Text()); });
        axiom.KeyDown([commitAxiom](foundation::IInspectable const& sender, input::KeyRoutedEventArgs const& args)
        {
            if (args.Key() == winrt::Windows::System::VirtualKey::Enter)
            {
                args.Handled(true);
                commitAxiom(sender.as<controls::TextBox>().Text());
            }
        });
        rules.Children().Append(axiom);

        // A box for each rule, and one more for a new one. Emptied, a rule goes.
        auto const ruleCount = std::min(generator.Grammar.size() + 1, seq::MaximumGrammarRules);

        for (size_t i = 0; i < ruleCount; ++i)
        {
            auto const commit = [weak, i](winrt::hstring const& text)
            {
                auto s = weak.get();

                if (!s)
                {
                    return;
                }

                std::wstring typed{ text };
                std::erase(typed, L' ');
                auto const equals = typed.find(L'=');

                s->EditGenerator(std::wstring{ res::GetString(L"UndoGeneratorSettings") }, [typed, equals, i](seq::GeneratorSettings& edited)
                {
                    if (equals == 1 && typed.size() >= 2)
                    {
                        seq::GrammarRule rule{ typed.front(), typed.substr(2, seq::MaximumGrammarLength) };

                        if (i < edited.Grammar.size())
                        {
                            edited.Grammar[i] = rule;
                        }
                        else
                        {
                            edited.Grammar.push_back(rule);
                        }
                    }
                    else if (typed.empty() && i < edited.Grammar.size())
                    {
                        edited.Grammar.erase(edited.Grammar.begin() + static_cast<ptrdiff_t>(i));
                    }
                });
            };

            controls::TextBox rule{};
            rule.Text(i < generator.Grammar.size() ? winrt::hstring{ RuleText(generator.Grammar[i]) } : winrt::hstring{});
            rule.PlaceholderText(res::GetString(L"LSystemRulePlaceholder"));
            rule.FontFamily(media::FontFamily{ L"Cascadia Mono,Consolas" });
            rule.Width(300);
            rule.HorizontalAlignment(xaml::HorizontalAlignment::Left);
            automation::AutomationProperties::SetName(rule, res::FormatString(L"LSystemRuleFormat", i + 1));
            rule.LostFocus([commit](foundation::IInspectable const& sender, auto&&) { commit(sender.as<controls::TextBox>().Text()); });
            rule.KeyDown([commit](foundation::IInspectable const& sender, input::KeyRoutedEventArgs const& args)
            {
                if (args.Key() == winrt::Windows::System::VirtualKey::Enter)
                {
                    args.Handled(true);
                    commit(sender.as<controls::TextBox>().Text());
                }
            });
            rules.Children().Append(rule);
        }

        {
            auto value = Text(winrt::hstring{}, BrushFor(m_palette.Text2));
            auto angle = MakeGeneratorSlider(res::GetString(L"LSystemAngle"), 180, generator.Angle, value, [edit](double chosen)
            {
                edit([chosen](seq::GeneratorSettings& edited) { edited.Angle = static_cast<uint8_t>(std::clamp(std::lround(chosen), 0L, 180L)); });
            });

            // The slider shows a percent; an angle reads better in degrees.
            value.Text(winrt::hstring{ std::format(L"{}\u00B0", generator.Angle) });
            angle.ValueChanged([value](auto&&, primitives::RangeBaseValueChangedEventArgs const& args)
            {
                value.Text(winrt::hstring{ std::format(L"{}\u00B0", std::lround(args.NewValue())) });
            });
            angle.Width(200);

            controls::StackPanel row{};
            row.Orientation(controls::Orientation::Horizontal);
            row.Spacing(10);
            row.Children().Append(Text(res::GetString(L"LSystemAngle"), BrushFor(m_palette.Text2), 12));
            row.Children().Append(angle);
            row.Children().Append(value);
            rules.Children().Append(row);
        }

        rules.Children().Append(Text(res::GetString(L"LSystemLegend"), secondary, 11));

        controls::Expander expander{};
        expander.Header(winrt::box_value(res::GetString(L"LSystemRules")));
        expander.Content(rules);
        expander.IsExpanded(matching == SIZE_MAX);
        expander.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
        expander.HorizontalContentAlignment(xaml::HorizontalAlignment::Left);
        settings.Children().Append(expander);

        controls::StackPanel side{};
        AddKeyAndRange(side, clip, edit, false);
        AddVelocity(side, clip, edit);
        side.Children().Append(MakeGeneratorNotesButton(clip.Id));

        body.Children().Append(MakeGeneratorLayout(left, settings, side));
    }

    // ---------------------------------------------------------------- change ringing

    _Use_decl_annotations_
    void MainWindow::AddRingingSettings(controls::StackPanel const& body, seq::Clip const& clip, GeneratorEdit const& edit)
    {
        auto const& generator = *clip.Generator;
        auto const color = ClipColor(clip);
        auto const weak = get_weak();
        auto const secondary = BrushFor(m_palette.Text3);
        auto const course = std::make_shared<std::vector<std::vector<uint8_t>>>(seq::RingingCourse(generator));

        auto drawing = MakeGeneratorCanvas([weak, color, course](canvas::CanvasDrawingSession const& ds, float width, float height)
        {
            auto s = weak.get();
            auto const found = s ? seq::FindClip(s->m_doc, s->m_editorClipId) : nullptr;

            if (found == nullptr || !found->Generator.has_value() || course->empty())
            {
                return;
            }

            auto const& settings = *found->Generator;
            auto const bells = static_cast<int64_t>(course->front().size());
            auto const steps = seq::GeneratorStepCount(*found);
            auto const length = std::max<int64_t>(1, seq::RingingRowStep(settings, course->size()));
            auto const block = settings.HandstrokeGap ? bells * 2 + 1 : bells;
            auto const playingStep = s->PlayingGeneratorStep(*found);
            auto const at = static_cast<int64_t>((s->ShownGeneratorPass(*found) * static_cast<uint64_t>(steps)) % static_cast<uint64_t>(length)) + std::max<int64_t>(0, playingStep);
            auto const wrapped = at % length;

            // Which row and place the step is, counting rows on from the start of the course.
            auto row = static_cast<size_t>(wrapped / block) * (settings.HandstrokeGap ? 2 : 1);
            auto place = static_cast<int32_t>(wrapped % block);

            if (settings.HandstrokeGap && place >= bells)
            {
                ++row;
                place = static_cast<int32_t>(place - bells);
            }

            row += static_cast<size_t>(at / length) * course->size();

            // Counted from one course on, so a few rows before the start of the course show too.
            auto const rows = std::min<size_t>(course->size() + 1, static_cast<size_t>(std::max(4.0f, height / 18.0f)));
            auto const playingRow = row + course->size();
            auto const first = playingRow - 3;

            seq::DrawBlueLine(ds, width, height, *course, first, rows, playingStep >= 0 ? playingRow : SIZE_MAX, playingStep >= 0 ? place : -1, settings.HandstrokeGap, color, s->m_palette);
        });

        drawing.Width(260);
        drawing.Height(400);
        automation::AutomationProperties::SetName(drawing, res::GetString(L"RingingDrawingName"));
        controls::ToolTipService::SetToolTip(drawing, winrt::box_value(res::GetString(L"RingingDrawingTip")));

        // As tall as the editor allows, so what's under it stays in view.
        m_fitGenerator = [weak, drawing]()
        {
            if (auto s = weak.get())
            {
                drawing.Height(std::clamp(s->GeneratorPanel().ActualHeight() - 28.0 - 80.0, 200.0, 460.0));
            }
        };

        m_fitGenerator();

        m_passCaption = Text(winrt::hstring{}, secondary, 12);

        controls::StackPanel left{};
        left.Spacing(8);
        left.Children().Append(FrameGeneratorDrawing(drawing));
        left.Children().Append(Text(res::FormatString(L"RingingCourseFormat", course->size()), secondary, 11));
        left.Children().Append(m_passCaption);

        controls::StackPanel settings{};

        std::vector<winrt::hstring> bells{};

        for (auto count = seq::MinimumBells; count <= seq::MaximumBells; ++count)
        {
            bells.push_back(winrt::hstring{ std::to_wstring(count) });
        }

        AddGeneratorSection(settings, L"RingingBells").Children().Append(MakeChipRow(bells, static_cast<size_t>(generator.Bells - seq::MinimumBells), res::GetString(L"RingingBells"),
            [edit](size_t index) { edit([index](seq::GeneratorSettings& edited) { edited.Bells = static_cast<uint8_t>(seq::MinimumBells + index); }); }));

        auto const method = AddGeneratorSection(settings, L"RingingMethod");
        auto methods = MakeChipRow(Strings({ L"RingingPlainHunt", L"RingingPlainBob", L"RingingGrandsire" }), static_cast<size_t>(generator.Method), res::GetString(L"RingingMethod"),
            [edit](size_t index) { edit([index](seq::GeneratorSettings& edited) { edited.Method = static_cast<seq::RingingMethod>(index); }); });

        // The gap sits in the same row as the methods, set apart, since it's on or off by itself.
        AppendSeparateChip(methods, MakeChip(res::GetString(L"RingingGap"), generator.HandstrokeGap, [edit](bool on)
        {
            edit([on](seq::GeneratorSettings& edited) { edited.HandstrokeGap = on; });
        }));

        method.Children().Append(methods);

        if (generator.Method == seq::RingingMethod::Grandsire && generator.Bells % 2 == 0)
        {
            // Wrapped to about the methods' width, so the note doesn't widen the column, and narrow
            // enough that its two lines come out about even.
            auto note = Text(res::GetString(L"RingingTenorCovers"), secondary, 11);
            note.MaxWidth(220);
            note.HorizontalAlignment(xaml::HorizontalAlignment::Left);
            method.Children().Append(note);
        }

        // A clip or a track whose most played notes become the bells, once or every pass.
        AddListenSource(AddGeneratorSection(settings, L"ListenTo"), clip, edit);

        controls::StackPanel side{};
        AddKeyAndRange(side, clip, edit, false);
        AddVelocity(side, clip, edit);
        side.Children().Append(MakeGeneratorNotesButton(clip.Id));

        body.Children().Append(MakeGeneratorLayout(left, settings, side));
    }
}
