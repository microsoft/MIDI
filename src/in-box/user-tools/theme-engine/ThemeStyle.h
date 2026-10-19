// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// How a part of the window looks: a small piece of CSS. A theme writes a part with CSS's own
// property names (background, border, border-radius, box-shadow, outline, opacity, filter, color,
// font-*, width, height, margin, padding), and an app draws it with whatever it draws with. A part
// can also carry values of its own for the app to read: a name ending in "color" is a color
// ("note-color": "track"), a number or a length is a number ("caption-height": 15), and anything
// else is a word ("cap-shape": "circle").

#include <functional>
#include <map>

#include "ThemeSyntax.h"

namespace themeengine
{
    // A part's look as a theme writes it. Every property is optional, so a state ("playing",
    // "selected") says only what it changes.
    struct StyleSpec
    {
        std::optional<std::vector<BackgroundLayer>> Background{};

        // top, right, bottom, left
        std::array<std::optional<Line>, 4> Border{};

        std::optional<Corners> Radius{};
        std::optional<std::vector<ShadowLayer>> Shadow{};
        std::optional<Line> Outline{};
        std::optional<double> OutlineOffset{};
        std::optional<double> Opacity{};
        std::optional<FilterValue> Filter{};

        std::optional<ColorExpression> Foreground{};
        std::optional<std::wstring> FontFamily{};
        std::optional<double> FontSize{};
        std::optional<int32_t> FontWeight{};
        std::optional<bool> Italic{};

        std::optional<double> Width{};
        std::optional<double> Height{};
        std::optional<Edges> Margin{};
        std::optional<Edges> Padding{};

        std::map<std::wstring, double, std::less<>> Numbers{};
        std::map<std::wstring, ColorExpression, std::less<>> Colors{};
        std::map<std::wstring, std::wstring, std::less<>> Words{};

        // Takes every property the other one sets.
        void MergeFrom(_In_ StyleSpec const& over);

        // Every color the style uses, for checking a theme's names.
        void ForEachColor(_In_ std::function<void(ColorExpression const&)> const& visit) const;
    };

    struct ResolvedStop
    {
        Color Value{};
        double Position{ 0 };
    };

    struct ResolvedBackground
    {
        BackgroundKind Kind{ BackgroundKind::None };
        Color Solid{};
        double Angle{ 180.0 };
        double CenterX{ 0.5 };
        double CenterY{ 0.5 };
        bool Circle{ false };
        std::optional<double> SizeX{};
        std::optional<double> SizeY{};

        // Positions filled in the way CSS fills them: from 0 to 1, evenly between the given ones.
        std::vector<ResolvedStop> Stops{};
    };

    struct ResolvedShadow
    {
        bool Inset{ false };
        double X{ 0 };
        double Y{ 0 };
        double Blur{ 0 };
        double Spread{ 0 };
        Color Value{};
    };

    struct ResolvedLine
    {
        double Width{ 0 };
        LineStyle Style{ LineStyle::None };
        Color Value{};

        bool IsVisible() const noexcept { return Width > 0 && Style != LineStyle::None && Value.A > 0; }
    };

    // A part's look with every color worked out, ready to draw.
    struct ResolvedStyle
    {
        // CSS order: the first layer is drawn on top.
        std::vector<ResolvedBackground> Background{};

        // top, right, bottom, left
        std::array<ResolvedLine, 4> Border{};

        Corners Radius{};
        std::vector<ResolvedShadow> Shadow{};
        ResolvedLine Outline{};
        double OutlineOffset{ 0 };
        double Opacity{ 1.0 };
        FilterValue Filter{};

        std::optional<Color> Foreground{};
        std::wstring FontFamily{};
        double FontSize{ 0 };
        int32_t FontWeight{ 0 };
        bool Italic{ false };

        std::optional<double> Width{};
        std::optional<double> Height{};
        Edges Margin{};
        Edges Padding{};

        std::map<std::wstring, double, std::less<>> Numbers{};
        std::map<std::wstring, Color, std::less<>> Colors{};
        std::map<std::wstring, std::wstring, std::less<>> Words{};

        // Whether there's anything to draw for the part itself: a background, a border, a shadow
        // or an outline.
        bool HasPlate() const noexcept;

        double Number(_In_ std::wstring_view name, _In_ double fallback) const;
        std::optional<Color> FindColor(_In_ std::wstring_view name) const;
        Color ColorOr(_In_ std::wstring_view name, _In_ Color const& fallback) const;
        std::wstring_view Word(_In_ std::wstring_view name, _In_ std::wstring_view fallback) const;

        // The radius of one corner for a box of this size, with percentages worked out and the
        // corners kept from overlapping, as CSS keeps them.
        double CornerRadius(_In_ size_t corner, _In_ double width, _In_ double height) const noexcept;
    };

    ResolvedStyle ResolveStyle(_In_ StyleSpec const& spec, _In_ ExpressionScope const& scope);

    // What CSS's saturate() and brightness() filters do to one color.
    Color ApplyFilter(_In_ Color const& color, _In_ FilterValue const& filter) noexcept;

    // Reads one CSS-like property into the style. False, with what's wrong, when the value can't
    // be read. A name the style doesn't know becomes a value of its own (see the top of the file).
    bool SetStyleProperty(
        _Inout_ StyleSpec& style,
        _In_ std::wstring_view name,
        _In_ std::wstring_view text,
        _Out_ std::wstring& error);

    // The same, for a property written as a JSON number.
    bool SetStyleNumber(
        _Inout_ StyleSpec& style,
        _In_ std::wstring_view name,
        _In_ double value,
        _Out_ std::wstring& error);
}
