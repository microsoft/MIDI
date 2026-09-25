// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The editor canvas: the work area, the page, the controls drawn exactly as they will look when
// the layout runs, and the overlay on top of them that carries selection, handles and guides.
//
// The controls are painted by the same SurfaceRenderer the runtime window uses. That is on
// purpose: a customer moving a fader two pixels has to be looking at the fader, not at a
// stand-in for it. In Edit mode nothing on the surface takes input, because hit testing in the
// editor is arithmetic against the document rather than XAML routing, and that arithmetic is
// what the snapping and the handles are already built on. Try mode hands the pointer back to
// the surface; see EditorTryMode.cpp.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "StringResources.h"
#include "PageTemplates.h"
#include "GlassControl.h"

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        namespace shapes = ::winrt::Microsoft::UI::Xaml::Shapes;

        constexpr double HandleSize = 8.0;

        // Smaller than a control's, so the two sets are never mistaken for each other when a
        // label sits right against the control it belongs to.
        constexpr double LabelHandleSize = 6.0;

        constexpr double MinimumCanvasScale = 0.1;
        constexpr double MaximumCanvasScale = 2.0;

        // What Fit leaves around the page, in screen pixels, on the tighter axis. Whatever is
        // left on the other axis is canvas, not padding.
        constexpr double FitPadding = 20.0;

        // Keeps a work area sized from the viewport just inside it, so it can never be what
        // brings a scroll bar in.
        constexpr double ViewportSlack = 2.0;

        // Room to get hold of a control that has been dragged off the page.
        constexpr double OffPageMargin = 64.0;

        // Below this many screen pixels apart, a grid layer is noise rather than a grid.
        constexpr double MinimumGridSpacingOnScreen = 7.0;

        // How far a pointer has to travel before a press becomes a drag. Below this a press and
        // release is a click, so selecting a control does not nudge it by a pixel.
        constexpr double DragThreshold = 3.0;

        winrt::Windows::UI::Color ToColor(_In_ glass::ThemeColor const& color) noexcept
        {
            return winrt::Windows::UI::ColorHelper::FromArgb(color.A, color.R, color.G, color.B);
        }

        media::Brush BrushNamed(_In_ wchar_t const* key)
        {
            return xaml::Application::Current().Resources().Lookup(box_value(key)).as<media::Brush>();
        }

        glass::EditRect RectOf(_In_ glass::Control const& control) noexcept
        {
            return { control.X, control.Y, control.Width, control.Height };
        }
    }

    // ---------------------------------------------------------------- building

    void EditorWindow::BuildPage()
    {
        try
        {
            UpdateDeckBrushes();
            RebuildSurface();
            RebuildOutline();
            UpdateStatusBar();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to draw the page.")
    }

    void EditorWindow::RebuildSurface()
    {
        try
        {
            auto const& document = m_editor.Document();

            UpdateWorkArea();

            m_renderer.Teardown();
            m_renderer.Build(SurfaceCanvas(), document, m_theme, m_editor.PageIndex());

            auto weak = get_weak();

            m_renderer.DescribeValue = [weak](uint32_t controlIndex, double value) -> std::wstring
                {
                    auto strong = weak.get();

                    return strong != nullptr && strong->m_player != nullptr
                        ? strong->m_player->DescribeValue(controlIndex, value)
                        : std::wstring{};
                };

            // Nothing on the surface takes input in Edit mode. Every press belongs to the
            // overlay, which knows about selection, handles and guides. Try mode flips it, and
            // has to be re-applied here because a rebuild makes new elements.
            ApplySurfaceInputMode();

            ApplyCanvasScale();
            RebuildGrid();
            UpdateOverlay();
            UpdateOffPageBar();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to rebuild the surface.")
    }

    // Everything the eye can see inside the canvas region is canvas, and it is the same surface
    // all the way out: the work area is the page and its margin, grown to hold any control
    // parked outside, and then grown again to cover the viewport. Before that last step it
    // stopped at the page's margin, which put a hard edge partway across an empty-looking area -
    // a control dragged past it was clipped, and at any zoom above Fit there was nowhere at all
    // to put one.
    //
    // Returns true when the area moved or resized, so a caller knows the grid and the overlay
    // have to be redrawn against the new origin.
    bool EditorWindow::UpdateWorkArea()
    {
        auto const& document = m_editor.Document();

        // The page and nothing around it. A fixed margin here would be space Fit has to find
        // room for, which is the opposite of "the page as large as it goes": the canvas that
        // surrounds the page is whatever the viewport has left over, below.
        auto work = glass::EditRect{
            0.0,
            0.0,
            static_cast<double>(document.PageWidth),
            static_cast<double>(document.PageHeight) };

        // A control parked outside the page has to stay reachable, so the area grows to hold it
        // with room to grab it by. Only for the part that is genuinely outside: growing by the
        // margin for a control that merely sits NEAR an edge made the work area bigger than the
        // viewport at Fit, which brought scroll bars in and pushed the page off center.
        if (auto const* const page = m_editor.CurrentPage())
        {
            auto left = work.X;
            auto top = work.Y;
            auto right = work.Right();
            auto bottom = work.Bottom();

            for (auto const& control : page->Controls)
            {
                if (control.X < 0.0) { left = std::min(left, control.X - OffPageMargin); }
                if (control.Y < 0.0) { top = std::min(top, control.Y - OffPageMargin); }

                if (control.X + control.Width > document.PageWidth)
                {
                    right = std::max(right, control.X + control.Width + OffPageMargin);
                }

                if (control.Y + control.Height > document.PageHeight)
                {
                    bottom = std::max(bottom, control.Y + control.Height + OffPageMargin);
                }
            }

            work = { left, top, right - left, bottom - top };
        }

        auto const viewportWidth = CanvasScroll().ActualWidth();
        auto const viewportHeight = CanvasScroll().ActualHeight();

        if (viewportWidth > ViewportSlack && viewportHeight > ViewportSlack && m_canvasScale > 0.0)
        {
            // A hair under the viewport, so a work area sized by the viewport can never be the
            // thing that brings a scroll bar in and shrinks the viewport that produced it.
            auto const visibleWidth = (viewportWidth - ViewportSlack) / m_canvasScale;
            auto const visibleHeight = (viewportHeight - ViewportSlack) / m_canvasScale;

            auto const centerX = document.PageWidth / 2.0;
            auto const centerY = document.PageHeight / 2.0;

            auto const left = std::min(work.X, centerX - visibleWidth / 2.0);
            auto const top = std::min(work.Y, centerY - visibleHeight / 2.0);
            auto const right = std::max(work.Right(), centerX + visibleWidth / 2.0);
            auto const bottom = std::max(work.Bottom(), centerY + visibleHeight / 2.0);

            work = { left, top, right - left, bottom - top };
        }

        auto const same =
            std::abs(work.X - m_workArea.X) < 0.5 &&
            std::abs(work.Y - m_workArea.Y) < 0.5 &&
            std::abs(work.Width - m_workArea.Width) < 0.5 &&
            std::abs(work.Height - m_workArea.Height) < 0.5;

        m_workArea = work;

        WorkAreaHost().Width(m_workArea.Width);
        WorkAreaHost().Height(m_workArea.Height);

        SurfaceCanvas().Width(m_workArea.Width);
        SurfaceCanvas().Height(m_workArea.Height);

        OverlayCanvas().Width(m_workArea.Width);
        OverlayCanvas().Height(m_workArea.Height);

        // The page is the bright rectangle; the rest is visibly working space.
        PageDeck().Width(document.PageWidth);
        PageDeck().Height(document.PageHeight);
        PageDeck().Margin({ -m_workArea.X, -m_workArea.Y, 0, 0 });

        GridCanvas().Width(m_workArea.Width);
        GridCanvas().Height(m_workArea.Height);

        // The surface is drawn in page coordinates, so it moves with the page inside the
        // larger work area.
        media::TranslateTransform shift{};
        shift.X(-m_workArea.X);
        shift.Y(-m_workArea.Y);
        SurfaceCanvas().RenderTransform(shift);

        return !same;
    }

    void EditorWindow::UpdateDeckBrushes()
    {
        try
        {
            PageDeck().Fill(glass::MakeDeckBrush(m_theme.Deck));

            // A hairline rim and nothing else. The page has to read as an object sitting on the
            // work area, and at this zoom a heavier edge reads as part of the layout.
            PageDeck().Stroke(media::SolidColorBrush(
                winrt::Windows::UI::ColorHelper::FromArgb(26, 255, 255, 255)));

            // Near black rather than a dimmed deck. The work area is not part of the layout, and
            // tinting it with the layout's own color is what made it look like one.
            WorkAreaFill().Background(media::SolidColorBrush(
                winrt::Windows::UI::ColorHelper::FromArgb(255, 10, 11, 14)));
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to color the work area.")
    }

    // The snap grid, on the page only, and only where the eye can resolve it.
    //
    // A 1280 x 800 page on an 8 px grid is thirteen thousand dots. At 38 % zoom they are three
    // pixels apart and read as noise rather than as a grid, so the fine layer is skipped once it
    // gets that close and the coarse one carries the alignment on its own.
    void EditorWindow::RebuildGrid()
    {
        try
        {
            GridCanvas().Children().Clear();

            auto const& document = m_editor.Document();
            auto const& snap = m_editor.Snap();

            if (!snap.GridEnabled || snap.GridSize <= 0.0 || m_canvasScale <= 0.0)
            {
                return;
            }

            auto const addDots = [this, &document](double spacing, uint8_t alpha)
                {
                    media::GeometryGroup dots{};

                    for (double y = 0.0; y <= document.PageHeight; y += spacing)
                    {
                        for (double x = 0.0; x <= document.PageWidth; x += spacing)
                        {
                            media::EllipseGeometry dot{};

                            dot.Center({ static_cast<float>(x - m_workArea.X), static_cast<float>(y - m_workArea.Y) });
                            dot.RadiusX(1.0);
                            dot.RadiusY(1.0);

                            dots.Children().Append(dot);
                        }
                    }

                    shapes::Path path{};

                    path.Data(dots);
                    path.IsHitTestVisible(false);
                    path.UseLayoutRounding(false);
                    path.Fill(media::SolidColorBrush(
                        winrt::Windows::UI::ColorHelper::FromArgb(alpha, 255, 255, 255)));

                    GridCanvas().Children().Append(path);
                };

            // Four cells apart, the way the comp draws it, so the eye has something to count in.
            auto const coarse = snap.GridSize * 4.0;

            if (snap.GridSize * m_canvasScale >= MinimumGridSpacingOnScreen)
            {
                addDots(snap.GridSize, 14);
            }

            if (coarse * m_canvasScale >= MinimumGridSpacingOnScreen)
            {
                addDots(coarse, 41);
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to draw the grid.")
    }

    void EditorWindow::ApplyCanvasScale()
    {
        try
        {
            auto const& document = m_editor.Document();

            auto const available = CanvasScroll().ActualWidth();
            auto const availableHeight = CanvasScroll().ActualHeight();

            if (available <= FitPadding * 2.0 ||
                availableHeight <= FitPadding * 2.0 ||
                document.PageWidth <= 0.0 ||
                document.PageHeight <= 0.0)
            {
                return;
            }

            if (m_zoomIsFit)
            {
                // Fit is about the PAGE, with a fixed gap around it. Fitting the work area
                // instead let the margin grow with the page and left a band of unused canvas
                // that read as part of the layout. Both axes take the same factor: a page is
                // never stretched.
                m_canvasScale = std::clamp(
                    std::min(
                        (available - FitPadding * 2.0) / document.PageWidth,
                        (availableHeight - FitPadding * 2.0) / document.PageHeight),
                    MinimumCanvasScale,
                    MaximumCanvasScale);
            }

            auto const scale = m_canvasScale;

            // The work area depends on the scale, because how much of it fits on screen does.
            if (UpdateWorkArea())
            {
                RebuildGrid();
                UpdateOverlay();
            }

            CanvasScale().ScaleX(scale);
            CanvasScale().ScaleY(scale);

            // The transform paints the work area scaled; the host carries the scaled size so
            // the scroll viewer knows how much there is and centers what fits.
            CanvasHost().Width(m_workArea.Width * scale);
            CanvasHost().Height(m_workArea.Height * scale);

            CanvasStatusText().Text(resources::FormatString(
                L"CanvasStatusFormat",
                std::to_wstring(m_editor.PageIndex() + 1),
                std::to_wstring(m_editor.Document().Pages.size()),
                std::to_wstring(m_editor.Document().PageWidth),
                std::to_wstring(m_editor.Document().PageHeight),
                std::to_wstring(static_cast<int32_t>(std::lround(scale * 100.0)))));

            ZoomInButton().IsEnabled(scale < MaximumCanvasScale);
            ZoomOutButton().IsEnabled(scale > MinimumCanvasScale);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to scale the canvas.")
    }

    // One step is a quarter in or out, which is coarse enough to be worth a click and fine
    // enough to land on something usable.
    _Use_decl_annotations_
    void EditorWindow::StepZoom(double factor)
    {
        m_zoomIsFit = false;
        m_canvasScale = std::clamp(m_canvasScale * factor, MinimumCanvasScale, MaximumCanvasScale);

        ApplyCanvasScale();
        RebuildGrid();
    }

    _Use_decl_annotations_
    void EditorWindow::OnZoomInClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        StepZoom(1.25);
    }

    _Use_decl_annotations_
    void EditorWindow::OnZoomOutClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        StepZoom(0.8);
    }

    _Use_decl_annotations_
    void EditorWindow::OnZoomFitClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        m_zoomIsFit = true;

        ApplyCanvasScale();
        RebuildGrid();
    }

    _Use_decl_annotations_
    void EditorWindow::OnCanvasSizeChanged(
        foundation::IInspectable const& sender,
        xaml::SizeChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyCanvasScale();
    }

    // ---------------------------------------------------------------- the overlay

    void EditorWindow::UpdateOverlay()
    {
        try
        {
            OverlayCanvas().Children().Clear();
            m_handleShapes.clear();

            auto const* const page = m_editor.CurrentPage();

            if (page == nullptr)
            {
                return;
            }

            // Try mode shows the page as it will actually look. Selection outlines and handles
            // drawn over a live surface would make it look like the editor again.
            if (m_tryMode)
            {
                return;
            }

            auto const accent = BrushNamed(L"AccentFillColorDefaultBrush");
            auto const critical = BrushNamed(L"SystemFillColorCriticalBrush");
            auto const tertiary = BrushNamed(L"TextFillColorTertiaryBrush");

            auto const addRectangle =
                [this](glass::EditRect const& rect, media::Brush const& stroke, double thickness, bool dashed)
                {
                    shapes::Rectangle shape{};

                    shape.Width(std::max(1.0, rect.Width));
                    shape.Height(std::max(1.0, rect.Height));
                    shape.Stroke(stroke);
                    shape.StrokeThickness(thickness);
                    shape.UseLayoutRounding(false);
                    shape.IsHitTestVisible(false);

                    if (dashed)
                    {
                        media::DoubleCollection dashes{};
                        dashes.Append(3.0);
                        dashes.Append(3.0);
                        shape.StrokeDashArray(dashes);
                    }

                    controls::Canvas::SetLeft(shape, rect.X - m_workArea.X);
                    controls::Canvas::SetTop(shape, rect.Y - m_workArea.Y);

                    OverlayCanvas().Children().Append(shape);

                    return shape;
                };

            // The page edge, so the active area is never in doubt.
            addRectangle(
                { 0.0, 0.0, static_cast<double>(m_editor.Document().PageWidth), static_cast<double>(m_editor.Document().PageHeight) },
                tertiary,
                1.0,
                false);

            // The rubber band, and the outline of a control being drawn. Dragging with nothing
            // on screen looks like nothing is happening until the pointer comes up.
            if (m_hasBand && m_dragMode == DragMode::RubberBand)
            {
                auto band = addRectangle(m_band, accent, 1.0, m_dragMode == DragMode::RubberBand);

                band.Fill(media::SolidColorBrush(
                    winrt::Windows::UI::ColorHelper::FromArgb(38, 96, 205, 255)));
            }

            for (auto const& control : page->Controls)
            {
                auto const rect = RectOf(control);

                // Off the page: ghosted and edged in red, counted in the warning strip, and
                // still selectable and draggable. Never moved back on its own.
                if (glass::IsOutsidePage(rect, m_editor.Document().PageWidth, m_editor.Document().PageHeight))
                {
                    addRectangle(rect, critical, 1.0, true);
                }

                if (m_editor.IsSelected(control.Id))
                {
                    addRectangle({ rect.X - 1.0, rect.Y - 1.0, rect.Width + 2.0, rect.Height + 2.0 }, accent, 1.0, false);
                }
            }

            // Badges say what the Tab key will do, because a hidden ordering is one nobody can
            // fix. Drawn for the selection only, so a page of two hundred is not a wall of them —
            // except while the order is being set, when every one of them is the point.
            std::vector<glass::Control const*> badged{};

            if (m_keyboardOrderMode)
            {
                for (auto const& control : page->Controls)
                {
                    badged.push_back(&control);
                }
            }
            else
            {
                badged = m_editor.SelectedControls();
            }

            for (auto const* const control : badged)
            {
                auto number = control->KeyboardOrder;
                auto picked = true;

                if (m_keyboardOrderMode)
                {
                    picked = KeyboardOrderNumberFor(control->Id, number);

                    if (!picked)
                    {
                        number = control->KeyboardOrder;
                    }
                }

                controls::Border badge{};
                badge.Background(picked ? accent : BrushNamed(L"ControlFillColorSecondaryBrush"));
                badge.CornerRadius({ 8, 8, 8, 8 });
                badge.Padding({ 5, 0, 5, 0 });
                badge.Height(16);
                badge.IsHitTestVisible(false);

                controls::TextBlock text{};

                // The number alone answers "what does Tab do"; the name answers "which one is
                // this", which is the question somebody looking at a badge usually has.
                text.Text(winrt::hstring{ control->Label.empty() || m_keyboardOrderMode
                    ? std::to_wstring(number)
                    : std::to_wstring(number) + L": " + control->Label });

                text.FontSize(control->Label.empty() || m_keyboardOrderMode ? 10 : 9);
                text.VerticalAlignment(xaml::VerticalAlignment::Center);

                text.Foreground(picked
                    ? BrushNamed(L"TextOnAccentFillColorPrimaryBrush")
                    : BrushNamed(L"TextFillColorTertiaryBrush"));

                badge.Child(text);

                controls::Canvas::SetLeft(badge, control->X - m_workArea.X);
                controls::Canvas::SetTop(badge, control->Y - m_workArea.Y - 19.0);

                OverlayCanvas().Children().Append(badge);
            }

            // Nothing is selected while the order is being set, so there are no handles to draw.
            if (m_keyboardOrderMode)
            {
                return;
            }

            // Eight handles, on a single selection only. Sizing several controls at once happens
            // through the inspector, where the numbers are visible.
            auto const selected = m_editor.SelectedControls();

            if (selected.size() == 1)
            {
                auto const rect = RectOf(*selected[0]);

                double const xs[]{ rect.X, rect.CenterX(), rect.Right() };
                double const ys[]{ rect.Y, rect.CenterY(), rect.Bottom() };

                for (int32_t row = 0; row < 3; ++row)
                {
                    for (int32_t column = 0; column < 3; ++column)
                    {
                        if (row == 1 && column == 1)
                        {
                            continue;
                        }

                        shapes::Rectangle handle{};

                        handle.Width(HandleSize);
                        handle.Height(HandleSize);
                        handle.Fill(BrushNamed(L"SolidBackgroundFillColorBaseBrush"));
                        handle.Stroke(accent);
                        handle.StrokeThickness(1.0);
                        handle.UseLayoutRounding(false);
                        handle.IsHitTestVisible(false);

                        controls::Canvas::SetLeft(handle, xs[column] - m_workArea.X - HandleSize / 2.0);
                        controls::Canvas::SetTop(handle, ys[row] - m_workArea.Y - HandleSize / 2.0);

                        OverlayCanvas().Children().Append(handle);
                        m_handleShapes.push_back(handle);
                    }
                }

                // The label gets its own outline and its own four corners, in a lighter weight
                // so it is never mistaken for the control's. Dragging them is how a caption is
                // given room a narrow control does not have.
                glass::EditRect labelRect{};

                if (TryGetLabelRect(*selected[0], labelRect))
                {
                    auto dashed = addRectangle(labelRect, tertiary, 1.0, true);
                    dashed.Fill(nullptr);

                    double const labelXs[]{ labelRect.X, labelRect.Right() };
                    double const labelYs[]{ labelRect.Y, labelRect.Bottom() };

                    for (auto const y : labelYs)
                    {
                        for (auto const x : labelXs)
                        {
                            shapes::Rectangle handle{};

                            handle.Width(LabelHandleSize);
                            handle.Height(LabelHandleSize);
                            handle.Fill(BrushNamed(L"SolidBackgroundFillColorBaseBrush"));
                            handle.Stroke(tertiary);
                            handle.StrokeThickness(1.0);
                            handle.UseLayoutRounding(false);
                            handle.IsHitTestVisible(false);

                            controls::Canvas::SetLeft(handle, x - m_workArea.X - LabelHandleSize / 2.0);
                            controls::Canvas::SetTop(handle, y - m_workArea.Y - LabelHandleSize / 2.0);

                            OverlayCanvas().Children().Append(handle);
                        }
                    }
                }
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to draw the overlay.")
    }

    // Where the selected control's label is painting, in page coordinates. The renderer is the
    // only thing that knows, because it is the one that resolved the placement.
    _Use_decl_annotations_
    bool EditorWindow::TryGetLabelRect(glass::Control const& control, glass::EditRect& rect) const
    {
        rect = {};

        if (control.Label.empty() || control.LabelPlaced == glass::LabelPlacementOverride::None)
        {
            return false;
        }

        auto const index = m_editor.ControlIndexOf(control.Id);

        if (index < 0)
        {
            return false;
        }

        size_t itemIndex{ 0 };

        if (!m_renderer.TryFindItem(static_cast<uint32_t>(index), itemIndex))
        {
            return false;
        }

        double x{ 0.0 };
        double y{ 0.0 };
        double width{ 0.0 };
        double height{ 0.0 };

        if (!m_renderer.TryGetLabelBox(itemIndex, x, y, width, height))
        {
            return false;
        }

        rect = { control.X + x, control.Y + y, width, height };

        return true;
    }

    // Follows a move drag without rebuilding the page.
    void EditorWindow::MoveDraggedItems()
    {
        auto const* const page = m_editor.CurrentPage();

        if (page == nullptr)
        {
            return;
        }

        for (size_t index = 0; index < page->Controls.size() && index < m_renderer.ItemCount(); ++index)
        {
            auto const& control = page->Controls[index];

            if (m_editor.IsSelected(control.Id))
            {
                // The surface canvas is already shifted by the work area, so the element takes
                // page coordinates as they stand.
                m_renderer.MoveItem(index, control.X, control.Y);
            }
        }
    }

    // The same for a resize drag. A size change has to re-lay the one control rather than move
    // it, because the track length and the pipe thickness are baked into its geometry, but it
    // still leaves the rest of the page alone.
    void EditorWindow::ResizeDraggedItems()
    {
        auto const* const page = m_editor.CurrentPage();

        if (page == nullptr)
        {
            return;
        }

        for (size_t index = 0; index < page->Controls.size() && index < m_renderer.ItemCount(); ++index)
        {
            auto const& control = page->Controls[index];

            if (m_editor.IsSelected(control.Id))
            {
                m_renderer.ResizeItem(index, control, m_theme);
            }
        }
    }

    void EditorWindow::UpdateOffPageBar()
    {
        try
        {
            // Not while something is being dragged. A control dropped from the palette arrives
            // centered under the pointer, so it straddles the edge for as long as the pointer is
            // near one, and a warning strip that opens and closes under the hand is noise. The
            // release path asks again.
            if (m_editor.IsDragging() || m_paletteDragPressed)
            {
                return;
            }

            auto const outside = m_editor.ControlsOutsidePage().size();

            OffPageBar().IsOpen(outside > 0);

            if (outside > 0)
            {
                OffPageBar().Title(outside == 1
                    ? resources::GetString(L"OffPageCountOne")
                    : resources::FormatString(L"OffPageCountFormat", std::to_wstring(outside)));

                OffPageBar().Message(resources::GetString(
                    outside == 1 ? L"OffPageMessageOne" : L"OffPageMessage"));
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to update the off page warning.")
    }

    // ---------------------------------------------------------------- hit testing

    _Use_decl_annotations_
    std::wstring EditorWindow::HitTest(double pageX, double pageY) const
    {
        auto const* const page = m_editor.CurrentPage();

        if (page == nullptr)
        {
            return {};
        }

        // Back to front, so the control drawn on top is the one somebody meant.
        for (auto iterator = page->Controls.rbegin(); iterator != page->Controls.rend(); ++iterator)
        {
            if (glass::ContainsPoint(RectOf(*iterator), pageX, pageY))
            {
                return iterator->Id;
            }
        }

        return {};
    }

    _Use_decl_annotations_
    glass::ResizeHandle EditorWindow::HitTestHandle(double pageX, double pageY, double& distance) const
    {
        distance = std::numeric_limits<double>::max();

        auto const selected = m_editor.SelectedControls();

        if (selected.size() != 1)
        {
            return glass::ResizeHandle::None;
        }

        auto const rect = RectOf(*selected[0]);

        // The handles are drawn at a fixed size on screen, so their reach in page coordinates
        // grows as the canvas is scaled down. Anything else would make them unhittable on a
        // 2560 x 1440 page.
        auto const reach = (HandleSize / std::max(m_canvasScale, MinimumCanvasScale));

        double const xs[]{ rect.X, rect.CenterX(), rect.Right() };
        double const ys[]{ rect.Y, rect.CenterY(), rect.Bottom() };

        constexpr glass::ResizeHandle handles[3][3]
        {
            { glass::ResizeHandle::TopLeft,    glass::ResizeHandle::Top,    glass::ResizeHandle::TopRight },
            { glass::ResizeHandle::Left,       glass::ResizeHandle::None,   glass::ResizeHandle::Right },
            { glass::ResizeHandle::BottomLeft, glass::ResizeHandle::Bottom, glass::ResizeHandle::BottomRight },
        };

        auto best = glass::ResizeHandle::None;

        for (int32_t row = 0; row < 3; ++row)
        {
            for (int32_t column = 0; column < 3; ++column)
            {
                if (handles[row][column] == glass::ResizeHandle::None)
                {
                    continue;
                }

                auto const away = std::max(std::abs(pageX - xs[column]), std::abs(pageY - ys[row]));

                if (away <= reach && away < distance)
                {
                    distance = away;
                    best = handles[row][column];
                }
            }
        }

        return best;
    }

    _Use_decl_annotations_
    glass::ResizeHandle EditorWindow::HitTestHandle(double pageX, double pageY) const
    {
        double distance{ 0.0 };

        return HitTestHandle(pageX, pageY, distance);
    }

    // The label's four corners. A label sitting right under its control shares an edge with it,
    // so this reports how far away the corner was and the caller takes whichever set is nearer.
    // Testing one set before the other would mean the control always swallowed the label's top
    // two corners, which is the most common layout there is.
    _Use_decl_annotations_
    glass::ResizeHandle EditorWindow::HitTestLabelHandle(double pageX, double pageY, double& distance) const
    {
        distance = std::numeric_limits<double>::max();

        auto const selected = m_editor.SelectedControls();

        glass::EditRect rect{};

        if (selected.size() != 1 || !TryGetLabelRect(*selected[0], rect))
        {
            return glass::ResizeHandle::None;
        }

        auto const reach = (LabelHandleSize / std::max(m_canvasScale, MinimumCanvasScale));

        constexpr glass::ResizeHandle corners[2][2]
        {
            { glass::ResizeHandle::TopLeft,    glass::ResizeHandle::TopRight },
            { glass::ResizeHandle::BottomLeft, glass::ResizeHandle::BottomRight },
        };

        double const xs[]{ rect.X, rect.Right() };
        double const ys[]{ rect.Y, rect.Bottom() };

        auto best = glass::ResizeHandle::None;

        for (int32_t row = 0; row < 2; ++row)
        {
            for (int32_t column = 0; column < 2; ++column)
            {
                auto const away = std::max(std::abs(pageX - xs[column]), std::abs(pageY - ys[row]));

                if (away <= reach && away < distance)
                {
                    distance = away;
                    best = corners[row][column];
                }
            }
        }

        return best;
    }

    _Use_decl_annotations_
    bool EditorWindow::HitTestLabelBody(double pageX, double pageY) const
    {
        auto const selected = m_editor.SelectedControls();

        glass::EditRect rect{};

        if (selected.size() != 1 || !TryGetLabelRect(*selected[0], rect))
        {
            return false;
        }

        // A label that sits on top of its own control would swallow every press on the control,
        // so only the part hanging outside it can start a label drag.
        if (glass::ContainsPoint(RectOf(*selected[0]), pageX, pageY))
        {
            return false;
        }

        return glass::ContainsPoint(rect, pageX, pageY);
    }

    // ---------------------------------------------------------------- dragging a label

    // Records where the box started, so every update is measured from there rather than
    // accumulating rounding across a hundred pointer moves.
    _Use_decl_annotations_
    bool EditorWindow::BeginLabelDrag(glass::ResizeHandle handle)
    {
        auto const selected = m_editor.SelectedControls();

        glass::EditRect rect{};

        if (selected.size() != 1 || !TryGetLabelRect(*selected[0], rect))
        {
            return false;
        }

        // Page coordinates come back from the renderer; the box is stored relative to the
        // control, so that moving the control takes its label with it.
        m_labelDragStart =
        {
            rect.X - selected[0]->X,
            rect.Y - selected[0]->Y,
            rect.Width,
            rect.Height,
        };

        m_labelDragHandle = handle;
        m_labelDragId = selected[0]->Id;

        return true;
    }

    _Use_decl_annotations_
    void EditorWindow::UpdateLabelDrag(double deltaX, double deltaY)
    {
        if (m_labelDragId.empty())
        {
            return;
        }

        auto box = m_labelDragStart;

        if (m_dragMode == DragMode::LabelResize)
        {
            box = glass::ApplyResize(m_labelDragStart, m_labelDragHandle, deltaX, deltaY, false);
        }
        else
        {
            box.X += deltaX;
            box.Y += deltaY;
        }

        box.Width = std::max(box.Width, glass::MinimumLabelBoxSize);
        box.Height = std::max(box.Height, glass::MinimumLabelBoxSize);

        if (m_editor.SetControlLabelBox(m_labelDragId, box.X, box.Y, box.Width, box.Height))
        {
            // The label is a XAML child beside the control, so a rebuild of the one item is
            // enough. A full page rebuild on every pointer move is what makes a drag stutter.
            RebuildSurface();
            UpdateOverlay();
            MarkChanged();
        }
    }

    // ---------------------------------------------------------------- pointer

    _Use_decl_annotations_
    void EditorWindow::OnCanvasPointerPressed(
        foundation::IInspectable const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        try
        {
            auto const point = args.GetCurrentPoint(OverlayCanvas());
            auto const pageX = PointToPageX(point.Position().X);
            auto const pageY = PointToPageY(point.Position().Y);

            // Clicking the canvas takes the keyboard off the toolbar. Leaving it there is what
            // popped the Undo button's "Ctrl+Z" hint over the page in the middle of a drag.
            CanvasScroll().Focus(xaml::FocusState::Pointer);

            m_dragPointerId = point.PointerId();
            m_dragStartPageX = pageX;
            m_dragStartPageY = pageY;
            m_dragMoved = false;
            m_pendingSelectId.clear();

            // While the order is being set, a press numbers a control rather than selecting or
            // moving it.
            if (HandleKeyboardOrderPress(pageX, pageY))
            {
                m_dragMode = DragMode::None;
                return;
            }

            // An armed tool drops its control right here, snapped, and then stays under the
            // pointer so it can be nudged into place before the button comes up. One press is
            // a placement; press and drag is a placement you aimed.
            if (m_hasArmedKind)
            {
                auto const kind = m_armedKind;

                m_hasArmedKind = false;
                SyncPaletteSelection();

                auto const id = m_editor.AddControlCentered(kind, pageX, pageY);

                m_dragMode = DragMode::None;
                m_hasBand = false;

                if (id.empty())
                {
                    UpdateStatusBar();
                    return;
                }

                RebuildSurface();
                RebuildOutline();
                RefreshInspector();
                UpdateOffPageBar();
                UpdateStatusBar();
                MarkChanged();

                m_dragMode = DragMode::Move;
                m_editor.BeginDrag();

                OverlayCanvas().CapturePointer(args.Pointer());
                return;
            }

            double controlAway{ 0.0 };
            double labelAway{ 0.0 };

            auto const handle = HitTestHandle(pageX, pageY, controlAway);
            auto const labelHandle = HitTestLabelHandle(pageX, pageY, labelAway);

            // Both sets are in reach where a label sits against its control, so the nearer one
            // wins. Order alone would always give the control the label's top two corners.
            if (labelHandle != glass::ResizeHandle::None && labelAway < controlAway)
            {
                if (BeginLabelDrag(labelHandle))
                {
                    m_dragMode = DragMode::LabelResize;
                    OverlayCanvas().CapturePointer(args.Pointer());
                    return;
                }
            }

            if (handle != glass::ResizeHandle::None)
            {
                m_dragMode = DragMode::Resize;
                m_editor.BeginResize(handle);
                OverlayCanvas().CapturePointer(args.Pointer());
                return;
            }

            if (labelHandle != glass::ResizeHandle::None && BeginLabelDrag(labelHandle))
            {
                m_dragMode = DragMode::LabelResize;
                OverlayCanvas().CapturePointer(args.Pointer());
                return;
            }

            if (HitTestLabelBody(pageX, pageY) && BeginLabelDrag(glass::ResizeHandle::None))
            {
                m_dragMode = DragMode::LabelMove;
                OverlayCanvas().CapturePointer(args.Pointer());
                return;
            }

            auto const hit = HitTest(pageX, pageY);

            auto const modifiers = args.KeyModifiers();

            // Shift as well as Ctrl. Shift is what everybody reaches for first, and a control
            // surface editor that only answers to Ctrl looks broken rather than opinionated.
            auto const extend =
                (modifiers & winrt::Windows::System::VirtualKeyModifiers::Control)
                    == winrt::Windows::System::VirtualKeyModifiers::Control ||
                (modifiers & winrt::Windows::System::VirtualKeyModifiers::Shift)
                    == winrt::Windows::System::VirtualKeyModifiers::Shift;

            if (hit.empty())
            {
                if (!extend)
                {
                    m_editor.ClearSelection();
                }

                m_dragMode = DragMode::RubberBand;
                m_bandExtends = extend;
                UpdateOverlay();
                RefreshInspector();
                UpdateStatusBar();
                OverlayCanvas().CapturePointer(args.Pointer());
                return;
            }

            if (extend)
            {
                m_editor.ToggleSelected(hit);
            }
            else if (!m_editor.IsSelected(hit))
            {
                m_editor.SelectOnly(hit);
            }
            else
            {
                // Already selected. Collapsing the selection now would break a drag of several
                // controls, so it waits until the pointer comes up without having moved.
                m_pendingSelectId = hit;
            }

            m_dragMode = DragMode::Move;
            m_editor.BeginDrag();

            UpdateOverlay();
            RefreshInspector();
            UpdateStatusBar();

            OverlayCanvas().CapturePointer(args.Pointer());
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to handle a press on the canvas.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnCanvasPointerMoved(
        foundation::IInspectable const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        try
        {
            if (m_dragMode == DragMode::None)
            {
                return;
            }

            auto const point = args.GetCurrentPoint(OverlayCanvas());

            if (point.PointerId() != m_dragPointerId)
            {
                return;
            }

            auto const deltaX = PointToPageX(point.Position().X) - m_dragStartPageX;
            auto const deltaY = PointToPageY(point.Position().Y) - m_dragStartPageY;

            if (!m_dragMoved &&
                std::abs(deltaX) * m_canvasScale < DragThreshold &&
                std::abs(deltaY) * m_canvasScale < DragThreshold)
            {
                return;
            }

            m_dragMoved = true;

            // Alt suspends snapping while a drag is under way, so free-form placement is always
            // available without changing a setting.
            auto const modifiers = args.KeyModifiers();
            m_editor.SetSnapSuspended(
                (modifiers & winrt::Windows::System::VirtualKeyModifiers::Menu)
                == winrt::Windows::System::VirtualKeyModifiers::Menu);

            glass::SnapOutcome outcome{};

            switch (m_dragMode)
            {
            case DragMode::Move:
                outcome = m_editor.UpdateDrag(deltaX, deltaY);
                MoveDraggedItems();
                break;

            case DragMode::Resize:
            {
                auto const shift = (modifiers & winrt::Windows::System::VirtualKeyModifiers::Shift)
                    == winrt::Windows::System::VirtualKeyModifiers::Shift;

                outcome = m_editor.UpdateResize(deltaX, deltaY, shift);
                ResizeDraggedItems();
                break;
            }

            case DragMode::LabelMove:
            case DragMode::LabelResize:
                UpdateLabelDrag(deltaX, deltaY);
                break;

            case DragMode::RubberBand:
            {
                glass::EditRect band
                {
                    std::min(m_dragStartPageX, m_dragStartPageX + deltaX),
                    std::min(m_dragStartPageY, m_dragStartPageY + deltaY),
                    std::abs(deltaX),
                    std::abs(deltaY),
                };

                m_band = band;
                m_hasBand = true;

                m_editor.SelectInRectangle(band, m_bandExtends);
                break;
            }

            default:
                break;
            }

            UpdateOverlay();

            // The guides go on last, so they are not wiped out by the overlay rebuild above.
            for (auto const& guide : outcome.Guides)
            {
                shapes::Line line{};

                line.Stroke(BrushNamed(L"SystemFillColorAttentionBrush"));
                line.StrokeThickness(1.0);
                line.IsHitTestVisible(false);

                if (guide.Axis == glass::GuideAxis::Vertical)
                {
                    line.X1(guide.Position - m_workArea.X);
                    line.X2(guide.Position - m_workArea.X);
                    line.Y1(guide.Start - m_workArea.Y);
                    line.Y2(guide.End - m_workArea.Y);
                }
                else
                {
                    line.X1(guide.Start - m_workArea.X);
                    line.X2(guide.End - m_workArea.X);
                    line.Y1(guide.Position - m_workArea.Y);
                    line.Y2(guide.Position - m_workArea.Y);
                }

                OverlayCanvas().Children().Append(line);
            }

            // Only the four numbers that are actually changing. A full inspector refresh rebuilds
            // the monitor list, the hue swatches and the preview, and doing that on every pointer
            // move is what made a drag stutter.
            RefreshInspectorGeometry();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to handle a drag on the canvas.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnCanvasPointerReleased(
        foundation::IInspectable const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        try
        {
            OverlayCanvas().ReleasePointerCapture(args.Pointer());

            if (m_dragMode == DragMode::Move)
            {
                m_editor.EndDrag();

                if (!m_dragMoved && !m_pendingSelectId.empty())
                {
                    m_editor.SelectOnly(m_pendingSelectId);
                }
            }
            else if (m_dragMode == DragMode::Resize)
            {
                m_editor.EndResize();
            }
            else if (m_dragMode == DragMode::LabelMove || m_dragMode == DragMode::LabelResize)
            {
                m_editor.EndCoalescing();
            }

            m_hasBand = false;

            auto const moved = m_dragMoved;

            m_dragMode = DragMode::None;
            m_dragMoved = false;
            m_pendingSelectId.clear();
            m_editor.SetSnapSuspended(false);

            // Back to a full build, so a label beside a moved control catches up and a resized
            // control gets geometry that matches its new size.
            if (moved)
            {
                RebuildSurface();
            }
            else
            {
                UpdateOverlay();
            }

            UpdateOffPageBar();
            RefreshInspector();
            UpdateStatusBar();
            SelectOutlineRowForSelection();

            if (moved)
            {
                MarkChanged();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to finish a drag on the canvas.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnCanvasPointerCaptureLost(
        foundation::IInspectable const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        // A gesture that ended because the window lost the pointer still has to end, or the next
        // press would be measured from the old origin.
        if (m_dragMode == DragMode::Move)
        {
            m_editor.EndDrag();
        }
        else if (m_dragMode == DragMode::Resize)
        {
            m_editor.EndResize();
        }

        m_dragMode = DragMode::None;
        m_dragMoved = false;
        m_editor.SetSnapSuspended(false);
    }

    _Use_decl_annotations_
    void EditorWindow::OnSelectOffPageClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        m_editor.SelectOutsidePage();

        UpdateOverlay();
        RefreshInspector();
        UpdateStatusBar();
        SelectOutlineRowForSelection();
    }

    // ---------------------------------------------------------------- pages

    void EditorWindow::RebuildPageRail()
    {
        try
        {
            PageRail().Children().Clear();

            auto const& document = m_editor.Document();

            auto const tabStyle = xaml::Application::Current().Resources()
                .Lookup(box_value(L"PageTabStyle")).as<xaml::Style>();

            for (size_t index = 0; index < document.Pages.size(); ++index)
            {
                auto const& page = document.Pages[index];

                controls::Primitives::ToggleButton tab{};

                tab.Style(tabStyle);
                tab.IsChecked(index == m_editor.PageIndex());

                auto const name = page.Name.empty()
                    ? std::wstring{ resources::FormatString(L"PageNameFormat", std::to_wstring(index + 1)) }
                    : page.Name;

                controls::StackPanel row{};

                row.Orientation(controls::Orientation::Horizontal);
                row.Spacing(6);
                row.VerticalAlignment(xaml::VerticalAlignment::Center);

                // A dot in the page's own hue, so the rail reads as a set of pages rather than
                // as a row of identical buttons.
                shapes::Ellipse dot{};

                dot.Width(6);
                dot.Height(6);
                dot.UseLayoutRounding(false);
                dot.VerticalAlignment(xaml::VerticalAlignment::Center);
                dot.Fill(media::SolidColorBrush(ToColor(
                    m_theme.HueSlots[index % glass::HueSlotCount])));

                controls::TextBlock text{};

                text.Text(winrt::hstring{ name });
                text.FontSize(11.5);
                text.VerticalAlignment(xaml::VerticalAlignment::Center);

                row.Children().Append(dot);
                row.Children().Append(text);

                tab.Content(row);
                tab.Tag(box_value(winrt::hstring{ std::to_wstring(index) }));

                xaml::Automation::AutomationProperties::SetName(tab, winrt::hstring{ name });

                tab.Click([weak = get_weak()](auto&& sender, auto&&)
                    {
                        auto strong = weak.get();

                        if (strong == nullptr)
                        {
                            return;
                        }

                        auto const button = sender.template try_as<controls::Primitives::ToggleButton>();

                        if (button == nullptr)
                        {
                            return;
                        }

                        // A Tag written in code is still read back as a string here, so that the
                        // rail and the markup-set tags elsewhere are read the same way.
                        auto const tag = std::wstring{
                            winrt::unbox_value_or<winrt::hstring>(button.Tag(), L"0") };

                        strong->m_editor.SetPageIndex(static_cast<size_t>(std::stoul(tag)));
                        strong->BuildPage();
                        strong->RebuildPageRail();
                        strong->RefreshInspector();
                    });

                PageRail().Children().Append(tab);
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to rebuild the page rail.")
    }

    _Use_decl_annotations_
    void EditorWindow::ShowEditorPage(size_t pageIndex)
    {
        try
        {
            if (pageIndex >= m_editor.Document().Pages.size() ||
                pageIndex == m_editor.PageIndex())
            {
                return;
            }

            m_editor.SetPageIndex(pageIndex);

            BuildPage();
            RebuildPageRail();
            RefreshInspector();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the page a control asked for.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnAddPageClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        auto const name = resources::FormatString(
            L"PageNameFormat", std::to_wstring(m_editor.Document().Pages.size() + 1));

        if (m_editor.AddPage(std::wstring{ name }))
        {
            BuildPage();
            RebuildPageRail();
            RefreshInspector();
            MarkChanged();
        }
    }
}
