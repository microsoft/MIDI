// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Fingers on a fretboard. Like a pad grid, a fretboard takes a finger per pointer, because a
// chord is several fingers on one control. A finger lands in one of three places, and keeps
// doing what it started doing until it lifts: fretting a string, sweeping the strum area, or
// dragging the neck strip.

#include "pch.h"
#include "InputRouter.h"
#include "GlassControl.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Input;

namespace glass
{
    namespace
    {
        // A pluck or a tap carries how hard it was hit where the hardware says and the customer
        // asked for it, the same as a pad.
        double FretVelocity(_In_ bool fromTouch, _In_ winrt::Microsoft::UI::Input::PointerPoint const& point)
        {
            if (!fromTouch)
            {
                return 1.0;
            }

            auto const pressure = point.Properties().Pressure();

            return pressure > 0.0f && pressure <= 1.0f ? pressure : 1.0;
        }

        // A pluck straight onto a string in the strum area, with no sweep to measure.
        constexpr double PluckVelocity = 0.75;
    }

    _Use_decl_annotations_
    void InputRouter::ShowFretFingers(Binding const& binding) noexcept
    {
        if (m_renderer == nullptr)
        {
            return;
        }

        std::array<SurfaceRenderer::FretFingerMark, MaximumPadFingers> marks{};
        size_t count{ 0 };

        for (auto const& finger : binding.FretFingers)
        {
            if (finger.PointerId == 0 || (finger.Region != FretRegion::Neck && finger.Region != FretRegion::Open))
            {
                continue;
            }

            marks[count++] = SurfaceRenderer::FretFingerMark{ finger.String, finger.Fret, finger.Across };
        }

        m_renderer->SetFretFingers(binding.ItemIndex, std::span<SurfaceRenderer::FretFingerMark const>{ marks.data(), count });
    }

