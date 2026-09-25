// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The controls whose drawing does not fit the "plate, track, pipe" shape the rest of the surface
// is built from: the two axis field, the ribbon, the piano keyboard, the clock generator, and the
// picture a control or a grouping panel shows.
//
// Same rules as the rest of the surface. Every number here is a ratio of the control's own
// rectangle rather than a pixel count, nothing is saturated at rest, and a hue only ever appears
// on the rim, on the value and in the glow.

#include "pch.h"
#include "SurfaceRenderer.h"
#include "LayoutStore.h"
#include "ThemeStore.h"

#include <winrt/Windows.Media.Core.h>
#include <winrt/Windows.Media.Playback.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>

#include <cmath>
#include <filesystem>

using namespace winrt;
using namespace winrt::Microsoft::UI::Composition;
using namespace winrt::Windows::Foundation::Numerics;

namespace media = ::winrt::Microsoft::UI::Xaml::Media;
namespace controls = ::winrt::Microsoft::UI::Xaml::Controls;

namespace glass
{
    namespace
    {
        // The field a puck moves in, inside the plate. Taken from the comp's XY pad, whose grid
        // runs right to the plate's rounded edge with the puck kept clear of it.
        constexpr float FieldInset = 3.0f;

        // The comp draws a 16 px puck on a 150 px field and a 30 px one on a 180 px joystick.
        constexpr float PuckFraction = 0.105f;
        constexpr float JoystickPuckFraction = 0.166f;
        constexpr float MinimumPuckSize = 8.0f;

        // The dot of hue inside a joystick's cap. The comp insets it 11 px on a 30 px puck,
        // which leaves a dot 8 px across - so this is its radius, not its width.
        constexpr float JoystickDotFraction = 4.0f / 30.0f;

        // The joystick's two guide rings, as fractions of the plate. The comp insets them 9 px
        // and 22 px on a 180 px control.
        constexpr float JoystickOuterRingInset = 0.05f;
        constexpr float JoystickInnerRingInset = 0.122f;

        // The crosshair through an XY pad's puck.
        constexpr float CrossThickness = 1.0f;

        // How far a ribbon's light spreads either side of the finger, as a fraction of the
        // control's short edge. Wide enough to read as a glow rather than as a line.
        constexpr float RibbonGlowSpread = 0.9f;
        constexpr int32_t RibbonGlowRings = 10;

        // A black key is shorter and narrower than a white one. Standard proportions: a little
        // under two thirds the length and about six tenths the width.
        constexpr float BlackKeyLengthFraction = 0.62f;
        constexpr float BlackKeyWidthFraction = 0.60f;

        // Which semitones in an octave are black, counting from C.
        constexpr bool BlackInOctave[12]
        {
            false, true, false, true, false, false, true, false, true, false, true, false
        };

        bool IsBlackKey(_In_ int32_t note) noexcept
        {
            auto const semitone = ((note % 12) + 12) % 12;

            return BlackInOctave[semitone];
        }

        // How many white keys there are below this note, counting from the lowest key drawn.
        int32_t WhiteKeysBelow(_In_ int32_t lowestNote, _In_ int32_t note) noexcept
        {
            int32_t count{ 0 };

            for (auto walk = lowestNote; walk < note; ++walk)
            {
                if (!IsBlackKey(walk))
                {
                    count++;
                }
            }

            return count;
        }

        winrt::Windows::UI::Color ToWindowsColor(_In_ ThemeColor const& color) noexcept
        {
            return winrt::Windows::UI::ColorHelper::FromArgb(color.A, color.R, color.G, color.B);
        }

        // A color a customer typed, or the one the theme would have chosen. A color that does
        // not parse falls back rather than turning into black, which is the difference between
        // a typo and an unusable control.
        ThemeColor ColorOr(_In_ std::wstring const& text, _In_ ThemeColor const& fallback) noexcept
        {
            ThemeColor parsed{};

            return TryParseColor(text, parsed) ? parsed : fallback;
        }

        // The clock's ring, from the comp: a 52 px ring on a 70 px control, stroked 3 px, with
        // the beat circle inset 9 px inside it.
        constexpr float ClockRingFraction = 0.74f;
        constexpr float ClockRingThickness = 3.0f;
        constexpr float ClockBeatInsetFraction = 0.17f;
        constexpr float ClockPipSize = 6.0f;
        constexpr float ClockPipGap = 5.0f;
        constexpr int32_t ClockPipCount = 4;
        constexpr float ClockPipStripHeight = 17.0f;

        // Where a fader's slot starts and ends inside its plate. The same inset the renderer
        // uses, repeated here so a stop number lines up with the mark it belongs to.
        constexpr float PipeInsetForLabels = 8.0f;
    }

    // ------------------------------------------------------------------ two axis

