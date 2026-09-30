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
#include "LfoShape.h"
#include "StepPattern.h"
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

        // A joystick's ball is most of the stick you grab; a reticle is wider than a disc so its
        // open middle stays open.
        constexpr float JoystickBallFraction = 0.38f;
        constexpr float ReticleFraction = 0.18f;

        // How thick a joystick's shaft is, as a share of the ball.
        constexpr float ShaftFraction = 0.16f;

        // The dotted line round the playing step stands this far off the step.
        constexpr float StepFocusOffset = 2.0f;

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

        // How strongly a natural key is outlined in the dark key color.
        constexpr double KeyOutlineStrength = 0.40;

        // A well: the field of a display, sunk into its plate.
        constexpr float WellCornerRadius = 2.0f;
        constexpr float WellShadePixels = 5.0f;

        // A display that is a window the size of the control.
        constexpr float WindowCornerRadius = 3.0f;

        // Inside a window, a value keeps its own color: the grid faint, the crosshair at half.
        constexpr double WindowGridStrength = 0.12;
        constexpr double WindowCrossStrength = 0.50;

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

        // The cycle an LFO draws inside its plate, and the bead riding it.
        constexpr float LfoInset = 9.0f;
        constexpr float LfoStrokeThickness = 1.6f;
        constexpr float LfoBeadFraction = 0.11f;
        constexpr float MinimumLfoBead = 3.0f;

        // How many straight pieces a curve is cut into. The corner cases are drawn from their
        // own corners instead, so this only ever has to be smooth enough for a sine.
        constexpr int32_t LfoCurveSteps = 48;

        // How bright the bead is while the sweep is stopped. The same figure the ribbon's light
        // rests at, for the same reason: it has to be visible without looking live.
        constexpr float LfoRestOpacity = 0.45f;

        // The platter, inside its plate.
        constexpr float TurntableInset = 5.0f;
        constexpr float TurntableGripInset = 3.0f;
        constexpr float TurntableGripThickness = 3.0f;
        constexpr float TurntableSpindleFraction = 0.13f;

        // One cycle of a wave, as points inside a unit square: x is the phase, y is the value
        // with 1 at the top. The shapes with corners are built from those corners rather than
        // sampled, or a square wave comes out with a slope on its edges.
        std::vector<float2> LfoCyclePoints(_In_ LfoSpec const& spec)
        {
            std::vector<float2> points{};

            auto const at = [&spec](double phase)
                {
                    return static_cast<float>(LfoValueAt(spec, phase, 0.5));
                };

            switch (spec.Wave)
            {
            case LfoWave::Square:
                points.push_back(float2{ 0.0f, at(0.0) });
                points.push_back(float2{ 0.5f, at(0.0) });
                points.push_back(float2{ 0.5f, at(0.75) });
                points.push_back(float2{ 1.0f, at(0.75) });
                break;

            case LfoWave::Triangle:
                points.push_back(float2{ 0.0f, at(0.0) });
                points.push_back(float2{ 0.25f, at(0.25) });
                points.push_back(float2{ 0.75f, at(0.75) });
                points.push_back(float2{ 1.0f, at(1.0) });
                break;

            case LfoWave::RampUp:
            case LfoWave::RampDown:
                points.push_back(float2{ 0.0f, at(0.0) });
                points.push_back(float2{ 1.0f, at(0.9999) });
                break;

            case LfoWave::Sine:
                for (int32_t step = 0; step <= LfoCurveSteps; ++step)
                {
                    auto const phase = static_cast<double>(step) / LfoCurveSteps;

                    points.push_back(float2{ static_cast<float>(phase), at(phase) });
                }
                break;

            // Noise has no cycle to draw. A line through the middle of the sweep, with the bead
            // jumping about on it, is the honest picture: a random value inside this range.
            default:
            {
                auto const middle = static_cast<float>(
                    std::clamp((spec.Lowest + spec.Highest) * 0.5, 0.0, 1.0));

                points.push_back(float2{ 0.0f, middle });
                points.push_back(float2{ 1.0f, middle });
                break;
            }
            }

            return points;
        }
    }

    // ---------------------------------------------------------------------- the well

    // Anything that SHOWS something is sunk into its plate rather than raised off it: a dark
    // field a few pixels inside the edge, with a shadow along its top, so the plate becomes its
    // frame. What you press stands up; what shows you something is cut in. Nothing is drawn on
    // a theme that asks for no well.
    _Use_decl_annotations_
    void SurfaceRenderer::AppendWell(
        Compositor const& compositor,
        SurfaceVisual& visual,
        ControlColors const& colors,
        float x,
        float y,
        float width,
        float height)
    {
        if (colors.Well.A == 0 || width < 2.0f || height < 2.0f)
        {
            return;
        }

        // On a theme whose displays are windows, the window is the whole control, whatever part
        // of it the control asked for.
        auto corner = visual.WellCorner >= 0.0f ? visual.WellCorner : WellCornerRadius;

        if (visual.Windowed)
        {
            x = 0.0f;
            y = 0.0f;
            width = visual.Width;
            height = visual.Height;
            corner = visual.WellCorner >= 0.0f ? visual.WellCorner : WindowCornerRadius;
        }
        else if (colors.RecessLip.A != 0)
        {
            // The light catching the lower edge of the well: the well again, a pixel lower,
            // under it. A window does this for itself once whatever it shows is drawn.
            auto lipGeometry = compositor.CreateRoundedRectangleGeometry();
            lipGeometry.Size(float2{ width, height });
            lipGeometry.Offset(float2{ x, y + 1.0f });
            lipGeometry.CornerRadius(float2{ corner, corner });

            auto lipShape = compositor.CreateSpriteShape(lipGeometry);
            lipShape.FillBrush(BrushFor(compositor, colors.RecessLip));

            visual.Shape.Shapes().Append(lipShape);
        }

        auto geometry = compositor.CreateRoundedRectangleGeometry();
        geometry.Size(float2{ width, height });
        geometry.Offset(float2{ x, y });
        geometry.CornerRadius(float2{ corner, corner });

        auto well = compositor.CreateSpriteShape(geometry);
        well.FillBrush(BrushFor(compositor, colors.Well));

        visual.Shape.Shapes().Append(well);

        if (colors.Recess.A > 0)
        {
            auto shade = compositor.CreateSpriteShape(geometry);
            shade.FillBrush(RecessBrush(compositor, colors.Recess, WellShadePixels / height));

            visual.Shape.Shapes().Append(shade);
        }
    }

    // ---------------------------------------------------------------------- the LFO

    // One cycle of the wave, drawn across the plate, with a bead showing where the sweep is now.
    //
    // The comps draw generators as a dial with a rate on it. A single cycle with a bead on it
    // says three things at once that a dial cannot: what shape is running, how far through it is,
    // and how much of the travel it actually covers.
    _Use_decl_annotations_
    void SurfaceRenderer::LayoutLfo(
        Compositor const& compositor,
        SurfaceVisual& visual,
        Control const& control,
        ControlColors const& colors,
        float width,
        float height)
    {
        AppendWell(compositor, visual, colors,
            FieldInset, FieldInset, width - FieldInset * 2.0f, height - FieldInset * 2.0f);

        auto const fieldX = LfoInset;
        auto const fieldW = std::max(width - LfoInset * 2.0f, 1.0f);

        // Room under the cycle for a label, and a little at the top so a peak is not cut off.
        auto const fieldY = LfoInset;
        auto const fieldH = std::max(height - LfoInset * 2.0f, 1.0f);

        visual.FieldX = fieldX;
        visual.FieldY = fieldY;
        visual.FieldWidth = fieldW;
        visual.FieldHeight = fieldH;

        auto const points = LfoCyclePoints(control.Lfo);

        auto const place = [&](float2 const& point)
            {
                return float2{
                    fieldX + point.x * fieldW,
                    fieldY + (1.0f - point.y) * fieldH };
            };

        // A track under the wave at the middle of the sweep, so a shallow sweep still reads as
        // something moving inside a range rather than as a line drawn slightly off center.
        auto const trackColor = colors.Marks;

        auto trackGeometry = compositor.CreateLineGeometry();
        trackGeometry.Start(float2{ fieldX, fieldY + fieldH * 0.5f });
        trackGeometry.End(float2{ fieldX + fieldW, fieldY + fieldH * 0.5f });

        auto trackShape = compositor.CreateSpriteShape(trackGeometry);
        trackShape.StrokeBrush(BrushFor(compositor, trackColor));
        trackShape.StrokeThickness(1.0f);

        visual.Shape.Shapes().Append(trackShape);

        // The wave itself. Composition has no polyline, so it is a run of line segments with
        // round joins, which at these sizes is indistinguishable from one path.
        auto waveColor = visual.Windowed ? colors.WellValue : colors.Pipe;
        waveColor.A = static_cast<uint8_t>(std::lround(waveColor.A * 0.85));

        auto const waveBrush = BrushFor(compositor, waveColor);

        for (size_t index = 0; index + 1 < points.size(); ++index)
        {
            auto geometry = compositor.CreateLineGeometry();
            geometry.Start(place(points[index]));
            geometry.End(place(points[index + 1]));

            auto shape = compositor.CreateSpriteShape(geometry);
            shape.StrokeBrush(waveBrush);
            shape.StrokeThickness(LfoStrokeThickness);
            shape.StrokeStartCap(CompositionStrokeCap::Round);
            shape.StrokeEndCap(CompositionStrokeCap::Round);

            visual.Shape.Shapes().Append(shape);
        }

        // The bead. It sits at the start of the cycle until something moves it.
        auto const bead = std::max(MinimumLfoBead, std::min(width, height) * LfoBeadFraction);

        visual.PuckRadius = bead;

        visual.PuckGeometry = compositor.CreateEllipseGeometry();
        visual.PuckGeometry.Radius(float2{ bead, bead });
        visual.PuckGeometry.Center(points.empty() ? float2{ fieldX, fieldY } : place(points.front()));

        auto beadShape = compositor.CreateSpriteShape(visual.PuckGeometry);
        beadShape.FillBrush(BrushFor(compositor, visual.Windowed ? colors.WellValue : colors.Pipe));

        visual.ValueShape.Shapes().Append(beadShape);

        // Nothing is riding the wave until it runs, so the bead is dimmed at rest the same way
        // a ribbon's light is.
        visual.ValueShape.Opacity(LfoRestOpacity);
    }

    // ---------------------------------------------------------------- the turntable

    // A platter, a spindle and a marker. The marker is what says the thing has been pushed, and
    // it is the only part that moves: a record with no marker on it looks identical at every
    // angle, which is exactly the complaint people have about jog wheels with no indicator.
    _Use_decl_annotations_
    void SurfaceRenderer::LayoutTurntable(
        Compositor const& compositor,
        SurfaceVisual& visual,
        Control const& control,
        ControlColors const& colors,
        float width,
        float height)
    {
        auto const center = float2{ width * 0.5f, height * 0.5f };
        auto const shortest = std::min(width, height);
        auto const outer = std::max(shortest * 0.5f - TurntableInset, 2.0f);

        visual.FieldX = center.x;
        visual.FieldY = center.y;
        visual.FieldWidth = outer;
        visual.FieldHeight = outer;

        // The platter face.
        auto faceGeometry = compositor.CreateEllipseGeometry();
        faceGeometry.Radius(float2{ outer, outer });
        faceGeometry.Center(center);

        auto faceShape = compositor.CreateSpriteShape(faceGeometry);
        faceShape.FillBrush(BrushFor(compositor, colors.Track));
        faceShape.StrokeBrush(BrushFor(compositor, colors.Rim));
        faceShape.StrokeThickness(1.0f);

        visual.Shape.Shapes().Append(faceShape);

        // The ridges around the edge. Drawn as a dashed ring rather than as one shape per
        // ridge, the same trick the Bigwig lamp ring uses.
        if (control.Turntable.ShowsGrip && outer > 14.0f)
        {
            auto gripGeometry = compositor.CreateEllipseGeometry();
            gripGeometry.Radius(float2{ outer - TurntableGripInset, outer - TurntableGripInset });
            gripGeometry.Center(center);

            auto gripShape = compositor.CreateSpriteShape(gripGeometry);
            gripShape.StrokeBrush(BrushFor(compositor, colors.Marks));
            gripShape.StrokeThickness(TurntableGripThickness);
            gripShape.IsStrokeNonScaling(false);
            gripShape.StrokeDashArray().Append(1.2f);
            gripShape.StrokeDashArray().Append(1.8f);

            visual.Shape.Shapes().Append(gripShape);
        }

        // The spindle.
        auto spindleGeometry = compositor.CreateEllipseGeometry();
        auto const spindle = std::max(2.0f, outer * TurntableSpindleFraction);

        spindleGeometry.Radius(float2{ spindle, spindle });
        spindleGeometry.Center(center);

        auto spindleShape = compositor.CreateSpriteShape(spindleGeometry);
        spindleShape.FillBrush(BrushFor(compositor, colors.Thumb));

        visual.Shape.Shapes().Append(spindleShape);

        // The marker, drawn pointing straight up and rotated by however far the platter has
        // been pushed. Rotating one shape costs nothing per frame; rebuilding it would not.
        auto markerGeometry = compositor.CreateRoundedRectangleGeometry();
        auto const markerWidth = std::max(2.0f, outer * 0.08f);
        auto const markerLength = std::max(4.0f, outer - spindle - 3.0f);

        markerGeometry.Size(float2{ markerWidth, markerLength });
        markerGeometry.Offset(float2{ center.x - markerWidth * 0.5f, center.y - outer + 2.0f });
        markerGeometry.CornerRadius(float2{ markerWidth * 0.5f, markerWidth * 0.5f });

        visual.PointerShape = compositor.CreateSpriteShape(markerGeometry);
        visual.PointerShape.FillBrush(BrushFor(compositor, colors.Pipe));
        visual.PointerShape.CenterPoint(center);

        visual.ValueShape.Shapes().Append(visual.PointerShape);

        // Where the platter is now. A control loaded at its default is not being pushed, so it
        // starts square.
        visual.ArcGeometry = nullptr;
    }

    // ---------------------------------------------------------------- the wheel

    // A drum in a slot, the way the wheel beside a keyboard looks: ridges to grip and one
    // painted line that says where it is. Only the drum moves, clipped to its window, so a turn
    // costs one offset.
    _Use_decl_annotations_
    void SurfaceRenderer::LayoutWheel(
        Compositor const& compositor,
        SurfaceVisual& visual,
        Control const& control,
        ControlColors const& colors,
        float width,
        float height)
    {
        auto const vertical = height >= width;

        visual.WheelVertical = vertical;

        auto const inset = std::max(3.0f, std::min(width, height) * 0.12f);
        auto const slotX = inset;
        auto const slotY = inset;
        auto const slotW = std::max(width - (inset * 2.0f), 4.0f);
        auto const slotH = std::max(height - (inset * 2.0f), 4.0f);

        AppendWell(compositor, visual, colors, slotX, slotY, slotW, slotH);

        auto const drumInset = std::max(1.5f, std::min(slotW, slotH) * 0.08f);
        auto const drumX = slotX + drumInset;
        auto const drumY = slotY + drumInset;
        auto const drumW = std::max(slotW - (drumInset * 2.0f), 2.0f);
        auto const drumH = std::max(slotH - (drumInset * 2.0f), 2.0f);
        auto const drumCorner = std::min(drumW, drumH) * 0.3f;

        auto const drumGeometry = [&]()
            {
                auto geometry = compositor.CreateRoundedRectangleGeometry();
                geometry.Size(float2{ drumW, drumH });
                geometry.Offset(float2{ drumX, drumY });
                geometry.CornerRadius(float2{ drumCorner, drumCorner });

                return geometry;
            };

        auto drumShape = compositor.CreateSpriteShape(drumGeometry());
        drumShape.FillBrush(BrushFor(compositor, colors.Thumb));
        drumShape.StrokeBrush(BrushFor(compositor, colors.Rim));
        drumShape.StrokeThickness(1.0f);

        visual.Shape.Shapes().Append(drumShape);

        visual.ValueShape.Clip(compositor.CreateGeometricClip(drumGeometry()));

        auto const length = vertical ? drumH : drumW;
        auto const across = vertical ? drumW : drumH;

        // How far the painted line travels from one end of the range to the other.
        visual.WheelTravel = length * 0.8f;

        visual.WheelDrum = compositor.CreateContainerShape();

        // Ridges past both ends by the whole travel, so however far it turns there is never a
        // bare patch rolling into view.
        auto const spacing = std::clamp(length / 14.0f, 4.0f, 9.0f);
        auto const ridgeBrush = BrushFor(compositor, colors.Marks);

        for (auto along = -visual.WheelTravel; along <= length + visual.WheelTravel; along += spacing)
        {
            auto line = compositor.CreateLineGeometry();

            if (vertical)
            {
                line.Start(float2{ drumX + (across * 0.15f), drumY + along });
                line.End(float2{ drumX + (across * 0.85f), drumY + along });
            }
            else
            {
                line.Start(float2{ drumX + along, drumY + (across * 0.15f) });
                line.End(float2{ drumX + along, drumY + (across * 0.85f) });
            }

            auto ridge = compositor.CreateSpriteShape(line);
            ridge.StrokeBrush(ridgeBrush);
            ridge.StrokeThickness(1.0f);

            visual.WheelDrum.Shapes().Append(ridge);
        }

        // The painted line, in the control's own color. In the middle of the drum when the
        // value is in the middle; the drum's offset does the rest.
        auto const markThickness = std::max(2.0f, length * 0.035f);

        auto markGeometry = compositor.CreateRoundedRectangleGeometry();

        if (vertical)
        {
            markGeometry.Size(float2{ across * 0.9f, markThickness });
            markGeometry.Offset(float2{ drumX + (across * 0.05f), drumY + (length * 0.5f) - (markThickness * 0.5f) });
        }
        else
        {
            markGeometry.Size(float2{ markThickness, across * 0.9f });
            markGeometry.Offset(float2{ drumX + (length * 0.5f) - (markThickness * 0.5f), drumY + (across * 0.05f) });
        }

        markGeometry.CornerRadius(float2{ markThickness * 0.5f, markThickness * 0.5f });

        auto mark = compositor.CreateSpriteShape(markGeometry);
        mark.FillBrush(BrushFor(compositor, colors.Pipe));

        visual.WheelDrum.Shapes().Append(mark);
        visual.ValueShape.Shapes().Append(visual.WheelDrum);

        // A drum is round, so its ends turn away into shadow. Fixed over the moving part.
        auto shade = compositor.CreateLinearGradientBrush();

        shade.StartPoint(vertical ? float2{ 0.0f, 0.0f } : float2{ 0.0f, 0.0f });
        shade.EndPoint(vertical ? float2{ 0.0f, 1.0f } : float2{ 1.0f, 0.0f });
        shade.ColorStops().Append(compositor.CreateColorGradientStop(0.0f, winrt::Windows::UI::ColorHelper::FromArgb(150, 0, 0, 0)));
        shade.ColorStops().Append(compositor.CreateColorGradientStop(0.3f, winrt::Windows::UI::ColorHelper::FromArgb(0, 0, 0, 0)));
        shade.ColorStops().Append(compositor.CreateColorGradientStop(0.7f, winrt::Windows::UI::ColorHelper::FromArgb(0, 0, 0, 0)));
        shade.ColorStops().Append(compositor.CreateColorGradientStop(1.0f, winrt::Windows::UI::ColorHelper::FromArgb(150, 0, 0, 0)));

        auto shadeShape = compositor.CreateSpriteShape(drumGeometry());
        shadeShape.FillBrush(shade);

        visual.ValueShape.Shapes().Append(shadeShape);

        // Where a spring brings it back to, marked on the housing either side of the slot.
        if (control.ReturnsToDefault)
        {
            auto const rest = static_cast<float>(std::clamp(control.DefaultValue, 0.0, 1.0));
            auto const at = (length * 0.5f) + ((0.5f - rest) * visual.WheelTravel);
            auto const markBrush = BrushFor(compositor, colors.Marks);

            for (auto const side : { 0, 1 })
            {
                auto notch = compositor.CreateLineGeometry();

                if (vertical)
                {
                    auto const y = drumY + at;
                    auto const x = side == 0 ? slotX - inset * 0.8f : slotX + slotW + inset * 0.2f;

                    notch.Start(float2{ x, y });
                    notch.End(float2{ x + (inset * 0.6f), y });
                }
                else
                {
                    auto const x = drumX + (length - at);
                    auto const y = side == 0 ? slotY - inset * 0.8f : slotY + slotH + inset * 0.2f;

                    notch.Start(float2{ x, y });
                    notch.End(float2{ x, y + (inset * 0.6f) });
                }

                auto notchShape = compositor.CreateSpriteShape(notch);
                notchShape.StrokeBrush(markBrush);
                notchShape.StrokeThickness(1.5f);

                visual.Shape.Shapes().Append(notchShape);
            }
        }

        visual.ArcGeometry = nullptr;
    }

    // ---------------------------------------------------------------- the switch

    // One slice of the face per position, in a well, with the chosen one lit in the control's
    // own color. The names are XAML text laid over the slices by LayoutSwitchLabels.
    _Use_decl_annotations_
    void SurfaceRenderer::LayoutSwitch(
        Compositor const& compositor,
        SurfaceVisual& visual,
        Control const& control,
        ControlColors const& colors,
        float width,
        float height)
    {
        auto const count = SwitchPositionCount(control);
        auto const across = width >= height;

        visual.SwitchPositions = count;
        visual.SwitchSegments.clear();
        visual.SwitchCells.clear();

        auto const fieldX = FieldInset;
        auto const fieldY = FieldInset;
        auto const fieldW = std::max(width - (FieldInset * 2.0f), 4.0f);
        auto const fieldH = std::max(height - (FieldInset * 2.0f), 4.0f);

        AppendWell(compositor, visual, colors, fieldX, fieldY, fieldW, fieldH);

        constexpr float gap = 3.0f;

        auto const length = across ? fieldW : fieldH;
        auto const slice = std::max((length - (gap * static_cast<float>(count + 1))) / static_cast<float>(count), 2.0f);
        auto const thickness = std::max((across ? fieldH : fieldW) - (gap * 2.0f), 2.0f);
        auto const corner = std::min(4.0f, std::min(slice, thickness) * 0.25f);

        auto rest = colors.Marks;
        rest.A = static_cast<uint8_t>(std::min<int>(rest.A, 44));

        visual.SwitchRestFill = BrushFor(compositor, rest);
        visual.SwitchLitFill = BrushFor(compositor, colors.Pipe);
        visual.SwitchRestInk = ReadableInk(colors.Track);
        visual.SwitchLitInk = ReadableInk(colors.Pipe);

        for (int32_t position = 0; position < count; ++position)
        {
            auto const start = gap + (static_cast<float>(position) * (slice + gap));

            auto const x = across ? fieldX + start : fieldX + gap;
            auto const y = across ? fieldY + gap : fieldY + start;
            auto const w = across ? slice : thickness;
            auto const h = across ? thickness : slice;

            auto geometry = compositor.CreateRoundedRectangleGeometry();
            geometry.Size(float2{ w, h });
            geometry.Offset(float2{ x, y });
            geometry.CornerRadius(float2{ corner, corner });

            auto segment = compositor.CreateSpriteShape(geometry);
            segment.FillBrush(visual.SwitchRestFill);

            visual.ValueShape.Shapes().Append(segment);
            visual.SwitchSegments.push_back(segment);
            visual.SwitchCells.push_back({ x, y, w, h });
        }

        visual.ArcGeometry = nullptr;
    }

    // ---------------------------------------------------------------- the step sequencer

    // A slot per step, in a well, with a bar in each step that plays, as tall as it plays hard.
    // A rest is an empty slot. The slot on each beat is a little brighter, so a pattern can be
    // counted by eye, and the step being played is lit in the control's own color.
    _Use_decl_annotations_
    void SurfaceRenderer::LayoutSteps(
        Compositor const& compositor,
        SurfaceVisual& visual,
        Control const& control,
        ControlColors const& colors,
        Theme const& theme,
        float width,
        float height)
    {
        auto const count = SequencerStepCount(control.Steps);

        visual.StepSlots.clear();
        visual.StepBars.clear();
        visual.StepSlotRestFills.clear();
        visual.CurrentStep = -1;
        visual.ArcGeometry = nullptr;

        auto const fieldX = FieldInset;
        auto const fieldY = FieldInset;
        auto const fieldW = std::max(width - (FieldInset * 2.0f), 4.0f);
        auto const fieldH = std::max(height - (FieldInset * 2.0f), 4.0f);

        AppendWell(compositor, visual, colors, fieldX, fieldY, fieldW, fieldH);

        if (count == 0)
        {
            return;
        }

        // Rows of up to sixteen, the way the step buttons on a drum machine wrap.
        constexpr int32_t StepsPerRow = 16;
        constexpr float gap = 3.0f;

        auto const rows = (count + StepsPerRow - 1) / StepsPerRow;
        auto const columns = (count + rows - 1) / rows;

        auto const cellW = std::max((fieldW - (gap * static_cast<float>(columns + 1))) / static_cast<float>(columns), 2.0f);
        auto const cellH = std::max((fieldH - (gap * static_cast<float>(rows + 1))) / static_cast<float>(rows), 2.0f);
        auto const corner = std::min(3.0f, std::min(cellW, cellH) * 0.2f);
        auto const inset = std::min(2.0f, cellW * 0.15f);

        auto slot = colors.Marks;
        slot.A = static_cast<uint8_t>(std::min<int>(slot.A, 34));

        auto beat = colors.Marks;
        beat.A = static_cast<uint8_t>(std::min<int>(beat.A, 62));

        auto const stepValue = visual.Windowed ? colors.WellValue : colors.Pipe;

        auto litSlot = stepValue;
        litSlot.A = static_cast<uint8_t>(std::lround(litSlot.A * 0.45));

        // Dimmer at rest than the pipe on other controls, so the step being played stands out
        // from across a room.
        auto restBar = stepValue;
        restBar.A = static_cast<uint8_t>(std::lround(restBar.A * 0.40));

        auto const slotBrush = BrushFor(compositor, slot);
        auto const beatBrush = BrushFor(compositor, beat);

        visual.StepSlotLitFill = BrushFor(compositor, litSlot);
        visual.StepBarRestFill = BrushFor(compositor, restBar);
        visual.StepBarLitFill = BrushFor(compositor, stepValue);

        // Triplets count in threes; anything slower than a step a beat puts every step on one.
        auto const perBeat = std::max(1, static_cast<int32_t>(std::lround(control.Steps.StepsPerBeat)));

        for (int32_t index = 0; index < count; ++index)
        {
            auto const x = fieldX + gap + (static_cast<float>(index % columns) * (cellW + gap));
            auto const y = fieldY + gap + (static_cast<float>(index / columns) * (cellH + gap));

            auto slotGeometry = compositor.CreateRoundedRectangleGeometry();
            slotGeometry.Size(float2{ cellW, cellH });
            slotGeometry.Offset(float2{ x, y });
            slotGeometry.CornerRadius(float2{ corner, corner });

            auto const restFill = (index % perBeat) == 0 ? beatBrush : slotBrush;

            auto slotShape = compositor.CreateSpriteShape(slotGeometry);
            slotShape.FillBrush(restFill);

            visual.ValueShape.Shapes().Append(slotShape);
            visual.StepSlots.push_back(slotShape);
            visual.StepSlotRestFills.push_back(restFill);

            auto const& step = control.Steps.Pattern[static_cast<size_t>(index)];

            CompositionSpriteShape barShape{ nullptr };

            if (step.On)
            {
                // Never so short it vanishes: a quiet step still plays.
                auto const velocity = static_cast<float>(std::clamp(step.Velocity, 0.0, 1.0));
                auto const barW = std::max(cellW - (inset * 2.0f), 1.0f);
                auto const barH = std::max((cellH - (inset * 2.0f)) * (0.15f + (0.85f * velocity)), 1.0f);

                auto barGeometry = compositor.CreateRoundedRectangleGeometry();
                barGeometry.Size(float2{ barW, barH });
                barGeometry.Offset(float2{ x + inset, y + cellH - inset - barH });
                barGeometry.CornerRadius(float2{ std::min(corner, barW * 0.3f), std::min(corner, barW * 0.3f) });

                barShape = compositor.CreateSpriteShape(barGeometry);
                barShape.FillBrush(visual.StepBarRestFill);

                visual.ValueShape.Shapes().Append(barShape);
            }

            visual.StepBars.push_back(barShape);
            visual.StepOrigins.push_back(float2{ x, y });
        }

        // The step that is playing gets a dotted line round it as well, the way a focused control
        // is marked. It waits, unpainted, until a step plays.
        if (theme.CurrentStep == CurrentStepStyle::DottedFocus)
        {
            visual.StepFocusGap = StepFocusOffset;
            visual.StepFocus = compositor.CreateRoundedRectangleGeometry();
            visual.StepFocus.Size(float2{ cellW + StepFocusOffset * 2.0f, cellH + StepFocusOffset * 2.0f });
            visual.StepFocus.Offset(float2{ -1000.0f, -1000.0f });

            auto ink = colors.Label;
            ink.A = 255;

            auto focusShape = compositor.CreateSpriteShape(visual.StepFocus);
            focusShape.StrokeBrush(BrushFor(compositor, ink));
            focusShape.StrokeThickness(1.0f);
            focusShape.StrokeDashArray().Append(1.0f);
            focusShape.StrokeDashArray().Append(1.0f);

            visual.ValueShape.Shapes().Append(focusShape);
        }
    }

    _Use_decl_annotations_
    void SurfaceRenderer::SetCurrentStep(size_t itemIndex, int32_t stepIndex) noexcept
    {
        if (itemIndex >= m_visuals.size())
        {
            return;
        }

        auto& visual = m_visuals[itemIndex];

        if (visual.Kind != ControlKind::Steps || visual.StepSlots.empty() || visual.CurrentStep == stepIndex)
        {
            return;
        }

        try
        {
            auto const paint = [&visual](int32_t index, bool lit)
                {
                    if (index < 0 || static_cast<size_t>(index) >= visual.StepSlots.size())
                    {
                        return;
                    }

                    auto const at = static_cast<size_t>(index);

                    visual.StepSlots[at].FillBrush(lit ? visual.StepSlotLitFill : visual.StepSlotRestFills[at]);

                    if (auto const& bar = visual.StepBars[at])
                    {
                        bar.FillBrush(lit ? visual.StepBarLitFill : visual.StepBarRestFill);
                    }
                };

            paint(visual.CurrentStep, false);
            paint(stepIndex, true);

            visual.CurrentStep = stepIndex;

            if (visual.StepFocus != nullptr)
            {
                auto const valid = stepIndex >= 0 && static_cast<size_t>(stepIndex) < visual.StepOrigins.size();
                auto const origin = valid ? visual.StepOrigins[static_cast<size_t>(stepIndex)] : float2{ -1000.0f, -1000.0f };

                visual.StepFocus.Offset(float2{ origin.x - visual.StepFocusGap, origin.y - visual.StepFocusGap });
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void SurfaceRenderer::LayoutSwitchLabels(size_t itemIndex, Control const& control)
    {
        if (itemIndex >= m_padNames.size() || itemIndex >= m_visuals.size() || m_host == nullptr)
        {
            return;
        }

        auto& names = m_padNames[itemIndex];
        auto const& visual = m_visuals[itemIndex];

        if (visual.SwitchCells.empty())
        {
            return;
        }

        try
        {
            controls::Canvas host{};

            host.IsHitTestVisible(false);
            host.Width(std::max(control.Width, 4.0));
            host.Height(std::max(control.Height, 4.0));

            // The switch is one control to a screen reader, which reads the chosen position as
            // its value. Every name read out again would be noise.
            xaml::Automation::AutomationProperties::SetAccessibilityView(
                host, xaml::Automation::Peers::AccessibilityView::Raw);

            media::Brush const restInk = media::SolidColorBrush{ ToWindowsColor(visual.SwitchRestInk) };
            media::Brush const litInk = media::SolidColorBrush{ ToWindowsColor(visual.SwitchLitInk) };

            auto const chosen = static_cast<size_t>(SwitchPositionAt(
                itemIndex < m_values.size() ? m_values[itemIndex] : 0.0, visual.SwitchPositions));

            auto const unbounded = winrt::Windows::Foundation::Size{
                std::numeric_limits<float>::infinity(),
                std::numeric_limits<float>::infinity() };

            for (size_t position = 0; position < visual.SwitchCells.size(); ++position)
            {
                auto const& cell = visual.SwitchCells[position];

                auto const name = position < control.Switch.Positions.size()
                    ? control.Switch.Positions[position]
                    : std::to_wstring(position + 1);

                controls::TextBlock text{};

                text.Text(winrt::hstring{ name });
                text.FontSize(std::clamp(cell.w * 0.42f, 9.0f, 16.0f));
                text.FontFamily(media::FontFamily{ L"Segoe UI Variable Text" });
                text.FontWeight(winrt::Windows::UI::Text::FontWeight{ 600 });
                text.TextAlignment(xaml::TextAlignment::Center);
                text.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
                text.MaxLines(1);
                text.Width(std::max(cell.z - 4.0f, 1.0f));
                text.IsHitTestVisible(false);
                text.Foreground(position == chosen ? litInk : restInk);

                text.Measure(unbounded);

                controls::Canvas::SetLeft(text, cell.x + 2.0f);
                controls::Canvas::SetTop(text, cell.y + ((cell.w - text.DesiredSize().Height) * 0.5f));

                host.Children().Append(text);

                names.Texts.push_back(text);
                names.RestInks.push_back(restInk);
                names.LitInks.push_back(litInk);
            }

            controls::Canvas::SetLeft(host, control.X);
            controls::Canvas::SetTop(host, control.Y);

            m_host.Children().Append(host);
            names.Host = host;
        }
        catch (...)
        {
            names = PadNameTexts{};
        }
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

        // Inside a window a value keeps its own color, even on a theme that draws every other
        // value in one.
        auto const lit = visual.Windowed ? colors.WellValue : colors.Pipe;

        auto const withStrength = [](ThemeColor color, double strength) noexcept
            {
                color.A = static_cast<uint8_t>(std::lround(255.0 * std::clamp(strength, 0.0, 1.0)));

                return color;
            };

        auto const fieldX = FieldInset;
        auto const fieldY = FieldInset;
        auto const fieldW = std::max(width - FieldInset * 2.0f, 1.0f);
        auto const fieldH = std::max(height - FieldInset * 2.0f, 1.0f);

        // A pad's field is a display, so it is sunk. A joystick's is round and ringed, and a
        // square well behind it would line up with nothing.
        if (!joystick)
        {
            AppendWell(compositor, visual, colors, fieldX, fieldY, fieldW, fieldH);
        }

        auto const puckFraction = theme.Puck == PuckStyle::Ball && joystick
            ? JoystickBallFraction
            : (theme.Puck == PuckStyle::Reticle ? ReticleFraction : (joystick ? JoystickPuckFraction : PuckFraction));

        auto const puck = std::max(MinimumPuckSize, std::min(width, height) * puckFraction);

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

        // The grid. It says where the middle and the quarters are, so a hand can find a
        // position rather than only the two ends. A joystick goes without: its rings already
        // say where the middle is, and a square grid on a round field lines up with nothing.
        if (!joystick && control.Ticks.Show && control.Ticks.Count >= MinimumTickCount)
        {
            auto const divisions = control.Ticks.Count - 1;

            auto lineClip = compositor.CreateRoundedRectangleGeometry();
            lineClip.Size(float2{ fieldW, fieldH });
            lineClip.Offset(float2{ fieldX, fieldY });
            lineClip.CornerRadius(
                float2{ static_cast<float>(theme.CornerRadius), static_cast<float>(theme.CornerRadius) });

            // The grid has to stop at the plate's rounded corner, not at its bounding box.
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
                    shape.FillBrush(BrushFor(compositor, visual.Windowed ? withStrength(lit, WindowGridStrength) : colors.Marks));

                    visual.Grid.Shapes().Append(shape);
                }
            }

            visual.Root.Children().InsertBelow(visual.Grid, visual.ValueShape);
        }

        // The crosshair. An XY pad has one because the value has to be readable from across a
        // room; a joystick's rings already do that job, so it goes without.
        if (!joystick)
        {
            auto crossColor = lit;
            crossColor.A = visual.Windowed
                ? static_cast<uint8_t>(std::lround(255.0 * WindowCrossStrength))
                : static_cast<uint8_t>(std::lround(crossColor.A * 0.38));

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

        // A ball or a reticle, drawn round the origin and moved as one group. A joystick's ball
        // rides a short shaft from the middle of the field.
        if (theme.Puck != PuckStyle::Disc)
        {
            if (joystick && theme.Puck == PuckStyle::Ball)
            {
                auto const middle = float2{ width * 0.5f, height * 0.5f };

                visual.Shaft = compositor.CreateLineGeometry();
                visual.Shaft.Start(middle);
                visual.Shaft.End(middle);

                auto shaftShape = compositor.CreateSpriteShape(visual.Shaft);
                shaftShape.StrokeBrush(BrushFor(compositor, ThemeColor{ 0xB8, 0xBA, 0xC0, 255 }));
                shaftShape.StrokeThickness(std::max(3.0f, puck * ShaftFraction));
                shaftShape.StrokeStartCap(CompositionStrokeCap::Round);
                shaftShape.StrokeEndCap(CompositionStrokeCap::Round);

                visual.ValueShape.Shapes().Append(shaftShape);
            }

            visual.PuckGroup = BuildPuckGroup(compositor, theme, lit, visual.PuckRadius);
            visual.PuckGroup.Offset(float2{ visual.FieldX, visual.FieldY });
            visual.PuckGeometry = nullptr;

            visual.ValueShape.Shapes().Append(visual.PuckGroup);

            if (joystick)
            {
                visual.CrossAcross = nullptr;
                visual.CrossDown = nullptr;
                visual.ArcGeometry = nullptr;
                visual.SweepGeometry = nullptr;
            }

            return;
        }

        // The puck. On a joystick it is a cap with a dot of hue in it, the way a real stick has
        // a molded top; on a pad it is the light itself.
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
            // On a theme whose lights have a white hot heart, the puck is white in the middle
            // and its own color at the edge, like a lamp seen head on.
            if (colors.ValueCore.A != 0)
            {
                auto hot = compositor.CreateRadialGradientBrush();
                hot.EllipseCenter(float2{ 0.5f, 0.5f });
                hot.EllipseRadius(float2{ 0.5f, 0.5f });

                auto rim = lit;
                rim.A = static_cast<uint8_t>(std::lround(lit.A * 0.60));

                for (auto const& [offset, color] : {
                    std::pair{ 0.0f, ThemeColor{ 255, 255, 255, 255 } },
                    std::pair{ 0.28f, ThemeColor{ 255, 255, 255, 255 } },
                    std::pair{ 0.46f, colors.ValueCore },
                    std::pair{ 0.72f, lit },
                    std::pair{ 1.0f, rim } })
                {
                    auto stop = compositor.CreateColorGradientStop();
                    stop.Offset(offset);
                    stop.Color(ToWindowsColor(color));

                    hot.ColorStops().Append(stop);
                }

                puckShape.FillBrush(hot);
            }
            else
            {
                puckShape.FillBrush(BrushFor(compositor, lit));
            }

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

                    auto glow = lit;
                    glow.A = static_cast<uint8_t>(std::clamp(std::lround(255.0 * step), 0L, 255L));

                    auto glowShape = compositor.CreateSpriteShape(visual.PuckGeometry);
                    glowShape.StrokeBrush(BrushFor(compositor, glow));
                    glowShape.StrokeThickness(static_cast<float>(puck * 1.3 * t));

                    visual.ValueShape.Shapes().Append(glowShape);
                }
            }

            visual.ValueShape.Shapes().Append(puckShape);

            // The puck is the biggest light on a page, so it throws the whole flare: a streak,
            // a ring and a faint star, riding with it.
            if (colors.Flare.A != 0 && theme.FlarePercent > 0)
            {
                visual.Flare = BuildFlare(compositor, colors, theme, visual.PuckRadius, true);

                if (visual.Flare != nullptr)
                {
                    visual.Flare.Offset(float3{ visual.FieldX, visual.FieldY, 0.0f });
                    visual.Root.Children().InsertAbove(visual.Flare, visual.ValueShape);
                }
            }
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

        // The strip the light runs along, sunk into the plate.
        AppendWell(compositor, visual, colors,
            inset, inset, std::max(width - inset * 2.0f, 1.0f), std::max(height - inset * 2.0f, 1.0f));

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

        // The theme's own light and dark, unless this keyboard names its own. The naturals are
        // outlined in the dark at a low strength, because on a light theme a light key against a
        // light deck has no edge of its own.
        auto const white = ColorOr(spec.WhiteKeyColor, colors.KeyWhite);
        auto const black = ColorOr(spec.BlackKeyColor, colors.KeyBlack);

        auto outline = black;
        outline.A = static_cast<uint8_t>(std::lround(black.A * KeyOutlineStrength));

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

                if (!blacks)
                {
                    shape.StrokeBrush(BrushFor(compositor, outline));
                    shape.StrokeThickness(1.0f);
                }

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

        auto const clockValue = visual.Windowed ? colors.WellValue : colors.Pipe;

        auto sweepShape = compositor.CreateSpriteShape(visual.SweepGeometry);
        sweepShape.StrokeBrush(BrushFor(compositor, clockValue));
        sweepShape.StrokeThickness(ClockRingThickness);

        visual.ValueShape.Shapes().Append(sweepShape);

        // The disc inside it, which is what flashes on the beat. Barely tinted while the clock
        // is stopped: a clock that looks lit when it is not sending is the one thing this
        // control must never do.
        auto beatGeometry = compositor.CreateEllipseGeometry();
        auto const beatRadius = std::max(radius - radius * 2.0f * ClockBeatInsetFraction, 2.0f);

        beatGeometry.Radius(float2{ beatRadius, beatRadius });
        beatGeometry.Center(center);

        auto beatDim = clockValue;
        beatDim.A = static_cast<uint8_t>(std::lround(beatDim.A * 0.07));

        auto beatLit = clockValue;
        beatLit.A = static_cast<uint8_t>(std::lround(beatLit.A * 0.55));

        visual.BeatOffBrush = BrushFor(compositor, beatDim).as<CompositionBrush>();
        visual.BeatOnBrush = BrushFor(compositor, beatLit).as<CompositionBrush>();

        visual.BeatShape = compositor.CreateSpriteShape(beatGeometry);
        visual.BeatShape.FillBrush(visual.BeatOffBrush);
        visual.BeatShape.StrokeBrush(BrushFor(compositor, colors.Rim));
        visual.BeatShape.StrokeThickness(1.0f);

        visual.ValueShape.Shapes().Append(visual.BeatShape);

        // Four pips under it, one per beat in the bar.
        auto pipLit = clockValue;

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

        if (visual.PuckGeometry == nullptr && visual.PuckGroup == nullptr)
        {
            return;
        }

        // An LFO borrows the puck geometry for its bead, but its bead is placed by the sweep,
        // not by its value - which is whether it is running. Without this every stopped LFO is
        // parked in the bottom left corner, because that is where a value of zero on both axes
        // puts a puck.
        if (visual.Kind == ControlKind::Lfo)
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

            if (visual.PuckGeometry != nullptr)
            {
                visual.PuckGeometry.Center(at);
            }

            if (visual.PuckGroup != nullptr)
            {
                visual.PuckGroup.Offset(at);
            }

            if (visual.Shaft != nullptr)
            {
                visual.Shaft.End(at);
            }

            if (visual.Flare != nullptr)
            {
                visual.Flare.Offset(float3{ at.x, at.y, 0.0f });
            }

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
        RefreshValueText(itemIndex);
    }

    _Use_decl_annotations_
    void SurfaceRenderer::SetSweepPosition(
        size_t itemIndex,
        double value,
        double phase,
        bool running) noexcept
    {
        if (itemIndex >= m_visuals.size())
        {
            return;
        }

        auto const& visual = m_visuals[itemIndex];

        if (visual.Kind != ControlKind::Lfo || visual.PuckGeometry == nullptr)
        {
            return;
        }

        try
        {
            auto const across = static_cast<float>(std::clamp(phase, 0.0, 1.0));
            auto const up = static_cast<float>(std::clamp(value, 0.0, 1.0));

            visual.PuckGeometry.Center(float2{
                visual.FieldX + across * visual.FieldWidth,
                visual.FieldY + (1.0f - up) * visual.FieldHeight });

            if (visual.ValueShape != nullptr)
            {
                visual.ValueShape.Opacity(running ? 1.0f : LfoRestOpacity);
            }
        }
        catch (...)
        {
        }
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

    // ------------------------------------------------------- elapsed time

    namespace
    {
        // Hours only once there are any. A stopwatch that reads 00:00:04.2 for the first hour
        // of its life is four characters of noise.
        std::wstring FormatElapsed(_In_ uint64_t milliseconds) noexcept
        {
            auto const tenths = (milliseconds / 100) % 10;
            auto const seconds = (milliseconds / 1000) % 60;
            auto const minutes = (milliseconds / 60000) % 60;
            auto const hours = milliseconds / 3600000;

            wchar_t text[32]{};

            if (hours > 0)
            {
                swprintf_s(text, L"%llu:%02llu:%02llu.%llu", hours, minutes, seconds, tenths);
            }
            else
            {
                swprintf_s(text, L"%llu:%02llu.%llu", minutes, seconds, tenths);
            }

            return text;
        }
    }

    _Use_decl_annotations_
    void SurfaceRenderer::LayoutElapsedText(
        size_t itemIndex,
        Control const& control,
        Theme const& theme)
    {
        if (itemIndex >= m_elapsedTexts.size())
        {
            return;
        }

        if (m_elapsedTexts[itemIndex] != nullptr)
        {
            uint32_t index{ 0 };

            if (m_host != nullptr && m_host.Children().IndexOf(m_elapsedTexts[itemIndex], index))
            {
                m_host.Children().RemoveAt(index);
            }

            m_elapsedTexts[itemIndex] = nullptr;
        }

        m_elapsedOrigins[itemIndex] = 0;

        if (control.Kind != ControlKind::TimeDisplay)
        {
            return;
        }

        try
        {
            auto const width = std::max(control.Width, 4.0);
            auto const height = std::max(control.Height, 4.0);

            auto const colors = ResolveControlColors(control, theme);

            // In a window the time is the window's own light, not an ink meant for the page.
            auto const windowed = itemIndex < m_visuals.size() && m_visuals[itemIndex].Windowed;

            controls::TextBlock text{};

            // Monospace, or every tenth of a second shuffles the digits sideways.
            text.FontFamily(media::FontFamily{ L"Cascadia Mono, Consolas" });
            text.FontSize(std::clamp(height * 0.42, 11.0, 48.0));
            text.Foreground(media::SolidColorBrush{ ToWindowsColor(windowed ? colors.WellInk : colors.Pipe) });
            text.IsHitTestVisible(false);
            text.TextAlignment(xaml::TextAlignment::Center);
            text.Width(width);
            text.Text(winrt::hstring{ FormatElapsed(0) });

            xaml::Automation::AutomationProperties::SetAccessibilityView(
                text, xaml::Automation::Peers::AccessibilityView::Raw);

            text.Measure(winrt::Windows::Foundation::Size{
                static_cast<float>(width), std::numeric_limits<float>::infinity() });

            auto const offset = (height - text.DesiredSize().Height) * 0.5;

            m_elapsedTextOffsets[itemIndex] = offset;
            m_elapsedOrigins[itemIndex] = ::GetTickCount64();

            controls::Canvas::SetLeft(text, control.X);
            controls::Canvas::SetTop(text, control.Y + offset);

            m_host.Children().Append(text);
            m_elapsedTexts[itemIndex] = text;

            StartElapsedTimerIfNeeded();
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void SurfaceRenderer::ResetElapsed(size_t itemIndex) noexcept
    {
        if (itemIndex >= m_elapsedOrigins.size() || m_elapsedOrigins[itemIndex] == 0)
        {
            return;
        }

        m_elapsedOrigins[itemIndex] = ::GetTickCount64();

        RefreshElapsedTexts();
    }

    _Use_decl_annotations_
    void SurfaceRenderer::SetElapsedRunning(bool running) noexcept
    {
        if (m_elapsedRunning == running)
        {
            return;
        }

        m_elapsedRunning = running;

        if (running)
        {
            // From zero, not from where it would have been. Nobody wants the stopwatch to
            // start a run already showing four minutes of editing.
            auto const now = ::GetTickCount64();

            for (auto& origin : m_elapsedOrigins)
            {
                if (origin != 0)
                {
                    origin = now;
                }
            }

            RefreshElapsedTexts();
            StartElapsedTimerIfNeeded();

            return;
        }

        if (m_elapsedTimer != nullptr)
        {
            m_elapsedTimer.Stop();
            m_elapsedTimer = nullptr;
        }

        try
        {
            for (auto const& text : m_elapsedTexts)
            {
                if (text != nullptr)
                {
                    text.Text(winrt::hstring{ FormatElapsed(0) });
                }
            }
        }
        catch (...)
        {
        }
    }

    void SurfaceRenderer::RefreshElapsedTexts() noexcept
    {
        if (!m_elapsedRunning)
        {
            return;
        }

        try
        {
            auto const now = ::GetTickCount64();

            for (size_t index = 0; index < m_elapsedTexts.size(); ++index)
            {
                if (m_elapsedTexts[index] == nullptr || m_elapsedOrigins[index] == 0)
                {
                    continue;
                }

                m_elapsedTexts[index].Text(winrt::hstring{
                    FormatElapsed(now - m_elapsedOrigins[index]) });
            }
        }
        catch (...)
        {
        }
    }

    void SurfaceRenderer::StartElapsedTimerIfNeeded()
    {
        try
        {
            if (m_elapsedTimer != nullptr || m_host == nullptr || !m_elapsedRunning)
            {
                return;
            }

            auto const queue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

            if (queue == nullptr)
            {
                return;
            }

            m_elapsedTimer = queue.CreateTimer();

            // Tenths are what is shown, so anything faster is work nobody can see. A page with
            // no time display on it never gets here at all.
            m_elapsedTimer.Interval(std::chrono::milliseconds{ 50 });

            m_elapsedTimer.Tick([this](auto&&, auto&&) { RefreshElapsedTexts(); });

            m_elapsedTimer.Start();
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

        if (m_tempoTexts[itemIndex] != nullptr)
        {
            uint32_t index{ 0 };

            if (m_host != nullptr && m_host.Children().IndexOf(m_tempoTexts[itemIndex], index))
            {
                m_host.Children().RemoveAt(index);
            }

            m_tempoTexts[itemIndex] = nullptr;
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

            // Printed on the page, or lit inside a window where the theme makes one.
            auto const windowed = itemIndex < m_visuals.size() && m_visuals[itemIndex].Windowed;
            auto const behind = windowed ? theme.WellColor : DeckColor();

            controls::TextBlock text{};

            // Monospace, so the number does not shuffle sideways every beat.
            text.FontFamily(media::FontFamily{ L"Cascadia Mono, Consolas" });
            text.FontSize(std::clamp(std::min(width, usable) * 0.18, 9.0, 20.0));
            text.Foreground(media::SolidColorBrush{ ToWindowsColor(ReadableInk(behind)) });
            text.IsHitTestVisible(false);
            text.TextAlignment(xaml::TextAlignment::Center);
            text.Width(width);
            text.Text(L"1");

            xaml::Automation::AutomationProperties::SetAccessibilityView(
                text, xaml::Automation::Peers::AccessibilityView::Raw);

            text.Measure(winrt::Windows::Foundation::Size{
                static_cast<float>(width), std::numeric_limits<float>::infinity() });

            auto const offset = usable * 0.5 - text.DesiredSize().Height * 0.5;

            m_beatTextOffsets[itemIndex] = offset;

            controls::Canvas::SetLeft(text, control.X);
            controls::Canvas::SetTop(text, control.Y + offset);

            m_host.Children().Append(text);
            m_beatTexts[itemIndex] = text;

            // The tempo, under the ring and above the pips. "How fast is this going" is the
            // question somebody asks of a clock, and the beat number does not answer it.
            controls::TextBlock tempo{};

            tempo.FontFamily(media::FontFamily{ L"Cascadia Mono, Consolas" });
            tempo.FontSize(std::clamp(width * 0.10, 8.0, 12.0));
            tempo.Foreground(media::SolidColorBrush{ ToWindowsColor(ReadableInk(behind)) });
            tempo.Opacity(0.7);
            tempo.IsHitTestVisible(false);
            tempo.TextAlignment(xaml::TextAlignment::Center);
            tempo.Width(width);
            tempo.Text(L"");

            xaml::Automation::AutomationProperties::SetAccessibilityView(
                tempo, xaml::Automation::Peers::AccessibilityView::Raw);

            tempo.Measure(winrt::Windows::Foundation::Size{
                static_cast<float>(width), std::numeric_limits<float>::infinity() });

            auto const tempoOffset = usable - tempo.DesiredSize().Height * 0.5;

            m_tempoTextOffsets[itemIndex] = tempoOffset;

            controls::Canvas::SetLeft(tempo, control.X);
            controls::Canvas::SetTop(tempo, control.Y + tempoOffset);

            m_host.Children().Append(tempo);
            m_tempoTexts[itemIndex] = tempo;

            // Show the configured tempo straight away. A clock that reads nothing until it is
            // started looks broken, and the number is the first thing a designer wants to check.
            SetClockTempo(itemIndex, control.Clock.BeatsPerMinute);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void SurfaceRenderer::SetClockTempo(size_t itemIndex, double beatsPerMinute) noexcept
    {
        if (itemIndex >= m_tempoTexts.size() || m_tempoTexts[itemIndex] == nullptr)
        {
            return;
        }

        try
        {
            if (beatsPerMinute <= 0.0)
            {
                m_tempoTexts[itemIndex].Text(L"");
                return;
            }

            wchar_t text[16]{};

            // Whole numbers unless the tempo is riding a knob, where a tenth is the difference
            // between two positions somebody can hear.
            if (std::abs(beatsPerMinute - std::round(beatsPerMinute)) < 0.05)
            {
                swprintf_s(text, L"%d BPM", static_cast<int32_t>(std::lround(beatsPerMinute)));
            }
            else
            {
                swprintf_s(text, L"%.1f BPM", beatsPerMinute);
            }

            m_tempoTexts[itemIndex].Text(text);
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

    namespace
    {
        // Puts the content at an exact size and position inside the clipped container, so that
        // the point of the source the customer named lands in the middle of the control.
        //
        // The size is computed rather than left to Stretch because cropping needs to know where
        // the edges of the source ended up, and Stretch does not say.
        void ArrangePictureContent(
            _In_ xaml::FrameworkElement const& content,
            _In_ Picture const& picture,
            _In_ double containerWidth,
            _In_ double containerHeight,
            _In_ double naturalWidth,
            _In_ double naturalHeight)
        {
            auto const rect = PictureCropRect(
                picture, containerWidth, containerHeight, naturalWidth, naturalHeight);

            content.Width(rect.Width);
            content.Height(rect.Height);

            controls::Canvas::SetLeft(content, rect.X);
            controls::Canvas::SetTop(content, rect.Y);
        }
    }

    // The media player behind a video element, so it can be shut down. A player left open keeps
    // a decoder and its threads alive long after the page it was on has gone.
    _Use_decl_annotations_
    void SurfaceRenderer::ClosePicture(xaml::FrameworkElement const& element) noexcept
    {
        try
        {
            auto const container = element.try_as<controls::Canvas>();

            if (container == nullptr || container.Children().Size() == 0)
            {
                return;
            }

            auto const player =
                container.Children().GetAt(0).try_as<controls::MediaPlayerElement>();

            if (player == nullptr)
            {
                return;
            }

            auto const media = player.MediaPlayer();

            player.SetMediaPlayer(nullptr);

            if (media != nullptr)
            {
                media.Pause();
                media.Close();
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void SurfaceRenderer::LayoutPicture(size_t itemIndex, Control const& control)
    {
        if (itemIndex >= m_pictures.size())
        {
            return;
        }

        auto const drop = [&]()
            {
                if (itemIndex < m_videos.size() && m_videos[itemIndex] != nullptr)
                {
                    CloseVideo(*m_videos[itemIndex]);
                    m_videos[itemIndex] = nullptr;
                }

                if (m_pictures[itemIndex] == nullptr)
                {
                    return;
                }

                ClosePicture(m_pictures[itemIndex]);

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

            // The content is laid out oversized and the container does the cropping, so the
            // container is what gets clipped and what the rest of the renderer moves around.
            controls::Canvas container{};

            container.Width(width);
            container.Height(height);
            container.IsHitTestVisible(false);
            container.Opacity(std::clamp(control.Image.Opacity, 0.0, 1.0));

            media::RectangleGeometry clip{};
            clip.Rect(winrt::Windows::Foundation::Rect{
                0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) });
            container.Clip(clip);

            xaml::Automation::AutomationProperties::SetAccessibilityView(
                container, xaml::Automation::Peers::AccessibilityView::Raw);

            xaml::FrameworkElement content{ nullptr };
            std::shared_ptr<SurfaceVideo> video{};

            auto const picture = control.Image;

            if (IsVideoFileName(control.Image.FileName))
            {
                // Clicks and the bar belong to an image control. A panel's fill sits behind the
                // controls on the panel, so it only plays.
                video = CreateVideo(uri, picture, width, height, control.Kind == ControlKind::Image);

                if (control.Kind == ControlKind::Image && itemIndex < m_elements.size())
                {
                    video->Owner = m_elements[itemIndex];
                }

                content = video->Element;
            }
            else
            {
                content = BuildImageContent(uri, picture, width, height);
            }

            if (content == nullptr)
            {
                return;
            }

            // Everything is positioned in pixels by ArrangePictureContent, so the element must
            // not second-guess it.
            content.HorizontalAlignment(xaml::HorizontalAlignment::Left);
            content.VerticalAlignment(xaml::VerticalAlignment::Top);
            content.IsHitTestVisible(false);

            ArrangePictureContent(content, picture, width, height, 0.0, 0.0);

            container.Children().Append(content);

            // A wash over the top. A XAML rectangle rather than a composition effect: video in
            // WinUI 3 reaches the screen through the system compositor, not this app's, so an
            // effect brush cannot be put in front of it. An alpha overlay can.
            if (auto const wash = BuildPictureTint(picture, width, height); wash != nullptr)
            {
                container.Children().Append(wash);
            }

            // The bar goes over both, so it can always be seen.
            if (video != nullptr && itemIndex < m_videos.size())
            {
                m_videos[itemIndex] = video;
                LayoutScrubber(*video);
            }

            controls::Canvas::SetLeft(container, control.X);
            controls::Canvas::SetTop(container, control.Y);

            // A grouping panel's fill belongs behind the controls it frames; an image control
            // draws where it was placed like anything else.
            if (control.Kind == ControlKind::Panel)
            {
                controls::Canvas::SetZIndex(container, -1);
                m_host.Children().InsertAt(0, container);
            }
            else
            {
                m_host.Children().Append(container);
            }

            m_pictures[itemIndex] = container;
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    xaml::FrameworkElement SurfaceRenderer::BuildPictureTint(
        Picture const& picture,
        double width,
        double height)
    {
        auto const strength = std::clamp(picture.TintStrength, 0.0, 1.0);

        if (picture.TintColor.empty() || strength <= 0.0)
        {
            return nullptr;
        }

        ThemeColor parsed{};

        if (!TryParseColor(picture.TintColor, parsed))
        {
            return nullptr;
        }

        xaml::Shapes::Rectangle wash{};

        wash.Width(width);
        wash.Height(height);
        wash.Fill(media::SolidColorBrush{ ToWindowsColor(parsed) });
        wash.Opacity(strength);
        wash.IsHitTestVisible(false);

        xaml::Automation::AutomationProperties::SetAccessibilityView(
            wash, xaml::Automation::Peers::AccessibilityView::Raw);

        controls::Canvas::SetLeft(wash, 0.0);
        controls::Canvas::SetTop(wash, 0.0);

        return wash;
    }

    _Use_decl_annotations_
    xaml::FrameworkElement SurfaceRenderer::BuildImageContent(
        foundation::Uri const& uri,
        Picture const& picture,
        double width,
        double height)
    {
        controls::Image image{};

        image.Stretch(media::Stretch::Fill);

        auto const extension = std::filesystem::path{ picture.FileName }.extension().wstring();

        if (_wcsicmp(extension.c_str(), L".svg") == 0)
        {
            // An SVG has no pixels of its own, so it is rasterized straight into the box the
            // crop worked out. Zoom makes it sharper rather than blockier.
            auto const zoom = std::clamp(picture.Zoom, MinimumPictureZoom, MaximumPictureZoom);

            media::Imaging::SvgImageSource source{};

            source.UriSource(uri);
            source.RasterizePixelWidth(width * zoom);
            source.RasterizePixelHeight(height * zoom);

            image.Source(source);

            return image;
        }

        media::Imaging::BitmapImage bitmap{};

        bitmap.UriSource(uri);
        image.Source(bitmap);

        auto const weak = winrt::make_weak(image.as<xaml::FrameworkElement>());

        // Same story as the video: the real size of the file only turns up once it is decoded.
        // The size is read back off the element rather than captured, so the handler does not
        // hold the bitmap alive by pointing at the thing that raised it.
        image.ImageOpened([weak, picture, width, height](auto const&, auto const&)
            {
                auto const element = weak.get();

                if (element == nullptr)
                {
                    return;
                }

                auto const target = element.try_as<controls::Image>();

                if (target == nullptr)
                {
                    return;
                }

                auto const source = target.Source().try_as<media::Imaging::BitmapImage>();

                if (source == nullptr)
                {
                    return;
                }

                auto const naturalWidth = static_cast<double>(source.PixelWidth());
                auto const naturalHeight = static_cast<double>(source.PixelHeight());

                if (naturalWidth <= 0.0 || naturalHeight <= 0.0)
                {
                    return;
                }

                ArrangePictureContent(
                    element, picture, width, height, naturalWidth, naturalHeight);
            });

        return image;
    }
}
