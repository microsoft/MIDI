// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// How loud a design section 28 generator plays: a chip for each way it can set a note's velocity,
// its own ways first, then the pass's notes drawn as stems over the band they play in. Drag the
// band's top or bottom edge, or its middle to move it, and hear the difference.

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
        constexpr double DrawingWidth = 400.0;
        constexpr double DrawingHeight = 64.0;

        // How near an edge of the band picks it up, and how far a wheel notch moves the band: 10%.
        constexpr float GripReach = 6.0f;
        constexpr int32_t WheelMove = 6554;

        // Each shape's name, in VelocityShape's order. What it does is the same key with "Tip".
        constexpr std::array<wchar_t const*, 17> ShapeKeys{
            L"VelocityShapeSteady", L"VelocityShapeBeat", L"VelocityShapeSwell", L"VelocityShapeRandom",
            L"VelocityShapeAge", L"VelocityShapeCrowding", L"VelocityShapeSurprise", L"VelocityShapeLearned",
            L"VelocityShapeSpeech", L"VelocityShapeEvolved", L"VelocityShapeFourths", L"VelocityShapeDistance",
            L"VelocityShapeClimb", L"VelocityShapeDepth", L"VelocityShapeLead", L"VelocityShapeStrokes", L"VelocityShapeTreble" };

        double Percent(uint32_t value) noexcept
        {
            return std::round(static_cast<double>(value) * 100.0 / 65535.0);
        }

        uint16_t FromPercent(double value) noexcept
        {
            return static_cast<uint16_t>(std::clamp(std::lround(value * 65535.0 / 100.0), 0L, 65535L));
        }

        // A dragged edge lands on a whole percent, as the boxes show it.
        uint16_t WholePercent(uint16_t velocity) noexcept
        {
            return FromPercent(Percent(velocity));
        }

        controls::TextBlock Text(winrt::hstring const& text, media::Brush const& brush, double size = 12)
        {
            controls::TextBlock block{};
            block.Text(text);
            block.FontSize(size);
            block.Foreground(brush);
            block.VerticalAlignment(xaml::VerticalAlignment::Center);
            return block;
        }
    }

    _Use_decl_annotations_
    void MainWindow::AddVelocity(controls::StackPanel const& panel, seq::Clip const& clip, GeneratorEdit const& edit)
    {
        auto const& generator = *clip.Generator;
        auto const section = AddGeneratorSection(panel, L"GeneratorVelocity");
        auto const weak = get_weak();

        // The ways this type sets velocity, its own first. Playing loud and soft as learned needs
        // a clip that said how loud; with none, it plays by surprise, and that chip is lit.
        auto shapes = seq::VelocityShapesFor(generator.Type);
        auto const levels = generator.LearnFrom == seq::LearnSource::Clip &&
            std::any_of(generator.Learned.begin(), generator.Learned.end(), [](seq::LearnedNote const& note) { return note.Level > 0; });

        if (!levels)
        {
            std::erase(shapes, seq::VelocityShape::Learned);
        }

        // Softer on each branch needs rules that branch, and most of the shapes don't. It stays
        // while it's in use.
        auto const branches = generator.Axiom.find(L'[') != std::wstring::npos ||
            std::any_of(generator.Grammar.begin(), generator.Grammar.end(), [](seq::GrammarRule const& rule) { return rule.Replacement.find(L'[') != std::wstring::npos; });

        if (!branches && generator.VelocityBy != seq::VelocityShape::Depth)
        {
            std::erase(shapes, seq::VelocityShape::Depth);
        }

        auto const plays = !levels && generator.VelocityBy == seq::VelocityShape::Learned ? seq::VelocityShape::Surprise : generator.VelocityBy;
        auto const selected = static_cast<size_t>(std::distance(shapes.begin(), std::find(shapes.begin(), shapes.end(), plays)));

        std::vector<winrt::hstring> labels{};

        for (auto const shape : shapes)
        {
            labels.push_back(res::GetString(ShapeKeys[std::min<size_t>(static_cast<size_t>(shape), ShapeKeys.size() - 1)]));
        }

        auto const row = MakeChipRow(labels, selected, res::GetString(L"GeneratorVelocity"), [edit, shapes](size_t index)
        {
            auto const shape = shapes[std::min(index, shapes.size() - 1)];
            edit([shape](seq::GeneratorSettings& edited) { edited.VelocityBy = shape; });
        });

        // Each chip says what it does when the pointer rests on it.
        auto const chips = row.Children().GetAt(0).as<winrt::midisequencer::WrapPanel>().Children();

        for (uint32_t i = 0; i < chips.Size() && i < shapes.size(); ++i)
        {
            auto const tip = res::GetString(std::wstring{ ShapeKeys[std::min<size_t>(static_cast<size_t>(shapes[i]), ShapeKeys.size() - 1)] } + L"Tip");
            controls::ToolTipService::SetToolTip(chips.GetAt(i), winrt::box_value(tip));
            automation::AutomationProperties::SetFullDescription(chips.GetAt(i), tip);
        }

        section.Children().Append(row);

        // The pass shown, each note as loud as it plays, over the band it plays in.
        auto const color = ClipColor(clip);

        auto drawing = MakeGeneratorCanvas([weak, color](canvas::CanvasDrawingSession const& ds, float width, float height)
        {
            auto s = weak.get();
            auto const found = s ? seq::FindClip(s->m_doc, s->m_editorClipId) : nullptr;

            if (found == nullptr || !found->Generator.has_value())
            {
                return;
            }

            // The pass shown, after what it heard when it keeps listening.
            auto const pass = s->ShownGeneratorPass(*found);
            auto const settings = s->ShownPassSettings(*found, pass);
            auto const hearing = s->ShownPassHearing(*found, pass);
            auto const steps = seq::GeneratorStepCount(*found);
            auto notes = seq::GeneratedPass(settings, found->Seed, steps, pass, hearing.Before ? &hearing : nullptr);
            auto low = std::min(settings.VelocityLow, settings.VelocityHigh);
            auto high = std::max(settings.VelocityLow, settings.VelocityHigh);

            // While the band is dragged, the notes move and stretch with it.
            if (auto const& drag = s->m_velocityDrag; drag.Active)
            {
                for (auto& note : notes)
                {
                    auto const share = high > low ? (static_cast<double>(note.Velocity) - low) / static_cast<double>(high - low) : 0.5;
                    note.Velocity = static_cast<uint16_t>(std::clamp(std::lround(drag.Low + share * (static_cast<double>(drag.High) - drag.Low)), 1L, 65535L));
                }

                low = drag.Low;
                high = drag.High;
            }

            seq::DrawVelocities(ds, width, height, notes, steps, settings.StepTicks, low, high, s->PlayingGeneratorStep(*found), color, s->m_palette);
        });

        drawing.Width(DrawingWidth);
        drawing.Height(DrawingHeight);

        // At the column's left edge, under the keyboard, however wide the chips make the column.
        drawing.HorizontalAlignment(xaml::HorizontalAlignment::Left);
        automation::AutomationProperties::SetName(drawing, res::GetString(L"GeneratorVelocityBand"));
        controls::ToolTipService::SetToolTip(drawing, winrt::box_value(res::GetString(L"GeneratorVelocityBand")));

        drawing.PointerPressed([weak](foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args)
        {
            auto s = weak.get();
            auto const found = s ? seq::FindClip(s->m_doc, s->m_editorClipId) : nullptr;

            if (found == nullptr || !found->Generator.has_value() || s->m_readOnly)
            {
                return;
            }

            auto const canvas = sender.as<canvasXaml::CanvasControl>();
            auto const height = static_cast<float>(canvas.ActualHeight());
            auto const point = args.GetCurrentPoint(canvas).Position();
            auto const y = point.Y;
            auto const& settings = *found->Generator;

            VelocityDrag drag{};
            drag.Active = true;
            drag.StartLow = std::min(settings.VelocityLow, settings.VelocityHigh);
            drag.StartHigh = std::max(settings.VelocityLow, settings.VelocityHigh);
            drag.Low = drag.StartLow;
            drag.High = drag.StartHigh;
            drag.From = seq::VelocityAtY(height, y);

            auto const top = seq::VelocityY(height, drag.StartHigh);
            auto const bottom = seq::VelocityY(height, drag.StartLow);
            auto const middle = (top + bottom) / 2.0f;

            // On the grips at the right, the nearer edge. Elsewhere, near an edge picks up that
            // edge, inside the band moves the whole band, and outside it the nearer edge jumps
            // there. Near is a third of a narrow band at most, so its middle can still be moved.
            auto const reach = std::min(GripReach, (bottom - top) / 3.0f);

            if (point.X >= static_cast<float>(canvas.ActualWidth()) - seq::VelocityGripRoom)
            {
                drag.Grip = y <= middle ? 1 : 0;
            }
            else if (std::abs(y - top) <= reach && y <= middle)
            {
                drag.Grip = 1;
            }
            else if (std::abs(y - bottom) <= reach)
            {
                drag.Grip = 0;
            }
            else if (y > top && y < bottom)
            {
                drag.Grip = 2;
            }
            else
            {
                drag.Grip = y < top ? 1 : 0;
                (drag.Grip == 0 ? drag.Low : drag.High) = WholePercent(drag.From);
            }

            s->m_velocityDrag = drag;
            canvas.CapturePointer(args.Pointer());
            canvas.Invalidate();
            args.Handled(true);
        });

        drawing.PointerMoved([weak](foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args)
        {
            auto s = weak.get();

            if (!s || !s->m_velocityDrag.Active)
            {
                return;
            }

            auto const canvas = sender.as<canvasXaml::CanvasControl>();
            auto const at = seq::VelocityAtY(static_cast<float>(canvas.ActualHeight()), args.GetCurrentPoint(canvas).Position().Y);
            auto& drag = s->m_velocityDrag;

            switch (drag.Grip)
            {
            case 0:
                drag.Low = std::min(WholePercent(at), drag.High);
                break;

            case 1:
                drag.High = std::max(WholePercent(at), drag.Low);
                break;

            default:
            {
                auto const moved = std::clamp(static_cast<int32_t>(at) - static_cast<int32_t>(drag.From),
                    -static_cast<int32_t>(drag.StartLow), 65535 - static_cast<int32_t>(drag.StartHigh));
                auto const width = static_cast<int32_t>(drag.StartHigh) - static_cast<int32_t>(drag.StartLow);
                auto const lowest = moved == 0 ? static_cast<int32_t>(drag.StartLow)
                    : std::clamp(static_cast<int32_t>(WholePercent(static_cast<uint16_t>(drag.StartLow + moved))), 0, 65535 - width);
                drag.Low = static_cast<uint16_t>(lowest);
                drag.High = static_cast<uint16_t>(lowest + width);
                break;
            }
            }

            canvas.Invalidate();
            args.Handled(true);
        });

        auto const letGo = [weak](foundation::IInspectable const& sender, input::PointerRoutedEventArgs const&)
        {
            auto s = weak.get();

            if (!s || !s->m_velocityDrag.Active)
            {
                return;
            }

            // Cleared first: letting go of the capture comes straight back here.
            auto const drag = s->m_velocityDrag;
            s->m_velocityDrag = VelocityDrag{};
            sender.as<canvasXaml::CanvasControl>().ReleasePointerCaptures();

            if (drag.Low != drag.StartLow || drag.High != drag.StartHigh)
            {
                s->SetGeneratorVelocity(drag.Low, drag.High);
            }
            else
            {
                sender.as<canvasXaml::CanvasControl>().Invalidate();
            }
        };

        drawing.PointerReleased(letGo);
        drawing.PointerCaptureLost(letGo);

        drawing.PointerWheelChanged([weak](foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args)
        {
            auto s = weak.get();
            args.Handled(true);

            auto const found = s ? seq::FindClip(s->m_doc, s->m_editorClipId) : nullptr;

            if (found == nullptr || !found->Generator.has_value())
            {
                return;
            }

            auto const canvas = sender.as<canvasXaml::CanvasControl>();
            auto const point = args.GetCurrentPoint(canvas);
            auto const at = point.Position();
            auto const delta = point.Properties().MouseWheelDelta();

            // One notch moves the band once, as on the range keyboard: a repeat of the last notch
            // is skipped, and so is one from outside the drawing.
            if (delta == 0 || point.Timestamp() == s->m_lastVelocityWheel ||
                at.X < 0 || at.Y < 0 || at.X > canvas.ActualWidth() || at.Y > canvas.ActualHeight())
            {
                return;
            }

            s->m_lastVelocityWheel = point.Timestamp();

            auto const& settings = *found->Generator;
            auto const low = static_cast<int32_t>(std::min(settings.VelocityLow, settings.VelocityHigh));
            auto const high = static_cast<int32_t>(std::max(settings.VelocityLow, settings.VelocityHigh));
            auto const move = delta > 0 ? std::min(WheelMove, 65535 - high) : -std::min(WheelMove, low);

            s->SetGeneratorVelocity(static_cast<uint16_t>(low + move), static_cast<uint16_t>(high + move));
        });

        section.Children().Append(drawing);

        // The same edges as boxes, in percent, for the keyboard and a screen reader. A box keeps its
        // focus as it changes the range, since only the drawing changes with it.
        auto const percentBox = [&](wchar_t const* nameKey, uint16_t value, bool lowest)
        {
            auto box = MakeGeneratorNumberBox(res::GetString(nameKey), 0, 100, Percent(value), false);
            box.Width(110);
            box.ValueChanged([weak, lowest, clipId = clip.Id](controls::NumberBox const&, controls::NumberBoxValueChangedEventArgs const& args)
            {
                auto s = weak.get();
                auto const newValue = args.NewValue();

                if (!s || s->m_settingVelocityBoxes || !std::isfinite(newValue) || newValue == args.OldValue())
                {
                    return;
                }

                auto const velocity = FromPercent(std::clamp(newValue, 0.0, 100.0));

                // After the box's own event, since the boxes are set again to match. A box that lets
                // go of its value as another clip opens changes nothing: the range was this clip's.
                s->DispatcherQueue().TryEnqueue([weak, lowest, velocity, clipId]()
                {
                    auto window = weak.get();
                    auto const found = window && window->m_editorClipId == clipId ? seq::FindClip(window->m_doc, clipId) : nullptr;

                    if (found != nullptr && found->Generator.has_value())
                    {
                        auto const low = std::min(found->Generator->VelocityLow, found->Generator->VelocityHigh);
                        auto const high = std::max(found->Generator->VelocityLow, found->Generator->VelocityHigh);
                        window->SetGeneratorVelocity(lowest ? velocity : low, lowest ? high : velocity);
                    }
                });
            });

            return box;
        };

        auto const secondary = BrushFor(m_palette.Text3);
        m_velocityLowBox = percentBox(L"GeneratorVelocityFrom", std::min(generator.VelocityLow, generator.VelocityHigh), true);
        m_velocityHighBox = percentBox(L"GeneratorVelocityTo", std::max(generator.VelocityLow, generator.VelocityHigh), false);

        controls::StackPanel ends{};
        ends.Orientation(controls::Orientation::Horizontal);
        ends.Spacing(8);
        ends.Children().Append(m_velocityLowBox);
        ends.Children().Append(Text(res::GetString(L"GeneratorRangeTo"), secondary));
        ends.Children().Append(m_velocityHighBox);
        section.Children().Append(ends);
    }

    _Use_decl_annotations_
    void MainWindow::SetGeneratorVelocity(uint16_t low, uint16_t high) noexcept
    {
        try
        {
            // Only this section shows the range, so the panel stays as it is. Built again under the
            // pointer while it drags or scrolls, the next press or notch there would be lost.
            EditGeneratorInPlace([low, high](seq::GeneratorSettings& edited)
            {
                edited.VelocityLow = low;
                edited.VelocityHigh = high;
            });

            // The boxes follow what was kept: a lowest typed above the highest swaps them.
            auto const found = seq::FindClip(m_doc, m_editorClipId);

            if (found != nullptr && found->Generator.has_value() && m_velocityLowBox != nullptr && m_velocityHighBox != nullptr)
            {
                m_settingVelocityBoxes = true;
                auto const done = wil::scope_exit([this]() noexcept { m_settingVelocityBoxes = false; });
                m_velocityLowBox.Value(Percent(std::min(found->Generator->VelocityLow, found->Generator->VelocityHigh)));
                m_velocityHighBox.Value(Percent(std::max(found->Generator->VelocityLow, found->Generator->VelocityHigh)));
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change the velocity range.")
    }
}