    _Use_decl_annotations_
    void SurfaceRenderer::LayoutTwoAxis(
        Compositor const& compositor,
        SurfaceVisual& visual,
        Control const& control,
        ControlColors const& colors,
        Theme const& theme,
        float width,
        float height)
    {
        auto const joystick = control.Kind == ControlKind::Joystick;

        auto const fieldX = FieldInset;
        auto const fieldY = FieldInset;
        auto const fieldW = std::max(width - FieldInset * 2.0f, 1.0f);
        auto const fieldH = std::max(height - FieldInset * 2.0f, 1.0f);

        auto const puck = std::max(
            MinimumPuckSize,
            std::min(width, height) * (joystick ? JoystickPuckFraction : PuckFraction));

        visual.PuckRadius = puck * 0.5f;

        // Inset by the puck's own radius, so the middle of the puck never leaves the field.
        visual.FieldX = fieldX + visual.PuckRadius;
        visual.FieldY = fieldY + visual.PuckRadius;
        visual.FieldWidth = std::max(fieldW - puck, 1.0f);
        visual.FieldHeight = std::max(fieldH - puck, 1.0f);

        if (joystick)
        {
            // Two hairline guide rings, and a third in the control's hue when it recenters. The
            // spring ring is the only signal that tells somebody the puck will come back.
            auto const center = float2{ width * 0.5f, height * 0.5f };
            auto const shortest = std::min(width, height);

            auto const ring = [&](float insetFraction, ThemeColor const& color)
                {
                    auto const radius = shortest * 0.5f - shortest * insetFraction;

                    if (radius <= 1.0f)
                    {
                        return;
                    }

                    auto geometry = compositor.CreateEllipseGeometry();
                    geometry.Radius(float2{ radius, radius });
                    geometry.Center(center);

                    auto shape = compositor.CreateSpriteShape(geometry);
                    shape.StrokeBrush(BrushFor(compositor, color));
                    shape.StrokeThickness(1.0f);

                    visual.Shape.Shapes().Append(shape);
                };

            ring(JoystickOuterRingInset, control.ReturnsToDefault ? colors.Rim : colors.Marks);
            ring(JoystickInnerRingInset, colors.Marks);
        }

        // The grid. Same idea on both shapes: the marks say where the middle and the quarters
        // are, so a hand can find a position rather than only the two ends.
        if (control.Ticks.Show && control.Ticks.Count >= MinimumTickCount)
        {
            auto const divisions = control.Ticks.Count - 1;

            auto lineClip = compositor.CreateRoundedRectangleGeometry();
            lineClip.Size(float2{ fieldW, fieldH });
            lineClip.Offset(float2{ fieldX, fieldY });
            lineClip.CornerRadius(joystick
                ? float2{ fieldW * 0.5f, fieldH * 0.5f }
                : float2{ static_cast<float>(theme.CornerRadius), static_cast<float>(theme.CornerRadius) });

            // A round field's grid has to stop at the circle, not at its bounding box.
            visual.Grid = compositor.CreateShapeVisual();
            visual.Grid.Size(float2{ width, height });
            visual.Grid.Clip(compositor.CreateGeometricClip(lineClip));

            for (int32_t line = 1; line < divisions; ++line)
            {
                auto const fraction = static_cast<float>(line) / static_cast<float>(divisions);

                for (auto const down : { false, true })
                {
                    auto geometry = compositor.CreateRoundedRectangleGeometry();

                    geometry.Size(down
                        ? float2{ fieldW, 1.0f }
                        : float2{ 1.0f, fieldH });

                    geometry.Offset(down
                        ? float2{ fieldX, fieldY + fieldH * fraction }
                        : float2{ fieldX + fieldW * fraction, fieldY });

                    auto shape = compositor.CreateSpriteShape(geometry);
                    shape.FillBrush(BrushFor(compositor, colors.Marks));

                    visual.Grid.Shapes().Append(shape);
                }
            }

            visual.Root.Children().InsertBelow(visual.Grid, visual.ValueShape);
        }

        // The crosshair. An XY pad has one because the value has to be readable from across a
        // room; a joystick's rings already do that job, so it goes without.
        if (!joystick)
        {
            auto crossColor = colors.Pipe;
            crossColor.A = static_cast<uint8_t>(std::lround(crossColor.A * 0.38));

            visual.CrossAcross = compositor.CreateRoundedRectangleGeometry();
            visual.CrossAcross.Size(float2{ fieldW, CrossThickness });

            auto acrossShape = compositor.CreateSpriteShape(visual.CrossAcross);
            acrossShape.FillBrush(BrushFor(compositor, crossColor));

            visual.CrossDown = compositor.CreateRoundedRectangleGeometry();
            visual.CrossDown.Size(float2{ CrossThickness, fieldH });

            auto downShape = compositor.CreateSpriteShape(visual.CrossDown);
            downShape.FillBrush(BrushFor(compositor, crossColor));

            visual.ValueShape.Shapes().Append(acrossShape);
            visual.ValueShape.Shapes().Append(downShape);
        }

        // The puck. On a joystick it is a cap with a dot of hue in it, the way a real stick has
        // a moulded top; on a pad it is the light itself.
        visual.PuckGeometry = compositor.CreateEllipseGeometry();
        visual.PuckGeometry.Radius(float2{ visual.PuckRadius, visual.PuckRadius });
        visual.PuckGeometry.Center(float2{ visual.FieldX, visual.FieldY });

        auto puckShape = compositor.CreateSpriteShape(visual.PuckGeometry);

        if (joystick)
        {
            puckShape.FillBrush(colors.Thumb == colors.ThumbEnd
                ? BrushFor(compositor, colors.Thumb).as<CompositionBrush>()
                : VerticalBrush(compositor, colors.Thumb, colors.ThumbEnd, false).as<CompositionBrush>());

            visual.ValueShape.Shapes().Append(puckShape);

            auto dotGeometry = compositor.CreateEllipseGeometry();
            auto const dot = std::max(2.0f, puck * JoystickDotFraction);

            dotGeometry.Radius(float2{ dot, dot });
            dotGeometry.Center(float2{ visual.FieldX, visual.FieldY });

            auto dotShape = compositor.CreateSpriteShape(dotGeometry);
            dotShape.FillBrush(BrushFor(compositor, colors.Pipe));

            visual.ValueShape.Shapes().Append(dotShape);

            // Both move together, so the dot is kept as the second geometry to offset.
            visual.CrossAcross = nullptr;
            visual.CrossDown = nullptr;
            visual.ArcGeometry = nullptr;
            visual.SweepGeometry = dotGeometry;
        }
        else
        {
            puckShape.FillBrush(BrushFor(compositor, colors.Pipe));

            // The halo, the same way a value bar gets one: rings of the puck's own geometry,
            // widest and faintest first.
            if (colors.Bloom.A > 0)
            {
                auto previous = 0.0;

                for (int32_t index = 0; index < 8; ++index)
                {
                    auto const t = 1.0 - static_cast<double>(index) / 8.0;
                    auto const cumulative = 0.5 * theme.GlowStrength / 100.0 * std::exp(-5.0 * t * t);
                    auto const step = cumulative - previous;

                    previous = cumulative;

                    if (step < 0.004)
                    {
                        continue;
                    }

                    auto glow = colors.Pipe;
                    glow.A = static_cast<uint8_t>(std::clamp(std::lround(255.0 * step), 0L, 255L));

                    auto glowShape = compositor.CreateSpriteShape(visual.PuckGeometry);
                    glowShape.StrokeBrush(BrushFor(compositor, glow));
                    glowShape.StrokeThickness(static_cast<float>(puck * 1.3 * t));

                    visual.ValueShape.Shapes().Append(glowShape);
                }
            }

            visual.ValueShape.Shapes().Append(puckShape);
        }
    }

