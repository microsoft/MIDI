// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Evolve (design section 28): the pass before and the eight copies made from it, like a family
// tree, the one chosen framed and each with a bar for its score; under that, how the score climbs
// from pass to pass. On the right, sliders for what it listens for.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "GeneratorDrawings.h"
#include "GeneratorKinds.h"
#include "SequenceGenerator.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        constexpr std::array<wchar_t const*, 5> RuleKeys{ L"EvolveSteps", L"EvolveShape", L"EvolveSyncopation", L"EvolveMotif", L"EvolveEnding" };

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
    }

    _Use_decl_annotations_
    void MainWindow::AddEvolveSettings(controls::StackPanel const& body, seq::Clip const& clip, GeneratorEdit const& edit)
    {
        auto const& generator = *clip.Generator;
        auto const color = ClipColor(clip);
        auto const weak = get_weak();
        auto const secondary = BrushFor(m_palette.Text3);

        auto family = MakeGeneratorCanvas([weak, color](canvas::CanvasDrawingSession const& ds, float width, float height)
        {
            auto s = weak.get();
            auto const found = s ? seq::FindClip(s->m_doc, s->m_editorClipId) : nullptr;

            if (found == nullptr || !found->Generator.has_value())
            {
                return;
            }

            // This pass on top, and the copies made from it below: the framed one plays next.
            auto const& settings = *found->Generator;
            auto const steps = seq::GeneratorStepCount(*found);
            auto const hearing = s->ShownPassHearing(*found, 0);
            auto const shown = seq::EvolveFamilyAt(settings, found->Seed, steps, settings.ChangesEachPass ? s->ShownGeneratorPass(*found) + 1 : 0, hearing.Before ? &hearing : nullptr);
            seq::DrawFamily(ds, width, height, shown, steps, std::min(settings.LowNote, settings.HighNote), std::max(settings.LowNote, settings.HighNote), color, s->m_palette);
        });

        family.Width(440);
        family.Height(270);
        automation::AutomationProperties::SetName(family, res::GetString(L"EvolveFamilyName"));
        controls::ToolTipService::SetToolTip(family, winrt::box_value(res::GetString(L"EvolveFamilyTip")));

        auto scores = MakeGeneratorCanvas([weak, color](canvas::CanvasDrawingSession const& ds, float width, float height)
        {
            auto s = weak.get();
            auto const found = s ? seq::FindClip(s->m_doc, s->m_editorClipId) : nullptr;

            if (found == nullptr || !found->Generator.has_value())
            {
                return;
            }

            // At least the first 64 passes, and always a little past the one shown.
            auto const shown = s->ShownGeneratorPass(*found);
            auto const count = std::clamp<uint64_t>(shown + 16, 64, 2000);
            auto const hearing = s->ShownPassHearing(*found, 0);
            seq::DrawScores(ds, width, height, seq::EvolveScores(*found->Generator, found->Seed, seq::GeneratorStepCount(*found), count, hearing.Before ? &hearing : nullptr), shown, color, s->m_palette);
        });

        scores.Width(440);
        scores.Height(54);
        automation::AutomationProperties::SetName(scores, res::GetString(L"EvolveScoresName"));
        controls::ToolTipService::SetToolTip(scores, winrt::box_value(res::GetString(L"EvolveScoresName")));

        m_passCaption = Text(winrt::hstring{}, secondary, 12);

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
                    s->PickGeneratorPass(static_cast<uint64_t>(std::max<int64_t>(0, static_cast<int64_t>(s->ShownGeneratorPass(*found)) + by)));
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
        left.Children().Append(FrameGeneratorDrawing(family));
        left.Children().Append(FrameGeneratorDrawing(scores));
        left.Children().Append(passes);

        controls::StackPanel settings{};

        // A clip or a track to start from and to sound like, once or every pass.
        AddListenSource(AddGeneratorSection(settings, L"ListenTo"), clip, edit);

        // What it listens for: a slider each, how much each rule counts.
        auto const rules = AddGeneratorSection(settings, L"EvolveListensFor");

        auto const slider = [&](controls::StackPanel const& panel, winrt::hstring const& name, double maximum, double value, std::function<void(double)> commit)
        {
            auto readout = Text(winrt::hstring{}, BrushFor(m_palette.Text2));
            auto control = MakeGeneratorSlider(name, maximum, value, readout, std::move(commit));
            control.Width(200);

            auto label = Text(name, BrushFor(m_palette.Text2), 12);
            label.Width(150);

            controls::StackPanel row{};
            row.Orientation(controls::Orientation::Horizontal);
            row.Spacing(10);
            row.Children().Append(label);
            row.Children().Append(control);
            row.Children().Append(readout);
            panel.Children().Append(row);
        };

        for (size_t i = 0; i < RuleKeys.size(); ++i)
        {
            slider(rules, res::GetString(RuleKeys[i]), 100, generator.Weights[i], [edit, i](double value)
            {
                edit([value, i](seq::GeneratorSettings& edited) { edited.Weights[i] = static_cast<uint8_t>(std::clamp(std::lround(value), 0L, 100L)); });
            });
        }

        // Sounding like what it listens to, once it has something to listen to.
        if (generator.ListenTo != seq::ListenKind::None)
        {
            slider(rules, res::GetString(L"EvolveSoundsLike"), 100, generator.SoundsLike, [edit](double value)
            {
                edit([value](seq::GeneratorSettings& edited) { edited.SoundsLike = static_cast<uint8_t>(std::clamp(std::lround(value), 0L, 100L)); });
            });
        }

        // How busy has no heading of its own: its row says it, set a little apart from the rules.
        controls::StackPanel busy{};
        busy.Margin(xaml::Thickness{ 0, 10, 0, 0 });
        settings.Children().Append(busy);

        slider(busy, res::GetString(L"GeneratorBusy"), 100, generator.Busy, [edit](double value)
        {
            edit([value](seq::GeneratorSettings& edited) { edited.Busy = static_cast<uint8_t>(std::clamp(std::lround(value), 0L, 100L)); });
        });

        AddGeneratorSection(settings, L"EvolveMutations").Children().Append(MakeChipRow(
            { L"1", L"2", L"3", L"4" }, static_cast<size_t>(generator.Mutations) - 1, res::GetString(L"EvolveMutations"),
            [edit](size_t index) { edit([index](seq::GeneratorSettings& edited) { edited.Mutations = static_cast<uint8_t>(index + 1); }); }));

        controls::StackPanel side{};
        AddKeyAndRange(side, clip, edit, false);
        AddVelocity(side, clip, edit);
        side.Children().Append(MakeGeneratorNotesButton(clip.Id));

        body.Children().Append(MakeGeneratorLayout(left, settings, side));
    }
}
