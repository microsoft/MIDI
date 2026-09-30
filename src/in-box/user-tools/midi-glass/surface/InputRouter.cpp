// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "InputRouter.h"
#include "GlassControl.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Input;

namespace glass
{
    _Use_decl_annotations_
    void InputRouter::Attach(SurfaceRenderer& renderer)
    {
        Detach();

        m_renderer = &renderer;
        m_bindings.reserve(renderer.ItemCount());

        for (size_t i = 0; i < renderer.ItemCount(); ++i)
        {
            auto const kind = renderer.KindAt(i);

            // A picture sends nothing, but a video on one can be asked to take clicks or to
            // draw a bar, and then it needs a pointer like anything else.
            auto const takesPictureInput = kind == ControlKind::Image &&
                (renderer.VideoTakesClicks(i) || renderer.VideoShowsScrubber(i));

            if (!IsInteractive(kind) && !takesPictureInput)
            {
                continue;
            }

            auto element = renderer.ElementAt(i);

            if (element == nullptr)
            {
                continue;
            }

            Binding binding{};

            binding.Element = element;
            binding.ItemIndex = i;
            binding.Kind = kind;
            binding.Value = element.SurfaceValue();
            binding.ValueY = renderer.ValueYAt(i);
            binding.ReturnsToRest = renderer.ReturnsToRestAt(i);
            binding.RestValue = renderer.RestValueAt(i);
            binding.RestValueY = renderer.RestValueYAt(i);
            binding.Drag = renderer.DragAxisAt(i);
            binding.Keyboard = renderer.KeyboardAt(i);
            binding.VelocityFromTouch = renderer.VelocityFromTouchAt(i);

            if (IsPadGrid(kind))
            {
                binding.PlaysPads = true;
                binding.PadGrid = renderer.PadGridAt(i);
                binding.PadLayout = renderer.PadLayoutAt(i);
            }

            // A generator runs while held or latches on a press, and which one is the control's
            // own setting rather than its kind's.
            auto const generator = kind == ControlKind::Lfo || kind == ControlKind::Steps;
            auto const latches = !generator || renderer.LatchesAt(i);

            binding.Momentary = IsMomentary(kind) || (generator && !latches);
            binding.Toggling = IsToggling(kind) || (generator && latches);
            binding.TurnDegreesForFullRange = renderer.TurnDegreesAt(i);
            binding.SwitchPositions = renderer.SwitchPositionsAt(i);

            m_bindings.push_back(std::move(binding));
        }

        for (size_t index = 0; index < m_bindings.size(); ++index)
        {
            auto& binding = m_bindings[index];

            binding.PressedHandler = PointerEventHandler(
                [this, index](foundation::IInspectable const&, PointerRoutedEventArgs const& args)
                {
                    OnPressed(index, args);
                });

            binding.MovedHandler = PointerEventHandler(
                [this, index](foundation::IInspectable const&, PointerRoutedEventArgs const& args)
                {
                    OnMoved(index, args);
                });

            binding.ReleasedHandler = PointerEventHandler(
                [this, index](foundation::IInspectable const&, PointerRoutedEventArgs const& args)
                {
                    OnReleased(index, args);
                });

            binding.CaptureLostHandler = PointerEventHandler(
                [this, index](foundation::IInspectable const&, PointerRoutedEventArgs const& args)
                {
                    // A pad grid holds a finger per pad, so losing one pointer ends that finger
                    // and leaves the rest of the chord alone.
                    if (index < m_bindings.size() && m_bindings[index].PlaysPads)
                    {
                        ReleasePad(m_bindings[index], args.Pointer().PointerId());
                        return;
                    }

                    OnCaptureLost(index);
                });

            binding.PressedToken = binding.Element.PointerPressed(binding.PressedHandler);
            binding.MovedToken = binding.Element.PointerMoved(binding.MovedHandler);
            binding.ReleasedToken = binding.Element.PointerReleased(binding.ReleasedHandler);
            binding.CaptureLostToken = binding.Element.PointerCaptureLost(binding.CaptureLostHandler);
        }
    }

