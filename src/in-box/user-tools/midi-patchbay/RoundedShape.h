// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Every rounded surface the app draws is a shape kept off whole pixels. A Border rounds its corner
// radius to whole pixels along with its bounds, so at 125% scaling its corners come out stepped.
namespace midipatchbay
{
    // The radius is to the outside of the edge, as a Border's is. A shape centers its edge half a
    // line in from its bounds, so its own radius is that much smaller.
    inline void SetRoundedEdge(
        _In_ shapes::Rectangle const& shape,
        _In_ double radius,
        _In_ media::Brush const& stroke,
        _In_ double thickness)
    {
        auto const edge = stroke == nullptr ? 0.0 : thickness;
        auto const inside = (std::max)(0.0, radius - edge / 2);

        shape.RadiusX(inside);
        shape.RadiusY(inside);
        shape.Stroke(stroke);
        shape.StrokeThickness(edge);
    }

    inline shapes::Rectangle MakeRoundedShape(
        _In_ double radius,
        _In_ media::Brush const& fill,
        _In_ media::Brush const& stroke = nullptr,
        _In_ double thickness = 1.0)
    {
        shapes::Rectangle shape{};

        shape.UseLayoutRounding(false);
        shape.Fill(fill);
        SetRoundedEdge(shape, radius, stroke, thickness);

        return shape;
    }

    struct RoundedPanel
    {
        controls::Grid Panel{ nullptr };
        shapes::Rectangle Shape{ nullptr };
    };

    // Content on a rounded surface, inset as far as a Border's edge and padding would put it.
    inline RoundedPanel MakeRoundedPanel(
        _In_ double radius,
        _In_ media::Brush const& fill,
        _In_ media::Brush const& stroke,
        _In_ xaml::Thickness const& padding,
        _In_ xaml::UIElement const& content,
        _In_ double thickness = 1.0)
    {
        RoundedPanel result{};

        result.Panel = controls::Grid{};
        result.Shape = MakeRoundedShape(radius, fill, stroke, thickness);
        result.Panel.Children().Append(result.Shape);

        auto const edge = stroke == nullptr ? 0.0 : thickness;

        // Square, so it has no corners to step.
        controls::Border holder{};
        holder.Padding(xaml::ThicknessHelper::FromLengths(
            padding.Left + edge, padding.Top + edge, padding.Right + edge, padding.Bottom + edge));
        holder.Child(content);
        result.Panel.Children().Append(holder);

        return result;
    }
}