    _Use_decl_annotations_
    void InputRouter::PressFret(Binding& binding, PointerRoutedEventArgs const& args)
    {
        if (binding.Element == nullptr)
        {
            return;
        }

        auto const pointerId = args.Pointer().PointerId();

        Binding::FretFinger* slot{ nullptr };

        for (auto& finger : binding.FretFingers)
        {
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

        auto const point = args.GetCurrentPoint(binding.Element);
        auto const x = point.Position().X;
        auto const y = point.Position().Y;
        auto const hit = FretAtPoint(binding.FretLayout, x, y);

        if (hit.Region == FretRegion::None)
        {
            return;
        }

        if (!binding.Element.CapturePointer(args.Pointer()))
        {
            return;
        }

        m_heldCount++;
        args.Handled(true);

        auto const first = std::none_of(binding.FretFingers.begin(), binding.FretFingers.end(),
            [](Binding::FretFinger const& finger) { return finger.PointerId != 0; });

        *slot = Binding::FretFinger{};
        slot->PointerId = pointerId;
        slot->Region = hit.Region;
        slot->LastY = y;
        slot->LastTime = point.Timestamp();

        if (first)
        {
            binding.Element.Focus(xaml::FocusState::Pointer);

            if (TouchChanged)
            {
                TouchChanged(binding.ItemIndex, true);
            }
        }

        switch (hit.Region)
        {
        case FretRegion::Open:
        case FretRegion::Neck:
        {
            slot->String = hit.String;
            slot->Fret = hit.Fret;
            slot->StartY = y;

            ShowFretFingers(binding);

            if (FretTouched)
            {
                FretTouch touch{};

                touch.Phase = FretTouchPhase::Down;
                touch.Touch = pointerId;
                touch.String = hit.String;
                touch.Fret = hit.Fret;
                touch.FretPosition = hit.FretPosition;
                touch.Velocity = FretVelocity(binding.VelocityFromTouch, point);

                FretTouched(binding.ItemIndex, touch);
            }

            break;
        }

        case FretRegion::Strum:
        {
            // Landing right on a string plucks it. Anywhere between two strings waits for the
            // sweep to cross one.
            auto const& layout = binding.FretLayout;

            if (hit.String >= 0 && static_cast<size_t>(hit.String) < layout.StringY.size() &&
                std::abs(y - layout.StringY[static_cast<size_t>(hit.String)]) <= layout.StringPitch * PluckReach &&
                FretTouched)
            {
                FretTouch touch{};

                touch.Phase = FretTouchPhase::Strum;
                touch.Touch = pointerId;
                touch.String = hit.String;
                touch.Velocity = binding.VelocityFromTouch ? FretVelocity(true, point) : PluckVelocity;

                FretTouched(binding.ItemIndex, touch);
            }

            break;
        }

        case FretRegion::Strip:
        {
            auto const firstFret = NeckWindowAt(binding.FretSpec, binding.FretLayout, x);

            slot->NeckFirst = firstFret;

            if (NeckMoved)
            {
                NeckMoved(binding.ItemIndex, firstFret);
            }

            break;
        }

        default:
            break;
        }
    }

    _Use_decl_annotations_
    void InputRouter::MoveFret(Binding& binding, PointerRoutedEventArgs const& args)
    {
        if (binding.Element == nullptr)
        {
            return;
        }

        auto const pointerId = args.Pointer().PointerId();

        Binding::FretFinger* held{ nullptr };

        for (auto& finger : binding.FretFingers)
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

        args.Handled(true);

        auto const point = args.GetCurrentPoint(binding.Element);
        auto const x = point.Position().X;
        auto const y = point.Position().Y;
        auto const& layout = binding.FretLayout;

        switch (held->Region)
        {
        case FretRegion::Open:
        case FretRegion::Neck:
        {
            // The finger keeps its string, the way a finger bending a real one carries it. Along
            // the neck it can slide anywhere, open column included.
            auto const fret = FretAtX(layout, x);
            auto const position = FretPositionAtX(layout, x);
            auto const across = y - held->StartY;
            auto const changed = fret != held->Fret || std::abs(across - held->Across) >= 0.5;

            held->Fret = fret;
            held->Across = across;

            if (changed)
            {
                ShowFretFingers(binding);
            }

            if (FretTouched)
            {
                FretTouch touch{};

                touch.Phase = FretTouchPhase::Move;
                touch.Touch = pointerId;
                touch.String = held->String;
                touch.Fret = fret;
                touch.FretPosition = position;
                touch.Bend = CrossBend(binding.FretSpec.BendRangeSemitones, across, layout.StringPitch);
                touch.Velocity = FretVelocity(binding.VelocityFromTouch, point);

                FretTouched(binding.ItemIndex, touch);
            }

            break;
        }

        case FretRegion::Strum:
        {
            std::array<int32_t, MaximumFretStrings> crossed{};

            auto const count = StringsCrossed(layout, held->LastY, y, crossed);
            auto const now = point.Timestamp();

            if (count > 0 && FretTouched)
            {
                auto const elapsed = now > held->LastTime ? static_cast<double>(now - held->LastTime) / 1000.0 : 0.0;
                auto const velocity = binding.VelocityFromTouch
                    ? FretVelocity(true, point)
                    : StrumVelocity(y - held->LastY, elapsed);

                for (uint32_t index = 0; index < count; ++index)
                {
                    FretTouch touch{};

                    touch.Phase = FretTouchPhase::Strum;
                    touch.Touch = pointerId;
                    touch.String = crossed[index];
                    touch.Velocity = velocity;

                    FretTouched(binding.ItemIndex, touch);
                }
            }

            held->LastY = y;
            held->LastTime = now;
            break;
        }

        case FretRegion::Strip:
        {
            auto const firstFret = NeckWindowAt(binding.FretSpec, layout, x);

            if (firstFret != held->NeckFirst)
            {
                held->NeckFirst = firstFret;

                if (NeckMoved)
                {
                    NeckMoved(binding.ItemIndex, firstFret);
                }
            }

            break;
        }

        default:
            break;
        }
    }

    _Use_decl_annotations_
    void InputRouter::ReleaseFret(Binding& binding, uint32_t pointerId)
    {
        if (pointerId == 0)
        {
            return;
        }

        for (auto& finger : binding.FretFingers)
        {
            if (finger.PointerId != pointerId)
            {
                continue;
            }

            auto const fretting = finger.Region == FretRegion::Neck || finger.Region == FretRegion::Open;

            finger = Binding::FretFinger{};

            m_heldCount = std::max(0, m_heldCount - 1);

            if (fretting)
            {
                ShowFretFingers(binding);

                if (FretTouched)
                {
                    FretTouch touch{};

                    touch.Phase = FretTouchPhase::Up;
                    touch.Touch = pointerId;
                    touch.Velocity = 0.0;

                    FretTouched(binding.ItemIndex, touch);
                }
            }

            auto const anyLeft = std::any_of(binding.FretFingers.begin(), binding.FretFingers.end(),
                [](Binding::FretFinger const& other) { return other.PointerId != 0; });

            if (!anyLeft && TouchChanged)
            {
                TouchChanged(binding.ItemIndex, false);
            }

            return;
        }
    }
}
