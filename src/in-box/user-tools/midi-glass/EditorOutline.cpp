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

            auto const* const page = m_editor.CurrentPage();

            auto items = winrt::single_threaded_observable_vector<foundation::IInspectable>();

            if (page != nullptr)
            {
                auto const& document = m_editor.Document();

                // Reading order is the keyboard order, because that is the property the layout
                // actually carries and the one somebody can change.
                std::vector<glass::Control const*> ordered{};

                for (auto const& control : page->Controls)
                {
                    ordered.push_back(&control);
                }

                std::stable_sort(
                    ordered.begin(),
                    ordered.end(),
                    [](glass::Control const* left, glass::Control const* right)
                    {
                        return left->KeyboardOrder < right->KeyboardOrder;
                    });

                for (auto const* const control : ordered)
                {
                    ::midiglass::EditorItemData data{};

                    data.Key = control->Id;
                    data.DisplayName = control->Label.empty()
                        ? NameForKind(control->Kind)
                        : control->Label;

                    data.Detail = NameForKind(control->Kind);
                    data.Glyph = std::wstring(1, GlyphForKind(control->Kind));
                    data.Badge = std::to_wstring(control->KeyboardOrder);
                    data.IndentPixels = 4.0;

                    data.IsOutsidePage = glass::IsOutsidePage(
                        { control->X, control->Y, control->Width, control->Height },
                        document.PageWidth,
                        document.PageHeight);

                    auto item = winrt::make_self<EditorItem>();
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
            }

            OutlineList().ItemsSource(items);

            m_updatingInspector = previous;

            SelectOutlineRowForSelection();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to rebuild the outline.")
    }

    void EditorWindow::SelectOutlineRowForSelection()
    {
        try
        {
            auto const previous = m_updatingInspector;
            m_updatingInspector = true;

            auto const* const control = SingleSelectedControl();

            if (control == nullptr)
            {
                OutlineList().SelectedIndex(-1);
            }
            else if (auto const source = OutlineList().ItemsSource()
                .try_as<collections::IVector<foundation::IInspectable>>())
            {
                for (uint32_t index = 0; index < source.Size(); ++index)
                {
                    auto const item = source.GetAt(index).try_as<midiglass::EditorItem>();

                    if (item != nullptr && std::wstring{ item.Key() } == control->Id)
                    {
                        OutlineList().SelectedIndex(static_cast<int32_t>(index));
                        break;
                    }
                }
            }

            m_updatingInspector = previous;
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to follow the selection in the outline.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnOutlineSelectionChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        try
        {
            auto const item = OutlineList().SelectedItem().try_as<midiglass::EditorItem>();

            if (item == nullptr)
            {
                return;
            }

            m_editor.SelectOnly(std::wstring{ item.Key() });

            UpdateOverlay();
            RefreshInspector();
            UpdateStatusBar();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to select from the outline.")
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