    void InputRouter::Detach()
    {
        for (auto& binding : m_bindings)
        {
            if (binding.Element == nullptr)
            {
                continue;
            }

            binding.Element.PointerPressed(binding.PressedToken);
            binding.Element.PointerMoved(binding.MovedToken);
            binding.Element.PointerReleased(binding.ReleasedToken);
            binding.Element.PointerCaptureLost(binding.CaptureLostToken);
        }

        m_bindings.clear();
        m_heldCount = 0;
        m_renderer = nullptr;
    }

    void InputRouter::ReleaseAll()
    {
        for (auto& binding : m_bindings)
        {
            // Every finger on a pad grid ends its own note. A grid never sets the single
            // pointer below, so it is handled before that is looked at.
            if (binding.PlaysPads)
            {
                for (auto const& finger : binding.PadFingers)
                {
                    if (finger.PointerId != 0)
                    {
                        ReleasePad(binding, finger.PointerId);
                    }
                }

                continue;
            }

            if (binding.PointerId == 0)
            {
                continue;
            }

            if (binding.Scrubbing)
            {
                binding.PointerId = 0;
                binding.Scrubbing = false;

                if (m_renderer != nullptr)
                {
                    m_renderer->EndScrub(binding.ItemIndex);
                }

                continue;
            }

            // A key held on a keyboard ends its note the same way a pad does, or a layout that
            // loses focus mid press leaves it sounding.
            if (PlaysKeys(binding.Kind))
            {
                TouchKeyboard(binding, 0.0, 0.0, false);
            }

            binding.PointerId = 0;

            if (TouchChanged)
            {
                TouchChanged(binding.ItemIndex, false);
            }

            if (binding.Momentary && Switched)
            {
                // A finger lifted off the window rather than off the pad still has to end the
                // note, or a layout that loses focus mid press leaves one sounding.
                Switched(binding.ItemIndex, false, 0.0);
            }
        }

        m_heldCount = 0;
    }

    _Use_decl_annotations_
    void InputRouter::Publish(Binding& binding, double value, bool isFinal)
    {
        auto snapped = std::clamp(value, 0.0, 1.0);

        if (Snap)
        {
            snapped = Snap(binding.ItemIndex, snapped);
        }

        binding.Value = snapped;

        if (binding.Element != nullptr)
        {
            winrt::get_self<winrt::midiglass::implementation::GlassControl>(binding.Element)
                ->SetValueDirect(snapped);
        }

        if (ValueChanged)
        {
            ValueChanged(binding.ItemIndex, snapped, isFinal);
        }
    }

    _Use_decl_annotations_
    void InputRouter::PublishY(Binding& binding, double value, bool isFinal)
    {
        auto const clamped = std::clamp(value, 0.0, 1.0);

        binding.ValueY = clamped;

        if (m_renderer != nullptr)
        {
            m_renderer->SetValueY(binding.ItemIndex, clamped);
        }

        if (ValueYChanged)
        {
            ValueYChanged(binding.ItemIndex, clamped, isFinal);
        }
    }

    _Use_decl_annotations_
    void InputRouter::TouchKeyboard(Binding& binding, double x, double y, bool down)
    {
        if (binding.Element == nullptr)
        {
            return;
        }

        auto const key = down
            ? KeyAtPosition(
                binding.Keyboard,
                binding.Element.ActualWidth(),
                binding.Element.ActualHeight(),
                x,
                y)
            : -1;

        if (key == binding.PressedKey)
        {
            return;
        }

        // Off before on, so sliding along the keyboard never leaves a note sounding behind the
        // finger.
        if (binding.PressedKey >= 0 && KeyChanged)
        {
            KeyChanged(binding.ItemIndex, binding.PressedKey, 0.0, false);
        }

        binding.PressedKey = key;

        if (m_renderer != nullptr)
        {
            m_renderer->SetPressedKey(binding.ItemIndex, key);
        }

        if (key >= 0 && KeyChanged)
        {
            auto const velocity = binding.Keyboard.VelocityFromKeyPosition
                ? KeyVelocityFromPosition(binding.Keyboard, key, binding.Element.ActualHeight(), y)
                : 1.0;

            KeyChanged(binding.ItemIndex, key, velocity, true);
        }
    }

