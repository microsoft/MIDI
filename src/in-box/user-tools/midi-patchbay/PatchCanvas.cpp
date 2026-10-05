// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "PatchCanvas.h"
#include "PatchLayout.h"
#include "StringResources.h"

// Shared with MIDI Glass, in midi-app-shared.
#include "FontCatalog.h"

namespace midipatchbay
{
    namespace
    {
        constexpr double HeaderHeight = 48.0;
        constexpr double ColumnHeaderHeight = 20.0;
        constexpr double PortRowHeight = 32.0;
        constexpr double NodeCornerRadius = 8.0;
        constexpr double DotDiameter = 14.0;
        constexpr double MinimapWidth = 170.0;
        constexpr double MinimapHeight = 116.0;

        constexpr double DefaultColumnX[2] = { 60.0, 460.0 };
        constexpr double ArrangeTopMargin = 48.0;
        constexpr double ArrangeRowGap = 32.0;

        // How close a drop has to be to a connection point to land on it.
        constexpr double PortSnapRadius = 36.0;

        // Space kept around the content when the view is fitted to it.
        constexpr double FitMargin = 24.0;

        // Past this a name is trimmed, and the row's tooltip carries the rest.
        constexpr double MaximumNodeWidth = 560.0;

        // Both sides of the border at its selected thickness, plus a pixel each way for rounding.
        constexpr double NodeWidthAllowance = 6.0;

        // Left between a newly added node and one it would otherwise have landed on.
        constexpr double NodeClearance = 40.0;

        // Within the connection layer: the selection glow, then the line, then its group label.
        constexpr int GlowZIndex = 0;
        constexpr int LineZIndex = 1;
        constexpr int HitAreaZIndex = 2;
        constexpr int PillZIndex = 3;

        // Blocks are smaller than endpoints, and have one way in and one way out.
        constexpr double BlockCornerRadius = 12.0;
        constexpr double BlockPortColumnWidth = 16.0;
        constexpr double BlockDotDiameter = 13.0;
        constexpr double BlockFallbackHeight = 80.0;
        constexpr double EndpointFallbackHeight = 200.0;

        // Within this of a connection, a block dropped from the palette goes into it.
        constexpr double ConnectionHitDistance = 14.0;
        constexpr int SamplesPerCurve = 24;

        // Room around an annotation's text, so the edge drawn when it is selected clears it.
        constexpr double AnnotationPaddingX = 6.0;
        constexpr double AnnotationPaddingY = 2.0;
        constexpr double AnnotationCornerRadius = 4.0;

        winrt::Windows::UI::Color Rgb(_In_ uint8_t r, _In_ uint8_t g, _In_ uint8_t b, _In_ uint8_t a = 255) noexcept
        {
            winrt::Windows::UI::Color color{};
            color.A = a;
            color.R = r;
            color.G = g;
            color.B = b;
            return color;
        }

        controls::TextBlock MakeText(
            _In_ winrt::hstring const& text,
            _In_ double fontSize,
            _In_ media::Brush const& brush,
            _In_ bool bold = false) noexcept
        {
            controls::TextBlock block{};

            block.Text(text);
            block.FontSize(fontSize);
            block.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
            block.TextWrapping(xaml::TextWrapping::NoWrap);
            block.VerticalAlignment(xaml::VerticalAlignment::Center);

            if (brush != nullptr)
            {
                block.Foreground(brush);
            }

            if (bold)
            {
                block.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
            }

            return block;
        }

        controls::FontIcon MakeGlyph(
            _In_ winrt::hstring const& glyph,
            _In_ double fontSize,
            _In_ media::Brush const& brush) noexcept
        {
            controls::FontIcon icon{};

            icon.Glyph(glyph);
            icon.FontSize(fontSize);

            if (brush != nullptr)
            {
                icon.Foreground(brush);
            }

            return icon;
        }

        // DoubleCollection has no initializer list constructor in this projection.
        media::DoubleCollection MakeDashArray(_In_ double on, _In_ double off) noexcept
        {
            media::DoubleCollection collection{};

            collection.Append(on);
            collection.Append(off);

            return collection;
        }

        // A button fills itself, square, on hover and press. Nothing clips a connection point's
        // row now, so that fill would show past the node's rounded corners. The rows draw their own.
        void ClearStateFills(_In_ controls::Button const& button)
        {
            auto const dictionary = button.Resources();

            for (auto const key : { L"ButtonBackground", L"ButtonBackgroundPointerOver",
                                    L"ButtonBackgroundPressed", L"ButtonBackgroundDisabled" })
            {
                dictionary.Insert(winrt::box_value(key), media::SolidColorBrush{ Rgb(0, 0, 0, 0) });
            }
        }

        // The category colors need a darker shade on a light background to stay readable.
        bool IsDarkTheme() noexcept
        {
            try
            {
                if (auto const brush = ThemeBrushes::Current().Get(L"TextFillColorPrimaryBrush").try_as<media::SolidColorBrush>())
                {
                    auto const color = brush.Color();

                    return static_cast<int>(color.R) + color.G + color.B > 3 * 128;
                }
            }
            catch (...)
            {
            }

            return true;
        }

        void AppendCurveSamples(
            _Inout_ std::vector<foundation::Point>& samples,
            _In_ foundation::Point const& p0,
            _In_ foundation::Point const& p1,
            _In_ foundation::Point const& p2,
            _In_ foundation::Point const& p3)
        {
            for (int i = 0; i <= SamplesPerCurve; i++)
            {
                auto const t = static_cast<float>(i) / SamplesPerCurve;
                auto const u = 1.0f - t;

                samples.push_back(foundation::Point{
                    u * u * u * p0.X + 3 * u * u * t * p1.X + 3 * u * t * t * p2.X + t * t * t * p3.X,
                    u * u * u * p0.Y + 3 * u * u * t * p1.Y + 3 * u * t * t * p2.Y + t * t * t * p3.Y });
            }
        }

        double DistanceToSegment(
            _In_ foundation::Point const& point,
            _In_ foundation::Point const& a,
            _In_ foundation::Point const& b) noexcept
        {
            auto const dx = static_cast<double>(b.X) - a.X;
            auto const dy = static_cast<double>(b.Y) - a.Y;
            auto const lengthSquared = dx * dx + dy * dy;

            auto t = lengthSquared <= 0 ? 0.0 : ((point.X - a.X) * dx + (point.Y - a.Y) * dy) / lengthSquared;
            t = std::clamp(t, 0.0, 1.0);

            auto const x = a.X + t * dx - point.X;
            auto const y = a.Y + t * dy - point.Y;

            return std::sqrt(x * x + y * y);
        }
    }

    // BitmapImage cannot render SVG and the shipped default endpoint art is SVG, so the decoder
    // is chosen by extension, the same way the Settings app does it.
    _Use_decl_annotations_
    media::ImageSource PatchCanvas::LoadEndpointImage(std::wstring const& path, int32_t pixelHeight) noexcept
    {
        if (path.empty())
        {
            return nullptr;
        }

        try
        {
            foundation::Uri const uri{ L"file:///" + winrt::hstring{ path } };

            if (midiapp::EndpointImageAssets::IsScalableVector(path))
            {
                media::Imaging::SvgImageSource source{};

                source.RasterizePixelHeight(pixelHeight);
                source.UriSource(uri);

                return source;
            }

            media::Imaging::BitmapImage bitmap{};

            bitmap.DecodePixelHeight(pixelHeight);
            bitmap.UriSource(uri);

            return bitmap;
        }
        catch (...)
        {
        }

        return nullptr;
    }

    _Use_decl_annotations_
    media::Brush PatchCanvas::ThemeBrush(std::wstring_view key, winrt::Windows::UI::Color fallback) noexcept
    {
        if (auto brush = ThemeBrushes::Current().Get(key))
        {
            return brush;
        }

        return media::SolidColorBrush{ fallback };
    }

    _Use_decl_annotations_
    media::Brush PatchCanvas::CategoryBrush(BlockCategory category, double opacity) noexcept
    {
        try
        {
            auto const dark = IsDarkTheme();

            winrt::Windows::UI::Color color{};

            switch (category)
            {
            case BlockCategory::Transform:
                color = dark ? Rgb(0xB4, 0xA7, 0xFF) : Rgb(0x5B, 0x4B, 0xC4);
                break;

            case BlockCategory::Sending:
                color = dark ? Rgb(0x5B, 0xE0, 0xB0) : Rgb(0x0B, 0x7A, 0x55);
                break;

            case BlockCategory::Generator:
                color = dark ? Rgb(0x6C, 0xC8, 0xFF) : Rgb(0x00, 0x5A, 0x9E);
                break;

            case BlockCategory::Annotation:
                color = dark ? Rgb(0xB8, 0xB8, 0xB8) : Rgb(0x5C, 0x5C, 0x5C);
                break;

            case BlockCategory::Distribution:
                color = dark ? Rgb(0xFF, 0x8F, 0xC8) : Rgb(0xA8, 0x1F, 0x6B);
                break;

            case BlockCategory::CapabilityInquiry:
                color = dark ? Rgb(0x8E, 0xE0, 0x4F) : Rgb(0x3D, 0x6E, 0x00);
                break;

            default:
                color = dark ? Rgb(0xFF, 0xB5, 0x47) : Rgb(0x9A, 0x5B, 0x00);
                break;
            }

            color.A = static_cast<uint8_t>(std::lround(std::clamp(opacity, 0.0, 1.0) * 255.0));

            return media::SolidColorBrush{ color };
        }
        catch (...)
        {
        }

        return nullptr;
    }

