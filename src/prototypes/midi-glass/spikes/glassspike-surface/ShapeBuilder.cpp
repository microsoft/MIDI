// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#include "pch.h"
#include "ShapeBuilder.h"

using namespace winrt;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Microsoft::UI::Composition;

namespace gspike
{
    namespace
    {
        Windows::UI::Color WithAlpha(Windows::UI::Color color, uint8_t alpha) noexcept
        {
            color.A = alpha;
            return color;
        }
    }

    Windows::UI::Color AlternateHueColor(uint8_t slot) noexcept
    {
        // Deliberately unrelated to the default table, so a control that failed to re-theme is
        // obvious rather than subtle.
        switch (slot % HueSlotCount)
        {
        case 0: return Windows::UI::Color{ 255, 0xF5, 0x6A, 0x1A };
        case 1: return Windows::UI::Color{ 255, 0x4A, 0x9E, 0xD6 };
        case 2: return Windows::UI::Color{ 255, 0xE8, 0xC1, 0x40 };
        case 3: return Windows::UI::Color{ 255, 0x6D, 0xBF, 0x4B };
        case 4: return Windows::UI::Color{ 255, 0xE0, 0x48, 0x3C };
        default: return Windows::UI::Color{ 255, 0xA6, 0x6C, 0xD6 };
        }
    }

    Windows::UI::Color AlternatePlateColor() noexcept
    {
        return Windows::UI::Color{ 0xDB, 0x3A, 0x3A, 0x3A };
    }

    void CompositionPalette::Create(Compositor const& compositor, bool alternate)
    {
        for (uint8_t slot = 0; slot < HueSlotCount; slot++)
        {
            m_hue[slot] = compositor.CreateColorBrush();
            m_bloom[slot] = compositor.CreateColorBrush();
        }

        m_plate = compositor.CreateColorBrush();
        m_track = compositor.CreateColorBrush();

        Recolor(alternate);
    }

    void CompositionPalette::Recolor(bool alternate)
    {
        for (uint8_t slot = 0; slot < HueSlotCount; slot++)
        {
            const auto color = alternate ? AlternateHueColor(slot) : HueColor(slot);

            m_hue[slot].Color(color);
            m_bloom[slot].Color(WithAlpha(color, 0x5A));
        }

        m_plate.Color(alternate ? AlternatePlateColor() : PlateColor());
        m_track.Color(TrackColor());
    }