    _Use_decl_annotations_
    void InputRouter::OnPressed(size_t index, PointerRoutedEventArgs const& args)
    {
        if (index >= m_bindings.size())
        {
            return;
        }

        auto& binding = m_bindings[index];

        // A pad grid takes a finger per pad, so the one-finger rule below does not apply to it.
        if (binding.PlaysPads)
        {
            PressPad(binding, args);
            return;
        }

        if (binding.Kind == ControlKind::Image)
        {
            PressPicture(binding, args);
            return;
        }

        // One finger per control. A second on the same control is ignored rather than fought
        // over, which would make the value jump between two positions.
        if (binding.PointerId != 0 || binding.Element == nullptr)
        {
            return;
        }

        // Called once and passed down. Measured at about fifteen microseconds, which is forty
        // five times what the MIDI send costs: this is the expensive call on the hot path.
        auto const point = args.GetCurrentPoint(binding.Element);

        binding.PointerId = args.Pointer().PointerId();

        if (binding.Element.CapturePointer(args.Pointer()))
        {
            m_heldCount++;
        }
        else
        {
            binding.PointerId = 0;
            return;
        }

        args.Handled(true);

        binding.Element.Focus(xaml::FocusState::Pointer);

        if (TouchChanged)
        {
            TouchChanged(binding.ItemIndex, true);
        }

        if (binding.Momentary)
        {
            binding.Value = 1.0;

            if (binding.Element != nullptr)
            {
                winrt::get_self<winrt::midiglass::implementation::GlassControl>(binding.Element)
                    ->SetValueDirect(1.0);
            }

            if (Switched)
            {
                // How hard it was hit, where the hardware says. A mouse reports one number
                // every time and most touch screens report nothing at all, which is why this
                // is off unless the customer asked for it: otherwise every pad on the page
                // would quietly send half velocity.
                auto velocity = 1.0;

                if (binding.VelocityFromTouch)
                {
                    auto const pressure = point.Properties().Pressure();

                    if (pressure > 0.0f && pressure <= 1.0f)
                    {
                        velocity = pressure;
                    }
                }

                Switched(binding.ItemIndex, true, velocity);
            }

            return;
        }

        if (binding.Toggling)
        {
            auto const isOn = binding.Value < 0.5;

            binding.Value = isOn ? 1.0 : 0.0;

            if (binding.Element != nullptr)
            {
                winrt::get_self<winrt::midiglass::implementation::GlassControl>(binding.Element)
                    ->SetValueDirect(binding.Value);
            }

            if (Switched)
            {
                Switched(binding.ItemIndex, isOn, 1.0);
            }

            return;
        }

        if (PlaysKeys(binding.Kind))
        {
            TouchKeyboard(binding, point.Position().X, point.Position().Y, true);
            return;
        }

        binding.StartValue = binding.Value;
        binding.StartY = point.Position().Y;
        binding.StartX = point.Position().X;

        // A knob turned round and round is measured from where the finger landed, so it turns
        // from where it is rather than jumping to the finger.
        if (binding.Drag == DragAxis::Circular)
        {
            binding.TurnedDegrees = 0.0;
            binding.TurnAnchored = IsFarEnoughToTurn(
                binding.Element.ActualWidth(),
                binding.Element.ActualHeight(),
                point.Position().X,
                point.Position().Y);

            if (binding.TurnAnchored)
            {
                binding.StartAngle = AngleAtPosition(
                    binding.Element.ActualWidth(),
                    binding.Element.ActualHeight(),
                    point.Position().X,
                    point.Position().Y);
            }
        }

        if (IsTurnedByHand(binding.Kind))
        {
            // Where the hand landed on the platter. Everything after this is measured from
            // here, so a platter can be picked up anywhere on its face.
            binding.StartAngle = AngleAtPosition(
                binding.Element.ActualWidth(),
                binding.Element.ActualHeight(),
                point.Position().X,
                point.Position().Y);

            binding.TurnedDegrees = 0.0;

            return;
        }

        // A switch goes to the position under the finger, and follows it across.
        if (binding.Kind == ControlKind::Switch)
        {
            Publish(binding, SwitchValueAtPoint(
                binding.Element.ActualWidth(),
                binding.Element.ActualHeight(),
                point.Position().X,
                point.Position().Y,
                binding.SwitchPositions), false);

            return;
        }

        if (UsesAbsolutePosition(binding.Kind))
        {
            Publish(binding, PositionToValue(
                binding.Kind,
                binding.Element.ActualWidth(),
                binding.Element.ActualHeight(),
                point.Position().X,
                point.Position().Y), false);

            if (UsesTwoAxes(binding.Kind))
            {
                PublishY(binding, PositionToValueY(
                    binding.Element.ActualHeight(), point.Position().Y), false);
            }
        }
    }