    // -------------------------------------------------------------------- ribbon

    _Use_decl_annotations_
    void SurfaceRenderer::LayoutRibbon(
        Compositor const& compositor,
        SurfaceVisual& visual,
        Control const& control,
        ControlColors const& colors,
        Theme const& theme,
        float width,
        float height)
    {
        // A ribbon reads the long way. Everything below is written for a horizontal one and
        // flipped for an upright one, the same way a fader is.
        auto const vertical = height > width;

        visual.Vertical = vertical;

        auto const along = vertical ? height : width;
        auto const across = vertical ? width : height;

        auto const inset = std::max(2.0f, across * 0.12f);

        visual.TrackOrigin = inset;
        visual.TrackLength = std::max(along - inset * 2.0f, 1.0f);
        visual.PipeCrossOffset = inset;
        visual.PipeThickness = std::max(across - inset * 2.0f, 1.0f);
        visual.RibbonSpan = across * RibbonGlowSpread;

        // Tick marks across the travel, if the customer asked for them. There is no groove for
        // them to sit beside, so they run the full width of the strip at low contrast.
        if (control.Ticks.Show && control.Ticks.Count >= MinimumTickCount)
        {
            for (int32_t tick = 0; tick < control.Ticks.Count; ++tick)
            {
                auto const fraction =
                    static_cast<float>(tick) / static_cast<float>(control.Ticks.Count - 1);

                auto const at = visual.TrackOrigin + visual.TrackLength * fraction;

                auto geometry = compositor.CreateRoundedRectangleGeometry();

                geometry.Size(vertical
                    ? float2{ visual.PipeThickness, 1.0f }
                    : float2{ 1.0f, visual.PipeThickness });

                geometry.Offset(vertical
                    ? float2{ inset, std::min(at, height - 1.0f) }
                    : float2{ std::min(at, width - 1.0f), inset });

                auto shape = compositor.CreateSpriteShape(geometry);
                shape.FillBrush(BrushFor(compositor, colors.Marks));

                visual.Shape.Shapes().Append(shape);
            }
        }

        // The light that follows the finger. Concentric rounded rectangles of one narrow bar,
        // widest and faintest first, so the value reads as a soft band under the hand instead
        // of a hard line. The same curve the value bars use.
        auto const core = std::max(2.0f, across * 0.10f);

        visual.RibbonGlow.clear();

        auto previous = 0.0;

        for (int32_t ring = RibbonGlowRings; ring >= 0; --ring)
        {
            auto const t = static_cast<double>(ring) / RibbonGlowRings;
            auto const cumulative = 0.78 * theme.GlowStrength / 100.0 * std::exp(-4.5 * t * t);
            auto const step = ring == 0 ? 1.0 : cumulative - previous;

            previous = cumulative;

            if (ring != 0 && step < 0.004)
            {
                continue;
            }

            auto geometry = compositor.CreateRoundedRectangleGeometry();

            geometry.Size(vertical
                ? float2{ visual.PipeThickness, core }
                : float2{ core, visual.PipeThickness });

            geometry.CornerRadius(float2{ core * 0.5f, core * 0.5f });
            geometry.Offset(float2{ -core * 4.0f, -core * 4.0f });

            auto shape = compositor.CreateSpriteShape(geometry);

            if (ring == 0)
            {
                shape.FillBrush(BrushFor(compositor, colors.Pipe));
            }
            else
            {
                auto glow = colors.Pipe;
                glow.A = static_cast<uint8_t>(std::clamp(std::lround(255.0 * step), 0L, 255L));

                shape.StrokeBrush(BrushFor(compositor, glow));
                shape.StrokeThickness(static_cast<float>(visual.RibbonSpan * t));
            }

            visual.RibbonGlow.push_back(geometry);
            visual.ValueShape.Shapes().Append(shape);
        }

        // Nothing is holding it, so the light sits at its value but does not shout about it.
        // Where that is comes from the value pass that follows this one.
        visual.ValueShape.Opacity(0.45f);
    }

