// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The words a theme file is written in. They're CSS's, so a theme can be ported from a CSS comp
// almost as it's written:
//
//   colors       #RRGGBB, #RRGGBBAA, rgb(), rgba(), oklch(), oklab(), color(srgb ...),
//                color-mix(in srgb|srgb-linear|oklab|oklch, a 30%, b), and the relative forms
//                oklch(from <color> l c h / alpha) and rgb(from <color> r g b / alpha), with
//                calc(), min(), max() and clamp() in any channel
//   names        a color of the theme's own ("surface", "text2"), "tint" (the window color the
//                customer picked), "track" (the color of the track being drawn), "transparent",
//                "white" and "black"; and in a channel, a number of the theme's own ("glaze")
//   backgrounds  a color, linear-gradient(145deg, a, b 72%), radial-gradient(circle at 35% 30%,
//                a, b 72%), radial-gradient(130% 150% at 25% 15%, a, b), or several, comma separated
//   shadows      CSS box-shadow lists: [inset] x y [blur [spread]] color, ...
//   lines        borders and outlines: width [solid|dashed|dotted] color
//
// Parsing never throws. A mistake gives an error naming where it is, and the theme loader reports
// it with the theme and the property it was in.

#include <sal.h>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ThemeColor.h"

namespace themeengine
{
    // What a color expression can look names up in while it's worked out.
    class ExpressionScope
    {
    public:
        virtual ~ExpressionScope() = default;

        // A color of the theme's own, "tint" or "track". Empty when there's no such color.
        virtual std::optional<Color> NamedColor(_In_ std::wstring_view name) const noexcept = 0;

        // A number of the theme's own. Empty when there's no such number.
        virtual std::optional<double> NamedNumber(_In_ std::wstring_view name) const noexcept = 0;
    };

    struct ColorNode;

    class ColorExpression
    {
    public:
        // Transparent.
        ColorExpression() = default;

        static ColorExpression FromColor(_In_ Color const& color);

        // Empty when the text is a color; otherwise what's wrong with it.
        static std::optional<ColorExpression> Parse(_In_ std::wstring_view text, _Out_ std::wstring& error);

        // A name that can't be found counts as transparent, and a missing number as 0; the loader
        // checks every name before a theme is used, so that's only ever a fallback.
        Color Evaluate(_In_ ExpressionScope const& scope) const noexcept;

        // The color and number names it refers to, so the loader can check them and the resolver
        // can tell which colors depend on the track being drawn.
        void CollectNames(_Inout_ std::vector<std::wstring>& colors, _Inout_ std::vector<std::wstring>& numbers) const;

        bool RefersTo(_In_ std::wstring_view colorName) const;

        std::wstring const& Text() const noexcept { return m_text; }

    private:
        std::shared_ptr<ColorNode const> m_root{};
        std::wstring m_text{};

        friend ColorExpression MakeExpressionFromNode(std::shared_ptr<ColorNode const> root);
    };

    // ---- style values ----

    struct GradientStop
    {
        ColorExpression Color{};

        // 0 to 1 along the gradient; empty to space the stops evenly, as CSS does.
        std::optional<double> Position{};
    };

    enum class BackgroundKind : int32_t
    {
        None,
        Solid,
        Linear,
        Radial,
    };

    struct BackgroundLayer
    {
        BackgroundKind Kind{ BackgroundKind::None };
        ColorExpression Color{};

        // Linear: CSS degrees, so 0 points up, 90 right and 180 (the default) down.
        double Angle{ 180.0 };

        // Radial: the center and size as shares of the box. With no size, a circle reaches the
        // farthest corner and an ellipse fills the box to its farthest corner.
        double CenterX{ 0.5 };
        double CenterY{ 0.5 };
        bool Circle{ false };
        std::optional<double> SizeX{};
        std::optional<double> SizeY{};

        std::vector<GradientStop> Stops{};
    };

    struct ShadowLayer
    {
        bool Inset{ false };
        double X{ 0 };
        double Y{ 0 };
        double Blur{ 0 };
        double Spread{ 0 };
        ColorExpression Color{};

        // A shadow list can name one of the theme's shadows in place of a layer, as CSS would with
        // var(): "0 0 0 2px accent, clip-lift". The loader puts the named layers in its place.
        std::wstring Reference{};
    };

    enum class LineStyle : int32_t
    {
        None,
        Solid,
        Dashed,
        Dotted,
    };

    struct Line
    {
        double Width{ 0 };
        LineStyle Style{ LineStyle::None };
        ColorExpression Color{};
    };

    struct Corner
    {
        double Value{ 0 };
        bool Percent{ false };
    };

    // top left, top right, bottom right, bottom left, as CSS lists them
    using Corners = std::array<Corner, 4>;

    // top, right, bottom, left, as CSS lists them
    using Edges = std::array<double, 4>;

    struct FilterValue
    {
        double Saturate{ 1.0 };
        double Brightness{ 1.0 };

        friend bool operator==(FilterValue const&, FilterValue const&) = default;
    };

    // Each returns false and says why when the text isn't one.
    bool ParseBackground(_In_ std::wstring_view text, _Out_ std::vector<BackgroundLayer>& layers, _Out_ std::wstring& error);
    bool ParseShadows(_In_ std::wstring_view text, _Out_ std::vector<ShadowLayer>& shadows, _Out_ std::wstring& error);
    bool ParseLine(_In_ std::wstring_view text, _Out_ Line& line, _Out_ std::wstring& error);
    bool ParseCorners(_In_ std::wstring_view text, _Out_ Corners& corners, _Out_ std::wstring& error);
    bool ParseEdges(_In_ std::wstring_view text, _Out_ Edges& edges, _Out_ std::wstring& error);
    bool ParseLength(_In_ std::wstring_view text, _Out_ double& value, _Out_ std::wstring& error);
    bool ParseFilter(_In_ std::wstring_view text, _Out_ FilterValue& filter, _Out_ std::wstring& error);

    // A font weight as a number, or one of CSS's names for one ("semibold", "bold").
    bool ParseFontWeight(_In_ std::wstring_view text, _Out_ int32_t& weight, _Out_ std::wstring& error);

    // Whether the text is a single name (letters, digits, '-', '_' and '.'), such as a reference
    // to one of the theme's named shadows.
    bool IsName(_In_ std::wstring_view text) noexcept;
}
