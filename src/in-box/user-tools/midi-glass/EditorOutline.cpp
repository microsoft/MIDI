// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The two left panes: the palette of controls that can be added, and the outline.
//
// The outline is not an accessibility bolt-on. It is how somebody using a screen reader builds a
// layout, and it is also the only sane way to find the right thing on a page of two hundred
// controls. It carries the keyboard order badge, because a hidden ordering is one nobody can fix.

#include "pch.h"
#include "EditorWindow.xaml.h"
#include "EditorItems.h"

#include "StringResources.h"
#include "ControlFactory.h"

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        namespace shapes = ::winrt::Microsoft::UI::Xaml::Shapes;

        // How far in an outline row starts, and how much further a member of a group goes.
        constexpr double OutlineIndent = 4.0;
        constexpr double OutlineGroupIndent = 20.0;

        // Matched against the palette search box. Case insensitive, and a match anywhere in the
        // name counts, because somebody typing "fad" should find Fader.
        bool Matches(_In_ std::wstring_view name, _In_ std::wstring_view query) noexcept
        {
            if (query.empty())
            {
                return true;
            }

            auto const found = ::FindNLSStringEx(
                LOCALE_NAME_USER_DEFAULT,
                LINGUISTIC_IGNORECASE | FIND_FROMSTART,
                name.data(), static_cast<int32_t>(name.size()),
                query.data(), static_cast<int32_t>(query.size()),
                nullptr, nullptr, nullptr, 0);

            return found >= 0;
        }

        wchar_t GlyphForKind(_In_ glass::ControlKind kind) noexcept
        {
            for (auto const& entry : glass::Palette())
            {
                // Several of the not-yet-built kinds borrow a real one's enum value to sit in
                // the palette, so they must never answer a lookup by kind.
                if (!entry.IsComing && entry.Kind == kind)
                {
                    return entry.Glyph;
                }
            }

            return L'\uE7C4';
        }

        std::wstring NameForKind(_In_ glass::ControlKind kind)
        {
            for (auto const& entry : glass::Palette())
            {
                if (!entry.IsComing && entry.Kind == kind)
                {
                    return std::wstring{ resources::GetString(entry.NameResourceKey) };
                }
            }

            return {};
        }

        glass::PaletteArt ArtForKind(_In_ glass::ControlKind kind)
        {
            for (auto const& entry : glass::Palette())
            {
                if (!entry.IsComing && entry.Kind == kind)
                {
                    return entry.Art;
                }
            }

            return {};
        }

        media::SolidColorBrush AccentAt(_In_ double opacity)
        {
            auto const accent = xaml::Application::Current().Resources()
                .Lookup(box_value(L"AccentFillColorDefaultBrush")).as<media::SolidColorBrush>();

            auto color = accent.Color();
            color.A = static_cast<uint8_t>(std::clamp(opacity, 0.0, 1.0) * 255.0);

            return media::SolidColorBrush{ color };
        }

        // The miniature a palette tile shows. Built from the numbers in the palette entry rather
        // than from a shape per kind, so adding a kind is a row of data and not a new case.
        xaml::UIElement BuildTileArt(_In_ glass::PaletteArt const& art)
        {
            namespace shapes = ::winrt::Microsoft::UI::Xaml::Shapes;

            switch (art.Shape)
            {
            case glass::PaletteArtShape::Glyph:
            case glass::PaletteArtShape::Sample:
            {
                controls::TextBlock text{};

                text.Text(art.Sample);
                text.FontSize(art.Shape == glass::PaletteArtShape::Glyph ? 15.0 : 11.0);
                text.HorizontalAlignment(xaml::HorizontalAlignment::Center);
                text.Foreground(AccentAt(1.0));

                if (art.Shape == glass::PaletteArtShape::Glyph)
                {
                    text.FontFamily(media::FontFamily{ L"Segoe Fluent Icons" });
                }
                else
                {
                    text.FontFamily(media::FontFamily{ L"Cascadia Mono, Consolas" });
                }

                return text;
            }

            case glass::PaletteArtShape::Ellipse:
            {
                shapes::Ellipse shape{};

                shape.Width(art.Width);
                shape.Height(art.Height);
                shape.UseLayoutRounding(false);

                if (art.StrokeOpacity > 0.0)
                {
                    shape.Stroke(AccentAt(art.StrokeOpacity));
                    shape.StrokeThickness(2.0);
                }

                if (art.FillOpacity > 0.0)
                {
                    shape.Fill(AccentAt(art.FillOpacity));
                }

                return shape;
            }

            case glass::PaletteArtShape::VerticalBars:
            case glass::PaletteArtShape::HorizontalBars:
            {
                auto const upright = art.Shape == glass::PaletteArtShape::VerticalBars;

                controls::StackPanel panel{};

                panel.Orientation(upright
                    ? controls::Orientation::Horizontal
                    : controls::Orientation::Vertical);
                panel.Spacing(3.0);
                panel.HorizontalAlignment(xaml::HorizontalAlignment::Center);

                for (int32_t i = 0; i < 3; ++i)
                {
                    shapes::Rectangle bar{};

                    bar.Width(upright ? 3.0 : art.Width);
                    bar.Height(upright ? art.Height : 2.0);
                    bar.RadiusX(1.0);
                    bar.RadiusY(1.0);
                    bar.UseLayoutRounding(false);
                    bar.Fill(AccentAt(art.FillOpacity));

                    panel.Children().Append(bar);
                }

                return panel;
            }

            case glass::PaletteArtShape::MeterBar:
            {
                controls::Grid grid{};

                grid.Width(art.Width);
                grid.Height(art.Height);

                shapes::Rectangle track{};

                track.RadiusX(art.CornerRadius);
                track.RadiusY(art.CornerRadius);
                track.UseLayoutRounding(false);
                track.Fill(AccentAt(0.18));

                shapes::Rectangle fill{};

                // Two thirds up, so the tile reads as a meter with a level rather than as a bar.
                fill.Height(art.Height * 0.62);
                fill.VerticalAlignment(xaml::VerticalAlignment::Bottom);
                fill.RadiusX(art.CornerRadius);
                fill.RadiusY(art.CornerRadius);
                fill.UseLayoutRounding(false);
                fill.Fill(AccentAt(art.FillOpacity));

                grid.Children().Append(track);
                grid.Children().Append(fill);

                return grid;
            }

            case glass::PaletteArtShape::Wave:
            {
                shapes::Path path{};

                path.Width(art.Width);
                path.Height(art.Height);
                path.Stroke(AccentAt(art.StrokeOpacity));
                path.StrokeThickness(1.6);
                path.UseLayoutRounding(false);
                path.Data(xaml::Markup::XamlBindingHelper::ConvertValue(
                    winrt::xaml_typename<media::Geometry>(),
                    box_value(L"M0,6 C3,0 6,0 9,6 C12,12 15,12 18,6"))
                    .as<media::Geometry>());

                return path;
            }

            case glass::PaletteArtShape::Keys:
            {
                controls::Grid grid{};

                grid.Width(art.Width);
                grid.Height(art.Height);

                controls::StackPanel whites{};

                whites.Orientation(controls::Orientation::Horizontal);
                whites.Spacing(1.0);

                constexpr int32_t whiteCount = 7;

                auto const whiteWidth = (art.Width - (whiteCount - 1)) / whiteCount;

                for (int32_t i = 0; i < whiteCount; ++i)
                {
                    shapes::Rectangle key{};

                    key.Width(whiteWidth);
                    key.Height(art.Height);
                    key.RadiusX(art.CornerRadius);
                    key.RadiusY(art.CornerRadius);
                    key.UseLayoutRounding(false);
                    key.Fill(AccentAt(0.55));

                    whites.Children().Append(key);
                }

                controls::Canvas blacks{};

                // Where the black keys fall in an octave: after the first, second, fourth,
                // fifth and sixth white key.
                for (auto const after : { 1, 2, 4, 5, 6 })
                {
                    shapes::Rectangle key{};

                    key.Width(whiteWidth * 0.6);
                    key.Height(art.Height * 0.6);
                    key.UseLayoutRounding(false);
                    key.Fill(AccentAt(1.0));

                    controls::Canvas::SetLeft(key, after * (whiteWidth + 1.0) - whiteWidth * 0.3);
                    controls::Canvas::SetTop(key, 0.0);

                    blacks.Children().Append(key);
                }

                grid.Children().Append(whites);
                grid.Children().Append(blacks);

                return grid;
            }

            case glass::PaletteArtShape::PadGrid:
            {
                // Two rows of four, with one pad lit the way a root note is.
                controls::Canvas canvas{};

                canvas.Width(art.Width);
                canvas.Height(art.Height);

                constexpr int32_t columns = 4;
                constexpr int32_t rows = 2;
                constexpr double gap = 1.5;

                auto const cell = std::min(
                    (art.Width - gap * (columns - 1)) / columns,
                    (art.Height - gap * (rows - 1)) / rows);

                for (int32_t row = 0; row < rows; ++row)
                {
                    for (int32_t column = 0; column < columns; ++column)
                    {
                        shapes::Rectangle pad{};

                        pad.Width(cell);
                        pad.Height(cell);
                        pad.RadiusX(art.CornerRadius);
                        pad.RadiusY(art.CornerRadius);
                        pad.UseLayoutRounding(false);
                        pad.Fill(AccentAt(row == 1 && column == 0 ? 1.0 : 0.45));

                        controls::Canvas::SetLeft(pad, column * (cell + gap));
                        controls::Canvas::SetTop(pad, row * (cell + gap));

                        canvas.Children().Append(pad);
                    }
                }

                return canvas;
            }

            case glass::PaletteArtShape::HexGrid:
            {
                // Three hexagons along the bottom and two nested above them, one lit.
                controls::Canvas canvas{};

                canvas.Width(art.Width);
                canvas.Height(art.Height);

                constexpr double gap = 1.2;

                auto const width = (art.Width - gap * 2.0) / 3.0;
                auto const height = width * 2.0 / 1.7320508075688772;
                auto const rowPitch = (width + gap) * 0.8660254037844386;

                auto const hexagon = [&](double left, double top, double opacity)
                    {
                        shapes::Polygon shape{};

                        media::PointCollection points{};

                        points.Append({ static_cast<float>(width * 0.5), 0.0f });
                        points.Append({ static_cast<float>(width), static_cast<float>(height * 0.25) });
                        points.Append({ static_cast<float>(width), static_cast<float>(height * 0.75) });
                        points.Append({ static_cast<float>(width * 0.5), static_cast<float>(height) });
                        points.Append({ 0.0f, static_cast<float>(height * 0.75) });
                        points.Append({ 0.0f, static_cast<float>(height * 0.25) });

                        shape.Points(points);
                        shape.UseLayoutRounding(false);
                        shape.Fill(AccentAt(opacity));

                        controls::Canvas::SetLeft(shape, left);
                        controls::Canvas::SetTop(shape, top);

                        canvas.Children().Append(shape);
                    };

                auto const bottom = art.Height - height;

                for (int32_t column = 0; column < 3; ++column)
                {
                    hexagon(column * (width + gap), bottom, column == 0 ? 1.0 : 0.45);
                }

                for (int32_t column = 0; column < 2; ++column)
                {
                    hexagon(column * (width + gap) + (width + gap) * 0.5, bottom - rowPitch, 0.45);
                }

                return canvas;
            }

            case glass::PaletteArtShape::Dial:
            {
                // The rim, and a pointer out to eleven o'clock, so a knob reads as something that
                // turns rather than as a lamp.
                controls::Canvas canvas{};

                canvas.Width(art.Width);
                canvas.Height(art.Height);

                shapes::Ellipse rim{};

                rim.Width(art.Width);
                rim.Height(art.Height);
                rim.UseLayoutRounding(false);
                rim.Stroke(AccentAt(art.StrokeOpacity));
                rim.StrokeThickness(2.0);

                auto const radius = art.Width / 2.0;
                auto const reach = radius - 3.0;

                shapes::Line pointer{};

                pointer.X1(radius);
                pointer.Y1(radius);
                pointer.X2(radius - 0.5 * reach);
                pointer.Y2(radius - 0.8660254037844386 * reach);
                pointer.UseLayoutRounding(false);
                pointer.Stroke(AccentAt(1.0));
                pointer.StrokeThickness(1.8);
                pointer.StrokeStartLineCap(media::PenLineCap::Round);
                pointer.StrokeEndLineCap(media::PenLineCap::Round);

                canvas.Children().Append(rim);
                canvas.Children().Append(pointer);

                return canvas;
            }

            case glass::PaletteArtShape::Platter:
            {
                // The edge of the record, the ring round its label and the spindle.
                controls::Grid grid{};

                grid.Width(art.Width);
                grid.Height(art.Height);

                auto const ring = [&](double diameter, double thickness, bool filled)
                    {
                        shapes::Ellipse shape{};

                        shape.Width(diameter);
                        shape.Height(diameter);
                        shape.HorizontalAlignment(xaml::HorizontalAlignment::Center);
                        shape.VerticalAlignment(xaml::VerticalAlignment::Center);
                        shape.UseLayoutRounding(false);

                        if (filled)
                        {
                            shape.Fill(AccentAt(1.0));
                        }
                        else
                        {
                            shape.Stroke(AccentAt(art.StrokeOpacity));
                            shape.StrokeThickness(thickness);
                        }

                        grid.Children().Append(shape);
                    };

                ring(art.Width, 1.6, false);
                ring(art.Width * 0.44, 1.2, false);
                ring(2.4, 0.0, true);

                return grid;
            }

            case glass::PaletteArtShape::CrosshairField:
            {
                // The field, and a crosshair with its dot down and to the left, the way an XY pad
                // on the page looks once it holds a value.
                controls::Canvas canvas{};

                canvas.Width(art.Width);
                canvas.Height(art.Height);

                shapes::Rectangle field{};

                field.Width(art.Width);
                field.Height(art.Height);
                field.RadiusX(art.CornerRadius);
                field.RadiusY(art.CornerRadius);
                field.UseLayoutRounding(false);
                field.Stroke(AccentAt(art.StrokeOpacity));
                field.StrokeThickness(1.0);

                canvas.Children().Append(field);

                auto const x = art.Width * 0.34;
                auto const y = art.Height * 0.66;

                auto const line = [&](double x1, double y1, double x2, double y2)
                    {
                        shapes::Line shape{};

                        shape.X1(x1);
                        shape.Y1(y1);
                        shape.X2(x2);
                        shape.Y2(y2);
                        shape.UseLayoutRounding(false);
                        shape.Stroke(AccentAt(0.55));
                        shape.StrokeThickness(1.0);

                        canvas.Children().Append(shape);
                    };

                line(1.5, y, art.Width - 1.5, y);
                line(x, 1.5, x, art.Height - 1.5);

                constexpr double dot = 5.0;

                shapes::Ellipse puck{};

                puck.Width(dot);
                puck.Height(dot);
                puck.UseLayoutRounding(false);
                puck.Fill(AccentAt(1.0));

                controls::Canvas::SetLeft(puck, x - dot / 2.0);
                controls::Canvas::SetTop(puck, y - dot / 2.0);

                canvas.Children().Append(puck);

                return canvas;
            }

            case glass::PaletteArtShape::Stick:
            {
                // The round field, an arrow at each compass point for the ways it moves, and the
                // ball in the middle it springs back to.
                controls::Canvas canvas{};

                canvas.Width(art.Width);
                canvas.Height(art.Height);

                shapes::Ellipse field{};

                field.Width(art.Width);
                field.Height(art.Height);
                field.UseLayoutRounding(false);
                field.Stroke(AccentAt(art.StrokeOpacity));
                field.StrokeThickness(1.4);

                canvas.Children().Append(field);

                auto const center = art.Width / 2.0;
                auto const tip = center - 2.4;
                auto const back = tip - 2.2;
                constexpr double halfBase = 1.6;

                constexpr double directions[][2]{ { 0.0, -1.0 }, { 1.0, 0.0 }, { 0.0, 1.0 }, { -1.0, 0.0 } };

                for (auto const& direction : directions)
                {
                    auto const dx = direction[0];
                    auto const dy = direction[1];

                    media::PointCollection points{};

                    points.Append({ static_cast<float>(center + dx * tip), static_cast<float>(center + dy * tip) });
                    points.Append({ static_cast<float>(center + dx * back - dy * halfBase), static_cast<float>(center + dy * back + dx * halfBase) });
                    points.Append({ static_cast<float>(center + dx * back + dy * halfBase), static_cast<float>(center + dy * back - dx * halfBase) });

                    shapes::Polygon arrow{};

                    arrow.Points(points);
                    arrow.UseLayoutRounding(false);
                    arrow.Fill(AccentAt(art.StrokeOpacity));

                    canvas.Children().Append(arrow);
                }

                constexpr double ball = 4.8;

                shapes::Ellipse knob{};

                knob.Width(ball);
                knob.Height(ball);
                knob.UseLayoutRounding(false);
                knob.Fill(AccentAt(1.0));

                controls::Canvas::SetLeft(knob, center - ball / 2.0);
                controls::Canvas::SetTop(knob, center - ball / 2.0);

                canvas.Children().Append(knob);

                return canvas;
            }

            case glass::PaletteArtShape::FaderCap:
            {
                // The slot, and the cap riding it a third of the way up, so a fader does not read
                // as a meter.
                controls::Canvas canvas{};

                canvas.Width(art.Width);
                canvas.Height(art.Height);

                constexpr double slotWidth = 5.0;
                constexpr double capHeight = 5.0;

                shapes::Rectangle slot{};

                slot.Width(slotWidth);
                slot.Height(art.Height);
                slot.RadiusX(std::min(art.CornerRadius, slotWidth / 2.0));
                slot.RadiusY(std::min(art.CornerRadius, slotWidth / 2.0));
                slot.UseLayoutRounding(false);
                slot.Fill(AccentAt(art.FillOpacity));

                controls::Canvas::SetLeft(slot, (art.Width - slotWidth) / 2.0);

                shapes::Rectangle cap{};

                cap.Width(art.Width);
                cap.Height(capHeight);
                cap.RadiusX(1.5);
                cap.RadiusY(1.5);
                cap.UseLayoutRounding(false);
                cap.Fill(AccentAt(1.0));

                controls::Canvas::SetTop(cap, art.Height * 0.70 - capHeight / 2.0);

                canvas.Children().Append(slot);
                canvas.Children().Append(cap);

                return canvas;
            }

            case glass::PaletteArtShape::Rectangle:
            default:
            {
                shapes::Rectangle shape{};

                shape.Width(art.Width);
                shape.Height(art.Height);
                shape.RadiusX(art.CornerRadius);
                shape.RadiusY(art.CornerRadius);
                shape.UseLayoutRounding(false);

                if (art.StrokeOpacity > 0.0)
                {
                    shape.Stroke(AccentAt(art.StrokeOpacity));
                    shape.StrokeThickness(1.0);
                }

                if (art.FillOpacity > 0.0)
                {
                    shape.Fill(AccentAt(art.FillOpacity));
                }

                return shape;
            }
            }
        }
    }

    void EditorWindow::BuildPalette()
    {
        try
        {
            auto const query = std::wstring{ PaletteSearch().Text() };

            PaletteHost().Children().Clear();
            m_paletteTiles.clear();

            auto const& application = xaml::Application::Current().Resources();

            auto const styleNamed = [&application](wchar_t const* key)
                {
                    return application.Lookup(box_value(key)).as<xaml::Style>();
                };

            auto const headingStyle = styleNamed(L"GroupHeadingStyle");
            auto const tileShapeStyle = styleNamed(L"PaletteTileShapeStyle");
            auto const tileStyle = styleNamed(L"PaletteTileStyle");

            for (auto const& group : glass::PaletteGroups())
            {
                // Built before the heading, because a group whose every tile was filtered out by
                // the search box must not leave its heading behind.
                controls::Grid grid{};

                grid.ColumnSpacing(3);
                grid.RowSpacing(3);

                for (int32_t column = 0; column < 3; ++column)
                {
                    controls::ColumnDefinition definition{};
                    definition.Width(xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
                    grid.ColumnDefinitions().Append(definition);
                }

                int32_t index{ 0 };

                for (auto const& entry : glass::Palette())
                {
                    if (entry.GroupResourceKey != group)
                    {
                        continue;
                    }

                    auto const name = std::wstring{ resources::GetString(entry.NameResourceKey) };

                    if (!Matches(name, query))
                    {
                        continue;
                    }

                    auto const row = index / 3;
                    auto const column = index % 3;

                    while (static_cast<int32_t>(grid.RowDefinitions().Size()) <= row)
                    {
                        controls::RowDefinition definition{};
                        definition.Height(xaml::GridLengthHelper::FromValueAndType(0, xaml::GridUnitType::Auto));
                        grid.RowDefinitions().Append(definition);
                    }

                    controls::Primitives::ToggleButton tile{};

                    tile.Height(52);
                    tile.Padding({ 1, 0, 1, 0 });
                    tile.MinWidth(0);
                    tile.CornerRadius({ 5, 5, 5, 5 });
                    tile.BorderThickness({ 0, 0, 0, 0 });
                    tile.Background(media::SolidColorBrush{ winrt::Windows::UI::Colors::Transparent() });
                    tile.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                    tile.HorizontalContentAlignment(xaml::HorizontalAlignment::Stretch);
                    tile.VerticalContentAlignment(xaml::VerticalAlignment::Stretch);
                    tile.IsEnabled(!entry.IsComing);
                    tile.Style(tileStyle);

                    controls::Grid content{};

                    shapes::Rectangle face{};

                    face.Style(tileShapeStyle);

                    // Understated at rest and highlighted by its edge when armed. A row of
                    // fully drawn buttons reads as chrome; what is wanted here is a set of
                    // tools, one of which is on.
                    face.Fill(application.Lookup(box_value(L"SubtleFillColorSecondaryBrush")).as<media::Brush>());
                    face.Stroke(application.Lookup(box_value(L"ControlStrokeColorDefaultBrush")).as<media::Brush>());

                    controls::Grid stack{};

                    // Two fixed rows, not a stack: the art box is always the same height, so
                    // every label in a row sits on the same line whatever its art is shaped
                    // like. Stacking them left the captions stepping up and down across a row.
                    controls::RowDefinition artRow{};
                    artRow.Height(xaml::GridLengthHelper::FromPixels(26));

                    controls::RowDefinition captionRow{};
                    captionRow.Height(xaml::GridLengthHelper::FromPixels(22));

                    stack.RowDefinitions().Append(artRow);
                    stack.RowDefinitions().Append(captionRow);
                    stack.Margin({ 0, 2, 0, 0 });

                    auto art = BuildTileArt(entry.Art);

                    if (auto const element = art.try_as<xaml::FrameworkElement>())
                    {
                        element.HorizontalAlignment(xaml::HorizontalAlignment::Center);
                        element.VerticalAlignment(xaml::VerticalAlignment::Center);

                        controls::Grid::SetRow(element, 0);
                    }

                    controls::TextBlock caption{};

                    caption.Text(name);
                    caption.FontSize(9);
                    caption.TextAlignment(xaml::TextAlignment::Center);
                    caption.TextWrapping(xaml::TextWrapping::Wrap);
                    caption.LineHeight(10);
                    caption.LineStackingStrategy(xaml::LineStackingStrategy::BlockLineHeight);
                    caption.HorizontalAlignment(xaml::HorizontalAlignment::Center);
                    caption.VerticalAlignment(xaml::VerticalAlignment::Top);
                    caption.Foreground(application
                        .Lookup(box_value(L"TextFillColorTertiaryBrush")).as<media::Brush>());

                    controls::Grid::SetRow(caption, 1);

                    stack.Children().Append(art);
                    stack.Children().Append(caption);

                    content.Children().Append(face);
                    content.Children().Append(stack);

                    tile.Content(content);

                    // A kind the design calls for that is not built yet is shown rather than
                    // hidden, so nobody hunts for it, and says so rather than looking broken.
                    xaml::Automation::AutomationProperties::SetName(tile, winrt::hstring{
                        entry.IsComing
                            ? std::wstring{ resources::FormatString(L"PaletteComingFormat", name) }
                            : name });

                    controls::ToolTipService::SetToolTip(tile, box_value(winrt::hstring{
                        entry.IsComing
                            ? std::wstring{ resources::FormatString(L"PaletteComingFormat", name) }
                            : name }));

                    if (!entry.IsComing)
                    {
                        auto const kind = entry.Kind;
                        auto weak = get_weak();

                        tile.Click([weak, kind](auto&& sender, auto&&)
                            {
                                if (auto strong = weak.get())
                                {
                                    strong->OnPaletteTileClick(
                                        sender.template as<controls::Primitives::ToggleButton>(), kind);
                                }
                            });

                        tile.DoubleTapped([weak, kind](auto&&, auto&& tapArgs)
                            {
                                tapArgs.Handled(true);

                                if (auto strong = weak.get())
                                {
                                    strong->OnPaletteTileDoubleTapped(kind);
                                }
                            });

                        // A ToggleButton marks pointer presses and releases handled in its own
                        // OnPointerPressed, so an ordinary subscription here never runs. These
                        // have to ask for handled events too.
                        tile.AddHandler(
                            xaml::UIElement::PointerPressedEvent(),
                            box_value(xaml::Input::PointerEventHandler{
                                [weak, kind](auto&& sender, auto&& pointerArgs)
                                {
                                    if (auto strong = weak.get())
                                    {
                                        strong->OnPaletteTilePressed(
                                            sender.template as<controls::Primitives::ToggleButton>(),
                                            kind,
                                            pointerArgs);
                                    }
                                } }),
                            true);

                        tile.AddHandler(
                            xaml::UIElement::PointerMovedEvent(),
                            box_value(xaml::Input::PointerEventHandler{
                                [weak](auto&&, auto&& pointerArgs)
                                {
                                    if (auto strong = weak.get())
                                    {
                                        strong->OnPaletteTileMoved(pointerArgs);
                                    }
                                } }),
                            true);

                        tile.AddHandler(
                            xaml::UIElement::PointerReleasedEvent(),
                            box_value(xaml::Input::PointerEventHandler{
                                [weak](auto&& sender, auto&& pointerArgs)
                                {
                                    if (auto strong = weak.get())
                                    {
                                        strong->OnPaletteTileReleased(
                                            sender.template as<controls::Primitives::ToggleButton>(),
                                            pointerArgs);
                                    }
                                } }),
                            true);

                        tile.AddHandler(
                            xaml::UIElement::PointerCaptureLostEvent(),
                            box_value(xaml::Input::PointerEventHandler{
                                [weak](auto&& sender, auto&& pointerArgs)
                                {
                                    if (auto strong = weak.get())
                                    {
                                        strong->OnPaletteTileReleased(
                                            sender.template as<controls::Primitives::ToggleButton>(),
                                            pointerArgs);
                                    }
                                } }),
                            true);

                        m_paletteTiles.push_back({ tile, kind });
                    }

                    controls::Grid::SetRow(tile, row);
                    controls::Grid::SetColumn(tile, column);

                    grid.Children().Append(tile);

                    index++;
                }

                if (index == 0)
                {
                    continue;
                }

                controls::TextBlock heading{};

                heading.Style(headingStyle);
                heading.Text(resources::GetString(group));

                PaletteHost().Children().Append(heading);
                PaletteHost().Children().Append(grid);
            }

            SyncPaletteSelection();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to build the palette.")
    }

    // Exactly one tile is armed at a time, and clicking the armed one puts the tool away.
    _Use_decl_annotations_
    void EditorWindow::OnPaletteTileClick(
        controls::Primitives::ToggleButton const& sender,
        glass::ControlKind kind)
    {
        UNREFERENCED_PARAMETER(sender);

        try
        {
            m_hasArmedKind = !(m_hasArmedKind && m_armedKind == kind);
            m_armedKind = kind;

            SyncPaletteSelection();
            UpdateStatusBar();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to arm a palette entry.")
    }

    // Double click is the other instinct: put one on the page now, at the first free spot,
    // without having to aim.
    _Use_decl_annotations_
    void EditorWindow::OnPaletteTileDoubleTapped(_In_ glass::ControlKind kind)
    {
        try
        {
            m_hasArmedKind = false;
            SyncPaletteSelection();

            if (!m_editor.AddControlAtFreeSpot(kind).empty())
            {
                RebuildSurface();
                RebuildOutline();
                RefreshInspector();
                UpdateStatusBar();
                MarkChanged();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to add a control.")
    }

    // ---------------------------------------------------------------- dragging a tile onto the page

    _Use_decl_annotations_
    void EditorWindow::OnPaletteTilePressed(
        controls::Primitives::ToggleButton const& sender,
        glass::ControlKind kind,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        if (!m_loaded || m_tryMode)
        {
            return;
        }

        try
        {
            auto const point = args.GetCurrentPoint(sender);

            m_paletteDragKind = kind;
            m_palettePointerId = point.PointerId();
            m_paletteDragId.clear();
            m_paletteDragPressed = sender.CapturePointer(args.Pointer());
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to start a palette drag.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnPaletteTileMoved(xaml::Input::PointerRoutedEventArgs const& args)
    {
        if (!m_paletteDragPressed)
        {
            return;
        }

        try
        {
            // Measured against the canvas even though the TILE holds the pointer. That is the
            // whole trick: capture keeps the gesture on the tile, and asking for the point
            // relative to the overlay converts it through the zoom for free.
            auto const point = args.GetCurrentPoint(OverlayCanvas());

            if (point.PointerId() != m_palettePointerId)
            {
                return;
            }

            auto const x = point.Position().X;
            auto const y = point.Position().Y;

            auto const overCanvas =
                x >= 0.0 && y >= 0.0 &&
                x <= OverlayCanvas().ActualWidth() &&
                y <= OverlayCanvas().ActualHeight();

            auto const pageX = PointToPageX(x);
            auto const pageY = PointToPageY(y);

            if (m_paletteDragId.empty())
            {
                if (!overCanvas)
                {
                    return;
                }

                // Real from the moment it reaches the page, not a ghost that turns into a
                // control on release. Centered under the pointer, so what is being aimed is the
                // control rather than its top left corner.
                auto const id = m_editor.AddControlCentered(m_paletteDragKind, pageX, pageY);

                if (id.empty())
                {
                    return;
                }

                m_paletteDragId = id;
                m_paletteDragStartPageX = pageX;
                m_paletteDragStartPageY = pageY;

                m_hasArmedKind = false;
                SyncPaletteSelection();

                RebuildSurface();
                RebuildOutline();
                RefreshInspector();
                UpdateStatusBar();

                m_editor.BeginDrag();
                return;
            }

            m_editor.UpdateDrag(pageX - m_paletteDragStartPageX, pageY - m_paletteDragStartPageY);

            MoveDraggedItems();
            UpdateOverlay();
            RefreshInspectorGeometry();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to drag a palette entry onto the page.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnPaletteTileReleased(
        controls::Primitives::ToggleButton const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        try
        {
            if (sender != nullptr)
            {
                sender.ReleasePointerCapture(args.Pointer());
            }

            if (!m_paletteDragPressed)
            {
                return;
            }

            m_paletteDragPressed = false;

            if (m_paletteDragId.empty())
            {
                return;
            }

            m_paletteDragId.clear();

            m_editor.EndDrag();

            RebuildSurface();
            RebuildOutline();
            RefreshInspector();
            UpdateOffPageBar();
            UpdateStatusBar();
            MarkChanged();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to finish a palette drag.")
    }

    void EditorWindow::SyncPaletteSelection()
    {
        for (auto const& [tile, kind] : m_paletteTiles)
        {
            tile.IsChecked(m_hasArmedKind && kind == m_armedKind);
        }
    }

    std::wstring EditorWindow::NameForArmedKind() const
    {
        return NameForKind(m_armedKind);
    }

    _Use_decl_annotations_
    void EditorWindow::OnPaletteSearchChanged(
        controls::AutoSuggestBox const& sender,
        controls::AutoSuggestBoxTextChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        if (args.Reason() == controls::AutoSuggestionBoxTextChangeReason::ProgrammaticChange)
        {
            return;
        }

        BuildPalette();
    }

    _Use_decl_annotations_
    void EditorWindow::OnAddToPageClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            // The keyboard path. Somebody who cannot click the page still has to be able to put
            // a control on it, so this places whichever palette tile is armed at the first free
            // spot rather than waiting for a pointer that is never coming.
            auto const kind = m_hasArmedKind ? m_armedKind : glass::ControlKind::Knob;

            m_hasArmedKind = false;
            SyncPaletteSelection();

            if (!m_editor.AddControlAtFreeSpot(kind).empty())
            {
                RebuildSurface();
                RebuildOutline();
                RefreshInspector();
                UpdateStatusBar();
                MarkChanged();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to add a control to the page.")
    }

    // ---------------------------------------------------------------- outline

    void EditorWindow::RebuildOutline()
    {
        try
        {
            auto const previous = m_updatingInspector;
            m_updatingInspector = true;

            auto const& document = m_editor.Document();

            auto items = winrt::single_threaded_observable_vector<foundation::IInspectable>();

            // Reading order is the keyboard order, because that is the property the layout
            // actually carries and the one somebody can change. A group is a heading with its
            // members under it, one step in, so the list shows what moves together.
            for (auto const& row : m_editor.OutlineRows())
            {
                ::midiglass::EditorItemData data{};

                data.Key = row.Id;
                data.IndentPixels = OutlineIndent + row.Depth * OutlineGroupIndent;

                auto item = winrt::make_self<EditorItem>();

                // A group goes by its number until somebody names it.
                auto const groupName = row.GroupNumber == 0
                    ? std::wstring{}
                    : row.GroupName.empty()
                        ? std::wstring{ resources::FormatString(L"OutlineGroupFormat", std::to_wstring(row.GroupNumber)) }
                        : row.GroupName;

                if (row.IsGroupHeading)
                {
                    data.IsGroup = true;
                    data.DisplayName = groupName;
                    data.AccessibleName = std::wstring{ resources::FormatString(
                        L"OutlineGroupAccessibleFormat", groupName, std::to_wstring(row.MemberCount)) };

                    item->Update(data);

                    // The same mark as the toolbar's Group button.
                    controls::FontIcon icon{};

                    icon.Glyph(L"\uE71D");
                    icon.FontSize(13.0);
                    icon.Foreground(AccentAt(1.0));
                    icon.HorizontalAlignment(xaml::HorizontalAlignment::Center);
                    icon.VerticalAlignment(xaml::VerticalAlignment::Center);

                    item->Art(icon);

                    items.Append(*item);
                    continue;
                }

                auto const* const control = document.FindControl(row.Id);

                if (control == nullptr)
                {
                    continue;
                }

                data.DisplayName = control->Label.empty()
                    ? NameForKind(control->Kind)
                    : control->Label;

                if (row.GroupNumber > 0)
                {
                    data.AccessibleName = std::wstring{ resources::FormatString(
                        L"OutlineMemberAccessibleFormat", data.DisplayName, groupName) };
                }

                data.Detail = NameForKind(control->Kind);
                data.Glyph = std::wstring(1, GlyphForKind(control->Kind));
                data.Badge = std::to_wstring(control->KeyboardOrder);

                data.IsOutsidePage = glass::IsOutsidePage(
                    { control->X, control->Y, control->Width, control->Height },
                    document.PageWidth,
                    document.PageHeight);

                data.IsLocked = control->Locked;

                if (control->Locked)
                {
                    data.AccessibleName = std::wstring{ resources::FormatString(
                        L"OutlineLockedAccessibleFormat",
                        data.AccessibleName.empty() ? data.DisplayName : data.AccessibleName) };
                }

                item->Update(data);

                // The same miniature the palette draws, built per row because a XAML element
                // belongs to one parent.
                auto art = BuildTileArt(ArtForKind(control->Kind));

                if (auto const element = art.try_as<xaml::FrameworkElement>())
                {
                    element.HorizontalAlignment(xaml::HorizontalAlignment::Center);
                    element.VerticalAlignment(xaml::VerticalAlignment::Center);
                }

                item->Art(art);

                items.Append(*item);
            }

            OutlineList().ItemsSource(items);

            m_updatingInspector = previous;

            SelectOutlineRowForSelection();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to rebuild the outline.")
    }

    // A group whose every member is selected shows as its heading. Anything else selected shows
    // as its own row.
    void EditorWindow::SelectOutlineRowForSelection()
    {
        try
        {
            auto const previous = m_updatingInspector;
            m_updatingInspector = true;

            auto const selected = OutlineList().SelectedItems();

            selected.Clear();

            if (auto const source = OutlineList().ItemsSource()
                .try_as<collections::IVector<foundation::IInspectable>>())
            {
                for (auto const& entry : source)
                {
                    auto const item = entry.try_as<midiglass::EditorItem>();

                    if (item == nullptr)
                    {
                        continue;
                    }

                    auto const key = std::wstring{ item.Key() };

                    auto show = false;

                    if (item.IsGroup())
                    {
                        show = m_editor.IsWholeGroupSelected(key);
                    }
                    else if (m_editor.IsSelected(key))
                    {
                        auto const* const control = m_editor.Document().FindControl(key);

                        show = control == nullptr ||
                            control->GroupId.empty() ||
                            !m_editor.IsWholeGroupSelected(control->GroupId);
                    }

                    if (show)
                    {
                        selected.Append(entry);
                    }
                }
            }

            m_updatingInspector = previous;
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to follow the selection in the outline.")
    }

    // A group's heading picks every member. A member's own row picks that one alone, the way a
    // second click on a group does on the canvas.
    _Use_decl_annotations_
    void EditorWindow::OnOutlineSelectionChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        if (m_updatingInspector)
        {
            return;
        }

        try
        {
            // Replacing the rows raises this too, naming rows that are gone. That is not somebody
            // picking anything, and reading it as a pick would empty the selection.
            if (auto const source = OutlineList().ItemsSource()
                .try_as<collections::IVector<foundation::IInspectable>>())
            {
                uint32_t index{ 0 };

                for (auto const& removed : args.RemovedItems())
                {
                    if (!source.IndexOf(removed, index))
                    {
                        return;
                    }
                }
            }

            m_editor.ClearSelection();

            for (auto const& entry : OutlineList().SelectedItems())
            {
                auto const item = entry.try_as<midiglass::EditorItem>();

                if (item == nullptr)
                {
                    continue;
                }

                if (item.IsGroup())
                {
                    m_editor.AddGroupToSelection(std::wstring{ item.Key() });
                }
                else
                {
                    m_editor.AddToSelection(std::wstring{ item.Key() });
                }
            }

            UpdateOverlay();
            RefreshInspector();
            UpdateStatusBar();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to select from the outline.")
    }

    // A right click on a row that is not picked picks it, the way a file list does, so the menu
    // is about the row under the pointer rather than about whatever was picked before.
    _Use_decl_annotations_
    void EditorWindow::OnOutlineContextRequested(
        xaml::UIElement const& sender,
        xaml::Input::ContextRequestedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        try
        {
            controls::ListViewItem container{ nullptr };

            for (auto element = args.OriginalSource().try_as<xaml::DependencyObject>();
                element != nullptr && container == nullptr;
                element = media::VisualTreeHelper::GetParent(element))
            {
                container = element.try_as<controls::ListViewItem>();
            }

            if (container == nullptr || container.IsSelected())
            {
                return;
            }

            auto const item = OutlineList().ItemFromContainer(container);

            // Emptied quietly, so the pick below is the one change the editor hears about.
            auto const previous = m_updatingInspector;
            m_updatingInspector = true;
            OutlineList().SelectedItems().Clear();
            m_updatingInspector = previous;

            OutlineList().SelectedItems().Append(item);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to pick the row under the menu.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnOutlineMenuOpening(foundation::IInspectable const& sender, foundation::IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            // The same rules as the toolbar's Group button, which Try mode turns off too.
            OutlineGroupItem().IsEnabled(
                !m_tryMode && m_editor.Selection().size() > 1 && !m_editor.SelectionIsOneGroup());

            OutlineUngroupItem().IsEnabled(!m_tryMode && m_editor.SelectionHasGroup());
            OutlineRenameItem().IsEnabled(CanRename());
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to fill in the outline menu.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnOutlineMoveUp(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        auto const* const control = SingleSelectedControl();

        if (control != nullptr && m_editor.SetKeyboardOrder(control->Id, control->KeyboardOrder - 1))
        {
            RebuildOutline();
            UpdateOverlay();
            RefreshInspector();
            MarkChanged();
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnOutlineMoveDown(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        auto const* const control = SingleSelectedControl();

        if (control != nullptr && m_editor.SetKeyboardOrder(control->Id, control->KeyboardOrder + 1))
        {
            RebuildOutline();
            UpdateOverlay();
            RefreshInspector();
            MarkChanged();
        }
    }
}