    // ------------------------------------------------------------ piano keyboard

    _Use_decl_annotations_
    void SurfaceRenderer::LayoutKeyboard(
        Compositor const& compositor,
        SurfaceVisual& visual,
        Control const& control,
        ControlColors const& colors,
        float width,
        float height)
    {
        visual.KeyShapes.clear();
        visual.KeyRestBrushes.clear();
        visual.PressedKey = -1;

        auto const& spec = control.Keyboard;

        auto const keys = std::clamp(spec.KeyCount, MinimumKeyboardKeys, MaximumKeyboardKeys);
        auto const lowest = std::clamp(spec.LowestNote, 0, 127);

        // A keyboard is sold by total keys, so the white count has to be counted rather than
        // divided out: 25 keys is 15 white, 49 is 29, and neither is a simple fraction.
        auto const whiteCount = std::max(1, WhiteKeysBelow(lowest, lowest + keys));

        auto const whiteWidth = width / static_cast<float>(whiteCount);
        auto const blackWidth = whiteWidth * BlackKeyWidthFraction;
        auto const blackHeight = height * BlackKeyLengthFraction;

        auto const white = ColorOr(spec.WhiteKeyColor, ThemeColor{ 232, 234, 238, 255 });
        auto const black = ColorOr(spec.BlackKeyColor, ThemeColor{ 22, 25, 31, 255 });

        auto const pressed = ColorOr(spec.PressedKeyColor, colors.Pipe);

        visual.KeyPressedBrush = BrushFor(compositor, pressed);

        auto const corner = std::min(3.0f, whiteWidth * 0.25f);

        // White keys first so the black ones land on top of them. Both are kept in one list in
        // key order, because that is the order a finger and a note number both count in.
        visual.KeyShapes.resize(static_cast<size_t>(keys), nullptr);
        visual.KeyRestBrushes.resize(static_cast<size_t>(keys), nullptr);

        for (auto const blacks : { false, true })
        {
            for (int32_t index = 0; index < keys; ++index)
            {
                auto const note = lowest + index;

                if (IsBlackKey(note) != blacks)
                {
                    continue;
                }

                auto const whitesBelow = static_cast<float>(WhiteKeysBelow(lowest, note));

                auto geometry = compositor.CreateRoundedRectangleGeometry();

                if (blacks)
                {
                    geometry.Size(float2{ blackWidth, blackHeight });
                    geometry.Offset(float2{ whitesBelow * whiteWidth - blackWidth * 0.5f, 0.0f });
                }
                else
                {
                    // A hairline of gap between white keys, so the edge of each one is visible
                    // against the next without drawing a separate line.
                    geometry.Size(float2{ std::max(whiteWidth - 1.0f, 1.0f), height });
                    geometry.Offset(float2{ whitesBelow * whiteWidth, 0.0f });
                }

                geometry.CornerRadius(float2{ corner, corner });

                auto shape = compositor.CreateSpriteShape(geometry);
                auto restBrush = BrushFor(compositor, blacks ? black : white);

                shape.FillBrush(restBrush);

                visual.KeyShapes[static_cast<size_t>(index)] = shape;
                visual.KeyRestBrushes[static_cast<size_t>(index)] = restBrush.as<CompositionBrush>();

                visual.ValueShape.Shapes().Append(shape);
            }
        }
    }

    // -------------------------------------------------------------- beat clock

