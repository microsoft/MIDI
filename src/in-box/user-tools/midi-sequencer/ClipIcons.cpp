// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ClipIcons.h"

// For winrt::xaml_typename. The window's XAML headers bring it in; this file doesn't use them.
#include <winrt/Windows.UI.Xaml.Interop.h>

namespace midisequencer
{
    namespace
    {
        // Three bars of different lengths, as the comps draw a notes clip.
        constexpr std::wstring_view NotesPath{
            L"M2.2,3 H5.8 A1.2,1.2 0 0 1 5.8,5.4 H2.2 A1.2,1.2 0 0 1 2.2,3 Z "
            L"M7.2,7 H12.8 A1.2,1.2 0 0 1 12.8,9.4 H7.2 A1.2,1.2 0 0 1 7.2,7 Z "
            L"M4.2,11 H6.8 A1.2,1.2 0 0 1 6.8,13.4 H4.2 A1.2,1.2 0 0 1 4.2,11 Z" };

        // A ring with three hits on it, the same as the launcher draws. The inner circle runs the
        // other way round, so with the nonzero fill rule (F1) it's a hole, and the hits that
        // cross the ring still fill.
        constexpr std::wstring_view EuclideanPath{
            L"F1 M1.8,8 A6.2,6.2 0 1 1 14.2,8 A6.2,6.2 0 1 1 1.8,8 Z "
            L"M3,8 A5,5 0 1 0 13,8 A5,5 0 1 0 3,8 Z "
            L"M6,2.4 A2,2 0 1 1 10,2.4 A2,2 0 1 1 6,2.4 Z "
            L"M11.3,9.7 A2,2 0 1 1 15.3,9.7 A2,2 0 1 1 11.3,9.7 Z "
            L"M2,12 A2,2 0 1 1 6,12 A2,2 0 1 1 2,12 Z" };

        // Section 28's symbols, made of dots, rings, lines and boxes on the same 16 unit square.
        // The same parts give the PathIcon's markup and the launcher's drawing.
        struct Part
        {
            enum class Shape : uint8_t { Dot, Ring, Line, Box };

            Shape Kind{ Shape::Dot };
            float A{ 0 };               // a dot's, a ring's or a box's left or middle; a line's start
            float B{ 0 };
            float C{ 0 };               // radius, a box's side, or a line's end
            float D{ 0 };               // a ring's stroke, a box's corners, or a line's end
            float E{ 0 };               // a line's width
        };

        constexpr Part Dot(float x, float y, float r) { return Part{ Part::Shape::Dot, x, y, r, 0, 0 }; }
        constexpr Part Ring(float x, float y, float r, float w) { return Part{ Part::Shape::Ring, x, y, r, w, 0 }; }
        constexpr Part Line(float x0, float y0, float x1, float y1, float w) { return Part{ Part::Shape::Line, x0, y0, x1, y1, w }; }
        constexpr Part Box(float x, float y, float side, float corner) { return Part{ Part::Shape::Box, x, y, side, corner, 0 }; }

        std::vector<Part> PartsFor(ClipIcon icon)
        {
            switch (icon)
            {
            case ClipIcon::Life:
                // A glider, the shape that walks across a Life board.
                return { Box(5.9f, 0.8f, 4.2f, 1.0f), Box(11.0f, 5.9f, 4.2f, 1.0f), Box(0.8f, 11.0f, 4.2f, 1.0f), Box(5.9f, 11.0f, 4.2f, 1.0f), Box(11.0f, 11.0f, 4.2f, 1.0f) };

            case ClipIcon::LifeDrums:
                // The same glider as drum heads seen from above: rings rather than squares.
                return { Ring(8.0f, 2.9f, 1.9f, 1.4f), Ring(13.1f, 8.0f, 1.9f, 1.4f), Ring(2.9f, 13.1f, 1.9f, 1.4f), Ring(8.0f, 13.1f, 1.9f, 1.4f), Ring(13.1f, 13.1f, 1.9f, 1.4f) };

            case ClipIcon::Learn:
                return { Line(8.0f, 3.0f, 3.0f, 13.0f, 1.2f), Line(3.0f, 13.0f, 13.0f, 13.0f, 1.2f), Line(13.0f, 13.0f, 8.0f, 3.0f, 1.2f),
                    Dot(8.0f, 3.0f, 2.4f), Dot(3.0f, 13.0f, 2.4f), Dot(13.0f, 13.0f, 2.4f) };

            case ClipIcon::Evolve:
                return { Line(4.0f, 1.5f, 12.0f, 5.5f, 1.3f), Line(12.0f, 5.5f, 4.0f, 10.5f, 1.3f), Line(4.0f, 10.5f, 12.0f, 14.5f, 1.3f),
                    Line(12.0f, 1.5f, 4.0f, 5.5f, 1.3f), Line(4.0f, 5.5f, 12.0f, 10.5f, 1.3f), Line(12.0f, 10.5f, 4.0f, 14.5f, 1.3f),
                    Line(4.0f, 5.5f, 12.0f, 5.5f, 1.1f), Line(4.0f, 10.5f, 12.0f, 10.5f, 1.1f) };

            case ClipIcon::Infinity:
                return { Ring(4.6f, 8.0f, 3.3f, 1.5f), Ring(11.4f, 8.0f, 3.3f, 1.5f) };

            case ClipIcon::LSystem:
                return { Line(8.0f, 15.0f, 8.0f, 3.0f, 1.3f), Line(8.0f, 11.5f, 3.5f, 7.5f, 1.3f), Line(8.0f, 8.5f, 12.5f, 5.0f, 1.3f),
                    Line(8.0f, 5.5f, 5.0f, 3.0f, 1.1f), Dot(3.5f, 7.5f, 1.5f), Dot(12.5f, 5.0f, 1.5f), Dot(5.0f, 3.0f, 1.3f), Dot(8.0f, 3.0f, 1.5f) };

            default:
                return {};
            }
        }