    _Use_decl_annotations_
    std::optional<winrt::Windows::UI::Color> PatchCanvas::ParseColorCode(std::wstring_view code) noexcept
    {
        try
        {
            auto const normal = AnnotationColorFrom(code);

            if (normal.empty())
            {
                return std::nullopt;
            }

            auto const value = std::stoul(normal.substr(1), nullptr, 16);

            return Rgb(
                static_cast<uint8_t>((value >> 16) & 0xFF),
                static_cast<uint8_t>((value >> 8) & 0xFF),
                static_cast<uint8_t>(value & 0xFF));
        }
        catch (...)
        {
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    std::wstring PatchCanvas::ColorCode(winrt::Windows::UI::Color const& color)
    {
        return std::format(L"#{:02X}{:02X}{:02X}", color.R, color.G, color.B);
    }

    _Use_decl_annotations_
    void PatchCanvas::ApplyAnnotationLook(controls::TextBlock const& text, AnnotationSettings const& settings) noexcept
    {
        try
        {
            auto const empty = settings.Text.empty();

            text.Text(empty ? resources::GetString(L"AnnotationPlaceholder") : winrt::hstring{ settings.Text });
            text.FontFamily(midiapp::fonts::FamilyFor(settings.FontFamily));
            text.FontSize(std::clamp(settings.FontSize, MinimumAnnotationFontSize, MaximumAnnotationFontSize));

            text.FontWeight(settings.Bold
                ? winrt::Microsoft::UI::Text::FontWeights::Bold()
                : winrt::Microsoft::UI::Text::FontWeights::Normal());

            // The hint is in italics, so an empty annotation is not mistaken for one that says so.
            text.FontStyle(settings.Italic || empty
                ? winrt::Windows::UI::Text::FontStyle::Italic
                : winrt::Windows::UI::Text::FontStyle::Normal);

            text.TextDecorations(settings.Underline
                ? winrt::Windows::UI::Text::TextDecorations::Underline
                : winrt::Windows::UI::Text::TextDecorations::None);

            auto const color = empty ? std::nullopt : ParseColorCode(settings.Color);

            if (color.has_value())
            {
                text.Foreground(media::SolidColorBrush{ color.value() });
            }
            else
            {
                text.Foreground(empty
                    ? ThemeBrush(L"TextFillColorTertiaryBrush", Rgb(0x90, 0x90, 0x90))
                    : ThemeBrush(L"TextFillColorPrimaryBrush", Rgb(0xFF, 0xFF, 0xFF)));
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void PatchCanvas::Initialize(
        controls::ScrollViewer const& scrollViewer,
        controls::Canvas const& surface,
        controls::Canvas const& minimap,
        Callbacks callbacks) noexcept
    {
        try
        {
            m_scrollViewer = scrollViewer;
            m_surface = surface;
            m_minimap = minimap;
            m_callbacks = std::move(callbacks);

            m_connectionLayer = controls::Canvas{};
            m_nodeLayer = controls::Canvas{};
            m_overlayLayer = controls::Canvas{};

            m_surface.Children().Append(m_connectionLayer);
            m_surface.Children().Append(m_nodeLayer);
            m_surface.Children().Append(m_overlayLayer);

            m_dragLine = shapes::Path{};
            m_dragLine.StrokeThickness(2.0);
            m_dragLine.Stroke(ThemeBrush(L"AccentFillColorDefaultBrush", Rgb(0x60, 0xCD, 0xFF)));
            m_dragLine.StrokeDashArray(MakeDashArray(4.0, 3.0));
            m_dragLine.Visibility(xaml::Visibility::Collapsed);
            m_dragLine.IsHitTestVisible(false);

            m_overlayLayer.Children().Append(m_dragLine);

            // Dragging deliberately does NOT depend on pointer capture. The connection points
            // are Buttons and the canvas sits in a ScrollViewer, and both take capture for their
            // own purposes; treating the resulting PointerCaptureLost as "the drag ended" is what
            // made dragging impossible. Instead the scroll viewer is watched with
            // handledEventsToo, so the moves arrive whoever happens to hold capture.
            auto const watch = [this](xaml::UIElement const& element)
                {
                    if (element == nullptr)
                    {
                        return;
                    }

                    element.AddHandler(xaml::UIElement::PointerMovedEvent(),
                        winrt::box_value(input::PointerEventHandler{
                            [this](auto&&, input::PointerRoutedEventArgs const& args) { OnSurfacePointerMoved(args); } }),
                        true);

                    element.AddHandler(xaml::UIElement::PointerReleasedEvent(),
                        winrt::box_value(input::PointerEventHandler{
                            [this](auto&&, input::PointerRoutedEventArgs const& args) { OnSurfacePointerReleased(args); } }),
                        true);

                    element.AddHandler(xaml::UIElement::PointerCanceledEvent(),
                        winrt::box_value(input::PointerEventHandler{
                            [this](auto&&, auto&&) { CancelDrags(); } }),
                        true);
                };

            // Only the scroll viewer: it is an ancestor of the surface, so registering on both
            // would deliver every move and release twice.
            watch(m_scrollViewer);

            // Clicking empty canvas clears the selection; the node handlers mark their own
            // events handled so this only fires for the background.
            m_surface.PointerPressed([this](auto&&, input::PointerRoutedEventArgs const& args)
                {
                    UNREFERENCED_PARAMETER(args);
                    FocusCanvas();
                    ClearArmedPort();
                    ClearSelection();
                });

            if (m_scrollViewer != nullptr)
            {
                m_scrollViewer.ViewChanged([this](auto&&, auto&&)
                    {
                        UpdateMinimap();

                        if (m_callbacks.ViewportChanged)
                        {
                            m_callbacks.ViewportChanged();
                        }
                    });

                // Focusable so arrow keys scroll it, and so Delete has somewhere on the canvas
                // to bubble up from.
                m_scrollViewer.IsTabStop(true);

                // A resize changes how much is in view without scrolling, which ViewChanged
                // does not report.
                m_scrollViewer.SizeChanged([this](auto&&, auto&&)
                    {
                        UpdateExtent();
                        UpdateMinimap();
                    });
            }

            if (m_minimap != nullptr)
            {
                m_minimap.PointerPressed([this](auto&&, input::PointerRoutedEventArgs const& args)
                    { OnMinimapPointerPressed(args); });

                m_minimap.PointerMoved([this](auto&&, input::PointerRoutedEventArgs const& args)
                    { OnMinimapPointerMoved(args); });

                m_minimap.PointerReleased([this](auto&&, input::PointerRoutedEventArgs const& args)
                    { OnMinimapPointerReleased(args); });

                m_minimap.PointerCaptureLost([this](auto&&, auto&&) { m_panningMinimap = false; });
                m_minimap.PointerCanceled([this](auto&&, auto&&) { m_panningMinimap = false; });

                // ProtectedCursor is not on the projected type, but the interface carrying it is.
                if (auto const element = m_minimap.try_as<xaml::IUIElementProtected>())
                {
                    element.ProtectedCursor(winrt::Microsoft::UI::Input::InputSystemCursor::Create(
                        winrt::Microsoft::UI::Input::InputSystemCursorShape::SizeAll));
                }
            }

            if (m_scrollViewer != nullptr)
            {
                // Blocks and endpoints arrive from the palette by drag and drop.
                AcceptDrops(m_scrollViewer);
            }

            m_initialized = true;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to initialize the canvas.")
    }

    _Use_decl_annotations_
    void PatchCanvas::AcceptDrops(xaml::UIElement const& element) noexcept
    {
        try
        {
            if (element == nullptr)
            {
                return;
            }

            element.AllowDrop(true);

            // DragEnter as well, so a drop made the moment the pointer arrives isn't turned away.
            element.DragEnter([this](auto&&, xaml::DragEventArgs const& args) { OnDragOver(args); });
            element.DragOver([this](auto&&, xaml::DragEventArgs const& args) { OnDragOver(args); });
            element.DragLeave([this](auto&&, auto&&) { SetDropTarget({}); });
            element.Drop([this](auto&&, xaml::DragEventArgs const& args) { OnDrop(args); });
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to take drops on the canvas.")
    }

    void PatchCanvas::Shutdown() noexcept
    {
        m_shuttingDown = true;
        m_patch = nullptr;
        m_nodes.clear();
        m_connections.clear();
    }

    _Use_decl_annotations_
    PatchCanvas::NodeVisual* PatchCanvas::FindNode(std::wstring const& nodeId) noexcept
    {
        auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
            [&nodeId](NodeVisual const& n) { return n.NodeId == nodeId; });

        return it == m_nodes.end() ? nullptr : &(*it);
    }

    _Use_decl_annotations_
    PatchCanvas::NodeVisual const* PatchCanvas::FindNode(std::wstring const& nodeId) const noexcept
    {
        auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
            [&nodeId](NodeVisual const& n) { return n.NodeId == nodeId; });

        return it == m_nodes.end() ? nullptr : &(*it);
    }

    _Use_decl_annotations_
    std::optional<foundation::Point> PatchCanvas::NodePosition(std::wstring const& nodeId) const noexcept
    {
        if (m_patch == nullptr)
        {
            return std::nullopt;
        }

        if (auto const* endpoint = m_patch->FindEndpoint(nodeId))
        {
            return foundation::Point{ static_cast<float>(endpoint->CanvasX), static_cast<float>(endpoint->CanvasY) };
        }

        if (auto const* block = m_patch->FindBlock(nodeId))
        {
            return foundation::Point{ static_cast<float>(block->CanvasX), static_cast<float>(block->CanvasY) };
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    void PatchCanvas::SetNodePosition(std::wstring const& nodeId, double x, double y) noexcept
    {
        if (m_patch == nullptr)
        {
            return;
        }

        // The model is const to the canvas everywhere else. Moving nodes is the one change it
        // makes itself, and the window is told so it can mark the patch unsaved.
        auto* patch = const_cast<PatchDocument*>(m_patch);

        if (auto* nodeX = patch->NodeX(nodeId))
        {
            *nodeX = x;
        }

        if (auto* nodeY = patch->NodeY(nodeId))
        {
            *nodeY = y;
        }

        if (auto* node = FindNode(nodeId); node != nullptr && node->Root != nullptr)
        {
            controls::Canvas::SetLeft(node->Root, x);
            controls::Canvas::SetTop(node->Root, y);
        }
    }

    _Use_decl_annotations_
    bool PatchCanvas::IsNodeSelected(std::wstring const& nodeId) const noexcept
    {
        return std::find(m_selectedNodeIds.begin(), m_selectedNodeIds.end(), nodeId) != m_selectedNodeIds.end();
    }

    _Use_decl_annotations_
    void PatchCanvas::Rebuild(
        PatchDocument const* patch,
        std::vector<LiveEndpoint> const& liveEndpoints,
        PatchAnalysis const& analysis) noexcept
    {
        if (!m_initialized || m_shuttingDown)
        {
            return;
        }

        try
        {
            m_patch = patch;
            m_liveEndpoints = liveEndpoints;
            m_loopMutedConnectionIds = analysis.LoopMutedConnectionIds;

            m_nodes.clear();
            m_connections.clear();

            m_nodeLayer.Children().Clear();
            m_connectionLayer.Children().Clear();

            if (patch == nullptr)
            {
                UpdateExtent();
                UpdateMinimap();
                return;
            }

            for (auto const& endpoint : patch->Endpoints)
            {
                auto const resolved = ResolveEndpoint(endpoint);
                auto const suggestion = resolved.has_value()
                    ? std::nullopt
                    : SuggestReplacementFor(endpoint);

                BuildNode(endpoint, resolved.has_value() ? &resolved.value() : nullptr, suggestion);
            }

            for (auto const& block : patch->Blocks)
            {
                if (IsAnnotation(block.Kind))
                {
                    BuildAnnotationNode(block);
                }
                else
                {
                    BuildBlockNode(block);
                }
            }

            // Whatever was selected and is gone now is let go of, without telling the window:
            // it is the one rebuilding, and it refreshes what depends on the selection itself.
            std::erase_if(m_selectedNodeIds, [this](std::wstring const& id) { return FindNode(id) == nullptr; });

            if ((m_selectionKind == CanvasSelectionKind::Endpoint || m_selectionKind == CanvasSelectionKind::Block) &&
                FindNode(m_selectedNodeId) == nullptr)
            {
                m_selectedNodeId = m_selectedNodeIds.empty() ? std::wstring{} : m_selectedNodeIds.back();
                m_selectionKind = m_selectedNodeId.empty()
                    ? CanvasSelectionKind::None
                    : (patch->IsBlock(m_selectedNodeId) ? CanvasSelectionKind::Block : CanvasSelectionKind::Endpoint);
            }

            if (m_selectionKind == CanvasSelectionKind::Connection && patch->FindConnection(m_selectedConnectionId) == nullptr)
            {
                m_selectionKind = CanvasSelectionKind::None;
                m_selectedConnectionId.clear();
            }

            for (auto& node : m_nodes)
            {
                ApplyNodeAppearance(node);
            }

            // Port offsets are measured once here, so dragging a node afterwards is arithmetic
            // rather than a visual tree walk per frame.
            m_surface.UpdateLayout();

            for (auto& node : m_nodes)
            {
                MeasurePorts(node);
            }

            BuildConnections(analysis);
            RedrawConnections();
            UpdateExtent();
            UpdateMinimap();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the canvas.")
    }

    _Use_decl_annotations_
    void PatchCanvas::BuildNode(
        PatchEndpoint const& endpoint,
        LiveEndpoint const* live,
        std::optional<LiveEndpoint> const& suggestion) noexcept
    {
        NodeVisual node{};

        node.NodeId = endpoint.Id;
        node.IsOffline = live == nullptr;
        node.Width = MinimumNodeWidth;

        auto const textPrimary = ThemeBrush(L"TextFillColorPrimaryBrush", Rgb(0xFF, 0xFF, 0xFF));
        auto const textSecondary = ThemeBrush(L"TextFillColorSecondaryBrush", Rgb(0xC8, 0xC8, 0xC8));
        auto const textTertiary = ThemeBrush(L"TextFillColorTertiaryBrush", Rgb(0x90, 0x90, 0x90));
        auto const accent = ThemeBrush(L"AccentFillColorDefaultBrush", Rgb(0x60, 0xCD, 0xFF));
        auto const critical = ThemeBrush(L"SystemFillColorCriticalBrush", Rgb(0xFF, 0x99, 0xA4));
        auto const success = ThemeBrush(L"SystemFillColorSuccessBrush", Rgb(0x6C, 0xCB, 0x5F));
        auto const divider = ThemeBrush(L"DividerStrokeColorDefaultBrush", Rgb(0x55, 0x55, 0x55));

        controls::StackPanel body{};

        // ---------------------------------------------------------------- header
        controls::Grid header{};

        header.Height(HeaderHeight);
        header.Padding(xaml::ThicknessHelper::FromLengths(10, 0, 10, 0));
        header.ColumnSpacing(9);

        for (auto const width : { xaml::GridLength{ 0, xaml::GridUnitType::Auto },
                                  xaml::GridLength{ 1, xaml::GridUnitType::Star },
                                  xaml::GridLength{ 0, xaml::GridUnitType::Auto } })
        {
            controls::ColumnDefinition column{};
            column.Width(width);
            header.ColumnDefinitions().Append(column);
        }

        controls::Border art{};
        art.Width(28);
        art.Height(28);
        art.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(6));
        art.VerticalAlignment(xaml::VerticalAlignment::Center);
        art.Background(ThemeBrush(L"ControlAltFillColorSecondaryBrush", Rgb(0x30, 0x3A, 0x45)));

        auto const isLoopback = live != nullptr && live->IsLoopback;

        // The customer's own picture wins where there is one. It is small at this size, but it
        // is the thing they chose to recognize the device by.
        auto const artwork = node.IsOffline || live == nullptr
            ? nullptr : LoadEndpointImage(live->ImagePath, 56);

        if (artwork != nullptr)
        {
            controls::Image image{};

            image.Source(artwork);
            image.Stretch(media::Stretch::Uniform);
            image.Margin(xaml::ThicknessHelper::FromUniformLength(3));

            art.Child(image);
        }
        else
        {
            art.Child(MakeGlyph(
                node.IsOffline ? L"\uE711" : (isLoopback ? L"\uE895" : L"\uE7F6"),
                14,
                node.IsOffline ? critical : accent));
        }

        controls::Grid::SetColumn(art, 0);
        header.Children().Append(art);

        controls::StackPanel titles{};
        titles.VerticalAlignment(xaml::VerticalAlignment::Center);

        node.NameText = MakeText(winrt::hstring{ endpoint.DisplayName }, 13, textPrimary, true);
        titles.Children().Append(node.NameText);

        std::wstring subtitle{};

        if (live != nullptr)
        {
            subtitle = live->ManufacturerName;

            if (!live->TransportCode.empty())
            {
                subtitle = subtitle.empty() ? live->TransportCode : subtitle + L" \u00B7 " + live->TransportCode;
            }
        }
        else
        {
            subtitle = std::wstring{ resources::GetString(L"NodeNotConnected") };
        }

        node.SubtitleText = MakeText(winrt::hstring{ subtitle }, 11, textTertiary);
        titles.Children().Append(node.SubtitleText);

        controls::Grid::SetColumn(titles, 1);
        header.Children().Append(titles);

        node.StatusDot = shapes::Ellipse{};
        node.StatusDot.Width(8);
        node.StatusDot.Height(8);
        node.StatusDot.VerticalAlignment(xaml::VerticalAlignment::Center);
        node.StatusDot.Fill(node.IsOffline ? critical : success);

        controls::Grid::SetColumn(node.StatusDot, 2);
        header.Children().Append(node.StatusDot);

        body.Children().Append(header);

        controls::Border headerRule{};
        headerRule.Height(1);
        headerRule.Background(divider);
        body.Children().Append(headerRule);

        // ---------------------------------------------------------------- ports
        controls::Grid columns{};
        columns.Padding(xaml::ThicknessHelper::FromLengths(0, 4, 0, 6));

        for (int i = 0; i < 2; i++)
        {
            controls::ColumnDefinition column{};
            column.Width(xaml::GridLength{ 1, xaml::GridUnitType::Star });
            columns.ColumnDefinitions().Append(column);
        }

        // Which groups get a row. A device that declares nothing still gets group 1 so it is
        // usable, and the node menu can open the rest.
        std::vector<int32_t> groups{};

        if (live != nullptr)
        {
            for (int32_t i = 0; i < MaximumGroupCount; i++)
            {
                if (live->DeclaredGroups[static_cast<size_t>(i)])
                {
                    groups.push_back(i);
                }
            }
        }

        if (endpoint.ShowAllGroups)
        {
            groups.clear();

            for (int32_t i = 0; i < MaximumGroupCount; i++)
            {
                groups.push_back(i);
            }
        }

        if (groups.empty())
        {
            groups.push_back(0);
        }

        // A device that declares every group would otherwise make a node taller than the window;
        // the customer opts into the rest from the node menu.
        if (!endpoint.ShowAllGroups && groups.size() > 8)
        {
            groups.resize(8);
        }

        for (int side = 0; side < 2; side++)
        {
            bool const isOutput = side == 1;

            controls::StackPanel column{};

            auto columnHeader = MakeText(
                resources::GetString(isOutput ? L"PortColumnOut" : L"PortColumnIn"), 11, textTertiary);

            columnHeader.Height(ColumnHeaderHeight);
            columnHeader.Margin(xaml::ThicknessHelper::FromLengths(12, 0, 12, 0));
            columnHeader.HorizontalAlignment(isOutput ? xaml::HorizontalAlignment::Right : xaml::HorizontalAlignment::Left);
            column.Children().Append(columnHeader);

            std::vector<int32_t> rows{ AllGroups };
            rows.insert(rows.end(), groups.begin(), groups.end());

            for (auto const groupIndex : rows)
            {
                PortKey key{};
                key.NodeId = endpoint.Id;
                key.IsOutput = isOutput;
                key.GroupIndex = groupIndex;

                controls::Button row{};
                row.Height(PortRowHeight);
                row.Padding(xaml::ThicknessHelper::FromUniformLength(0));
                row.BorderThickness(xaml::ThicknessHelper::FromUniformLength(0));
                row.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(0));
                row.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                row.HorizontalContentAlignment(xaml::HorizontalAlignment::Stretch);
                row.VerticalContentAlignment(xaml::VerticalAlignment::Center);
                row.Background(media::SolidColorBrush{ Rgb(0, 0, 0, 0) });
                ClearStateFills(row);

                controls::Grid rowGrid{};

                // Inside the node and rounded like it, with the dot hanging over the edge beside it.
                controls::Border highlight{};
                highlight.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(4));
                highlight.Margin(isOutput
                    ? xaml::ThicknessHelper::FromLengths(0, 1, 3, 1)
                    : xaml::ThicknessHelper::FromLengths(3, 1, 0, 1));
                highlight.Background(media::SolidColorBrush{ Rgb(0, 0, 0, 0) });
                rowGrid.Children().Append(highlight);

                std::wstring groupName{};

                if (live != nullptr && groupIndex != AllGroups)
                {
                    groupName = live->GroupName(groupIndex, isOutput);
                }

                auto const groupLabel = DescribeGroupIndex(groupIndex, groupName, !isOutput);

                // A name longer than the widest node allowed is trimmed.
                if (!groupName.empty())
                {
                    controls::ToolTipService::SetToolTip(row, winrt::box_value(groupLabel));
                }

                auto label = MakeText(
                    groupLabel,
                    12,
                    groupIndex == AllGroups ? textPrimary : textSecondary,
                    groupIndex == AllGroups);

                label.HorizontalAlignment(isOutput ? xaml::HorizontalAlignment::Right : xaml::HorizontalAlignment::Left);

                label.Margin(isOutput
                    ? xaml::ThicknessHelper::FromLengths(8, 0, 14, 0)
                    : xaml::ThicknessHelper::FromLengths(14, 0, 8, 0));

                rowGrid.Children().Append(label);

                shapes::Ellipse dot{};
                dot.Width(DotDiameter);
                dot.Height(DotDiameter);
                dot.StrokeThickness(2);
                dot.Stroke(textTertiary);
                dot.Fill(ThemeBrush(L"SolidBackgroundFillColorSecondaryBrush", Rgb(0x2B, 0x2B, 0x2B)));
                dot.VerticalAlignment(xaml::VerticalAlignment::Center);
                dot.HorizontalAlignment(isOutput ? xaml::HorizontalAlignment::Right : xaml::HorizontalAlignment::Left);
                // Centered on the node's edge, half in and half out.
                dot.Margin(isOutput
                    ? xaml::ThicknessHelper::FromLengths(0, 0, -DotDiameter / 2, 0)
                    : xaml::ThicknessHelper::FromLengths(-DotDiameter / 2, 0, 0, 0));

                rowGrid.Children().Append(dot);

                row.Content(rowGrid);

                xaml::Automation::AutomationProperties::SetName(row,
                    resources::FormatString(L"PortAccessibleNameFormat",
                        resources::GetString(isOutput ? L"PortColumnOut" : L"PortColumnIn"),
                        groupLabel,
                        endpoint.DisplayName));

                AttachPortHandlers(row, key);

                PortVisual port{};
                port.Key = key;
                port.Dot = dot;
                port.Row = row;
                port.Label = label;
                port.LabelBrush = groupIndex == AllGroups ? textPrimary : textSecondary;
                port.Highlight = highlight;

                node.Ports.push_back(std::move(port));

                column.Children().Append(row);
            }

            controls::Grid::SetColumn(column, side);
            columns.Children().Append(column);
        }

        body.Children().Append(columns);

        // ---------------------------------------------------------------- alert
        if (node.IsOffline)
        {
            controls::Border alert{};

            alert.Margin(xaml::ThicknessHelper::FromLengths(8, 0, 8, 8));
            alert.Padding(xaml::ThicknessHelper::FromLengths(10, 8, 10, 8));
            alert.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(5));
            alert.Background(ThemeBrush(L"SystemFillColorCriticalBackgroundBrush", Rgb(0x44, 0x27, 0x2A)));
            alert.BorderThickness(xaml::ThicknessHelper::FromUniformLength(1));
            alert.BorderBrush(critical);

            controls::StackPanel alertBody{};
            alertBody.Spacing(6);

            auto message = MakeText(
                suggestion.has_value()
                    ? resources::FormatString(L"NodeReplacementSuggestedFormat", suggestion->Name)
                    : resources::GetString(L"NodeOfflineMessage"),
                11,
                textSecondary);

            message.TextWrapping(xaml::TextWrapping::Wrap);
            message.TextTrimming(xaml::TextTrimming::None);
            alertBody.Children().Append(message);

            alert.Child(alertBody);

            node.AlertPanel = alert;
            body.Children().Append(alert);
        }

        // ---------------------------------------------------------------- root
        controls::Border card{};

        card.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(NodeCornerRadius));
        card.BorderThickness(xaml::ThicknessHelper::FromUniformLength(1));
        card.Background(ThemeBrush(L"CardBackgroundFillColorDefaultBrush", Rgb(0x2B, 0x2B, 0x2B)));
        card.Shadow(media::ThemeShadow{});

        controls::Grid root{};

        root.Width(node.Width);
        root.Children().Append(card);
        root.Children().Append(body);

        controls::Canvas::SetLeft(root, endpoint.CanvasX);
        controls::Canvas::SetTop(root, endpoint.CanvasY);

        AttachNodeHandlers(root, endpoint.Id, false);

        node.Root = root;
        node.Card = card;

        m_nodeLayer.Children().Append(root);

        // Measured once in the tree, where the text has its real font and scale.
        foundation::Size const unbounded{
            std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() };

        header.Measure(unbounded);

        auto widest = static_cast<double>(header.DesiredSize().Width);

        for (auto const& port : node.Ports)
        {
            port.Label.Measure(unbounded);

            // The In and Out halves are always equal, so the wider label sets both.
            widest = std::max(widest, 2.0 * port.Label.DesiredSize().Width);
        }

        node.Width = std::clamp(std::ceil(widest) + NodeWidthAllowance, MinimumNodeWidth, MaximumNodeWidth);
        root.Width(node.Width);

        m_nodes.push_back(std::move(node));

        ApplyNodeAppearance(m_nodes.back());
    }

    _Use_decl_annotations_
    void PatchCanvas::AttachNodeHandlers(xaml::UIElement const& root, std::wstring const& nodeId, bool isBlock) noexcept
    {
        try
        {
            root.PointerPressed([this, nodeId](auto&&, input::PointerRoutedEventArgs const& args)
                {
                    args.Handled(true);

                    FocusCanvas();
                    CancelDrags();

                    OnNodePointerPressed(nodeId, args);
                });

            root.RightTapped([this, nodeId, isBlock](auto&&, input::RightTappedRoutedEventArgs const& args)
                {
                    args.Handled(true);

                    // The menu acts on what is selected, so a node right-clicked on its own is
                    // selected first.
                    if (!IsNodeSelected(nodeId))
                    {
                        Select(isBlock ? CanvasSelectionKind::Block : CanvasSelectionKind::Endpoint, nodeId);
                    }

                    if (m_callbacks.NodeContextMenuRequested)
                    {
                        m_callbacks.NodeContextMenuRequested(nodeId,
                            m_scrollViewer == nullptr ? foundation::Point{} : args.GetPosition(m_scrollViewer));
                    }
                });

            if (isBlock)
            {
                root.DoubleTapped([this, nodeId](auto&&, input::DoubleTappedRoutedEventArgs const& args)
                    {
                        args.Handled(true);

                        // The second press started a drag, and its release goes to whatever the
                        // double-click opens.
                        FinishNodeDrag();

                        if (m_callbacks.BlockActivated)
                        {
                            m_callbacks.BlockActivated(nodeId);
                        }
                    });
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to wire up a node.")
    }

    _Use_decl_annotations_
    void PatchCanvas::AttachPortHandlers(controls::Button const& row, PortKey const& key) noexcept
    {
        try
        {
            auto const keyText = key.ToString();

            row.PointerEntered([this, key](auto&&, auto&&)
                {
                    m_hoverRowPort = key;
                    RefreshPortAppearance();
                });

            row.PointerExited([this, keyText](auto&&, auto&&)
                {
                    if (m_hoverRowPort.has_value() && m_hoverRowPort->ToString() == keyText)
                    {
                        m_hoverRowPort.reset();
                        RefreshPortAppearance();
                    }
                });

            // Either end can start the drag. Insisting on Out first is a rule the customer
            // cannot see, and a drag that does nothing reads as a broken hit target.
            // AddHandler, not row.PointerPressed: ButtonBase marks PointerPressed handled in
            // its class handler, and a plain instance handler never sees a handled event.
            // This is what stopped a drag from ever starting.
            row.AddHandler(xaml::UIElement::PointerPressedEvent(),
                winrt::box_value(input::PointerEventHandler{
                    [this, key](auto&&, input::PointerRoutedEventArgs const& args)
                    {
                        args.Handled(true);

                        FocusCanvas();
                        CancelDrags();

                        BeginConnectionDrag(key);
                    } }),
                true);

            row.Click([this, key](auto&&, auto&&) { OnPortClicked(key); });
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to wire up a connection point.")
    }

    _Use_decl_annotations_
    void PatchCanvas::BuildBlockNode(PatchBlock const& block) noexcept
    {
        try
        {
            NodeVisual node{};

            node.NodeId = block.Id;
            node.IsBlock = true;
            node.Category = CategoryOf(block.Kind);
            node.IsBypassed = block.Bypassed;
            node.Width = BlockNodeWidth;

            auto const textPrimary = ThemeBrush(L"TextFillColorPrimaryBrush", Rgb(0xFF, 0xFF, 0xFF));
            auto const textSecondary = ThemeBrush(L"TextFillColorSecondaryBrush", Rgb(0xC8, 0xC8, 0xC8));
            auto const textTertiary = ThemeBrush(L"TextFillColorTertiaryBrush", Rgb(0x90, 0x90, 0x90));

            auto const name = BlockDisplayName(block);

            controls::Grid body{};

            for (auto const width : { xaml::GridLength{ BlockPortColumnWidth, xaml::GridUnitType::Pixel },
                                      xaml::GridLength{ 1, xaml::GridUnitType::Star },
                                      xaml::GridLength{ BlockPortColumnWidth, xaml::GridUnitType::Pixel } })
            {
                controls::ColumnDefinition column{};
                column.Width(width);
                body.ColumnDefinitions().Append(column);
            }

            // ------------------------------------------------- what it is, what it does
            controls::StackPanel content{};
            content.Padding(xaml::ThicknessHelper::FromLengths(0, 9, 0, 10));
            content.Spacing(4);

            controls::Grid header{};
            header.ColumnSpacing(8);

            for (auto const width : { xaml::GridLength{ 0, xaml::GridUnitType::Auto },
                                      xaml::GridLength{ 1, xaml::GridUnitType::Star } })
            {
                controls::ColumnDefinition column{};
                column.Width(width);
                header.ColumnDefinitions().Append(column);
            }

            auto const badgeLabel = BlockKindBadge(block.Kind);

            controls::Border badge{};
            badge.Width(26);
            badge.Height(24);
            badge.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(6));
            badge.VerticalAlignment(xaml::VerticalAlignment::Center);
            badge.Background(CategoryBrush(node.Category, 0.18));

            auto badgeText = MakeText(badgeLabel, badgeLabel.size() >= 3 ? 9.0 : 12.0, CategoryBrush(node.Category), true);
            badgeText.TextTrimming(xaml::TextTrimming::None);
            badgeText.HorizontalAlignment(xaml::HorizontalAlignment::Center);
            badge.Child(badgeText);

            header.Children().Append(badge);

            node.NameText = MakeText(name, 12.5, textPrimary, true);
            controls::Grid::SetColumn(node.NameText, 1);
            header.Children().Append(node.NameText);

            content.Children().Append(header);

            node.SubtitleText = MakeText(
                block.Bypassed
                    ? resources::GetString(IsGenerator(block.Kind) ? L"BlockBypassedGeneratorCaption" : L"BlockBypassedCaption")
                    : DescribeBlock(block.Kind, block.Settings),
                11.5,
                block.Bypassed ? textTertiary : textSecondary);
            node.SubtitleText.TextWrapping(xaml::TextWrapping::Wrap);
            node.SubtitleText.MaxLines(2);
            node.SubtitleText.Margin(xaml::ThicknessHelper::FromLengths(34, 0, 0, 0));
            content.Children().Append(node.SubtitleText);

            controls::Grid::SetColumn(content, 1);
            body.Children().Append(content);

            // ------------------------------------------------- the way in and the way out
            for (int side = 0; side < 2; side++)
            {
                bool const isOutput = side == 1;

                // MIDI clock and MIDI Time Code make their own messages, so they take nothing in.
                if (!isOutput && !HasInput(block.Kind))
                {
                    continue;
                }

                PortKey key{};
                key.NodeId = block.Id;
                key.IsOutput = isOutput;
                key.GroupIndex = AllGroups;

                controls::Button row{};
                row.Padding(xaml::ThicknessHelper::FromUniformLength(0));
                row.BorderThickness(xaml::ThicknessHelper::FromUniformLength(0));
                row.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(0));
                row.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                row.VerticalAlignment(xaml::VerticalAlignment::Stretch);
                row.HorizontalContentAlignment(xaml::HorizontalAlignment::Stretch);
                row.VerticalContentAlignment(xaml::VerticalAlignment::Center);
                row.Background(media::SolidColorBrush{ Rgb(0, 0, 0, 0) });
                ClearStateFills(row);

                shapes::Ellipse dot{};
                dot.Width(BlockDotDiameter);
                dot.Height(BlockDotDiameter);
                dot.StrokeThickness(2);
                dot.Stroke(textTertiary);
                dot.Fill(ThemeBrush(L"SolidBackgroundFillColorSecondaryBrush", Rgb(0x2B, 0x2B, 0x2B)));
                dot.VerticalAlignment(xaml::VerticalAlignment::Center);
                dot.HorizontalAlignment(isOutput ? xaml::HorizontalAlignment::Right : xaml::HorizontalAlignment::Left);
                dot.Margin(isOutput
                    ? xaml::ThicknessHelper::FromLengths(0, 0, -BlockDotDiameter / 2, 0)
                    : xaml::ThicknessHelper::FromLengths(-BlockDotDiameter / 2, 0, 0, 0));

                row.Content(dot);

                xaml::Automation::AutomationProperties::SetName(row,
                    resources::FormatString(L"BlockPortAccessibleNameFormat",
                        resources::GetString(isOutput ? L"PortColumnOut" : L"PortColumnIn"),
                        name));

                AttachPortHandlers(row, key);

                PortVisual port{};
                port.Key = key;
                port.Dot = dot;
                port.Row = row;

                node.Ports.push_back(std::move(port));

                controls::Grid::SetColumn(row, isOutput ? 2 : 0);
                body.Children().Append(row);
            }

            // ------------------------------------------------- root
            controls::Border card{};

            card.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(BlockCornerRadius));
            card.BorderThickness(xaml::ThicknessHelper::FromUniformLength(1));
            card.Background(ThemeBrush(L"CardBackgroundFillColorDefaultBrush", Rgb(0x2B, 0x2B, 0x2B)));
            card.Shadow(media::ThemeShadow{});

            // A Border cannot dash its outline, so a bypassed block gets one drawn over it.
            node.DashedOutline = shapes::Rectangle{};
            node.DashedOutline.RadiusX(BlockCornerRadius);
            node.DashedOutline.RadiusY(BlockCornerRadius);
            node.DashedOutline.StrokeThickness(1.5);
            node.DashedOutline.StrokeDashArray(MakeDashArray(4.0, 3.0));
            node.DashedOutline.IsHitTestVisible(false);
            node.DashedOutline.Visibility(block.Bypassed ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            controls::Grid root{};

            root.Width(node.Width);
            root.Children().Append(card);
            root.Children().Append(node.DashedOutline);
            root.Children().Append(body);

            controls::ToolTipService::SetToolTip(root, winrt::box_value(winrt::hstring{
                std::wstring{ BlockKindName(block.Kind) } + L"\n" + std::wstring{ BlockKindHint(block.Kind) } }));

            xaml::Automation::AutomationProperties::SetName(root, name);
            xaml::Automation::AutomationProperties::SetHelpText(root, node.SubtitleText.Text());

            controls::Canvas::SetLeft(root, block.CanvasX);
            controls::Canvas::SetTop(root, block.CanvasY);

            AttachNodeHandlers(root, block.Id, true);

            node.Root = root;
            node.Card = card;

            m_nodeLayer.Children().Append(root);
            m_nodes.push_back(std::move(node));

            ApplyNodeAppearance(m_nodes.back());
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build a block on the canvas.")
    }

    _Use_decl_annotations_
    void PatchCanvas::BuildAnnotationNode(PatchBlock const& block) noexcept
    {
        try
        {
            NodeVisual node{};

            node.NodeId = block.Id;
            node.IsBlock = true;
            node.IsAnnotation = true;
            node.Category = BlockCategory::Annotation;

            controls::TextBlock text{};
            text.TextWrapping(xaml::TextWrapping::NoWrap);
            text.TextTrimming(xaml::TextTrimming::None);

            ApplyAnnotationLook(text, block.Settings.Annotation);

            // Clear rather than empty, so a press between the letters still picks it up.
            controls::Border card{};
            card.Padding(xaml::ThicknessHelper::FromLengths(
                AnnotationPaddingX, AnnotationPaddingY, AnnotationPaddingX, AnnotationPaddingY));
            card.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(AnnotationCornerRadius));
            card.BorderThickness(xaml::ThicknessHelper::FromUniformLength(1.5));
            card.Background(media::SolidColorBrush{ Rgb(0, 0, 0, 0) });
            card.Child(text);

            controls::Grid root{};
            root.Children().Append(card);

            xaml::Automation::AutomationProperties::SetName(root, text.Text());

            controls::Canvas::SetLeft(root, block.CanvasX);
            controls::Canvas::SetTop(root, block.CanvasY);

            AttachNodeHandlers(root, block.Id, true);

            node.Root = root;
            node.Card = card;
            node.AnnotationText = text;

            m_nodeLayer.Children().Append(root);

            // Measured in the tree, where the text has its real font.
            root.Measure(foundation::Size{
                std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() });

            node.Width = std::ceil(root.DesiredSize().Width);
            node.Height = std::ceil(root.DesiredSize().Height);

            m_nodes.push_back(std::move(node));

            ApplyNodeAppearance(m_nodes.back());
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build an annotation on the canvas.")
    }

    _Use_decl_annotations_
    void PatchCanvas::MeasurePorts(NodeVisual& node) noexcept
    {
        try
        {
            if (node.Root == nullptr)
            {
                return;
            }

            node.Height = node.Root.ActualHeight();

            for (auto& port : node.Ports)
            {
                if (port.Dot == nullptr)
                {
                    continue;
                }

                auto const transform = port.Dot.TransformToVisual(node.Root);
                auto const center = transform.TransformPoint(
                    foundation::Point{ static_cast<float>(DotDiameter / 2), static_cast<float>(DotDiameter / 2) });

                port.OffsetX = center.X;
                port.OffsetY = center.Y;
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to measure the node connection points.")
    }

    _Use_decl_annotations_
    std::optional<foundation::Point> PatchCanvas::PortPoint(PortKey const& key) noexcept
    {
        auto* node = FindNode(key.NodeId);
        auto const origin = NodePosition(key.NodeId);

        if (node == nullptr || !origin.has_value())
        {
            return std::nullopt;
        }

        for (auto const& port : node->Ports)
        {
            if (port.Key == key)
            {
                return foundation::Point{
                    origin->X + static_cast<float>(port.OffsetX),
                    origin->Y + static_cast<float>(port.OffsetY) };
            }
        }

        // A connection can name a group the node no longer shows, for example after a device
        // came back declaring fewer groups. Falling back to the all-groups point keeps the
        // connection visible instead of silently vanishing.
        for (auto const& port : node->Ports)
        {
            if (port.Key.IsOutput == key.IsOutput && port.Key.GroupIndex == AllGroups)
            {
                return foundation::Point{
                    origin->X + static_cast<float>(port.OffsetX),
                    origin->Y + static_cast<float>(port.OffsetY) };
            }
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    void PatchCanvas::BuildConnections(PatchAnalysis const& analysis) noexcept
    {
        UNREFERENCED_PARAMETER(analysis);

        if (m_patch == nullptr)
        {
            return;
        }

        auto const textSecondary = ThemeBrush(L"TextFillColorSecondaryBrush", Rgb(0xC8, 0xC8, 0xC8));

        for (auto const& connection : m_patch->Connections)
        {
            ConnectionVisual visual{};

            visual.ConnectionId = connection.Id;
            visual.IsMuted = connection.Muted;
            visual.IsLoopMuted = m_loopMutedConnectionIds.count(connection.Id) != 0;

            visual.Glow = shapes::Path{};
            visual.Glow.StrokeThickness(9.0);
            visual.Glow.StrokeEndLineCap(xaml::Media::PenLineCap::Round);
            visual.Glow.IsHitTestVisible(false);
            visual.Glow.Visibility(xaml::Visibility::Collapsed);

            controls::Canvas::SetZIndex(visual.Glow, GlowZIndex);

            m_connectionLayer.Children().Append(visual.Glow);

            visual.Line = shapes::Path{};
            visual.Line.StrokeThickness(2.0);
            visual.Line.StrokeEndLineCap(xaml::Media::PenLineCap::Round);
            visual.Line.IsHitTestVisible(false);

            controls::Canvas::SetZIndex(visual.Line, LineZIndex);

            m_connectionLayer.Children().Append(visual.Line);

            auto const connectionId = connection.Id;

            visual.HitArea = shapes::Path{};
            visual.HitArea.StrokeThickness(16.0);
            visual.HitArea.StrokeEndLineCap(xaml::Media::PenLineCap::Round);

            // Transparent rather than null: a null stroke is not hit tested at all.
            visual.HitArea.Stroke(media::SolidColorBrush{ Rgb(0, 0, 0, 0) });

            controls::Canvas::SetZIndex(visual.HitArea, HitAreaZIndex);

            visual.HitArea.PointerPressed([this, connectionId](auto&&, input::PointerRoutedEventArgs const& args)
                {
                    args.Handled(true);
                    FocusCanvas();
                    CancelDrags();
                    Select(CanvasSelectionKind::Connection, connectionId);

                    // Grabbing a cord picks up whichever end is nearer, so it can be dropped on
                    // a different connection point.
                    auto const position = args.GetCurrentPoint(m_surface).Position();

                    if (auto const* connection = m_patch == nullptr
                        ? nullptr : m_patch->FindConnection(connectionId))
                    {
                        auto const sourcePoint = PortPoint(PortKey{
                            connection->SourceId, true, connection->SourceGroupIndex });
                        auto const destinationPoint = PortPoint(PortKey{
                            connection->DestinationId, false, connection->DestinationGroupIndex });

                        if (sourcePoint.has_value() && destinationPoint.has_value())
                        {
                            auto const distanceTo = [&position](foundation::Point const& p)
                                {
                                    auto const dx = position.X - p.X;
                                    auto const dy = position.Y - p.Y;
                                    return dx * dx + dy * dy;
                                };

                            BeginRetargetDrag(connectionId,
                                distanceTo(sourcePoint.value()) <= distanceTo(destinationPoint.value()));
                        }
                    }
                });

            m_connectionLayer.Children().Append(visual.HitArea);

            controls::Border pill{};

            controls::Canvas::SetZIndex(pill, PillZIndex);

            pill.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(11));
            pill.Padding(xaml::ThicknessHelper::FromLengths(9, 2, 9, 2));
            pill.BorderThickness(xaml::ThicknessHelper::FromUniformLength(1));

            // Opaque on purpose. The card brushes are a few percent white in dark mode, so the
            // line the label sits on would show straight through it.
            pill.Background(ThemeBrush(L"SolidBackgroundFillColorTertiaryBrush", Rgb(0x28, 0x28, 0x28)));

            auto pillText = MakeText(L"", 11, textSecondary);
            pill.Child(pillText);

            pill.PointerPressed([this, connectionId](auto&&, input::PointerRoutedEventArgs const& args)
                {
                    args.Handled(true);
                    FocusCanvas();
                    Select(CanvasSelectionKind::Connection, connectionId);
                });

            visual.Pill = pill;
            visual.PillText = pillText;

            m_connections.push_back(std::move(visual));
        }

        // Child order alone did not hold the label above a line that crosses it, so the three
        // parts of a connection carry an explicit z-index.
        for (auto const& visual : m_connections)
        {
            if (visual.Pill != nullptr)
            {
                m_connectionLayer.Children().Append(visual.Pill);
            }
        }

        for (auto& visual : m_connections)
        {
            ApplyConnectionAppearance(visual);
        }
    }

    void PatchCanvas::RedrawConnections() noexcept
    {
        if (m_patch == nullptr)
        {
            return;
        }

        try
        {
            for (auto& visual : m_connections)
            {
                auto const* connection = m_patch->FindConnection(visual.ConnectionId);

                if (connection == nullptr || visual.Line == nullptr)
                {
                    continue;
                }

                PortKey sourceKey{ connection->SourceId, true, connection->SourceGroupIndex };
                PortKey destinationKey{ connection->DestinationId, false, connection->DestinationGroupIndex };

                auto const from = PortPoint(sourceKey);
                auto const to = PortPoint(destinationKey);

                visual.Samples.clear();

                if (!from.has_value() || !to.has_value())
                {
                    visual.Line.Visibility(xaml::Visibility::Collapsed);

                    if (visual.Glow != nullptr)
                    {
                        visual.Glow.Visibility(xaml::Visibility::Collapsed);
                    }

                    if (visual.HitArea != nullptr)
                    {
                        visual.HitArea.Visibility(xaml::Visibility::Collapsed);
                    }

                    if (visual.Pill != nullptr)
                    {
                        visual.Pill.Visibility(xaml::Visibility::Collapsed);
                    }

                    continue;
                }

                visual.Line.Visibility(xaml::Visibility::Visible);

                if (visual.HitArea != nullptr)
                {
                    visual.HitArea.Visibility(xaml::Visibility::Visible);
                }

                if (visual.Pill != nullptr)
                {
                    visual.Pill.Visibility(xaml::Visibility::Visible);
                }

                auto const dx = std::abs(to->X - from->X);
                auto const reach = static_cast<float>(std::clamp(dx * 0.55, 42.0, 140.0));

                // Built twice on purpose: a Geometry is a DependencyObject and cannot be the
                // Data of two Paths at once, so sharing one with the glow throws.
                auto const buildGeometry = [&]()
                    {
                        media::PathGeometry geometry{};
                        media::PathFigure figure{};

                        figure.StartPoint(from.value());

                        if (to->X < from->X + 20)
                        {
                            // A feedback connection: bow it under everything rather than
                            // dragging it back through the nodes it came from.
                            auto const bow = static_cast<float>(std::max(from->Y, to->Y) + 130.0);
                            auto const mid = (from->X + to->X) / 2;

                            media::BezierSegment first{};
                            first.Point1(foundation::Point{ from->X + 90, from->Y });
                            first.Point2(foundation::Point{ mid + 80, bow });
                            first.Point3(foundation::Point{ mid, bow });

                            media::BezierSegment second{};
                            second.Point1(foundation::Point{ mid - 80, bow });
                            second.Point2(foundation::Point{ to->X - 90, to->Y });
                            second.Point3(to.value());

                            figure.Segments().Append(first);
                            figure.Segments().Append(second);
                        }
                        else
                        {
                            media::BezierSegment segment{};
                            segment.Point1(foundation::Point{ from->X + reach, from->Y });
                            segment.Point2(foundation::Point{ to->X - reach, to->Y });
                            segment.Point3(to.value());

                            figure.Segments().Append(segment);
                        }

                        geometry.Figures().Append(figure);

                        return geometry;
                    };

                visual.Line.Data(buildGeometry());

                // The same curves, as points, for finding what a drop landed on.
                if (to->X < from->X + 20)
                {
                    auto const bow = static_cast<float>(std::max(from->Y, to->Y) + 130.0);
                    auto const mid = (from->X + to->X) / 2;

                    AppendCurveSamples(visual.Samples, from.value(),
                        foundation::Point{ from->X + 90, from->Y }, foundation::Point{ mid + 80, bow }, foundation::Point{ mid, bow });
                    AppendCurveSamples(visual.Samples, foundation::Point{ mid, bow },
                        foundation::Point{ mid - 80, bow }, foundation::Point{ to->X - 90, to->Y }, to.value());
                }
                else
                {
                    AppendCurveSamples(visual.Samples, from.value(),
                        foundation::Point{ from->X + reach, from->Y }, foundation::Point{ to->X - reach, to->Y }, to.value());
                }

                if (visual.HitArea != nullptr)
                {
                    visual.HitArea.Data(buildGeometry());
                }

                if (visual.Glow != nullptr)
                {
                    visual.Glow.Data(buildGeometry());
                    ApplyConnectionAppearance(visual);
                }

                if (visual.PillText != nullptr)
                {
                    auto const sourceIsBlock = m_patch->IsBlock(connection->SourceId);
                    auto const destinationIsBlock = m_patch->IsBlock(connection->DestinationId);

                    winrt::hstring pill{};

                    if (!sourceIsBlock && !destinationIsBlock)
                    {
                        pill = connection->SourceGroupIndex == AllGroups &&
                            connection->DestinationGroupIndex == AllGroups
                            ? resources::GetString(L"ConnectionPillAllGroups")
                            : resources::FormatString(L"ConnectionPillFormat",
                                connection->SourceGroupIndex == AllGroups
                                    ? std::wstring{ resources::GetString(L"ConnectionPillAny") }
                                    : std::to_wstring(connection->SourceGroupIndex + 1),
                                connection->DestinationGroupIndex == AllGroups
                                    ? std::wstring{ resources::GetString(L"ConnectionPillSame") }
                                    : std::to_wstring(connection->DestinationGroupIndex + 1));
                    }
                    else if (!sourceIsBlock && connection->SourceGroupIndex != AllGroups)
                    {
                        // Only an endpoint end has a group, so only that end can say one.
                        pill = resources::FormatString(L"ConnectionPillFromGroupFormat", connection->SourceGroupIndex + 1);
                    }
                    else if (!destinationIsBlock && connection->DestinationGroupIndex != AllGroups)
                    {
                        pill = resources::FormatString(L"ConnectionPillToGroupFormat", connection->DestinationGroupIndex + 1);
                    }

                    visual.PillText.Text(pill);

                    if (visual.Pill != nullptr)
                    {
                        visual.Pill.Visibility(pill.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
                    }
                }

                if (visual.Pill != nullptr && visual.Pill.Visibility() == xaml::Visibility::Visible)
                {
                    visual.Pill.Measure(foundation::Size{ 400, 40 });

                    auto const size = visual.Pill.DesiredSize();

                    // On the curve rather than between the endpoints: a bowed feedback
                    // connection would otherwise label itself on top of a node.
                    auto const mid = visual.Line.Data() != nullptr
                        ? visual.Line.Data().Bounds()
                        : foundation::Rect{};

                    auto const pillX = to->X < from->X + 20
                        ? mid.X + mid.Width / 2
                        : (from->X + to->X) / 2;

                    auto const pillY = to->X < from->X + 20
                        ? mid.Y + mid.Height - size.Height / 2
                        : (from->Y + to->Y) / 2;

                    controls::Canvas::SetLeft(visual.Pill, pillX - size.Width / 2);
                    controls::Canvas::SetTop(visual.Pill, pillY - size.Height / 2);
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to draw the connections.")
    }

    _Use_decl_annotations_
    void PatchCanvas::ApplyNodeAppearance(NodeVisual& node) noexcept
    {
        if (node.Root == nullptr || node.Card == nullptr)
        {
            return;
        }

        auto const critical = ThemeBrush(L"SystemFillColorCriticalBrush", Rgb(0xFF, 0x99, 0xA4));
        auto const accent = ThemeBrush(L"AccentFillColorDefaultBrush", Rgb(0x60, 0xCD, 0xFF));
        auto const stroke = ThemeBrush(L"CardStrokeColorDefaultBrush", Rgb(0x40, 0x40, 0x40));
        auto const tertiary = ThemeBrush(L"TextFillColorTertiaryBrush", Rgb(0x90, 0x90, 0x90));

        auto const selected = IsNodeSelected(node.NodeId);

        // Text has no edge of its own, so only a selected annotation shows one.
        if (node.IsAnnotation)
        {
            node.Card.BorderBrush(selected ? accent : media::SolidColorBrush{ Rgb(0, 0, 0, 0) });
            return;
        }

        if (node.IsBlock)
        {
            // The edge carries the category, so a filter and a transform are told apart at a
            // glance; a bypassed block keeps only its dashed outline.
            node.Card.BorderBrush(selected
                ? accent
                : (node.IsBypassed ? media::SolidColorBrush{ Rgb(0, 0, 0, 0) } : CategoryBrush(node.Category, 0.55)));

            if (node.DashedOutline != nullptr)
            {
                node.DashedOutline.Stroke(tertiary);
            }
        }
        else
        {
            node.Card.BorderBrush(selected ? accent : (node.IsOffline ? critical : stroke));
        }

        node.Card.BorderThickness(xaml::ThicknessHelper::FromUniformLength(selected ? 2.0 : 1.0));

        // Lifting the card casts the shadow, which reads as selection without relying on the
        // border color alone.
        node.Card.Translation(winrt::Windows::Foundation::Numerics::float3{ 0, 0, selected ? 28.0f : 0.0f });
    }

    _Use_decl_annotations_
    void PatchCanvas::ApplyConnectionAppearance(ConnectionVisual& visual) noexcept
    {
        if (visual.Line == nullptr)
        {
            return;
        }

        auto const accent = ThemeBrush(L"AccentFillColorDefaultBrush", Rgb(0x60, 0xCD, 0xFF));
        auto const critical = ThemeBrush(L"SystemFillColorCriticalBrush", Rgb(0xFF, 0x99, 0xA4));
        auto const stroke = ThemeBrush(L"CardStrokeColorDefaultBrush", Rgb(0x40, 0x40, 0x40));

        // A different hue rather than a thicker line of the same color, so selection does not
        // depend on judging two widths against each other.
        auto const selectedStroke = ThemeBrush(L"TextFillColorPrimaryBrush", Rgb(0xFF, 0xFF, 0xFF));

        auto const selected = m_selectionKind == CanvasSelectionKind::Connection &&
            m_selectedConnectionId == visual.ConnectionId;

        // A block being dragged from the palette would go into this one.
        auto const dropTarget = !m_dropTargetConnectionId.empty() && m_dropTargetConnectionId == visual.ConnectionId;

        if (visual.Glow != nullptr)
        {
            visual.Glow.Visibility(selected || dropTarget ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            visual.Glow.Stroke(visual.IsLoopMuted ? critical : accent);
            visual.Glow.Opacity(dropTarget ? 0.6 : 0.35);
        }

        if (visual.IsLoopMuted)
        {
            visual.Line.Stroke(selected ? selectedStroke : critical);
            visual.Line.StrokeThickness(selected ? 3.5 : 2.5);
            visual.Line.StrokeDashArray(MakeDashArray(6.0, 4.0));
        }
        else if (visual.IsMuted)
        {
            visual.Line.Stroke(selected ? selectedStroke : stroke);
            visual.Line.StrokeThickness(selected ? 3.5 : 2.0);
            visual.Line.StrokeDashArray(MakeDashArray(3.0, 3.0));
        }
        else
        {
            visual.Line.Stroke(selected ? selectedStroke : accent);
            visual.Line.StrokeThickness(selected ? 3.5 : 2.0);
            visual.Line.StrokeDashArray(nullptr);
            visual.Line.Opacity(selected ? 1.0 : 0.72);
        }

        if (visual.Pill != nullptr)
        {
            visual.Pill.BorderBrush(visual.IsLoopMuted ? critical : (selected ? accent : stroke));
        }
    }

    _Use_decl_annotations_
    void PatchCanvas::RefreshStatus(std::unordered_map<std::wstring, RouteStats> const& stats) noexcept
    {
        try
        {
            for (auto& visual : m_connections)
            {
                auto const it = stats.find(visual.ConnectionId);

                if (visual.Line == nullptr)
                {
                    continue;
                }

                // A route with no live endpoints is drawn faded so "waiting for a device" reads
                // differently from "wired up and quiet".
                if (!visual.IsLoopMuted && !visual.IsMuted)
                {
                    visual.Line.Opacity(it != stats.end() && it->second.IsActive ? 0.85 : 0.32);
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to refresh the canvas status.")
    }

    _Use_decl_annotations_
    void PatchCanvas::Select(CanvasSelectionKind kind, std::wstring const& id) noexcept
    {
        try
        {
            m_selectionKind = id.empty() ? CanvasSelectionKind::None : kind;
            m_selectedNodeId = kind == CanvasSelectionKind::Endpoint || kind == CanvasSelectionKind::Block ? id : std::wstring{};
            m_selectedConnectionId = kind == CanvasSelectionKind::Connection ? id : std::wstring{};

            m_selectedNodeIds.clear();

            if (!m_selectedNodeId.empty())
            {
                m_selectedNodeIds.push_back(m_selectedNodeId);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to change the selection.")

        for (auto& node : m_nodes)
        {
            ApplyNodeAppearance(node);
        }

        for (auto& visual : m_connections)
        {
            ApplyConnectionAppearance(visual);
        }

        if (m_callbacks.SelectionChanged)
        {
            m_callbacks.SelectionChanged();
        }
    }

    _Use_decl_annotations_
    void PatchCanvas::SelectNodes(std::vector<std::wstring> const& nodeIds) noexcept
    {
        try
        {
            m_selectedNodeIds.clear();

            for (auto const& id : nodeIds)
            {
                if (FindNode(id) != nullptr && !IsNodeSelected(id))
                {
                    m_selectedNodeIds.push_back(id);
                }
            }

            m_selectedConnectionId.clear();
            m_selectedNodeId = m_selectedNodeIds.empty() ? std::wstring{} : m_selectedNodeIds.back();

            m_selectionKind = m_selectedNodeId.empty()
                ? CanvasSelectionKind::None
                : (m_patch != nullptr && m_patch->IsBlock(m_selectedNodeId) ? CanvasSelectionKind::Block : CanvasSelectionKind::Endpoint);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to select several nodes.")

        for (auto& node : m_nodes)
        {
            ApplyNodeAppearance(node);
        }

        for (auto& visual : m_connections)
        {
            ApplyConnectionAppearance(visual);
        }

        if (m_callbacks.SelectionChanged)
        {
            m_callbacks.SelectionChanged();
        }
    }

    void PatchCanvas::ClearSelection() noexcept
    {
        Select(CanvasSelectionKind::None, {});
    }

    _Use_decl_annotations_
    void PatchCanvas::OnNodePointerPressed(
        std::wstring const& nodeId,
        input::PointerRoutedEventArgs const& args) noexcept
    {
        try
        {
            if (m_patch == nullptr)
            {
                return;
            }

            auto const kind = m_patch->IsBlock(nodeId) ? CanvasSelectionKind::Block : CanvasSelectionKind::Endpoint;

            auto const control = (args.KeyModifiers() & winrt::Windows::System::VirtualKeyModifiers::Control) ==
                winrt::Windows::System::VirtualKeyModifiers::Control;

            if (control)
            {
                // Ctrl adds a node to what is selected, or takes it away again.
                auto next = m_selectedNodeIds;

                if (IsNodeSelected(nodeId))
                {
                    std::erase(next, nodeId);
                }
                else
                {
                    next.push_back(nodeId);
                }

                SelectNodes(next);
                return;
            }

            if (!IsNodeSelected(nodeId))
            {
                Select(kind, nodeId);
            }
            else if (m_selectedNodeId != nodeId)
            {
                // Pressing one of several selected nodes makes it the one the inspector shows,
                // and keeps the rest so they can be dragged together.
                m_selectedNodeId = nodeId;
                m_selectionKind = kind;

                if (m_callbacks.SelectionChanged)
                {
                    m_callbacks.SelectionChanged();
                }
            }

            auto const point = args.GetCurrentPoint(m_surface);

            if (!point.Properties().IsLeftButtonPressed())
            {
                return;
            }

            m_draggingNode = true;
            m_dragNodeId = nodeId;
            m_dragStartPointer = point.Position();
            m_dragStartPositions.clear();

            for (auto const& id : m_selectedNodeIds)
            {
                if (auto const position = NodePosition(id))
                {
                    m_dragStartPositions.emplace_back(id, position.value());
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to start a node drag.")
    }

    _Use_decl_annotations_
    void PatchCanvas::OnSurfacePointerMoved(input::PointerRoutedEventArgs const& args) noexcept
    {
        try
        {
            if (!m_draggingNode && !m_draggingConnection)
            {
                return;
            }

            auto const point = args.GetCurrentPoint(m_surface);

            // The release went somewhere else, such as a dialog that opened in the middle of the
            // drag. A cord can be drawn with two clicks, so only a node drag ends here.
            if (m_draggingNode && !point.Properties().IsLeftButtonPressed())
            {
                FinishNodeDrag();
                return;
            }

            ApplyDragPosition(point.Position());
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to move a node.")
    }

    _Use_decl_annotations_
    void PatchCanvas::ApplyDragPosition(foundation::Point const& position) noexcept
    {
        try
        {
            if (m_draggingConnection)
            {
                UpdateConnectionDrag(position);
                return;
            }

            if (m_patch == nullptr || m_dragStartPositions.empty())
            {
                return;
            }

            // The group moves as one, so it stops at the edge as a whole rather than squashing.
            double left{ std::numeric_limits<double>::max() };
            double top{ std::numeric_limits<double>::max() };

            for (auto const& [id, start] : m_dragStartPositions)
            {
                left = std::min(left, static_cast<double>(start.X));
                top = std::min(top, static_cast<double>(start.Y));
            }

            auto const dx = std::max(-left, static_cast<double>(position.X - m_dragStartPointer.X));
            auto const dy = std::max(-top, static_cast<double>(position.Y - m_dragStartPointer.Y));

            for (auto const& [id, start] : m_dragStartPositions)
            {
                SetNodePosition(id, start.X + dx, start.Y + dy);
            }

            RedrawConnections();
            UpdateMinimap();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to move a node.")
    }

    _Use_decl_annotations_
    void PatchCanvas::OnSurfacePointerReleased(input::PointerRoutedEventArgs const& args) noexcept
    {
        try
        {
            // Move events are coalesced and a quick drag can deliver none near the target, so
            // where the button came up is what decides the result, for cords and nodes alike.
            if (m_draggingConnection || m_draggingNode)
            {
                ApplyDragPosition(args.GetCurrentPoint(m_surface).Position());
            }

            if (m_draggingConnection)
            {
                EndConnectionDrag();
                return;
            }

            FinishNodeDrag();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to finish a node drag.")
    }

    void PatchCanvas::FinishNodeDrag() noexcept
    {
        try
        {
            if (!m_draggingNode)
            {
                return;
            }

            m_draggingNode = false;

            UpdateExtent();
            UpdateMinimap();

            // A press that never moved anything is a click: nothing to save, nothing to undo.
            auto const moved = std::any_of(m_dragStartPositions.begin(), m_dragStartPositions.end(),
                [this](auto const& entry)
                {
                    auto const now = NodePosition(entry.first);
                    return now.has_value() && (now->X != entry.second.X || now->Y != entry.second.Y);
                });

            m_dragStartPositions.clear();

            if (moved && m_callbacks.LayoutChanged)
            {
                m_callbacks.LayoutChanged();
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to finish a node drag.")
    }

    _Use_decl_annotations_
    void PatchCanvas::BeginConnectionDrag(PortKey const& key) noexcept
    {
        m_draggingConnection = true;
        m_retargeting = false;
        m_dragMoved = false;
        m_dragSourcePort = key;
        m_dragWantsOutput = !key.IsOutput;
        m_hoverPort.reset();

        auto const anchor = PortPoint(key);
        m_dragAnchor = anchor.value_or(foundation::Point{});

        if (m_dragLine != nullptr)
        {
            m_dragLine.Visibility(xaml::Visibility::Visible);
        }

        RefreshPortAppearance();
    }

    _Use_decl_annotations_
    void PatchCanvas::BeginRetargetDrag(std::wstring const& connectionId, bool movingSource) noexcept
    {
        try
        {
            if (m_patch == nullptr)
            {
                return;
            }

            auto const* connection = m_patch->FindConnection(connectionId);

            if (connection == nullptr)
            {
                return;
            }

            // The end that is NOT moving stays pinned, so the line rubber bands from there.
            auto const anchor = movingSource
                ? PortPoint(PortKey{ connection->DestinationId, false, connection->DestinationGroupIndex })
                : PortPoint(PortKey{ connection->SourceId, true, connection->SourceGroupIndex });

            if (!anchor.has_value())
            {
                return;
            }

            m_draggingConnection = true;
            m_retargeting = true;
            m_retargetConnectionId = connectionId;
            m_retargetMovingSource = movingSource;
            m_dragMoved = false;
            m_dragWantsOutput = movingSource;
            m_dragAnchor = anchor.value();
            m_hoverPort.reset();

            if (m_dragLine != nullptr)
            {
                m_dragLine.Visibility(xaml::Visibility::Visible);
            }

            RefreshPortAppearance();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to pick up the connection.")
    }

    _Use_decl_annotations_
    std::optional<PortKey> PatchCanvas::FindPortNear(foundation::Point const& point, bool wantOutput) noexcept
    {
        if (m_patch == nullptr)
        {
            return std::nullopt;
        }

        std::optional<PortKey> best{};
        double bestDistance = PortSnapRadius;

        for (auto const& node : m_nodes)
        {
            auto const origin = NodePosition(node.NodeId);

            if (!origin.has_value())
            {
                continue;
            }

            for (auto const& port : node.Ports)
            {
                if (port.Key.IsOutput != wantOutput)
                {
                    continue;
                }

                auto const dx = point.X - (origin->X + port.OffsetX);
                auto const dy = point.Y - (origin->Y + port.OffsetY);
                auto const distance = std::sqrt(dx * dx + dy * dy);

                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best = port.Key;
                }
            }
        }

        return best;
    }

    _Use_decl_annotations_
    void PatchCanvas::UpdateConnectionDrag(foundation::Point const& point) noexcept
    {
        try
        {
            if (m_dragLine == nullptr)
            {
                return;
            }

            auto const from = m_dragAnchor;

            if (std::abs(point.X - from.X) > 6 || std::abs(point.Y - from.Y) > 6)
            {
                m_dragMoved = true;
            }

            auto const target = FindPortNear(point, m_dragWantsOutput);

            if (target.has_value() != m_hoverPort.has_value() ||
                (target.has_value() && !(target.value() == m_hoverPort.value())))
            {
                m_hoverPort = target;
                RefreshPortAppearance();
            }

            // Snap the loose end onto the target, so it is obvious where the drop will land.
            auto end = point;

            if (target.has_value())
            {
                if (auto const snapped = PortPoint(target.value()))
                {
                    end = snapped.value();
                }
            }

            auto const reach = static_cast<float>(std::clamp(std::abs(end.X - from.X) * 0.55, 42.0, 140.0));

            media::PathGeometry geometry{};
            media::PathFigure figure{};

            figure.StartPoint(from);

            media::BezierSegment segment{};
            segment.Point1(foundation::Point{ from.X + reach, from.Y });
            segment.Point2(foundation::Point{ end.X - reach, end.Y });
            segment.Point3(end);

            figure.Segments().Append(segment);
            geometry.Figures().Append(figure);

            m_dragLine.Data(geometry);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to draw the connection being made.")
    }

    void PatchCanvas::EndConnectionDrag() noexcept
    {
        try
        {
            if (!m_draggingConnection)
            {
                return;
            }

            m_draggingConnection = false;

            if (m_dragLine != nullptr)
            {
                m_dragLine.Visibility(xaml::Visibility::Collapsed);
            }

            auto const target = m_hoverPort;
            auto const wasRetargeting = m_retargeting;
            auto const retargetId = m_retargetConnectionId;
            auto const movingSource = m_retargetMovingSource;

            m_hoverPort.reset();
            m_retargeting = false;

            RefreshPortAppearance();

            // A press that never moved is a click, and the click handler is what arms the port
            // for the keyboard friendly two step path.
            m_suppressNextPortClick = m_dragMoved;

            if (!m_dragMoved || !target.has_value())
            {
                return;
            }

            if (wasRetargeting)
            {
                RequestRetarget(retargetId, movingSource, target.value());
                return;
            }

            // The two ends have to be opposite sides; which one the drag started from does not
            // matter, so the pair is ordered here rather than being demanded of the customer.
            if (target->IsOutput == m_dragSourcePort.IsOutput)
            {
                return;
            }

            if (m_dragSourcePort.IsOutput)
            {
                RequestConnection(m_dragSourcePort, target.value());
            }
            else
            {
                RequestConnection(target.value(), m_dragSourcePort);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to finish making a connection.")
    }

    _Use_decl_annotations_
    void PatchCanvas::RequestRetarget(
        std::wstring const& connectionId,
        bool movingSource,
        PortKey const& port) noexcept
    {
        try
        {
            if (!m_callbacks.ConnectionRetargetRequested || m_patch == nullptr)
            {
                return;
            }

            auto const* existing = m_patch->FindConnection(connectionId);

            if (existing == nullptr)
            {
                return;
            }

            auto updated = *existing;

            if (movingSource)
            {
                updated.SourceId = port.NodeId;
                updated.SourceGroupIndex = port.GroupIndex;
            }
            else
            {
                updated.DestinationId = port.NodeId;
                updated.DestinationGroupIndex = port.GroupIndex;
            }

            m_callbacks.ConnectionRetargetRequested(connectionId, std::move(updated));
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to move the connection.")
    }

    _Use_decl_annotations_
    void PatchCanvas::OnPortClicked(PortKey const& key) noexcept
    {
        try
        {
            if (m_suppressNextPortClick)
            {
                m_suppressNextPortClick = false;
                return;
            }

            if (m_armedPort.has_value() && m_armedPort->IsOutput != key.IsOutput)
            {
                auto const first = m_armedPort.value();

                ClearArmedPort();

                if (first.IsOutput)
                {
                    RequestConnection(first, key);
                }
                else
                {
                    RequestConnection(key, first);
                }

                return;
            }

            ClearArmedPort();

            m_armedPort = key;

            RefreshPortAppearance();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to select a connection point.")
    }

    void PatchCanvas::ClearArmedPort() noexcept
    {
        if (!m_armedPort.has_value())
        {
            return;
        }

        m_armedPort.reset();

        RefreshPortAppearance();
    }

    void PatchCanvas::RefreshPortAppearance() noexcept
    {
        for (auto& node : m_nodes)
        {
            for (auto& port : node.Ports)
            {
                ApplyPortAppearance(port);
            }
        }
    }

    // Delete is handled on the scroll viewer, which only sees it while the canvas has focus.
    void PatchCanvas::FocusCanvas() noexcept
    {
        if (m_scrollViewer != nullptr)
        {
            m_scrollViewer.Focus(xaml::FocusState::Pointer);
        }
    }

    void PatchCanvas::CancelDrags() noexcept
    {
        m_draggingNode = false;

        if (m_draggingConnection)
        {
            m_draggingConnection = false;
            m_retargeting = false;
            m_hoverPort.reset();

            if (m_dragLine != nullptr)
            {
                m_dragLine.Visibility(xaml::Visibility::Collapsed);
            }

            RefreshPortAppearance();
        }
    }

    _Use_decl_annotations_
    void PatchCanvas::ApplyPortAppearance(PortVisual& port) noexcept
    {
        if (port.Dot == nullptr)
        {
            return;
        }

        auto const accent = ThemeBrush(L"AccentFillColorDefaultBrush", Rgb(0x60, 0xCD, 0xFF));
        auto const tertiary = ThemeBrush(L"TextFillColorTertiaryBrush", Rgb(0x90, 0x90, 0x90));
        auto const secondary = ThemeBrush(L"TextFillColorSecondaryBrush", Rgb(0xC8, 0xC8, 0xC8));

        auto const armed = m_armedPort.has_value() && m_armedPort.value() == port.Key;
        auto const hovered = m_hoverRowPort.has_value() && m_hoverRowPort.value() == port.Key;

        // While a connection is being made, every end that could receive it is filled in, so
        // the customer can see where the drag is allowed to land instead of guessing.
        auto const candidate =
            (m_draggingConnection && m_dragWantsOutput == port.Key.IsOutput) ||
            (m_armedPort.has_value() && m_armedPort->IsOutput != port.Key.IsOutput);

        // The one it would actually snap to right now.
        auto const snapped = m_hoverPort.has_value() && m_hoverPort.value() == port.Key;

        auto const filled = armed || candidate || snapped;

        port.Dot.Stroke(filled || hovered ? accent : tertiary);
        port.Dot.StrokeThickness(snapped ? 3.0 : 2.0);
        port.Dot.Fill(filled
            ? accent
            : ThemeBrush(L"SolidBackgroundFillColorSecondaryBrush", Rgb(0x2B, 0x2B, 0x2B)));

        if (port.Highlight != nullptr)
        {
            port.Highlight.Background(hovered
                ? ThemeBrush(L"SubtleFillColorSecondaryBrush", Rgb(0xFF, 0xFF, 0xFF, 0x0F))
                : media::SolidColorBrush{ Rgb(0, 0, 0, 0) });
        }

        if (port.Label != nullptr)
        {
            port.Label.Foreground(hovered || filled ? secondary : port.LabelBrush);
        }
    }

    _Use_decl_annotations_
    void PatchCanvas::RequestConnection(PortKey const& source, PortKey const& destination) noexcept
    {
        if (!m_callbacks.ConnectionRequested)
        {
            return;
        }

        PatchConnection candidate{};

        candidate.Id = PatchDocument::NewId();
        candidate.SourceId = source.NodeId;
        candidate.SourceGroupIndex = source.GroupIndex;
        candidate.DestinationId = destination.NodeId;
        candidate.DestinationGroupIndex = destination.GroupIndex;

        m_callbacks.ConnectionRequested(candidate);
    }

    void PatchCanvas::UpdateExtent() noexcept
    {
        try
        {
            double right{ 0 };
            double bottom{ 0 };

            for (auto const& node : m_nodes)
            {
                auto const origin = NodePosition(node.NodeId);

                if (!origin.has_value())
                {
                    continue;
                }

                auto const height = node.Height > 0
                    ? node.Height
                    : (node.IsBlock ? BlockFallbackHeight : EndpointFallbackHeight);

                right = std::max(right, origin->X + node.Width);
                bottom = std::max(bottom, origin->Y + height);
            }

            // The canvas is always bigger than what is on it, so there is somewhere to drag a
            // node to. The viewport size is the floor so a nearly empty patch does not scroll.
            auto const viewportWidth = m_scrollViewer != nullptr ? m_scrollViewer.ViewportWidth() : 0.0;
            auto const viewportHeight = m_scrollViewer != nullptr ? m_scrollViewer.ViewportHeight() : 0.0;

            m_extent.Width = static_cast<float>(std::max(right + CanvasMargin, viewportWidth));
            m_extent.Height = static_cast<float>(std::max(bottom + CanvasMargin, viewportHeight));

            m_surface.Width(m_extent.Width);
            m_surface.Height(m_extent.Height);

            for (auto const& layer : { m_connectionLayer, m_nodeLayer, m_overlayLayer })
            {
                if (layer != nullptr)
                {
                    layer.Width(m_extent.Width);
                    layer.Height(m_extent.Height);
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to size the canvas.")
    }

    _Use_decl_annotations_
    void PatchCanvas::FitToContent(float maximumZoom) noexcept
    {
        try
        {
            if (m_scrollViewer == nullptr || m_patch == nullptr || m_nodes.empty())
            {
                return;
            }

            // A window that starts minimized has no viewport yet, and fitting to nothing would
            // drop straight to the lowest zoom.
            if (m_scrollViewer.ViewportWidth() <= 0 || m_scrollViewer.ViewportHeight() <= 0)
            {
                return;
            }

            double left{ std::numeric_limits<double>::max() };
            double top{ std::numeric_limits<double>::max() };
            double right{ std::numeric_limits<double>::lowest() };
            double bottom{ std::numeric_limits<double>::lowest() };

            auto const include = [&](double x, double y, double width, double height)
                {
                    left = std::min(left, x);
                    top = std::min(top, y);
                    right = std::max(right, x + width);
                    bottom = std::max(bottom, y + height);
                };

            for (auto const& node : m_nodes)
            {
                auto const origin = NodePosition(node.NodeId);

                if (!origin.has_value())
                {
                    continue;
                }

                auto const height = node.Height > 0
                    ? node.Height
                    : (node.IsBlock ? BlockFallbackHeight : EndpointFallbackHeight);

                include(origin->X, origin->Y, node.Width, height);
            }

            // A connection back to an earlier node bows below the nodes, label and all.
            for (auto const& visual : m_connections)
            {
                if (visual.Line != nullptr &&
                    visual.Line.Visibility() == xaml::Visibility::Visible &&
                    visual.Line.Data() != nullptr)
                {
                    auto const bounds = visual.Line.Data().Bounds();
                    include(bounds.X, bounds.Y, bounds.Width, bounds.Height);
                }

                if (visual.Pill != nullptr && visual.Pill.Visibility() == xaml::Visibility::Visible)
                {
                    auto const size = visual.Pill.DesiredSize();

                    include(controls::Canvas::GetLeft(visual.Pill), controls::Canvas::GetTop(visual.Pill),
                        size.Width, size.Height);
                }
            }

            auto const contentWidth = std::max(1.0, right - left + 2 * FitMargin);
            auto const contentHeight = std::max(1.0, bottom - top + 2 * FitMargin);

            auto const zoom = static_cast<float>(std::clamp(
                std::min(m_scrollViewer.ViewportWidth() / contentWidth,
                         m_scrollViewer.ViewportHeight() / contentHeight),
                static_cast<double>(MinimumZoom),
                static_cast<double>(std::clamp(maximumZoom, MinimumZoom, MaximumZoom))));

            m_scrollViewer.ChangeView(
                winrt::box_value((left - FitMargin) * zoom).as<foundation::IReference<double>>(),
                winrt::box_value((top - FitMargin) * zoom).as<foundation::IReference<double>>(),
                winrt::box_value(zoom).as<foundation::IReference<float>>());
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to fit the canvas to its content.")
    }

    void PatchCanvas::AutoArrange() noexcept
    {
        try
        {
            if (m_patch == nullptr || m_nodes.empty())
            {
                return;
            }

            // The real sizes, now that the nodes are built.
            ArrangeInColumns(*const_cast<PatchDocument*>(m_patch), [this](std::wstring const& id) -> NodeSize
                {
                    if (auto const* node = FindNode(id))
                    {
                        return NodeSize{ node->Width, node->Height > 0
                            ? node->Height
                            : (node->IsBlock ? BlockFallbackHeight : EndpointFallbackHeight) };
                    }

                    return EstimatedNodeSize(*m_patch, id);
                });

            for (auto const& node : m_nodes)
            {
                if (auto const origin = NodePosition(node.NodeId); origin.has_value() && node.Root != nullptr)
                {
                    controls::Canvas::SetLeft(node.Root, origin->X);
                    controls::Canvas::SetTop(node.Root, origin->Y);
                }
            }

            RedrawConnections();
            UpdateExtent();
            UpdateMinimap();

            if (m_callbacks.LayoutChanged)
            {
                m_callbacks.LayoutChanged();
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to arrange the canvas.")
    }

    _Use_decl_annotations_
    void PatchCanvas::RefreshAnnotation(std::wstring const& blockId) noexcept
    {
        try
        {
            auto* node = FindNode(blockId);
            auto const* block = m_patch == nullptr ? nullptr : m_patch->FindBlock(blockId);

            if (node == nullptr || block == nullptr || node->AnnotationText == nullptr || node->Root == nullptr)
            {
                return;
            }

            ApplyAnnotationLook(node->AnnotationText, block->Settings.Annotation);
            xaml::Automation::AutomationProperties::SetName(node->Root, node->AnnotationText.Text());

            node->Root.Measure(foundation::Size{
                std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() });

            node->Width = std::ceil(node->Root.DesiredSize().Width);
            node->Height = std::ceil(node->Root.DesiredSize().Height);

            UpdateExtent();
            UpdateMinimap();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to redraw an annotation.")
    }

    void PatchCanvas::UpdateMinimap() noexcept
    {
        try
        {
            if (m_minimap == nullptr)
            {
                return;
            }

            m_minimap.Children().Clear();

            if (m_patch == nullptr || m_nodes.empty() || m_extent.Width <= 0 || m_extent.Height <= 0)
            {
                m_minimap.Visibility(xaml::Visibility::Collapsed);
                return;
            }

            m_minimap.Visibility(xaml::Visibility::Visible);
            m_minimap.Width(MinimapWidth);
            m_minimap.Height(MinimapHeight);

            auto const zoom = m_scrollViewer != nullptr && m_scrollViewer.ZoomFactor() > 0
                ? static_cast<double>(m_scrollViewer.ZoomFactor()) : 1.0;

            // What is in view counts too, so zoomed out past the content the box still fits.
            auto const worldWidth = std::max(static_cast<double>(m_extent.Width),
                m_scrollViewer != nullptr ? m_scrollViewer.ViewportWidth() / zoom : 0.0);
            auto const worldHeight = std::max(static_cast<double>(m_extent.Height),
                m_scrollViewer != nullptr ? m_scrollViewer.ViewportHeight() / zoom : 0.0);

            m_minimapScale = std::min(MinimapWidth / worldWidth, MinimapHeight / worldHeight);

            auto const scale = m_minimapScale;

            auto const accent = ThemeBrush(L"AccentFillColorDefaultBrush", Rgb(0x60, 0xCD, 0xFF));
            auto const critical = ThemeBrush(L"SystemFillColorCriticalBrush", Rgb(0xFF, 0x99, 0xA4));
            auto const nodeBrush = ThemeBrush(L"TextFillColorTertiaryBrush", Rgb(0x90, 0x90, 0x90));

            for (auto const& node : m_nodes)
            {
                auto const origin = NodePosition(node.NodeId);

                if (!origin.has_value())
                {
                    continue;
                }

                auto const height = node.Height > 0
                    ? node.Height
                    : (node.IsBlock ? BlockFallbackHeight : EndpointFallbackHeight);

                shapes::Rectangle rectangle{};

                rectangle.Width(std::max(2.0, node.Width * scale));
                rectangle.Height(std::max(2.0, height * scale));
                rectangle.RadiusX(1);
                rectangle.RadiusY(1);
                rectangle.Fill(node.IsBlock
                    ? CategoryBrush(node.Category)
                    : (node.IsOffline ? critical : nodeBrush));

                controls::Canvas::SetLeft(rectangle, origin->X * scale);
                controls::Canvas::SetTop(rectangle, origin->Y * scale);

                m_minimap.Children().Append(rectangle);
            }

            if (auto const box = MinimapViewportRect(); box.Width > 0 && box.Height > 0)
            {
                shapes::Rectangle viewport{};

                viewport.Width(std::max(4.0, static_cast<double>(box.Width)));
                viewport.Height(std::max(4.0, static_cast<double>(box.Height)));
                viewport.Stroke(accent);
                viewport.StrokeThickness(1.0);

                controls::Canvas::SetLeft(viewport, box.X);
                controls::Canvas::SetTop(viewport, box.Y);

                m_minimap.Children().Append(viewport);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to draw the canvas map.")
    }

    foundation::Rect PatchCanvas::MinimapViewportRect() const noexcept
    {
        try
        {
            if (m_scrollViewer != nullptr && m_minimapScale > 0 && m_scrollViewer.ZoomFactor() > 0)
            {
                auto const perPixel = m_minimapScale / static_cast<double>(m_scrollViewer.ZoomFactor());

                return foundation::Rect{
                    static_cast<float>(m_scrollViewer.HorizontalOffset() * perPixel),
                    static_cast<float>(m_scrollViewer.VerticalOffset() * perPixel),
                    static_cast<float>(m_scrollViewer.ViewportWidth() * perPixel),
                    static_cast<float>(m_scrollViewer.ViewportHeight() * perPixel) };
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to place the view on the canvas map.")

        return {};
    }

    _Use_decl_annotations_
    void PatchCanvas::OnMinimapPointerPressed(input::PointerRoutedEventArgs const& args) noexcept
    {
        try
        {
            if (m_minimap == nullptr || m_minimapScale <= 0)
            {
                return;
            }

            auto const point = args.GetCurrentPoint(m_minimap);

            if (!point.Properties().IsLeftButtonPressed())
            {
                return;
            }

            args.Handled(true);

            auto const position = point.Position();
            auto const box = MinimapViewportRect();

            auto const onBox = position.X >= box.X && position.X <= box.X + box.Width &&
                position.Y >= box.Y && position.Y <= box.Y + box.Height;

            // Grabbing the box keeps it under the pointer where it was taken; pressing anywhere
            // else brings that spot to the middle of the view.
            m_minimapGrabOffset = onBox
                ? foundation::Point{ position.X - box.X, position.Y - box.Y }
                : foundation::Point{ box.Width / 2, box.Height / 2 };

            m_panningMinimap = true;
            m_minimap.CapturePointer(args.Pointer());

            PanToMinimapPoint(position);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to start moving the view from the canvas map.")
    }

    _Use_decl_annotations_
    void PatchCanvas::OnMinimapPointerMoved(input::PointerRoutedEventArgs const& args) noexcept
    {
        try
        {
            if (!m_panningMinimap || m_minimap == nullptr)
            {
                return;
            }

            args.Handled(true);
            PanToMinimapPoint(args.GetCurrentPoint(m_minimap).Position());
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to move the view from the canvas map.")
    }

    _Use_decl_annotations_
    void PatchCanvas::OnMinimapPointerReleased(input::PointerRoutedEventArgs const& args) noexcept
    {
        try
        {
            if (!m_panningMinimap || m_minimap == nullptr)
            {
                return;
            }

            m_panningMinimap = false;
            args.Handled(true);

            // Moves are coalesced, so the release is what says where the view ends up.
            PanToMinimapPoint(args.GetCurrentPoint(m_minimap).Position());
            m_minimap.ReleasePointerCapture(args.Pointer());
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to finish moving the view from the canvas map.")
    }

    _Use_decl_annotations_
    void PatchCanvas::PanToMinimapPoint(foundation::Point const& point) noexcept
    {
        try
        {
            if (m_scrollViewer == nullptr || m_minimapScale <= 0 || m_scrollViewer.ZoomFactor() <= 0)
            {
                return;
            }

            // Scroll offsets are in zoomed pixels, the map is in canvas units times its scale.
            auto const pixelsPerMapUnit = static_cast<double>(m_scrollViewer.ZoomFactor()) / m_minimapScale;

            auto const left = std::max(0.0, (point.X - m_minimapGrabOffset.X) * pixelsPerMapUnit);
            auto const top = std::max(0.0, (point.Y - m_minimapGrabOffset.Y) * pixelsPerMapUnit);

            m_scrollViewer.ChangeView(
                winrt::box_value(left).as<foundation::IReference<double>>(),
                winrt::box_value(top).as<foundation::IReference<double>>(),
                nullptr,
                true);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to move the view from the canvas map.")
    }

    _Use_decl_annotations_
    void PatchCanvas::MoveClearOfOtherNodes(std::wstring const& nodeId) noexcept
    {
        try
        {
            auto* node = FindNode(nodeId);
            auto const start = NodePosition(nodeId);

            if (node == nullptr || node->Root == nullptr || !start.has_value())
            {
                return;
            }

            double x = start->X;
            double const y = start->Y;

            // Only ever moves right, and each pass gets past one more node, so this ends.
            for (size_t pass = 0; pass <= m_nodes.size(); pass++)
            {
                auto clear = true;

                for (auto const& other : m_nodes)
                {
                    if (other.NodeId == nodeId)
                    {
                        continue;
                    }

                    auto const placed = NodePosition(other.NodeId);

                    if (!placed.has_value())
                    {
                        continue;
                    }

                    auto const overlaps =
                        x < placed->X + other.Width + NodeClearance &&
                        placed->X < x + node->Width + NodeClearance &&
                        y < placed->Y + other.Height &&
                        placed->Y < y + node->Height;

                    if (overlaps)
                    {
                        x = placed->X + other.Width + NodeClearance;
                        clear = false;
                    }
                }

                if (clear)
                {
                    break;
                }
            }

            if (x == start->X)
            {
                return;
            }

            SetNodePosition(nodeId, x, y);

            RedrawConnections();
            UpdateExtent();
            UpdateMinimap();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to move a new node clear of the others.")
    }

    _Use_decl_annotations_
    void PatchCanvas::BringIntoView(std::wstring const& nodeId) noexcept
    {
        try
        {
            auto const* node = FindNode(nodeId);
            auto const origin = NodePosition(nodeId);

            if (node == nullptr || !origin.has_value() || m_scrollViewer == nullptr)
            {
                return;
            }

            // So the scroll range already takes in a node that was just moved past the old edge.
            m_scrollViewer.UpdateLayout();

            auto const zoom = static_cast<double>(m_scrollViewer.ZoomFactor());

            if (zoom <= 0 || m_scrollViewer.ViewportWidth() <= 0 || m_scrollViewer.ViewportHeight() <= 0)
            {
                return;
            }

            auto const height = node->Height > 0
                ? node->Height
                : (node->IsBlock ? BlockFallbackHeight : EndpointFallbackHeight);

            // In canvas units. Bigger than the view, its top left corner is what shows.
            auto const reveal = [](double start, double size, double viewStart, double viewSize)
                {
                    if (start - FitMargin < viewStart || size + 2 * FitMargin > viewSize)
                    {
                        return start - FitMargin;
                    }

                    if (start + size + FitMargin > viewStart + viewSize)
                    {
                        return start + size + FitMargin - viewSize;
                    }

                    return viewStart;
                };

            auto const viewLeft = m_scrollViewer.HorizontalOffset() / zoom;
            auto const viewTop = m_scrollViewer.VerticalOffset() / zoom;

            auto const left = reveal(origin->X, node->Width, viewLeft, m_scrollViewer.ViewportWidth() / zoom);
            auto const top = reveal(origin->Y, height, viewTop, m_scrollViewer.ViewportHeight() / zoom);

            if (left == viewLeft && top == viewTop)
            {
                return;
            }

            m_scrollViewer.ChangeView(
                winrt::box_value((std::max)(0.0, left) * zoom).as<foundation::IReference<double>>(),
                winrt::box_value((std::max)(0.0, top) * zoom).as<foundation::IReference<double>>(),
                nullptr);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to bring a node into view.")
    }

    foundation::Point PatchCanvas::ViewCenter() const noexcept
    {
        try
        {
            if (m_scrollViewer != nullptr && m_scrollViewer.ZoomFactor() > 0)
            {
                auto const zoom = static_cast<double>(m_scrollViewer.ZoomFactor());

                return foundation::Point{
                    static_cast<float>((m_scrollViewer.HorizontalOffset() + m_scrollViewer.ViewportWidth() / 2) / zoom),
                    static_cast<float>((m_scrollViewer.VerticalOffset() + m_scrollViewer.ViewportHeight() / 2) / zoom) };
            }
        }
        catch (...)
        {
        }

        return foundation::Point{ 240, 160 };
    }

    _Use_decl_annotations_
    std::wstring PatchCanvas::ConnectionAt(foundation::Point const& point) const noexcept
    {
        try
        {
            std::wstring best{};
            double bestDistance = ConnectionHitDistance;

            for (auto const& visual : m_connections)
            {
                if (visual.Line == nullptr || visual.Line.Visibility() != xaml::Visibility::Visible)
                {
                    continue;
                }

                for (size_t i = 1; i < visual.Samples.size(); i++)
                {
                    auto const distance = DistanceToSegment(point, visual.Samples[i - 1], visual.Samples[i]);

                    if (distance < bestDistance)
                    {
                        bestDistance = distance;
                        best = visual.ConnectionId;
                    }
                }
            }

            return best;
        }
        catch (...)
        {
        }

        return {};
    }

    _Use_decl_annotations_
    void PatchCanvas::OnDragOver(xaml::DragEventArgs const& args) noexcept
    {
        try
        {
            namespace transfer = winrt::Windows::ApplicationModel::DataTransfer;

            auto const view = args.DataView();
            auto const properties = view == nullptr ? nullptr : view.Properties();

            auto const isBlock = properties != nullptr && properties.HasKey(PaletteBlockKindProperty);
            auto const isEndpoint = properties != nullptr && properties.HasKey(PaletteEndpointProperty);

            if (!isBlock && !isEndpoint)
            {
                args.AcceptedOperation(transfer::DataPackageOperation::None);
                SetDropTarget({});
                return;
            }

            args.AcceptedOperation(transfer::DataPackageOperation::Copy);

            // Only a step that passes on what a connection carries can go into one: not a
            // generator, which passes on only what it makes, and not an annotation.
            auto canInsert = false;

            if (isBlock)
            {
                auto const key = winrt::unbox_value_or<winrt::hstring>(properties.Lookup(PaletteBlockKindProperty), L"");
                auto const kind = BlockKindFromKey(key);

                canInsert = kind.has_value() && CanGoIntoConnection(kind.value());
            }

            auto const target = canInsert ? ConnectionAt(args.GetPosition(m_surface)) : std::wstring{};

            SetDropTarget(target);

            if (auto const ui = args.DragUIOverride())
            {
                ui.Caption(target.empty()
                    ? resources::GetString(L"DropAddToPatch")
                    : resources::GetString(L"DropIntoConnection"));
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to follow a drag over the canvas.")
    }

    _Use_decl_annotations_
    void PatchCanvas::OnDrop(xaml::DragEventArgs const& args) noexcept
    {
        try
        {
            SetDropTarget({});

            auto const view = args.DataView();
            auto const properties = view == nullptr ? nullptr : view.Properties();

            if (properties == nullptr)
            {
                return;
            }

            auto const point = args.GetPosition(m_surface);

            if (properties.HasKey(PaletteBlockKindProperty))
            {
                auto const key = winrt::unbox_value_or<winrt::hstring>(properties.Lookup(PaletteBlockKindProperty), L"");
                auto const kind = BlockKindFromKey(key);

                if (kind.has_value() && m_callbacks.BlockDropped)
                {
                    m_callbacks.BlockDropped(kind.value(), point,
                        CanGoIntoConnection(kind.value()) ? ConnectionAt(point) : std::wstring{});
                }

                return;
            }

            if (properties.HasKey(PaletteEndpointProperty) && m_callbacks.EndpointDropped)
            {
                auto const id = winrt::unbox_value_or<winrt::hstring>(properties.Lookup(PaletteEndpointProperty), L"");

                if (!id.empty())
                {
                    m_callbacks.EndpointDropped(std::wstring{ id }, point);
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to take a drop on the canvas.")
    }

    _Use_decl_annotations_
    void PatchCanvas::SetDropTarget(std::wstring const& connectionId) noexcept
    {
        try
        {
            if (m_dropTargetConnectionId == connectionId)
            {
                return;
            }

            m_dropTargetConnectionId = connectionId;

            for (auto& visual : m_connections)
            {
                ApplyConnectionAppearance(visual);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show where a drop would land.")
    }
}