    _Use_decl_annotations_
    void SurfaceRenderer::LayoutClock(
        Compositor const& compositor,
        SurfaceVisual& visual,
        Control const& control,
        ControlColors const& colors,
        float width,
        float height)
    {
        UNREFERENCED_PARAMETER(control);

        visual.PipShapes.clear();

        // The ring sits above the row of pips, both centered.
        auto const usable = std::max(height - ClockPipStripHeight, 8.0f);
        auto const diameter = std::min(width, usable) * ClockRingFraction;
        auto const radius = std::max(diameter * 0.5f, 4.0f);

        auto const center = float2{ width * 0.5f, ClockPipStripHeight * 0.0f + usable * 0.5f };

        // The unlit ring, which is what says a stopped clock is still a clock.
        auto trackGeometry = compositor.CreateEllipseGeometry();
        trackGeometry.Radius(float2{ radius, radius });
        trackGeometry.Center(center);

        auto trackShape = compositor.CreateSpriteShape(trackGeometry);
        trackShape.StrokeBrush(BrushFor(compositor, colors.Track));
        trackShape.StrokeThickness(ClockRingThickness);

        visual.Shape.Shapes().Append(trackShape);

        // The sweep: a full turn per bar, starting at twelve o'clock. Trimmed rather than
        // animated, so the picture can never disagree with what is actually being sent.
        visual.SweepGeometry = compositor.CreateEllipseGeometry();
        visual.SweepGeometry.Radius(float2{ radius, radius });
        visual.SweepGeometry.Center(center);
        visual.SweepGeometry.TrimOffset(0.75f);
        visual.SweepGeometry.TrimStart(0.0f);
        visual.SweepGeometry.TrimEnd(0.0f);

        auto sweepShape = compositor.CreateSpriteShape(visual.SweepGeometry);
        sweepShape.StrokeBrush(BrushFor(compositor, colors.Pipe));
        sweepShape.StrokeThickness(ClockRingThickness);

        visual.ValueShape.Shapes().Append(sweepShape);

        // The disc inside it, which is what flashes on the beat. Barely tinted while the clock
        // is stopped: a clock that looks lit when it is not sending is the one thing this
        // control must never do.
        auto beatGeometry = compositor.CreateEllipseGeometry();
        auto const beatRadius = std::max(radius - radius * 2.0f * ClockBeatInsetFraction, 2.0f);

        beatGeometry.Radius(float2{ beatRadius, beatRadius });
        beatGeometry.Center(center);

        auto beatDim = colors.Pipe;
        beatDim.A = static_cast<uint8_t>(std::lround(beatDim.A * 0.07));

        auto beatLit = colors.Pipe;
        beatLit.A = static_cast<uint8_t>(std::lround(beatLit.A * 0.55));

        visual.BeatOffBrush = BrushFor(compositor, beatDim).as<CompositionBrush>();
        visual.BeatOnBrush = BrushFor(compositor, beatLit).as<CompositionBrush>();

        visual.BeatShape = compositor.CreateSpriteShape(beatGeometry);
        visual.BeatShape.FillBrush(visual.BeatOffBrush);
        visual.BeatShape.StrokeBrush(BrushFor(compositor, colors.Rim));
        visual.BeatShape.StrokeThickness(1.0f);

        visual.ValueShape.Shapes().Append(visual.BeatShape);

        // Four pips under it, one per beat in the bar.
        auto pipLit = colors.Pipe;

        auto pipDim = ReadableInk(DeckColor());
        pipDim.A = 36;

        visual.PipLitBrush = BrushFor(compositor, pipLit).as<CompositionBrush>();
        visual.PipDimBrush = BrushFor(compositor, pipDim).as<CompositionBrush>();

        auto const stripWidth = ClockPipCount * ClockPipSize + (ClockPipCount - 1) * ClockPipGap;
        auto const stripLeft = (width - stripWidth) * 0.5f;
        auto const stripTop = height - ClockPipStripHeight + (ClockPipStripHeight - ClockPipSize) * 0.5f;

        if (stripLeft >= 0.0f && stripTop >= 0.0f)
        {
            for (int32_t pip = 0; pip < ClockPipCount; ++pip)
            {
                auto geometry = compositor.CreateEllipseGeometry();

                geometry.Radius(float2{ ClockPipSize * 0.5f, ClockPipSize * 0.5f });
                geometry.Center(float2{
                    stripLeft + pip * (ClockPipSize + ClockPipGap) + ClockPipSize * 0.5f,
                    stripTop + ClockPipSize * 0.5f });

                auto shape = compositor.CreateSpriteShape(geometry);
                shape.FillBrush(visual.PipDimBrush);

                visual.ValueShape.Shapes().Append(shape);
                visual.PipShapes.push_back(shape);
            }
        }
    }

    // ------------------------------------------------------- moving what they show