        void AppendCircle(std::wstring& path, float x, float y, float r, bool clockwise)
        {
            auto const sweep = clockwise ? 1 : 0;
            path += std::format(L"M{:.2f},{:.2f} A{:.2f},{:.2f} 0 1 {} {:.2f},{:.2f} A{:.2f},{:.2f} 0 1 {} {:.2f},{:.2f} Z ",
                x - r, y, r, r, sweep, x + r, y, r, r, sweep, x - r, y);
        }

        // Every part goes round the same way, so with the nonzero rule (F1) where they overlap
        // stays filled, and only a ring's middle is a hole.
        std::wstring PathFor(ClipIcon icon)
        {
            std::wstring path{ L"F1 " };

            for (auto const& part : PartsFor(icon))
            {
                switch (part.Kind)
                {
                case Part::Shape::Dot:
                    AppendCircle(path, part.A, part.B, part.C, true);
                    break;

                case Part::Shape::Ring:
                    AppendCircle(path, part.A, part.B, part.C + part.D / 2.0f, true);
                    AppendCircle(path, part.A, part.B, part.C - part.D / 2.0f, false);
                    break;

                case Part::Shape::Line:
                {
                    // A rectangle along the line, a half width past each end so lines meet cleanly.
                    auto const dx = part.C - part.A;
                    auto const dy = part.D - part.B;
                    auto const length = std::max(0.001f, std::hypot(dx, dy));
                    auto const ux = dx / length * part.E / 2.0f;
                    auto const uy = dy / length * part.E / 2.0f;

                    std::array<std::pair<float, float>, 4> corners{ {
                        { part.A - ux - uy, part.B - uy + ux },
                        { part.C + ux - uy, part.D + uy + ux },
                        { part.C + ux + uy, part.D + uy - ux },
                        { part.A - ux + uy, part.B - uy - ux } } };

                    // Clockwise on screen has a positive area with y going down.
                    double area{ 0 };

                    for (size_t i = 0; i < corners.size(); ++i)
                    {
                        auto const& [x0, y0] = corners[i];
                        auto const& [x1, y1] = corners[(i + 1) % corners.size()];
                        area += static_cast<double>(x0) * y1 - static_cast<double>(x1) * y0;
                    }

                    if (area < 0)
                    {
                        std::reverse(corners.begin(), corners.end());
                    }

                    path += std::format(L"M{:.2f},{:.2f} L{:.2f},{:.2f} L{:.2f},{:.2f} L{:.2f},{:.2f} Z ",
                        corners[0].first, corners[0].second, corners[1].first, corners[1].second,
                        corners[2].first, corners[2].second, corners[3].first, corners[3].second);
                    break;
                }

                case Part::Shape::Box:
                {
                    auto const x = part.A;
                    auto const y = part.B;
                    auto const s = part.C;
                    auto const r = part.D;

                    path += std::format(L"M{:.2f},{:.2f} H{:.2f} A{:.2f},{:.2f} 0 0 1 {:.2f},{:.2f} V{:.2f} A{:.2f},{:.2f} 0 0 1 {:.2f},{:.2f} "
                        L"H{:.2f} A{:.2f},{:.2f} 0 0 1 {:.2f},{:.2f} V{:.2f} A{:.2f},{:.2f} 0 0 1 {:.2f},{:.2f} Z ",
                        x + r, y, x + s - r, r, r, x + s, y + r, y + s - r, r, r, x + s - r, y + s,
                        x + r, r, r, x, y + s - r, y + r, r, r, x + r, y);
                    break;
                }
                }
            }

            return path;
        }
    }

    _Use_decl_annotations_
    ClipIcon IconFor(Clip const& clip) noexcept
    {
        switch (clip.Kind)
        {
        case ClipKind::Pattern:
            return clip.Pattern.has_value() ? IconFor(clip.Pattern->Mode) : ClipIcon::DrumPattern;

        case ClipKind::Generator:
            return clip.Generator.has_value() ? IconFor(clip.Generator->Type) : ClipIcon::Euclidean;

        case ClipKind::Notes:
        default:
            return ClipIcon::Notes;
        }
    }