    _Use_decl_annotations_
    void InputRouter::OnMoved(size_t index, PointerRoutedEventArgs const& args)
    {
        if (index >= m_bindings.size())
        {
            return;
        }

        auto& binding = m_bindings[index];

        if (binding.PlaysPads)
        {
            MovePad(binding, args);
            return;
        }

        if (binding.PointerId != args.Pointer().PointerId() || binding.Element == nullptr)
        {
            return;
        }

        if (binding.Scrubbing)
        {
            auto const position = args.GetCurrentPoint(binding.Element).Position();
            double fraction{ 0.0 };

            args.Handled(true);

            if (m_renderer != nullptr &&
                m_renderer->TryGetScrubFraction(binding.ItemIndex, position.X, position.Y, true, fraction))
            {
                m_renderer->ScrubVideo(binding.ItemIndex, fraction);
            }

            return;
        }

        if (binding.Momentary || binding.Toggling)
        {
            return;
        }

        auto const point = args.GetCurrentPoint(binding.Element);

        args.Handled(true);

        if (IsTurnedByHand(binding.Kind))
        {
            auto const angle = AngleAtPosition(
                binding.Element.ActualWidth(),
                binding.Element.ActualHeight(),
                point.Position().X,
                point.Position().Y);

            auto const span = binding.TurnDegreesForFullRange > 0.0
                ? binding.TurnDegreesForFullRange
                : 180.0;

            // Accumulated the short way round, so pushing the platter past the top keeps going
            // instead of snapping back to the other side. Held at the ends rather than allowed
            // to run past them: without that, a platter spun three times has to be unwound three
            // times before pushing it the other way does anything.
            binding.TurnedDegrees = std::clamp(
                binding.TurnedDegrees + AngleDelta(binding.StartAngle, angle),
                -span * 0.5,
                span * 0.5);

            binding.StartAngle = angle;

            // The middle means "not moving", which is what makes a pitch bend row a nudge and a
            // 0 to 127 controller row sit at 64 the way DJ software expects a jog wheel to.
            Publish(binding, 0.5 + binding.TurnedDegrees / span, false);

            return;
        }

        if (PlaysKeys(binding.Kind))
        {
            TouchKeyboard(binding, point.Position().X, point.Position().Y, true);
            return;
        }

        if (binding.Kind == ControlKind::Switch)
        {
            Publish(binding, SwitchValueAtPoint(
                binding.Element.ActualWidth(),
                binding.Element.ActualHeight(),
                point.Position().X,
                point.Position().Y,
                binding.SwitchPositions), false);

            return;
        }

        if (UsesAbsolutePosition(binding.Kind))
        {
            Publish(binding, PositionToValue(
                binding.Kind,
                binding.Element.ActualWidth(),
                binding.Element.ActualHeight(),
                point.Position().X,
                point.Position().Y), false);

            if (UsesTwoAxes(binding.Kind))
            {
                PublishY(binding, PositionToValueY(
                    binding.Element.ActualHeight(), point.Position().Y), false);
            }

            return;
        }

        // A wheel turns under the thumb: it moves by how far the finger travels, never jumps to
        // where it lands, and its own length is the whole range, like the wheel on a keyboard.
        if (binding.Kind == ControlKind::Wheel)
        {
            auto const width = binding.Element.ActualWidth();
            auto const height = binding.Element.ActualHeight();

            auto const moved = height >= width
                ? (binding.StartY - point.Position().Y) / std::max(height, 40.0)
                : (point.Position().X - binding.StartX) / std::max(width, 40.0);

            Publish(binding, binding.StartValue + moved, false);
            return;
        }

        // Round and round: the knob follows how far the finger goes round its middle, clockwise
        // for more.
        if (binding.Drag == DragAxis::Circular)
        {
            auto const width = binding.Element.ActualWidth();
            auto const height = binding.Element.ActualHeight();
            auto const x = point.Position().X;
            auto const y = point.Position().Y;

            if (!IsFarEnoughToTurn(width, height, x, y))
            {
                binding.TurnAnchored = false;
                return;
            }

            auto const angle = AngleAtPosition(width, height, x, y);

            // Back out of the middle, the turn carries on from here rather than from wherever
            // the finger went in.
            if (!binding.TurnAnchored)
            {
                binding.StartAngle = angle;
                binding.TurnAnchored = true;
                return;
            }

            binding.TurnedDegrees = ClampKnobTurn(
                binding.StartValue,
                binding.TurnedDegrees + AngleDelta(binding.StartAngle, angle));

            binding.StartAngle = angle;

            Publish(binding, binding.StartValue + binding.TurnedDegrees / KnobTurnDegrees, false);
            return;
        }

        // A knob has no travel under the finger, so it is nudged rather than set. Up is more,
        // which is what every plug-in does, and a circle is hard to trace on a small knob. A row
        // of knobs in a narrow strip can be set to drag sideways instead.
        auto const delta = binding.Drag == DragAxis::Horizontal
            ? (point.Position().X - binding.StartX) / KnobDragPixels
            : (binding.StartY - point.Position().Y) / KnobDragPixels;

        Publish(binding, binding.StartValue + delta, false);
    }

