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

            if (IsMomentary(binding.Kind) && Switched)
            {
                // A finger lifted off the window rather than off the pad still has to end the
                // note, or a layout that loses focus mid press leaves one sounding.
                Switched(binding.ItemIndex, false);
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

        if (IsMomentary(binding.Kind))
        {
            binding.Value = 1.0;

            if (binding.Element != nullptr)
            {
                winrt::get_self<winrt::midiglass::implementation::GlassControl>(binding.Element)
                    ->SetValueDirect(1.0);
            }

            if (Switched)
            {
                Switched(binding.ItemIndex, true);
            }

            return;
        }

        if (IsToggling(binding.Kind))
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
                Switched(binding.ItemIndex, isOn);
            }

            return;
        }

        binding.StartValue = binding.Value;
        binding.StartY = point.Position().Y;

        if (UsesAbsolutePosition(binding.Kind))
        {
            Publish(binding, PositionToValue(
                binding.Kind,
                binding.Element.ActualWidth(),
                binding.Element.ActualHeight(),
                point.Position().X,
                point.Position().Y), false);
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

        if (IsMomentary(binding.Kind) || IsToggling(binding.Kind))
        {
            return;
        }

        auto const point = args.GetCurrentPoint(binding.Element);

        args.Handled(true);

        if (UsesAbsolutePosition(binding.Kind))
        {
            Publish(binding, PositionToValue(
                binding.Kind,
                binding.Element.ActualWidth(),
                binding.Element.ActualHeight(),
                point.Position().X,
                point.Position().Y), false);

            return;
        }

        // A knob has no travel under the finger, so it is nudged rather than set. Up is more,
        // which is what every plug-in does.
        auto const delta = (binding.StartY - point.Position().Y) / KnobDragPixels;

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

        if (IsMomentary(binding.Kind))
        {
            binding.Value = 0.0;

            if (binding.Element != nullptr)
            {
                winrt::get_self<winrt::midiglass::implementation::GlassControl>(binding.Element)
                    ->SetValueDirect(0.0);
            }

            if (Switched)
            {
                Switched(binding.ItemIndex, false);
            }

            return;
        }

        if (IsToggling(binding.Kind))
        {
            return;
        }

        // The last value is always sent. Without this a throttled fader settles a few units from
        // where the finger left it, and the surface and the desk disagree for the rest of the
        // session.
        Publish(binding, binding.Value, true);
    }
}