    _Use_decl_annotations_
    ClipIcon IconFor(PatternMode mode) noexcept
    {
        return mode == PatternMode::Melodic ? ClipIcon::MelodicPattern : ClipIcon::DrumPattern;
    }

    _Use_decl_annotations_
    ClipIcon IconFor(GeneratorType type) noexcept
    {
        switch (type)
        {
        case GeneratorType::ChanceMelody: return ClipIcon::ChanceMelody;
        case GeneratorType::Arpeggiator: return ClipIcon::Arpeggiator;
        case GeneratorType::Life: return ClipIcon::Life;
        case GeneratorType::LifeDrums: return ClipIcon::LifeDrums;
        case GeneratorType::Learn: return ClipIcon::Learn;
        case GeneratorType::Words: return ClipIcon::Words;
        case GeneratorType::Evolve: return ClipIcon::Evolve;
        case GeneratorType::Infinity: return ClipIcon::Infinity;
        case GeneratorType::LSystem: return ClipIcon::LSystem;
        case GeneratorType::ChangeRinging: return ClipIcon::ChangeRinging;

        case GeneratorType::Euclidean:
        default:
            return ClipIcon::Euclidean;
        }
    }

    _Use_decl_annotations_
    wchar_t const* ClipIconGlyph(ClipIcon icon) noexcept
    {
        switch (icon)
        {
        case ClipIcon::DrumPattern:
            // GridviewGroup. Microsoft listed it only in February 2026, but Windows 11 25H2, the
            // oldest Windows these tools support, has it (checked October 2026).
            return L"\uF207";

        case ClipIcon::MelodicPattern:
            return L"\uEC4F";       // MusicNote

        case ClipIcon::ChanceMelody:
            return L"\uE8B1";       // Shuffle: notes picked by chance

        case ClipIcon::Arpeggiator:
            return L"\uE86F";       // SignalBars4: notes stepping up

        case ClipIcon::Words:
            return L"\uE8D2";       // Font

        case ClipIcon::ChangeRinging:
            return L"\uEA8F";       // Ringer: a bell

        default:
            return nullptr;
        }
    }

    _Use_decl_annotations_
    std::wstring_view ClipIconPath(ClipIcon icon) noexcept
    {
        switch (icon)
        {
        case ClipIcon::Notes:
            return NotesPath;

        case ClipIcon::Euclidean:
            return EuclideanPath;

        case ClipIcon::Life:
        case ClipIcon::LifeDrums:
        case ClipIcon::Learn:
        case ClipIcon::Evolve:
        case ClipIcon::Infinity:
        case ClipIcon::LSystem:
        {
            // Made once, and kept for the life of the app.
            static std::array<std::wstring, 16> const paths = []()
            {
                std::array<std::wstring, 16> made{};

                try
                {
                    for (size_t i = 0; i < made.size(); ++i)
                    {
                        made[i] = PathFor(static_cast<ClipIcon>(i));
                    }
                }
                catch (...)
                {
                }

                return made;
            }();

            return paths[static_cast<size_t>(icon) % paths.size()];
        }

        default:
            return {};
        }
    }

    _Use_decl_annotations_
    bool DrawClipIcon(canvas::CanvasDrawingSession const& ds, ClipIcon icon, float x, float y, float size, Color color)
    {
        auto const parts = PartsFor(icon);

        if (parts.empty())
        {
            return false;
        }

        auto const s = size / 16.0f;

        canvasGeometry::CanvasStrokeStyle square{};
        square.StartCap(canvasGeometry::CanvasCapStyle::Square);
        square.EndCap(canvasGeometry::CanvasCapStyle::Square);

        for (auto const& part : parts)
        {
            switch (part.Kind)
            {
            case Part::Shape::Dot:
                ds.FillCircle(x + part.A * s, y + part.B * s, part.C * s, color);
                break;

            case Part::Shape::Ring:
                ds.DrawCircle(x + part.A * s, y + part.B * s, part.C * s, color, part.D * s);
                break;

            case Part::Shape::Line:
                ds.DrawLine(x + part.A * s, y + part.B * s, x + part.C * s, y + part.D * s, color, part.E * s, square);
                break;

            case Part::Shape::Box:
                ds.FillRoundedRectangle(x + part.A * s, y + part.B * s, part.C * s, part.C * s, part.D * s, part.D * s, color);
                break;
            }
        }

        return true;
    }

    _Use_decl_annotations_
    controls::IconElement MakeClipIcon(ClipIcon icon)
    {
        if (auto const glyph = ClipIconGlyph(icon); glyph != nullptr)
        {
            controls::FontIcon font{};
            font.Glyph(glyph);
            return font;
        }

        // A geometry belongs to one element, so each icon gets its own.
        controls::PathIcon path{};
        path.Data(xaml::Markup::XamlBindingHelper::ConvertValue(
            winrt::xaml_typename<media::Geometry>(),
            winrt::box_value(winrt::hstring{ ClipIconPath(icon) }))
            .as<media::Geometry>());

        return path;
    }
}