    _Use_decl_annotations_
    void InputRouter::OnReleased(size_t index, PointerRoutedEventArgs const& args)
    {
        if (index >= m_bindings.size())
        {
            return;
        }

        auto& binding = m_bindings[index];

        if (binding.PlaysPads)
        {
            auto const pointerId = args.Pointer().PointerId();

            if (binding.Element != nullptr)
            {
                binding.Element.ReleasePointerCapture(args.Pointer());
            }

            args.Handled(true);

            // Releasing the capture may already have ended this finger through the capture
            // lost handler. Ending it twice is harmless: the second finds nothing.
            ReleasePad(binding, pointerId);
            return;
        }

        if (binding.PointerId != args.Pointer().PointerId())
        {
            return;
        }

        if (binding.Element != nullptr)
        {
            binding.Element.ReleasePointerCapture(args.Pointer());
        }

        args.Handled(true);

        OnCaptureLost(index);
    }

    _Use_decl_annotations_
    void InputRouter::OnCaptureLost(size_t index)
    {
        if (index >= m_bindings.size())
        {
            return;
        }

        auto& binding = m_bindings[index];

        if (binding.PointerId == 0)
        {
            return;
        }

        binding.PointerId = 0;
        m_heldCount = std::max(0, m_heldCount - 1);

        // A finger coming off a video's bar lets the video carry on. Nothing was sent, so there
        // is nothing to put back.
        if (binding.Scrubbing)
        {
            binding.Scrubbing = false;

            if (m_renderer != nullptr)
            {
                m_renderer->EndScrub(binding.ItemIndex);
            }

            return;
        }

        if (TouchChanged)
        {
            TouchChanged(binding.ItemIndex, false);
        }

        if (binding.Momentary)
        {
            binding.Value = 0.0;

            if (binding.Element != nullptr)
            {
                winrt::get_self<winrt::midiglass::implementation::GlassControl>(binding.Element)
                    ->SetValueDirect(0.0);
            }

            if (Switched)
            {
                Switched(binding.ItemIndex, false, 0.0);
            }

            return;
        }

        if (binding.Toggling)
        {
            return;
        }

        if (PlaysKeys(binding.Kind))
        {
            TouchKeyboard(binding, 0.0, 0.0, false);
            return;
        }

        // A pitch wheel springs back the moment the finger leaves it. It is published as the end
        // of the same gesture rather than as a new one, so the throttle's trailing send carries
        // the rest value and the desk cannot be left holding a bend.
        if (binding.ReturnsToRest)
        {
            binding.Value = binding.RestValue;
            binding.ValueY = binding.RestValueY;
        }

        if (UsesTwoAxes(binding.Kind))
        {
            PublishY(binding, binding.ValueY, true);
        }

        // The last value is always sent. Without this a throttled fader settles a few units from
        // where the finger left it, and the surface and the desk disagree for the rest of the
        // session.
        Publish(binding, binding.Value, true);
    }