    CompositionControl BuildCompositionControl(
        Compositor const& compositor,
        CompositionPalette const& palette,
        ControlDescriptor const& d)
    {
        CompositionControl control{};
        control.Kind = d.Kind;

        control.Root = compositor.CreateContainerVisual();
        control.Root.Offset(float3{ d.X, d.Y, 0.0f });
        control.Root.Size(float2{ d.Width, d.Height });

        // The bloom sits behind the plate and is the only thing that moves when a control is
        // touched. Setting a visual's opacity does not invalidate layout anywhere.
        control.Bloom = compositor.CreateSpriteVisual();
        control.Bloom.Brush(palette.Bloom(d.HueSlot));
        control.Bloom.Offset(float3{ -6.0f, -6.0f, 0.0f });
        control.Bloom.Size(float2{ d.Width + 12.0f, d.Height + 12.0f });
        control.Bloom.Opacity(0.0f);
        control.Root.Children().InsertAtTop(control.Bloom);

        control.Shape = compositor.CreateShapeVisual();
        control.Shape.Size(float2{ d.Width, d.Height });
        control.Root.Children().InsertAtTop(control.Shape);

        // plate and rim
        control.PlateGeometry = compositor.CreateRoundedRectangleGeometry();
        control.PlateGeometry.Size(float2{ d.Width - 1.0f, d.Height - 1.0f });
        control.PlateGeometry.Offset(float2{ 0.5f, 0.5f });
        control.PlateGeometry.CornerRadius(float2{ PlateCornerRadius, PlateCornerRadius });

        control.PlateShape = compositor.CreateSpriteShape(control.PlateGeometry);
        control.PlateShape.FillBrush(palette.Plate());
        control.PlateShape.StrokeBrush(palette.Hue(d.HueSlot));
        control.PlateShape.StrokeThickness(1.0f);
        control.Shape.Shapes().Append(control.PlateShape);

        if (d.Kind == ControlKind::Knob)
        {
            const float radius = std::min(d.Width, d.Height) * 0.5f - PipeInset;

            auto trackGeometry = compositor.CreateEllipseGeometry();
            trackGeometry.Radius(float2{ radius, radius });
            trackGeometry.Center(float2{ d.Width * 0.5f, d.Height * 0.5f });
            trackGeometry.TrimOffset(0.625f);
            trackGeometry.TrimStart(0.0f);
            trackGeometry.TrimEnd(KnobSweepDegrees / 360.0f);

            auto trackShape = compositor.CreateSpriteShape(trackGeometry);
            trackShape.StrokeBrush(palette.Track());
            trackShape.StrokeThickness(KnobArcThickness);
            trackShape.IsStrokeNonScaling(true);
            control.Shape.Shapes().Append(trackShape);

            control.ArcGeometry = compositor.CreateEllipseGeometry();
            control.ArcGeometry.Radius(float2{ radius, radius });
            control.ArcGeometry.Center(float2{ d.Width * 0.5f, d.Height * 0.5f });
            control.ArcGeometry.TrimOffset(0.625f);
            control.ArcGeometry.TrimStart(0.0f);
            control.ArcGeometry.TrimEnd(0.0f);

            control.PipeShape = compositor.CreateSpriteShape(control.ArcGeometry);
            control.PipeShape.StrokeBrush(palette.Hue(d.HueSlot));
            control.PipeShape.StrokeThickness(KnobArcThickness);
            control.Shape.Shapes().Append(control.PipeShape);
        }
        else
        {
            const bool vertical = d.Kind == ControlKind::Fader;

            const float trackX = vertical ? (d.Width * 0.5f - 4.0f) : PipeInset;
            const float trackY = vertical ? PipeInset : (d.Height - PipeInset - 8.0f);
            const float trackW = vertical ? 8.0f : (d.Width - PipeInset * 2.0f);
            const float trackH = vertical ? (d.Height - PipeInset * 2.0f) : 8.0f;

            auto trackGeometry = compositor.CreateRoundedRectangleGeometry();
            trackGeometry.Size(float2{ trackW, trackH });
            trackGeometry.Offset(float2{ trackX, trackY });
            trackGeometry.CornerRadius(float2{ 4.0f, 4.0f });

            auto trackShape = compositor.CreateSpriteShape(trackGeometry);
            trackShape.FillBrush(palette.Track());
            control.Shape.Shapes().Append(trackShape);

            control.PipeGeometry = compositor.CreateRoundedRectangleGeometry();
            control.PipeGeometry.CornerRadius(float2{ 4.0f, 4.0f });
            control.PipeGeometry.Size(float2{ vertical ? trackW : 0.0f, vertical ? 0.0f : trackH });
            control.PipeGeometry.Offset(float2{ trackX, vertical ? (trackY + trackH) : trackY });

            control.PipeShape = compositor.CreateSpriteShape(control.PipeGeometry);
            control.PipeShape.FillBrush(palette.Hue(d.HueSlot));
            control.Shape.Shapes().Append(control.PipeShape);

            control.TrackOrigin = vertical ? trackY : trackX;
            control.TrackLength = vertical ? trackH : trackW;
            control.PipeThickness = vertical ? trackW : trackH;
            control.PipeCrossOffset = vertical ? trackX : trackY;
        }

        SetCompositionValue(control, d.Value, 0.0f);

        return control;
    }

    void SetCompositionValue(CompositionControl const& control, float value, float bloom) noexcept
    {
        const float clamped = std::clamp(value, 0.0f, 1.0f);

        if (control.Kind == ControlKind::Knob)
        {
            control.ArcGeometry.TrimEnd(clamped * (KnobSweepDegrees / 360.0f));
        }
        else if (control.Kind == ControlKind::Fader)
        {
            const float filled = control.TrackLength * clamped;

            control.PipeGeometry.Size(float2{ control.PipeThickness, filled });
            control.PipeGeometry.Offset(float2{
                control.PipeCrossOffset,
                control.TrackOrigin + control.TrackLength - filled });
        }
        else
        {
            control.PipeGeometry.Size(float2{ control.TrackLength * clamped, control.PipeThickness });
        }

        control.Bloom.Opacity(std::clamp(bloom, 0.0f, 1.0f));
    }

    bool VerifyCompositionTheme(std::vector<CompositionControl> const& controls, bool alternate)
    {
        if (controls.empty())
        {
            return false;
        }

        for (auto const& control : controls)
        {
            auto brush = control.PlateShape.StrokeBrush().try_as<CompositionColorBrush>();

            if (!brush)
            {
                return false;
            }

            const auto actual = brush.Color();

            bool matched = false;

            for (uint8_t slot = 0; slot < HueSlotCount && !matched; slot++)
            {
                const auto candidate = alternate ? AlternateHueColor(slot) : HueColor(slot);

                matched = candidate.R == actual.R && candidate.G == actual.G && candidate.B == actual.B;
            }

            if (!matched)
            {
                return false;
            }
        }

        return true;
    }
}
