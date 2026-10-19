// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Learn and Words (design section 28). Learn draws what it learned as the scale's notes round a
// circle, with a line for each move, and lights the moves the pass shown makes. Words draws the
// words as notes, each with its letter over it, and plays what's typed as it's typed.

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
        constexpr std::array<wchar_t const*, 12> KeyNames{ L"C", L"C#", L"D", L"D#", L"E", L"F", L"F#", L"G", L"G#", L"A", L"A#", L"B" };

        // Typing waits this long for the next key before it's heard.
        constexpr auto TypingRest = std::chrono::milliseconds{ 450 };

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

        // The scale's notes in the key, by name: C, D, E...
        std::vector<std::wstring> DegreeNames(seq::GeneratorSettings const& generator)
        {
            std::vector<std::wstring> names{};

            for (auto const step : seq::ScaleNotes(0, generator.Scale, 0, 11))
            {
                names.emplace_back(KeyNames[(generator.ScaleRoot + step) % 12]);
            }

            return names;
        }
    }

    _Use_decl_annotations_
    void MainWindow::AddLearnSettings(controls::StackPanel const& body, seq::Clip const& clip, GeneratorEdit const& edit)
    {
        auto const& generator = *clip.Generator;
        auto const color = ClipColor(clip);
        auto const weak = get_weak();
        auto const clipId = clip.Id;
        auto const secondary = BrushFor(m_palette.Text3);

        // ---- what it learned, round a circle

        auto graph = MakeGeneratorCanvas([weak, color](canvas::CanvasDrawingSession const& ds, float width, float height)
        {
            auto s = weak.get();
            auto const found = s ? seq::FindClip(s->m_doc, s->m_editorClipId) : nullptr;

            if (found == nullptr || !found->Generator.has_value())
            {
                return;
            }

            // What the pass shown plays, after what it heard when it keeps listening.
            auto const settings = s->ShownPassSettings(*found, s->ShownGeneratorPass(*found));
            auto const steps = seq::GeneratorStepCount(*found);
            auto const pass = seq::GeneratedPass(settings, found->Seed, steps, s->ShownGeneratorPass(*found));
            auto const playingStep = s->PlayingGeneratorStep(*found);

            std::vector<int32_t> walk{};
            int32_t playing{ -1 };

            for (auto const& note : pass)
            {
                auto const degree = seq::ScaleDegree(settings, note.Number);
                walk.push_back(degree);

                if (playingStep >= note.Step && playingStep < note.Step + note.Steps)
                {
                    playing = degree;
                }
            }

            seq::DrawLearnedMoves(ds, width, height, seq::LearnedMoves(settings), DegreeNames(settings), walk, playing, color, s->m_palette);
        });

        graph.Width(300);
        graph.Height(300);
        automation::AutomationProperties::SetName(graph, res::GetString(L"LearnGraphName"));
        controls::ToolTipService::SetToolTip(graph, winrt::box_value(res::GetString(L"LearnGraphTip")));

        // ---- and what the pass shown plays

        auto roll = MakeGeneratorCanvas([weak, color](canvas::CanvasDrawingSession const& ds, float width, float height)
        {
            auto s = weak.get();
            auto const found = s ? seq::FindClip(s->m_doc, s->m_editorClipId) : nullptr;

            if (found == nullptr || !found->Generator.has_value())
            {
                return;
            }

            auto const settings = s->ShownPassSettings(*found, s->ShownGeneratorPass(*found));
            auto const stepTicks = std::max<int64_t>(1, settings.StepTicks);
            auto const& meter = s->m_doc.Meter.empty() ? seq::MeterChange{} : s->m_doc.Meter.front();

            std::vector<seq::RollNote> notes{};

            for (auto const& note : seq::GeneratedPass(settings, found->Seed, seq::GeneratorStepCount(*found), s->ShownGeneratorPass(*found)))
            {
                notes.push_back(seq::RollNote{ note.Step * stepTicks, note.Steps * stepTicks, note.Number, note.Velocity, false });
            }

            auto const playing = s->PlayingGeneratorStep(*found) >= 0 ? s->m_editorPlayhead : -1;

            // The notes it plays, with a little room, and at least an octave.
            int32_t low{ 127 };
            int32_t high{ 0 };

            for (auto const& note : notes)
            {
                low = std::min<int32_t>(low, note.Number);
                high = std::max<int32_t>(high, note.Number);
            }

            if (notes.empty())
            {
                low = settings.LowNote;
                high = settings.HighNote;
            }

            low -= 2;
            high += 2;

            while (high - low < 12)
            {
                --low;
                ++high;
            }

            seq::DrawRoll(ds, width, height, notes, found->Length, seq::TicksPerBar(meter), seq::TicksPerBeat(meter),
                static_cast<uint8_t>(std::clamp(low, 0, 127)), static_cast<uint8_t>(std::clamp(high, 0, 127)), 0.0f, color, playing, s->m_palette);
        });

        roll.Width(300);
        roll.Height(96);
        automation::AutomationProperties::SetName(roll, res::GetString(L"LearnPassRollName"));

        // The circle as big as the editor's height allows, and the roll as wide as the circle.
        m_fitGenerator = [weak, graph, roll]()
        {
            if (auto s = weak.get())
            {
                auto const size = std::clamp(s->GeneratorPanel().ActualHeight() - 28.0 - 96.0 - 40.0 - 48.0, 220.0, 360.0);
                graph.Width(size);
                graph.Height(size);
                roll.Width(size);
            }
        };

        m_fitGenerator();

        m_passCaption = Text(winrt::hstring{}, secondary, 12);

        // Step through passes without playing, to see how it wanders.
        auto const stepper = [&](wchar_t const* glyph, wchar_t const* nameKey, int64_t by)
        {
            controls::Button button{};
            button.Style(RootGrid().Resources().Lookup(winrt::box_value(L"SubtleSmallButtonStyle")).as<xaml::Style>());
            button.Width(28);
            button.Padding(xaml::Thickness{ 0 });

            controls::FontIcon icon{};
            icon.Glyph(glyph);
            icon.FontSize(12);
            button.Content(icon);

            automation::AutomationProperties::SetName(button, res::GetString(nameKey));
            controls::ToolTipService::SetToolTip(button, winrt::box_value(res::GetString(nameKey)));
            button.IsEnabled(generator.ChangesEachPass);
            button.Click([weak, by](auto&&, auto&&)
            {
                auto s = weak.get();
                auto const found = s ? seq::FindClip(s->m_doc, s->m_editorClipId) : nullptr;

                if (found != nullptr)
                {
                    auto const shown = static_cast<int64_t>(s->ShownGeneratorPass(*found));
                    s->PickGeneratorPass(static_cast<uint64_t>(std::max<int64_t>(0, shown + by)));
                }
            });

            return button;
        };

        controls::StackPanel passes{};
        passes.Orientation(controls::Orientation::Horizontal);
        passes.Spacing(4);
        passes.Children().Append(stepper(L"\uE76B", L"GeneratorPassPrevious", -1));
        passes.Children().Append(m_passCaption);
        passes.Children().Append(stepper(L"\uE76C", L"GeneratorPassNext", 1));

        controls::StackPanel left{};
        left.Spacing(8);
        left.Children().Append(FrameGeneratorDrawing(graph));
        left.Children().Append(FrameGeneratorDrawing(roll));
        left.Children().Append(passes);

        // ---- where it learns from, and how closely it follows

        controls::StackPanel settings{};
        auto const from = AddGeneratorSection(settings, L"LearnFrom");

        // The tunes and words built in, as one row. A clip or a track in the sequence is picked
        // from the list under them.
        auto const sourceIndex = generator.LearnFrom == seq::LearnSource::Tune ? static_cast<size_t>(generator.Tune) :
            generator.LearnFrom == seq::LearnSource::Words ? seq::TuneCount : SIZE_MAX;

        from.Children().Append(MakeChipRow(Strings({ L"LearnOde", L"LearnFrere", L"LearnTwinkle", L"LearnWords" }), sourceIndex, res::GetString(L"LearnFrom"),
            [edit](size_t index)
            {
                edit([index](seq::GeneratorSettings& edited)
                {
                    if (index < seq::TuneCount)
                    {
                        edited.LearnFrom = seq::LearnSource::Tune;
                        edited.Tune = static_cast<uint8_t>(index);
                    }
                    else
                    {
                        edited.LearnFrom = seq::LearnSource::Words;
                    }

                    seq::PrepareGenerator(edited);
                });
            }));

        if (generator.LearnFrom == seq::LearnSource::Words)

        {
            // Words typed into the box this one replaces, not kept yet, carry over to it.
            auto const typed = WordsStillTyped(clip.Id);
            auto const pending = typed != nullptr ? typed : std::make_shared<std::wstring>(generator.Text);

            controls::TextBox words{};
            words.Text(winrt::hstring{ *pending });
            words.MaxLength(static_cast<int32_t>(seq::MaximumGeneratorText));
            words.Width(360);
            words.HorizontalAlignment(xaml::HorizontalAlignment::Left);
            words.PlaceholderText(res::GetString(L"WordsBox"));
            automation::AutomationProperties::SetName(words, res::GetString(L"WordsBoxName"));

            xaml::DispatcherTimer rest{};
            rest.Interval(TypingRest);

            rest.Tick([weak, pending, clipId = clip.Id](foundation::IInspectable const& sender, auto&&)
            {
                sender.as<xaml::DispatcherTimer>().Stop();

                if (auto s = weak.get(); s && s->m_editorClipId == clipId)
                {
                    s->EditGeneratorInPlace([text = *pending](seq::GeneratorSettings& edited) { edited.Text = text; });
                }
            });

            words.TextChanged([pending, rest](foundation::IInspectable const& sender, auto&&)
            {
                *pending = std::wstring{ sender.as<controls::TextBox>().Text() };
                rest.Stop();
                rest.Start();
            });

            TrackTypedWords(rest, pending, clip.Id, typed != nullptr);
            from.Children().Append(words);
        }

        AddListenSource(from, clip, edit);

        AddGeneratorSection(settings, L"LearnMemory").Children().Append(MakeChipRow(
            Strings({ L"LearnWanders", L"LearnBalanced", L"LearnStaysClose" }), static_cast<size_t>(generator.Order) - 1, res::GetString(L"LearnMemory"),
            [edit](size_t index) { edit([index](seq::GeneratorSettings& edited) { edited.Order = static_cast<uint8_t>(index + 1); }); }));

        {
            auto const repeats = AddGeneratorSection(settings, L"LearnRepeats");
            auto value = Text(winrt::hstring{}, BrushFor(m_palette.Text2));
            auto slider = MakeGeneratorSlider(res::GetString(L"LearnRepeats"), 100, generator.Keep, value, [edit](double chosen)
            {
                edit([chosen](seq::GeneratorSettings& edited) { edited.Keep = static_cast<uint8_t>(std::clamp(std::lround(chosen), 0L, 100L)); });
            });

            slider.Width(220);
            slider.IsEnabled(generator.ChangesEachPass);

            controls::StackPanel row{};
            row.Orientation(controls::Orientation::Horizontal);
            row.Spacing(10);
            row.Children().Append(slider);
            row.Children().Append(value);
            repeats.Children().Append(row);
        }

        controls::StackPanel side{};
        AddKeyAndRange(side, clip, edit, true);
        AddVelocity(side, clip, edit);
        side.Children().Append(MakeGeneratorNotesButton(clipId));

        body.Children().Append(MakeGeneratorLayout(left, settings, side));
    }

    _Use_decl_annotations_
    void MainWindow::AddWordsSettings(controls::StackPanel const& body, seq::Clip const& clip, GeneratorEdit const& edit)
    {
        auto const& generator = *clip.Generator;
        auto const color = ClipColor(clip);
        auto const weak = get_weak();
        auto const secondary = BrushFor(m_palette.Text3);

        // The words as notes, each with its letter over it. Worked out as it draws, so what's
        // typed shows straight away.
        auto drawing = MakeGeneratorCanvas([weak, color](canvas::CanvasDrawingSession const& ds, float width, float height)
        {
            auto s = weak.get();
            auto const found = s ? seq::FindClip(s->m_doc, s->m_editorClipId) : nullptr;

            if (found == nullptr || !found->Generator.has_value())
            {
                return;
            }

            auto const& settings = *found->Generator;
            auto const spelled = seq::SpellWords(settings);
            auto const length = seq::SpelledLength(spelled);
            auto const steps = seq::GeneratorStepCount(*found);
            auto const shown = s->ShownGeneratorPass(*found);
            auto const playingStep = s->PlayingGeneratorStep(*found);

            // Words that fit start each pass; longer words carry on from pass to pass.
            auto const from = length <= steps || length == 0 ? int64_t{ 0 } : static_cast<int64_t>((shown * static_cast<uint64_t>(steps)) % static_cast<uint64_t>(length));
            auto const count = std::max(steps, length <= steps ? length : steps);
            auto const playing = playingStep >= 0 ? from + playingStep : -1;

            seq::DrawSpelling(ds, width, height, spelled, from, count, std::min(settings.LowNote, settings.HighNote), std::max(settings.LowNote, settings.HighNote), playing, color, s->m_palette);
        });

        drawing.Height(190);
        automation::AutomationProperties::SetName(drawing, res::GetString(L"WordsDrawingName"));

        m_passCaption = Text(winrt::hstring{}, secondary, 12);

        controls::StackPanel left{};
        left.Spacing(8);
        left.Children().Append(FrameGeneratorDrawing(drawing));
        left.Children().Append(m_passCaption);

        m_fitGenerator = [weak, drawing]()
        {
            if (auto s = weak.get())
            {
                // Beside the words box and the key and range when there's room, or beside the
                // words box alone.
                drawing.Width(s->GeneratorDrawingWidth(300.0, 760.0));
            }
        };

        m_fitGenerator();

        controls::StackPanel settings{};
        auto const section = AddGeneratorSection(settings, L"WordsTitle");

        // Words typed into the box this one replaces, not kept yet, carry over to it.
        auto const typed = WordsStillTyped(clip.Id);
        auto const pending = typed != nullptr ? typed : std::make_shared<std::wstring>(generator.Text);

        controls::TextBox words{};
        words.Text(winrt::hstring{ *pending });
        words.MaxLength(static_cast<int32_t>(seq::MaximumGeneratorText));
        words.FontSize(16);
        words.Width(380);
        words.HorizontalAlignment(xaml::HorizontalAlignment::Left);
        words.PlaceholderText(res::GetString(L"WordsBox"));
        automation::AutomationProperties::SetName(words, res::GetString(L"WordsBoxName"));

        // What's typed is heard a moment after the last key, without building the panel again,
        // so the box keeps its place.
        xaml::DispatcherTimer rest{};
        rest.Interval(TypingRest);

        rest.Tick([weak, pending, clipId = clip.Id](foundation::IInspectable const& sender, auto&&)
        {
            sender.as<xaml::DispatcherTimer>().Stop();

            if (auto s = weak.get(); s && s->m_editorClipId == clipId)
            {
                s->EditGeneratorInPlace([text = *pending](seq::GeneratorSettings& edited) { edited.Text = text; });
            }
        });

        words.TextChanged([pending, rest](foundation::IInspectable const& sender, auto&&)
        {
            *pending = std::wstring{ sender.as<controls::TextBox>().Text() };
            rest.Stop();
            rest.Start();
        });

        TrackTypedWords(rest, pending, clip.Id, typed != nullptr);

        section.Children().Append(words);

        // Wrapped to the box's width, so it lines up under it.
        auto legend = Text(res::GetString(L"WordsLegend"), secondary, 11);
        legend.MaxWidth(380);
        legend.HorizontalAlignment(xaml::HorizontalAlignment::Left);
        section.Children().Append(legend);

        controls::StackPanel side{};
        AddKeyAndRange(side, clip, edit, false);
        AddVelocity(side, clip, edit);
        side.Children().Append(MakeGeneratorNotesButton(clip.Id));

        body.Children().Append(MakeGeneratorLayout(left, settings, side));
    }

    void MainWindow::FlushGeneratorText() noexcept
    {
        try
        {
            if (m_generatorTextTimer == nullptr || !m_generatorTextTimer.IsEnabled())
            {
                return;
            }

            // Stopped first: the edit below comes back here.
            m_generatorTextTimer.Stop();

            if (m_generatorTextPending != nullptr && m_generatorTextClip == m_editorClipId)
            {
                EditGeneratorInPlace([text = *m_generatorTextPending](seq::GeneratorSettings& edited) { edited.Text = text; });
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to keep the words typed.")
    }

    _Use_decl_annotations_
    std::shared_ptr<std::wstring> MainWindow::WordsStillTyped(std::wstring const& clipId) const noexcept
    {
        try
        {
            if (m_generatorTextTimer != nullptr && m_generatorTextTimer.IsEnabled() && m_generatorTextPending != nullptr && m_generatorTextClip == clipId)
            {
                return m_generatorTextPending;
            }
        }
        catch (...)
        {
        }

        return nullptr;
    }

    _Use_decl_annotations_
    void MainWindow::TrackTypedWords(xaml::DispatcherTimer const& timer, std::shared_ptr<std::wstring> pending, std::wstring const& clipId, bool waiting) noexcept
    {
        try
        {
            // The box this replaces stops waiting; what it was waiting on, the new one waits on.
            if (m_generatorTextTimer != nullptr)
            {
                m_generatorTextTimer.Stop();
            }

            m_generatorTextTimer = timer;
            m_generatorTextPending = std::move(pending);
            m_generatorTextClip = clipId;

            if (waiting)
            {
                timer.Start();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to keep the words typed.")
    }
}