    namespace
    {
        // How hard a pad was hit, where the hardware says and the customer asked for it. A
        // mouse reports one number every time and most touch screens report nothing at all.
        double PadVelocity(_In_ bool fromTouch, _In_ winrt::Microsoft::UI::Input::PointerPoint const& point)
        {
            if (!fromTouch)
            {
                return 1.0;
            }

            auto const pressure = point.Properties().Pressure();

            return pressure > 0.0f && pressure <= 1.0f ? pressure : 1.0;
        }
    }

    _Use_decl_annotations_
    void InputRouter::PressPicture(Binding& binding, PointerRoutedEventArgs const& args)
    {
        if (m_renderer == nullptr || binding.Element == nullptr || binding.PointerId != 0 ||
            !m_renderer->HasVideo(binding.ItemIndex))
        {
            return;
        }

        auto const position = args.GetCurrentPoint(binding.Element).Position();
        double fraction{ 0.0 };

        // On the bar: held, so the drag can carry on past the ends of the bar.
        if (m_renderer->TryGetScrubFraction(binding.ItemIndex, position.X, position.Y, false, fraction))
        {
            if (!binding.Element.CapturePointer(args.Pointer()))
            {
                return;
            }

            binding.PointerId = args.Pointer().PointerId();
            binding.Scrubbing = true;
            m_heldCount++;

            args.Handled(true);

            m_renderer->ScrubVideo(binding.ItemIndex, fraction);
            return;
        }

        if (m_renderer->VideoTakesClicks(binding.ItemIndex))
        {
            args.Handled(true);

            m_renderer->ToggleVideo(binding.ItemIndex);
        }
    }

    _Use_decl_annotations_
    void InputRouter::PressPad(Binding& binding, PointerRoutedEventArgs const& args)
    {
        if (binding.Element == nullptr)
        {
            return;
        }

        auto const pointerId = args.Pointer().PointerId();

        Binding::PadFinger* slot{ nullptr };

        for (auto& finger : binding.PadFingers)
        {
            // Already down on this grid. It must not become a second note for one finger.
            if (finger.PointerId == pointerId)
            {
                return;
            }

            if (slot == nullptr && finger.PointerId == 0)
            {
                slot = &finger;
            }
        }

        if (slot == nullptr)
        {
            return;
        }

        // Called once and passed down, for the same reason as everywhere else here.
        auto const point = args.GetCurrentPoint(binding.Element);
        auto const x = point.Position().X;
        auto const y = point.Position().Y;

        auto const cell = PadAtPoint(binding.PadLayout, x, y);

        // Past the end of a short row, or on a pad off the end of the note range.
        if (cell < 0 || binding.PadLayout.Cells[static_cast<size_t>(cell)].Note < 0)
        {
            return;
        }

        if (!binding.Element.CapturePointer(args.Pointer()))
        {
            return;
        }

        m_heldCount++;
        args.Handled(true);

        auto const first = binding.PadFingerCount == 0;

        slot->PointerId = pointerId;
        slot->Cell = cell;
        binding.PadFingerCount++;

        if (first)
        {
            binding.Element.Focus(xaml::FocusState::Pointer);

            if (TouchChanged)
            {
                TouchChanged(binding.ItemIndex, true);
            }
        }

        if (m_renderer != nullptr)
        {
            m_renderer->SetPadHeld(binding.ItemIndex, cell, true);
        }

        if (PadTouched)
        {
            PadTouch touch{};

            touch.Phase = PadTouchPhase::Down;
            touch.Touch = pointerId;
            touch.Note = binding.PadLayout.Cells[static_cast<size_t>(cell)].Note;
            touch.Velocity = PadVelocity(binding.VelocityFromTouch, point);
            touch.Pitch = PitchAtPoint(binding.PadLayout, binding.PadGrid, cell, x);

            PadTouched(binding.ItemIndex, touch);
        }
    }

