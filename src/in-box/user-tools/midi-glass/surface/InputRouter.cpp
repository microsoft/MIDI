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

            if (!IsInteractive(kind))
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

            auto const latches = kind != ControlKind::Lfo || renderer.LatchesAt(i);

            binding.Momentary = IsMomentary(kind) || (kind == ControlKind::Lfo && !latches);
            binding.Toggling = IsToggling(kind) || (kind == ControlKind::Lfo && latches);
            binding.TurnDegreesForFullRange = renderer.TurnDegreesAt(i);

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
                [this, index](foundation::IInspectable const&, PointerRoutedEventArgs const&)
                {
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
            if (binding.PointerId == 0)
            {
                continue;
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
        if (index >= m_bindings.size() || m_viewMode)
        {
            return;
        }

        auto& binding = m_bindings[index];

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

        if (binding.PointerId != args.Pointer().PointerId() || binding.Element == nullptr)
        {
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

        // A knob has no travel under the finger, so it is nudged rather than set. Up is more,
        // which is what every plug-in does, and a circle is hard to trace on glass. A row of
        // knobs in a narrow strip can be set to drag sideways instead.
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
}
