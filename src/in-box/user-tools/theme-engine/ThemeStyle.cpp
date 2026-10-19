// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "ThemeStyle.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace themeengine
{
    namespace
    {
        wchar_t AsciiLower(wchar_t c) noexcept
        {
            return (c >= L'A' && c <= L'Z') ? static_cast<wchar_t>(c - L'A' + L'a') : c;
        }

        std::wstring Lower(std::wstring_view text)
        {
            std::wstring result{ text };
            std::transform(result.begin(), result.end(), result.begin(), AsciiLower);
            return result;
        }

        std::wstring_view Trim(std::wstring_view text) noexcept
        {
            auto const start = text.find_first_not_of(L" \t\r\n");

            if (start == std::wstring_view::npos)
            {
                return {};
            }

            auto const end = text.find_last_not_of(L" \t\r\n");
            return text.substr(start, end - start + 1);
        }

        bool EndsWith(std::wstring_view text, std::wstring_view suffix) noexcept
        {
            return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
        }

        // A font family list as CSS writes it, with the quotes taken off each name.
        std::wstring FontFamilyText(std::wstring_view text)
        {
            std::wstring result{};

            for (auto c : text)
            {
                if (c != L'"' && c != L'\'')
                {
                    result.push_back(c);
                }
            }

            return std::wstring{ Trim(result) };
        }

        std::vector<ResolvedStop> ResolveStops(std::vector<GradientStop> const& stops, ExpressionScope const& scope)
        {
            std::vector<ResolvedStop> resolved{};
            resolved.reserve(stops.size());

            for (auto const& stop : stops)
            {
                resolved.push_back({ stop.Color.Evaluate(scope), stop.Position.value_or(-1.0) });
            }

            if (resolved.empty())
            {
                return resolved;
            }

            // CSS: the first stop defaults to 0 and the last to 1, a stop never goes back past the
            // one before it, and stops with no position share the space between their neighbors.
            if (resolved.front().Position < 0) { resolved.front().Position = 0.0; }
            if (resolved.back().Position < 0) { resolved.back().Position = 1.0; }

            auto previous = resolved.front().Position;

            for (auto& stop : resolved)
            {
                if (stop.Position >= 0)
                {
                    stop.Position = std::max(stop.Position, previous);
                    previous = stop.Position;
                }
            }

            for (size_t i = 1; i < resolved.size(); ++i)
            {
                if (resolved[i].Position >= 0)
                {
                    continue;
                }

                auto next = i;
                while (next < resolved.size() && resolved[next].Position < 0) { ++next; }

                auto const from = resolved[i - 1].Position;
                auto const to = resolved[next].Position;
                auto const gaps = static_cast<double>(next - i + 1);

                for (auto k = i; k < next; ++k)
                {
                    resolved[k].Position = from + (to - from) * static_cast<double>(k - i + 1) / gaps;
                }
            }

            return resolved;
        }

        ResolvedLine ResolveLine(std::optional<Line> const& line, ExpressionScope const& scope)
        {
            if (!line.has_value())
            {
                return {};
            }

            return { line->Width, line->Style, line->Color.Evaluate(scope) };
        }
    }

    _Use_decl_annotations_
    void StyleSpec::MergeFrom(StyleSpec const& over)
    {
        if (over.Background.has_value()) { Background = over.Background; }

        for (size_t i = 0; i < Border.size(); ++i)
        {
            if (over.Border[i].has_value()) { Border[i] = over.Border[i]; }
        }

        if (over.Radius.has_value()) { Radius = over.Radius; }
        if (over.Shadow.has_value()) { Shadow = over.Shadow; }
        if (over.Outline.has_value()) { Outline = over.Outline; }
        if (over.OutlineOffset.has_value()) { OutlineOffset = over.OutlineOffset; }
        if (over.Opacity.has_value()) { Opacity = over.Opacity; }
        if (over.Filter.has_value()) { Filter = over.Filter; }
        if (over.Foreground.has_value()) { Foreground = over.Foreground; }
        if (over.FontFamily.has_value()) { FontFamily = over.FontFamily; }
        if (over.FontSize.has_value()) { FontSize = over.FontSize; }
        if (over.FontWeight.has_value()) { FontWeight = over.FontWeight; }
        if (over.Italic.has_value()) { Italic = over.Italic; }
        if (over.Width.has_value()) { Width = over.Width; }
        if (over.Height.has_value()) { Height = over.Height; }
        if (over.Margin.has_value()) { Margin = over.Margin; }
        if (over.Padding.has_value()) { Padding = over.Padding; }

        for (auto const& [name, value] : over.Numbers) { Numbers.insert_or_assign(name, value); }
        for (auto const& [name, value] : over.Colors) { Colors.insert_or_assign(name, value); }
        for (auto const& [name, value] : over.Words) { Words.insert_or_assign(name, value); }
    }

    _Use_decl_annotations_
    void StyleSpec::ForEachColor(std::function<void(ColorExpression const&)> const& visit) const
    {
        if (Background.has_value())
        {
            for (auto const& layer : *Background)
            {
                visit(layer.Color);

                for (auto const& stop : layer.Stops)
                {
                    visit(stop.Color);
                }
            }
        }

        for (auto const& side : Border)
        {
            if (side.has_value()) { visit(side->Color); }
        }

        if (Shadow.has_value())
        {
            for (auto const& shadow : *Shadow)
            {
                visit(shadow.Color);
            }
        }

        if (Outline.has_value()) { visit(Outline->Color); }
        if (Foreground.has_value()) { visit(*Foreground); }

        for (auto const& [name, color] : Colors)
        {
            visit(color);
        }
    }

    bool ResolvedStyle::HasPlate() const noexcept
    {
        if (!Shadow.empty() || Outline.IsVisible())
        {
            return true;
        }

        for (auto const& side : Border)
        {
            if (side.IsVisible()) { return true; }
        }

        for (auto const& layer : Background)
        {
            if (layer.Kind != BackgroundKind::None) { return true; }
        }

        return false;
    }

    _Use_decl_annotations_
    double ResolvedStyle::Number(std::wstring_view name, double fallback) const
    {
        auto const found = Numbers.find(name);
        return found != Numbers.end() ? found->second : fallback;
    }

    _Use_decl_annotations_
    std::optional<Color> ResolvedStyle::FindColor(std::wstring_view name) const
    {
        auto const found = Colors.find(name);
        return found != Colors.end() ? std::optional<Color>{ found->second } : std::nullopt;
    }

    _Use_decl_annotations_
    Color ResolvedStyle::ColorOr(std::wstring_view name, Color const& fallback) const
    {
        return FindColor(name).value_or(fallback);
    }

    _Use_decl_annotations_
    std::wstring_view ResolvedStyle::Word(std::wstring_view name, std::wstring_view fallback) const
    {
        auto const found = Words.find(name);
        return found != Words.end() ? std::wstring_view{ found->second } : fallback;
    }

    _Use_decl_annotations_
    double ResolvedStyle::CornerRadius(size_t corner, double width, double height) const noexcept
    {
        if (corner >= Radius.size() || width <= 0 || height <= 0)
        {
            return 0.0;
        }

        auto const absolute = [&](Corner const& c) { return c.Percent ? c.Value * std::min(width, height) : c.Value; };

        std::array<double, 4> radii{};

        for (size_t i = 0; i < radii.size(); ++i)
        {
            radii[i] = std::max(0.0, absolute(Radius[i]));
        }

        // CSS scales every corner down together when two would overlap along a side.
        auto factor = 1.0;
        auto const limit = [&factor](double side, double a, double b)
        {
            if (a + b > side && a + b > 0)
            {
                factor = std::min(factor, side / (a + b));
            }
        };

        limit(width, radii[0], radii[1]);
        limit(width, radii[3], radii[2]);
        limit(height, radii[0], radii[3]);
        limit(height, radii[1], radii[2]);

        return radii[corner] * factor;
    }

    _Use_decl_annotations_
    ResolvedStyle ResolveStyle(StyleSpec const& spec, ExpressionScope const& scope)
    {
        ResolvedStyle style{};

        if (spec.Background.has_value())
        {
            for (auto const& layer : *spec.Background)
            {
                ResolvedBackground resolved{};
                resolved.Kind = layer.Kind;
                resolved.Angle = layer.Angle;
                resolved.CenterX = layer.CenterX;
                resolved.CenterY = layer.CenterY;
                resolved.Circle = layer.Circle;
                resolved.SizeX = layer.SizeX;
                resolved.SizeY = layer.SizeY;

                if (layer.Kind == BackgroundKind::Solid)
                {
                    resolved.Solid = layer.Color.Evaluate(scope);
                }
                else
                {
                    resolved.Stops = ResolveStops(layer.Stops, scope);
                }

                style.Background.push_back(std::move(resolved));
            }
        }

        for (size_t i = 0; i < spec.Border.size(); ++i)
        {
            style.Border[i] = ResolveLine(spec.Border[i], scope);
        }

        style.Radius = spec.Radius.value_or(Corners{});

        if (spec.Shadow.has_value())
        {
            for (auto const& shadow : *spec.Shadow)
            {
                if (!shadow.Reference.empty())
                {
                    continue;
                }

                style.Shadow.push_back({ shadow.Inset, shadow.X, shadow.Y, shadow.Blur, shadow.Spread, shadow.Color.Evaluate(scope) });
            }
        }

        style.Outline = ResolveLine(spec.Outline, scope);
        style.OutlineOffset = spec.OutlineOffset.value_or(0.0);
        style.Opacity = std::clamp(spec.Opacity.value_or(1.0), 0.0, 1.0);
        style.Filter = spec.Filter.value_or(FilterValue{});

        if (spec.Foreground.has_value())
        {
            style.Foreground = spec.Foreground->Evaluate(scope);
        }

        style.FontFamily = spec.FontFamily.value_or(std::wstring{});
        style.FontSize = spec.FontSize.value_or(0.0);
        style.FontWeight = spec.FontWeight.value_or(0);
        style.Italic = spec.Italic.value_or(false);
        style.Width = spec.Width;
        style.Height = spec.Height;
        style.Margin = spec.Margin.value_or(Edges{});
        style.Padding = spec.Padding.value_or(Edges{});
        style.Numbers = spec.Numbers;
        style.Words = spec.Words;

        for (auto const& [name, color] : spec.Colors)
        {
            style.Colors.emplace(name, color.Evaluate(scope));
        }

        return style;
    }

    _Use_decl_annotations_
    Color ApplyFilter(Color const& color, FilterValue const& filter) noexcept
    {
        if (filter == FilterValue{})
        {
            return color;
        }

        auto const s = filter.Saturate;

        // The matrix CSS gives saturate(), then brightness() as a plain scale.
        auto const r = (0.213 + 0.787 * s) * color.R + (0.715 - 0.715 * s) * color.G + (0.072 - 0.072 * s) * color.B;
        auto const g = (0.213 - 0.213 * s) * color.R + (0.715 + 0.285 * s) * color.G + (0.072 - 0.072 * s) * color.B;
        auto const b = (0.213 - 0.213 * s) * color.R + (0.715 - 0.715 * s) * color.G + (0.072 + 0.928 * s) * color.B;

        auto const k = filter.Brightness;

        return Color{ r * k, g * k, b * k, color.A }.Clamped();
    }

    _Use_decl_annotations_
    bool SetStyleProperty(StyleSpec& style, std::wstring_view name, std::wstring_view text, std::wstring& error)
    {
        error.clear();

        auto const key = Lower(Trim(name));
        auto const value = Trim(text);

        auto const color = [&](std::optional<ColorExpression>& target) -> bool
        {
            auto parsed = ColorExpression::Parse(value, error);

            if (!parsed.has_value())
            {
                return false;
            }

            target = std::move(*parsed);
            return true;
        };

        auto const line = [&](std::optional<Line>& target) -> bool
        {
            Line parsed{};

            if (!ParseLine(value, parsed, error))
            {
                return false;
            }

            target = std::move(parsed);
            return true;
        };

        auto const length = [&](std::optional<double>& target) -> bool
        {
            double parsed{};

            if (!ParseLength(value, parsed, error))
            {
                return false;
            }

            target = parsed;
            return true;
        };

        if (key == L"background" || key == L"background-color" || key == L"background-image")
        {
            std::vector<BackgroundLayer> layers{};

            if (!ParseBackground(value, layers, error))
            {
                return false;
            }

            style.Background = std::move(layers);
            return true;
        }

        if (key == L"border")
        {
            std::optional<Line> parsed{};

            if (!line(parsed))
            {
                return false;
            }

            style.Border = { parsed, parsed, parsed, parsed };
            return true;
        }

        if (key == L"border-top") { return line(style.Border[0]); }
        if (key == L"border-right") { return line(style.Border[1]); }
        if (key == L"border-bottom") { return line(style.Border[2]); }
        if (key == L"border-left") { return line(style.Border[3]); }

        if (key == L"border-radius" || key == L"radius")
        {
            Corners corners{};

            if (!ParseCorners(value, corners, error))
            {
                return false;
            }

            style.Radius = corners;
            return true;
        }

        if (key == L"box-shadow" || key == L"shadow")
        {
            std::vector<ShadowLayer> shadows{};

            if (!ParseShadows(value, shadows, error))
            {
                return false;
            }

            style.Shadow = std::move(shadows);
            return true;
        }

        if (key == L"outline") { return line(style.Outline); }
        if (key == L"outline-offset") { return length(style.OutlineOffset); }
        if (key == L"opacity" || key == L"font-size" || key == L"width" || key == L"height")
        {
            auto& target =
                key == L"opacity" ? style.Opacity :
                key == L"font-size" ? style.FontSize :
                key == L"width" ? style.Width : style.Height;

            return length(target);
        }

        if (key == L"filter")
        {
            FilterValue filter{};

            if (!ParseFilter(value, filter, error))
            {
                return false;
            }

            style.Filter = filter;
            return true;
        }

        if (key == L"color") { return color(style.Foreground); }

        if (key == L"font-family" || key == L"font")
        {
            style.FontFamily = FontFamilyText(value);

            if (style.FontFamily->empty())
            {
                error = L"A font family needs a name.";
                return false;
            }

            return true;
        }

        if (key == L"font-weight")
        {
            int32_t weight{};

            if (!ParseFontWeight(value, weight, error))
            {
                return false;
            }

            style.FontWeight = weight;
            return true;
        }

        if (key == L"font-style")
        {
            auto const lower = Lower(value);

            if (lower != L"italic" && lower != L"normal")
            {
                error = L"font-style is italic or normal.";
                return false;
            }

            style.Italic = lower == L"italic";
            return true;
        }

        if (key == L"margin" || key == L"padding")
        {
            Edges edges{};

            if (!ParseEdges(value, edges, error))
            {
                return false;
            }

            (key == L"margin" ? style.Margin : style.Padding) = edges;
            return true;
        }

        if (key.empty())
        {
            error = L"A property needs a name.";
            return false;
        }

        // Values of the part's own.
        if (EndsWith(key, L"color"))
        {
            std::optional<ColorExpression> parsed{};

            if (!color(parsed))
            {
                return false;
            }

            style.Colors.insert_or_assign(key, std::move(*parsed));
            return true;
        }

        double number{};
        std::wstring ignored{};

        if (ParseLength(value, number, ignored))
        {
            style.Numbers.insert_or_assign(key, number);
            return true;
        }

        if (value.empty())
        {
            error = std::format(L"\"{}\" needs a value.", key);
            return false;
        }

        style.Words.insert_or_assign(key, std::wstring{ value });
        return true;
    }

    _Use_decl_annotations_
    bool SetStyleNumber(StyleSpec& style, std::wstring_view name, double value, std::wstring& error)
    {
        error.clear();

        auto const key = Lower(Trim(name));

        if (!std::isfinite(value))
        {
            error = std::format(L"\"{}\" isn't a number.", key);
            return false;
        }

        if (key == L"opacity") { style.Opacity = value; return true; }
        if (key == L"font-size") { style.FontSize = value; return true; }
        if (key == L"font-weight") { style.FontWeight = static_cast<int32_t>(value); return true; }
        if (key == L"width") { style.Width = value; return true; }
        if (key == L"height") { style.Height = value; return true; }
        if (key == L"outline-offset") { style.OutlineOffset = value; return true; }

        if (key == L"border-radius" || key == L"radius")
        {
            Corner const corner{ std::max(0.0, value), false };
            style.Radius = Corners{ corner, corner, corner, corner };
            return true;
        }

        if (key == L"margin" || key == L"padding")
        {
            (key == L"margin" ? style.Margin : style.Padding) = Edges{ value, value, value, value };
            return true;
        }

        if (key.empty())
        {
            error = L"A property needs a name.";
            return false;
        }

        style.Numbers.insert_or_assign(key, value);
        return true;
    }
}