    _Use_decl_annotations_
    void InputRouter::MovePad(Binding& binding, PointerRoutedEventArgs const& args)
    {
        if (binding.Element == nullptr)
        {
            return;
        }

        auto const pointerId = args.Pointer().PointerId();

        Binding::PadFinger* held{ nullptr };

        for (auto& finger : binding.PadFingers)
        {
            if (finger.PointerId == pointerId)
            {
                held = &finger;
                break;
            }
        }

        if (held == nullptr)
        {
            return;
        }

        auto const point = args.GetCurrentPoint(binding.Element);
        auto const x = point.Position().X;
        auto const y = point.Position().Y;

        args.Handled(true);

        auto cell = PadAtPoint(binding.PadLayout, x, y);

        // Off the grid, or onto a pad that plays nothing: the finger keeps the pad it was on,
        // rather than a slide ending the note the moment it strays past an edge.
        if (cell < 0 || binding.PadLayout.Cells[static_cast<size_t>(cell)].Note < 0)
        {
            cell = held->Cell;
        }

        auto const changed = cell != held->Cell;

        if (changed)
        {
            if (m_renderer != nullptr)
            {
                m_renderer->SetPadHeld(binding.ItemIndex, held->Cell, false);
                m_renderer->SetPadHeld(binding.ItemIndex, cell, true);
            }

            held->Cell = cell;
        }

        // Only a note that bends cares how far across its pad a finger has gone. For anything
        // else, a finger moving about on one pad is not news.
        if (!changed && binding.PadGrid.Glide != PadGlide::PerNoteBend)
        {
            return;
        }

        if (PadTouched)
        {
            PadTouch touch{};

            touch.Phase = PadTouchPhase::Move;
            touch.Touch = pointerId;
            touch.Note = binding.PadLayout.Cells[static_cast<size_t>(cell)].Note;
            touch.Velocity = PadVelocity(binding.VelocityFromTouch, point);
            touch.Pitch = PitchAtPoint(binding.PadLayout, binding.PadGrid, cell, x);

            PadTouched(binding.ItemIndex, touch);
        }
    }

    _Use_decl_annotations_
    void InputRouter::ReleasePad(Binding& binding, uint32_t pointerId)
    {
        if (pointerId == 0)
        {
            return;
        }

        for (auto& finger : binding.PadFingers)
        {
            if (finger.PointerId != pointerId)
            {
                continue;
            }

            auto const cell = finger.Cell;

            finger.PointerId = 0;
            finger.Cell = -1;

            if (binding.PadFingerCount > 0)
            {
                binding.PadFingerCount--;
            }

            m_heldCount = std::max(0, m_heldCount - 1);

            if (m_renderer != nullptr)
            {
                m_renderer->SetPadHeld(binding.ItemIndex, cell, false);
            }

            if (PadTouched)
            {
                PadTouch touch{};

                touch.Phase = PadTouchPhase::Up;
                touch.Touch = pointerId;
                touch.Velocity = 0.0;

                if (cell >= 0 && static_cast<size_t>(cell) < binding.PadLayout.Cells.size())
                {
                    touch.Note = binding.PadLayout.Cells[static_cast<size_t>(cell)].Note;
                }

                PadTouched(binding.ItemIndex, touch);
            }

            if (binding.PadFingerCount == 0 && TouchChanged)
            {
                TouchChanged(binding.ItemIndex, false);
            }

            return;
        }
    }
}