    _Use_decl_annotations_
    void SurfaceRenderer::MovePuck(size_t itemIndex) noexcept
    {
        if (itemIndex >= m_visuals.size())
        {
            return;
        }

        auto& visual = m_visuals[itemIndex];

        if (visual.PuckGeometry == nullptr)
        {
            return;
        }

        try
        {
            auto const x = static_cast<float>(std::clamp(m_values[itemIndex], 0.0, 1.0));

            // Bottom is zero. Screen coordinates run the other way and every joystick, pad and
            // plug-in in the world says up is more, so the flip happens here once.
            auto const y = 1.0f - static_cast<float>(std::clamp(m_valuesY[itemIndex], 0.0, 1.0));

            auto const at = float2{
                visual.FieldX + visual.FieldWidth * x,
                visual.FieldY + visual.FieldHeight * y };

            visual.PuckGeometry.Center(at);

            // The joystick's dot of hue rides inside its cap.
            if (visual.SweepGeometry != nullptr && visual.Kind == ControlKind::Joystick)
            {
                visual.SweepGeometry.Center(at);
            }

            if (visual.CrossAcross != nullptr)
            {
                visual.CrossAcross.Offset(float2{ FieldInset, at.y - CrossThickness * 0.5f });
            }

            if (visual.CrossDown != nullptr)
            {
                visual.CrossDown.Offset(float2{ at.x - CrossThickness * 0.5f, FieldInset });
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void SurfaceRenderer::MoveRibbonLight(size_t itemIndex) noexcept
    {
        if (itemIndex >= m_visuals.size())
        {
            return;
        }

        auto& visual = m_visuals[itemIndex];

        if (visual.RibbonGlow.empty())
        {
            return;
        }

        try
        {
            auto const value = static_cast<float>(std::clamp(m_values[itemIndex], 0.0, 1.0));

            auto const along = visual.Vertical
                ? visual.TrackOrigin + visual.TrackLength * (1.0f - value)
                : visual.TrackOrigin + visual.TrackLength * value;

            for (auto const& geometry : visual.RibbonGlow)
            {
                auto const size = geometry.Size();

                auto const half = (visual.Vertical ? size.y : size.x) * 0.5f;

                geometry.Offset(visual.Vertical
                    ? float2{ visual.PipeCrossOffset, along - half }
                    : float2{ along - half, visual.PipeCrossOffset });
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void SurfaceRenderer::SetValueY(size_t itemIndex, double value) noexcept
    {
        if (itemIndex >= m_valuesY.size())
        {
            return;
        }

        m_valuesY[itemIndex] = std::clamp(value, 0.0, 1.0);

        MovePuck(itemIndex);
    }

    _Use_decl_annotations_
    void SurfaceRenderer::SetPressedKey(size_t itemIndex, int32_t key) noexcept
    {
        if (itemIndex >= m_visuals.size())
        {
            return;
        }

        auto& visual = m_visuals[itemIndex];

        if (visual.KeyShapes.empty() || visual.PressedKey == key)
        {
            return;
        }

        try
        {
            auto const paint = [&](int32_t which, bool down)
                {
                    if (which < 0 || static_cast<size_t>(which) >= visual.KeyShapes.size())
                    {
                        return;
                    }

                    auto const& shape = visual.KeyShapes[static_cast<size_t>(which)];

                    if (shape == nullptr)
                    {
                        return;
                    }

                    shape.FillBrush(down
                        ? visual.KeyPressedBrush
                        : visual.KeyRestBrushes[static_cast<size_t>(which)]);
                };

            paint(visual.PressedKey, false);
            paint(key, true);

            visual.PressedKey = key;
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void SurfaceRenderer::SetBeat(
        size_t itemIndex,
        int32_t beatInBar,
        double phase,
        bool running) noexcept
    {
        if (itemIndex >= m_visuals.size())
        {
            return;
        }

        auto& visual = m_visuals[itemIndex];

        if (visual.SweepGeometry == nullptr || visual.Kind != ControlKind::BeatClock)
        {
            return;
        }

        try
        {
            // A whole turn per bar. A sweep that resets every beat is four times as much
            // movement and says nothing more.
            auto const through = running
                ? std::clamp(
                    (static_cast<double>(beatInBar) + std::clamp(phase, 0.0, 1.0)) / ClockPipCount,
                    0.0,
                    1.0)
                : 0.0;

            visual.SweepGeometry.TrimEnd(static_cast<float>(through));

            if (visual.BeatShape != nullptr)
            {
                // Lit for the first part of each beat, which is what reads as a pulse rather
                // than as a light that is simply on.
                auto const lit = running && phase < 0.25;

                visual.BeatShape.FillBrush(lit ? visual.BeatOnBrush : visual.BeatOffBrush);
            }

            for (size_t pip = 0; pip < visual.PipShapes.size(); ++pip)
            {
                auto const lit = running && static_cast<int32_t>(pip) == beatInBar;

                visual.PipShapes[pip].FillBrush(lit ? visual.PipLitBrush : visual.PipDimBrush);
            }

            // Counted from one, the way a musician counts a bar.
            if (itemIndex < m_beatTexts.size() && m_beatTexts[itemIndex] != nullptr)
            {
                m_beatTexts[itemIndex].Text(winrt::hstring{
                    std::to_wstring(std::clamp(beatInBar, 0, 15) + 1) });

                m_beatTexts[itemIndex].Opacity(running ? 1.0 : 0.45);
            }
        }
        catch (...)
        {
        }
    }

    // ------------------------------------------------------- the beat count on a clock

    _Use_decl_annotations_
    void SurfaceRenderer::LayoutBeatText(
        size_t itemIndex,
        Control const& control,
        Theme const& theme)
    {
        if (itemIndex >= m_beatTexts.size())
        {
            return;
        }

        if (m_beatTexts[itemIndex] != nullptr)
        {
            uint32_t index{ 0 };

            if (m_host != nullptr && m_host.Children().IndexOf(m_beatTexts[itemIndex], index))
            {
                m_host.Children().RemoveAt(index);
            }

            m_beatTexts[itemIndex] = nullptr;
        }

        if (control.Kind != ControlKind::BeatClock)
        {
            return;
        }

        try
        {
            auto const width = std::max(control.Width, 4.0);
            auto const height = std::max(control.Height, 4.0);

            auto const usable = std::max(height - ClockPipStripHeight, 8.0);

            controls::TextBlock text{};

            // Monospace, so the number does not shuffle sideways every beat.
            text.FontFamily(media::FontFamily{ L"Cascadia Mono, Consolas" });
            text.FontSize(std::clamp(std::min(width, usable) * 0.18, 9.0, 20.0));
            text.Foreground(media::SolidColorBrush{ ToWindowsColor(ReadableInk(DeckColor())) });
            text.IsHitTestVisible(false);
            text.TextAlignment(xaml::TextAlignment::Center);
            text.Width(width);
            text.Text(L"1");

            xaml::Automation::AutomationProperties::SetAccessibilityView(
                text, xaml::Automation::Peers::AccessibilityView::Raw);

            UNREFERENCED_PARAMETER(theme);

            text.Measure(winrt::Windows::Foundation::Size{
                static_cast<float>(width), std::numeric_limits<float>::infinity() });

            auto const offset = usable * 0.5 - text.DesiredSize().Height * 0.5;

            m_beatTextOffsets[itemIndex] = offset;

            controls::Canvas::SetLeft(text, control.X);
            controls::Canvas::SetTop(text, control.Y + offset);

            m_host.Children().Append(text);
            m_beatTexts[itemIndex] = text;
        }
        catch (...)
        {
        }
    }

    // ------------------------------------------------------- the numbers at the stops

    _Use_decl_annotations_
    void SurfaceRenderer::LayoutDetentValues(
        size_t itemIndex,
        Control const& control,
        Theme const& theme)
    {
        if (itemIndex >= m_detentTexts.size())
        {
            return;
        }

        auto const drop = [&]()
            {
                if (m_detentTexts[itemIndex] == nullptr)
                {
                    return;
                }

                uint32_t index{ 0 };

                if (m_host != nullptr && m_host.Children().IndexOf(m_detentTexts[itemIndex], index))
                {
                    m_host.Children().RemoveAt(index);
                }

                m_detentTexts[itemIndex] = nullptr;
            };

        drop();

        if (!control.ShowDetentValues)
        {
            return;
        }

        try
        {
            auto const labels = DetentStopLabels(control);

            if (labels.size() < 2)
            {
                return;
            }

            auto const width = static_cast<float>(std::max(control.Width, 4.0));
            auto const height = static_cast<float>(std::max(control.Height, 4.0));

            auto const colors = ResolveControlColors(control, theme);

            controls::Canvas host{};

            host.IsHitTestVisible(false);
            host.Width(width);
            host.Height(height);

            // The control beside it already says what it is; a screen reader reading a column
            // of numbers as well would be noise.
            xaml::Automation::AutomationProperties::SetAccessibilityView(
                host, xaml::Automation::Peers::AccessibilityView::Raw);

            auto const round = control.Kind == ControlKind::Knob ||
                control.Kind == ControlKind::Encoder;

            auto const vertical = !round && height >= width;

            auto const ink = media::SolidColorBrush{ ToWindowsColor(colors.Label) };

            for (size_t index = 0; index < labels.size(); ++index)
            {
                controls::TextBlock text{};

                text.Text(winrt::hstring{ labels[index] });
                text.FontSize(9.0);
                text.FontFamily(media::FontFamily{ L"Cascadia Mono, Consolas" });
                text.Foreground(ink);
                text.IsHitTestVisible(false);

                // Measured rather than guessed, because a number's width is what decides
                // whether it lands on the control or beside it.
                text.Measure(winrt::Windows::Foundation::Size{
                    std::numeric_limits<float>::infinity(),
                    std::numeric_limits<float>::infinity() });

                auto const size = text.DesiredSize();

                auto const fraction =
                    static_cast<float>(index) / static_cast<float>(labels.size() - 1);

                if (round)
                {
                    // Around the outside of the arc, three quarters of a turn starting at
                    // seven o'clock, the same travel the pointer takes.
                    auto const dial = std::min(width, height);
                    auto const radius = dial * 0.5f + size.Height * 0.55f;

                    auto const angle = (-135.0f - 90.0f + 270.0f * fraction) * 3.14159265f / 180.0f;

                    controls::Canvas::SetLeft(text,
                        width * 0.5f + std::cos(angle) * radius - size.Width * 0.5);
                    controls::Canvas::SetTop(text,
                        height * 0.5f + std::sin(angle) * radius - size.Height * 0.5);
                }
                else if (vertical)
                {
                    // Down the left side, bottom end first, so the numbers read the way the
                    // travel does.
                    controls::Canvas::SetLeft(text, -size.Width - 4.0);
                    controls::Canvas::SetTop(text,
                        height - PipeInsetForLabels - (height - PipeInsetForLabels * 2.0f) * fraction
                            - size.Height * 0.5);
                }
                else
                {
                    controls::Canvas::SetLeft(text,
                        PipeInsetForLabels + (width - PipeInsetForLabels * 2.0f) * fraction
                            - size.Width * 0.5);
                    controls::Canvas::SetTop(text, height + 2.0);
                }

                host.Children().Append(text);
            }

            controls::Canvas::SetLeft(host, control.X);
            controls::Canvas::SetTop(host, control.Y);

            m_host.Children().Append(host);
            m_detentTexts[itemIndex] = host;
        }
        catch (...)
        {
        }
    }

    // ------------------------------------------------------------------ picture

    _Use_decl_annotations_
    void SurfaceRenderer::LayoutPicture(size_t itemIndex, Control const& control)
    {
        if (itemIndex >= m_pictures.size())
        {
            return;
        }

        auto const drop = [&]()
            {
                if (m_pictures[itemIndex] == nullptr)
                {
                    return;
                }

                uint32_t index{ 0 };

                if (m_host != nullptr && m_host.Children().IndexOf(m_pictures[itemIndex], index))
                {
                    m_host.Children().RemoveAt(index);
                }

                m_pictures[itemIndex] = nullptr;
            };

        // Only the two kinds that have somewhere to put one. Everything else ignores the field,
        // so changing a control's kind cannot leave a picture hanging behind it.
        if (control.Image.IsEmpty() ||
            (control.Kind != ControlKind::Image && control.Kind != ControlKind::Panel))
        {
            drop();
            return;
        }

        try
        {
            auto const path = ControlPicturePath(m_layoutFilePath, control.Image.FileName);

            if (path.empty())
            {
                drop();
                return;
            }

            drop();

            auto const width = std::max(control.Width, 4.0);
            auto const height = std::max(control.Height, 4.0);

            foundation::Uri const uri{ L"file:///" + winrt::hstring{ path } };

            xaml::FrameworkElement element{ nullptr };

            if (IsVideoFileName(control.Image.FileName))
            {
                // A video on a control surface that stops four seconds in looks broken, so a
                // loop is the default and the customer turns it off rather than on.
                winrt::Windows::Media::Playback::MediaPlayer media{};

                media.IsLoopingEnabled(control.Image.Loops);
                media.AutoPlay(true);

                // Silent. A layout is a control surface, and audio out of a decorative clip
                // during a set is never what anybody wanted.
                media.IsMuted(true);
                media.Source(winrt::Windows::Media::Core::MediaSource::CreateFromUri(uri));

                controls::MediaPlayerElement player{};

                player.AreTransportControlsEnabled(false);
                player.Stretch(control.Image.Fit == BackgroundFit::Stretch
                    ? media::Stretch::Fill
                    : control.Image.Fit == BackgroundFit::Centered
                        ? media::Stretch::None
                        : media::Stretch::UniformToFill);

                player.SetMediaPlayer(media);

                element = player;
            }
            else
            {
                controls::Image image{};

                auto const extension = std::filesystem::path{ control.Image.FileName }
                    .extension().wstring();

                if (_wcsicmp(extension.c_str(), L".svg") == 0)
                {
                    media::Imaging::SvgImageSource source{};

                    source.UriSource(uri);
                    source.RasterizePixelWidth(width);
                    source.RasterizePixelHeight(height);

                    image.Source(source);
                }
                else
                {
                    media::Imaging::BitmapImage bitmap{};

                    bitmap.UriSource(uri);
                    image.Source(bitmap);
                }

                switch (control.Image.Fit)
                {
                case BackgroundFit::Centered:
                    image.Stretch(media::Stretch::None);
                    break;

                case BackgroundFit::Stretch:
                    image.Stretch(media::Stretch::Fill);
                    break;

                case BackgroundFit::Tiled:
                    image.Stretch(media::Stretch::UniformToFill);
                    break;

                default:
                    image.Stretch(media::Stretch::Uniform);
                    break;
                }

                element = image;
            }

            if (element == nullptr)
            {
                return;
            }

            element.Width(width);
            element.Height(height);
            element.IsHitTestVisible(false);
            element.Opacity(std::clamp(control.Image.Opacity, 0.0, 1.0));

            // Trimmed to the control's own rectangle, so a picture that does not match the
            // shape of the control does not spill over the controls next to it.
            media::RectangleGeometry clip{};
            clip.Rect(winrt::Windows::Foundation::Rect{
                0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) });
            element.Clip(clip);

            xaml::Automation::AutomationProperties::SetAccessibilityView(
                element, xaml::Automation::Peers::AccessibilityView::Raw);

            controls::Canvas::SetLeft(element, control.X);
            controls::Canvas::SetTop(element, control.Y);

            // A grouping panel's fill belongs behind the controls it frames; an image control
            // draws where it was placed like anything else.
            if (control.Kind == ControlKind::Panel)
            {
                controls::Canvas::SetZIndex(element, -1);
                m_host.Children().InsertAt(0, element);
            }
            else
            {
                m_host.Children().Append(element);
            }

            m_pictures[itemIndex] = element;
        }
        catch (...)
        {
        }
    }
}
