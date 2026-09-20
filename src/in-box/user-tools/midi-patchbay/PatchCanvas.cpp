// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "PatchCanvas.h"
#include "StringResources.h"

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

        // Within the connection layer: the selection glow, then the line, then its group label.
        constexpr int GlowZIndex = 0;
        constexpr int LineZIndex = 1;
        constexpr int HitAreaZIndex = 2;
        constexpr int PillZIndex = 3;

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

        // BitmapImage cannot render SVG and the shipped default endpoint art is SVG, so the
        // decoder is chosen by extension, the same way the Settings app does it.
        media::ImageSource LoadEndpointImage(_In_ std::wstring const& path, _In_ int32_t pixelHeight) noexcept
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
            }

            m_initialized = true;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to initialize the canvas.")
    }

    void PatchCanvas::Shutdown() noexcept
    {
        m_shuttingDown = true;
        m_patch = nullptr;
        m_nodes.clear();
        m_connections.clear();
    }

    _Use_decl_annotations_
    PatchCanvas::NodeVisual* PatchCanvas::FindNode(std::wstring const& endpointId) noexcept
    {
        auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
            [&endpointId](NodeVisual const& n) { return n.EndpointId == endpointId; });

        return it == m_nodes.end() ? nullptr : &(*it);
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

            auto& catalog = EndpointCatalog::Current();

            for (auto const& endpoint : patch->Endpoints)
            {
                auto const resolved = catalog.Resolve(endpoint);
                auto const suggestion = resolved.has_value()
                    ? std::nullopt
                    : catalog.SuggestReplacement(endpoint);

                BuildNode(endpoint, resolved.has_value() ? &resolved.value() : nullptr, suggestion);
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

        node.EndpointId = endpoint.Id;
        node.IsOffline = live == nullptr;
        node.Width = NodeWidth;

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
                key.EndpointId = endpoint.Id;
                key.IsOutput = isOutput;
                key.GroupIndex = groupIndex;

                controls::Button row{};
                row.Height(PortRowHeight);
                row.Padding(xaml::ThicknessHelper::FromUniformLength(0));
                row.BorderThickness(xaml::ThicknessHelper::FromUniformLength(0));
                row.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(4));
                row.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                row.HorizontalContentAlignment(xaml::HorizontalAlignment::Stretch);
                row.VerticalContentAlignment(xaml::VerticalAlignment::Center);
                row.Background(media::SolidColorBrush{ Rgb(0, 0, 0, 0) });

                controls::Grid rowGrid{};

                std::wstring portName{};

                if (live != nullptr && groupIndex != AllGroups)
                {
                    // the label is the same name this group carries as a MIDI 1.0 port, which is
                    // what the customer already sees in every other app
                    portName = live->PortName(groupIndex, isOutput);

                    // A single group device names its port after the device, so repeating it on
                    // every row says nothing and pushes the useful part out of view.
                    if (!portName.empty() && !endpoint.DisplayName.empty() &&
                        (portName.find(endpoint.DisplayName) != std::wstring::npos ||
                         endpoint.DisplayName.find(portName) != std::wstring::npos))
                    {
                        portName.clear();
                    }
                }

                auto const groupLabel = DescribeGroupIndex(groupIndex, portName);

                auto label = MakeText(
                    groupLabel,
                    12,
                    groupIndex == AllGroups ? textPrimary : textSecondary,
                    groupIndex == AllGroups);

                label.HorizontalAlignment(isOutput ? xaml::HorizontalAlignment::Right : xaml::HorizontalAlignment::Left);

                label.Margin(isOutput
                    ? xaml::ThicknessHelper::FromLengths(8, 0, 22, 0)
                    : xaml::ThicknessHelper::FromLengths(22, 0, 8, 0));

                rowGrid.Children().Append(label);

                shapes::Ellipse dot{};
                dot.Width(DotDiameter);
                dot.Height(DotDiameter);
                dot.StrokeThickness(2);
                dot.Stroke(textTertiary);
                dot.Fill(ThemeBrush(L"SolidBackgroundFillColorSecondaryBrush", Rgb(0x2B, 0x2B, 0x2B)));
                dot.VerticalAlignment(xaml::VerticalAlignment::Center);
                dot.HorizontalAlignment(isOutput ? xaml::HorizontalAlignment::Right : xaml::HorizontalAlignment::Left);
                // Just inside the edge, not straddling it: the node's rounded border clips its
                // child, and a dot hanging over the edge renders as a half circle.
                dot.Margin(isOutput
                    ? xaml::ThicknessHelper::FromLengths(0, 0, 4, 0)
                    : xaml::ThicknessHelper::FromLengths(4, 0, 0, 0));

                rowGrid.Children().Append(dot);

                row.Content(rowGrid);

                xaml::Automation::AutomationProperties::SetName(row,
                    resources::FormatString(L"PortAccessibleNameFormat",
                        resources::GetString(isOutput ? L"PortColumnOut" : L"PortColumnIn"),
                        groupLabel,
                        endpoint.DisplayName));

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

                PortVisual port{};
                port.Key = key;
                port.Dot = dot;
                port.Row = row;
                port.Label = label;
                port.LabelBrush = groupIndex == AllGroups ? textPrimary : textSecondary;

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
        controls::Border root{};

        root.Width(node.Width);
        root.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(NodeCornerRadius));
        root.BorderThickness(xaml::ThicknessHelper::FromUniformLength(1));
        root.Background(ThemeBrush(L"CardBackgroundFillColorDefaultBrush", Rgb(0x2B, 0x2B, 0x2B)));
        root.Shadow(media::ThemeShadow{});
        root.Child(body);

        controls::Canvas::SetLeft(root, endpoint.CanvasX);
        controls::Canvas::SetTop(root, endpoint.CanvasY);

        auto const endpointId = endpoint.Id;

        root.PointerPressed([this, endpointId](auto&&, input::PointerRoutedEventArgs const& args)
            {
                args.Handled(true);

                FocusCanvas();
                CancelDrags();

                OnNodePointerPressed(endpointId, args);
            });

        root.RightTapped([this, endpointId](auto&&, input::RightTappedRoutedEventArgs const& args)
            {
                args.Handled(true);

                if (m_callbacks.EndpointContextMenuRequested)
                {
                    m_callbacks.EndpointContextMenuRequested(endpointId,
                        m_scrollViewer == nullptr ? foundation::Point{} : args.GetPosition(m_scrollViewer));
                }
            });

        node.Root = root;

        m_nodeLayer.Children().Append(root);
        m_nodes.push_back(std::move(node));

        ApplyNodeAppearance(m_nodes.back());
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
        auto* node = FindNode(key.EndpointId);

        if (node == nullptr || m_patch == nullptr)
        {
            return std::nullopt;
        }

        auto const* endpoint = m_patch->FindEndpoint(key.EndpointId);

        if (endpoint == nullptr)
        {
            return std::nullopt;
        }

        for (auto const& port : node->Ports)
        {
            if (port.Key == key)
            {
                return foundation::Point{
                    static_cast<float>(endpoint->CanvasX + port.OffsetX),
                    static_cast<float>(endpoint->CanvasY + port.OffsetY) };
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
                    static_cast<float>(endpoint->CanvasX + port.OffsetX),
                    static_cast<float>(endpoint->CanvasY + port.OffsetY) };
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
                            connection->SourceEndpointId, true, connection->SourceGroupIndex });
                        auto const destinationPoint = PortPoint(PortKey{
                            connection->DestinationEndpointId, false, connection->DestinationGroupIndex });

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

                PortKey sourceKey{ connection->SourceEndpointId, true, connection->SourceGroupIndex };
                PortKey destinationKey{ connection->DestinationEndpointId, false, connection->DestinationGroupIndex };

                auto const from = PortPoint(sourceKey);
                auto const to = PortPoint(destinationKey);

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
                    visual.PillText.Text(connection->SourceGroupIndex == AllGroups &&
                        connection->DestinationGroupIndex == AllGroups
                        ? resources::GetString(L"ConnectionPillAllGroups")
                        : resources::FormatString(L"ConnectionPillFormat",
                            connection->SourceGroupIndex == AllGroups
                                ? std::wstring{ resources::GetString(L"ConnectionPillAny") }
                                : std::to_wstring(connection->SourceGroupIndex + 1),
                            connection->DestinationGroupIndex == AllGroups
                                ? std::wstring{ resources::GetString(L"ConnectionPillSame") }
                                : std::to_wstring(connection->DestinationGroupIndex + 1)));
                }

                if (visual.Pill != nullptr)
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
        if (node.Root == nullptr)
        {
            return;
        }

        auto const critical = ThemeBrush(L"SystemFillColorCriticalBrush", Rgb(0xFF, 0x99, 0xA4));
        auto const accent = ThemeBrush(L"AccentFillColorDefaultBrush", Rgb(0x60, 0xCD, 0xFF));
        auto const stroke = ThemeBrush(L"CardStrokeColorDefaultBrush", Rgb(0x40, 0x40, 0x40));

        auto const selected = m_selectionKind == CanvasSelectionKind::Endpoint &&
            m_selectedEndpointId == node.EndpointId;

        node.Root.BorderBrush(selected ? accent : (node.IsOffline ? critical : stroke));
        node.Root.BorderThickness(xaml::ThicknessHelper::FromUniformLength(selected ? 2.0 : 1.0));

        // Lifting the card casts the shadow, which reads as selection without relying on the
        // border color alone.
        node.Root.Translation(winrt::Windows::Foundation::Numerics::float3{ 0, 0, selected ? 28.0f : 0.0f });
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

        if (visual.Glow != nullptr)
        {
            visual.Glow.Visibility(selected ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            visual.Glow.Stroke(visual.IsLoopMuted ? critical : accent);
            visual.Glow.Opacity(0.35);
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
        m_selectionKind = kind;
        m_selectedEndpointId = kind == CanvasSelectionKind::Endpoint ? id : std::wstring{};
        m_selectedConnectionId = kind == CanvasSelectionKind::Connection ? id : std::wstring{};

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
        std::wstring const& endpointId,
        input::PointerRoutedEventArgs const& args) noexcept
    {
        try
        {
            Select(CanvasSelectionKind::Endpoint, endpointId);

            if (m_patch == nullptr)
            {
                return;
            }

            auto const* endpoint = m_patch->FindEndpoint(endpointId);

            if (endpoint == nullptr)
            {
                return;
            }

            auto const point = args.GetCurrentPoint(m_surface);

            if (!point.Properties().IsLeftButtonPressed())
            {
                return;
            }

            m_draggingNode = true;
            m_dragNodeId = endpointId;
            m_dragStartPointer = point.Position();
            m_dragStartX = endpoint->CanvasX;
            m_dragStartY = endpoint->CanvasY;
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

            ApplyDragPosition(args.GetCurrentPoint(m_surface).Position());
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

            auto* node = FindNode(m_dragNodeId);

            if (node == nullptr || node->Root == nullptr || m_patch == nullptr)
            {
                return;
            }

            // The model is const to the canvas everywhere else; this is the one place it moves,
            // and the window is told so it can mark the patch unsaved.
            auto* endpoint = const_cast<PatchDocument*>(m_patch)->FindEndpoint(m_dragNodeId);

            if (endpoint == nullptr)
            {
                return;
            }

            endpoint->CanvasX = std::max(0.0, m_dragStartX + (position.X - m_dragStartPointer.X));
            endpoint->CanvasY = std::max(0.0, m_dragStartY + (position.Y - m_dragStartPointer.Y));

            controls::Canvas::SetLeft(node->Root, endpoint->CanvasX);
            controls::Canvas::SetTop(node->Root, endpoint->CanvasY);

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

            if (!m_draggingNode)
            {
                return;
            }

            m_draggingNode = false;

            UpdateExtent();
            UpdateMinimap();

            if (m_callbacks.LayoutChanged)
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
                ? PortPoint(PortKey{ connection->DestinationEndpointId, false, connection->DestinationGroupIndex })
                : PortPoint(PortKey{ connection->SourceEndpointId, true, connection->SourceGroupIndex });

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
            auto const* endpoint = m_patch->FindEndpoint(node.EndpointId);

            if (endpoint == nullptr)
            {
                continue;
            }

            for (auto const& port : node.Ports)
            {
                if (port.Key.IsOutput != wantOutput)
                {
                    continue;
                }

                auto const dx = point.X - (endpoint->CanvasX + port.OffsetX);
                auto const dy = point.Y - (endpoint->CanvasY + port.OffsetY);
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
                updated.SourceEndpointId = port.EndpointId;
                updated.SourceGroupIndex = port.GroupIndex;
            }
            else
            {
                updated.DestinationEndpointId = port.EndpointId;
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

        if (port.Row != nullptr)
        {
            port.Row.Background(hovered
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
        candidate.SourceEndpointId = source.EndpointId;
        candidate.SourceGroupIndex = source.GroupIndex;
        candidate.DestinationEndpointId = destination.EndpointId;
        candidate.DestinationGroupIndex = destination.GroupIndex;

        m_callbacks.ConnectionRequested(candidate);
    }

    void PatchCanvas::UpdateExtent() noexcept
    {
        try
        {
            double right{ 0 };
            double bottom{ 0 };

            if (m_patch != nullptr)
            {
                for (auto const& endpoint : m_patch->Endpoints)
                {
                    auto const* node = FindNode(endpoint.Id);
                    auto const height = node != nullptr && node->Height > 0 ? node->Height : 200.0;

                    right = std::max(right, endpoint.CanvasX + NodeWidth);
                    bottom = std::max(bottom, endpoint.CanvasY + height);
                }
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

    void PatchCanvas::FitToContent() noexcept
    {
        try
        {
            if (m_scrollViewer == nullptr || m_patch == nullptr || m_patch->Endpoints.empty())
            {
                return;
            }

            double left{ std::numeric_limits<double>::max() };
            double top{ std::numeric_limits<double>::max() };
            double right{ 0 };
            double bottom{ 0 };

            for (auto const& endpoint : m_patch->Endpoints)
            {
                auto const* node = FindNode(endpoint.Id);
                auto const height = node != nullptr && node->Height > 0 ? node->Height : 200.0;

                left = std::min(left, endpoint.CanvasX);
                top = std::min(top, endpoint.CanvasY);
                right = std::max(right, endpoint.CanvasX + NodeWidth);
                bottom = std::max(bottom, endpoint.CanvasY + height);
            }

            auto const contentWidth = std::max(1.0, right - left + 48.0);
            auto const contentHeight = std::max(1.0, bottom - top + 48.0);

            auto const zoom = static_cast<float>(std::clamp(
                std::min(m_scrollViewer.ViewportWidth() / contentWidth,
                         m_scrollViewer.ViewportHeight() / contentHeight),
                0.4, 1.0));

            m_scrollViewer.ChangeView(
                winrt::box_value((left - 24.0) * zoom).as<foundation::IReference<double>>(),
                winrt::box_value((top - 24.0) * zoom).as<foundation::IReference<double>>(),
                winrt::box_value(zoom).as<foundation::IReference<float>>());
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to fit the canvas to its content.")
    }

    void PatchCanvas::AutoArrange() noexcept
    {
        try
        {
            if (m_patch == nullptr || m_patch->Endpoints.empty())
            {
                return;
            }

            auto* patch = const_cast<PatchDocument*>(m_patch);

            // Anything that only sends goes in the left column; everything else on the right.
            // That is the shape nearly every patch ends up in when arranged by hand.
            std::unordered_set<std::wstring> hasIncoming{};

            for (auto const& connection : patch->Connections)
            {
                hasIncoming.insert(connection.DestinationEndpointId);
            }

            double columnY[2] = { ArrangeTopMargin, ArrangeTopMargin };

            for (auto& endpoint : patch->Endpoints)
            {
                auto const column = hasIncoming.count(endpoint.Id) != 0 ? 1 : 0;

                auto const* node = FindNode(endpoint.Id);
                auto const height = node != nullptr && node->Height > 0 ? node->Height : 200.0;

                endpoint.CanvasX = DefaultColumnX[column];
                endpoint.CanvasY = columnY[column];

                columnY[column] += height + ArrangeRowGap;

                if (node != nullptr && node->Root != nullptr)
                {
                    controls::Canvas::SetLeft(node->Root, endpoint.CanvasX);
                    controls::Canvas::SetTop(node->Root, endpoint.CanvasY);
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

    void PatchCanvas::UpdateMinimap() noexcept
    {
        try
        {
            if (m_minimap == nullptr)
            {
                return;
            }

            m_minimap.Children().Clear();

            if (m_patch == nullptr || m_patch->Endpoints.empty() || m_extent.Width <= 0 || m_extent.Height <= 0)
            {
                m_minimap.Visibility(xaml::Visibility::Collapsed);
                return;
            }

            m_minimap.Visibility(xaml::Visibility::Visible);
            m_minimap.Width(MinimapWidth);
            m_minimap.Height(MinimapHeight);

            auto const scale = std::min(
                MinimapWidth / static_cast<double>(m_extent.Width),
                MinimapHeight / static_cast<double>(m_extent.Height));

            auto const accent = ThemeBrush(L"AccentFillColorDefaultBrush", Rgb(0x60, 0xCD, 0xFF));
            auto const critical = ThemeBrush(L"SystemFillColorCriticalBrush", Rgb(0xFF, 0x99, 0xA4));
            auto const nodeBrush = ThemeBrush(L"TextFillColorTertiaryBrush", Rgb(0x90, 0x90, 0x90));

            for (auto const& endpoint : m_patch->Endpoints)
            {
                auto const* node = FindNode(endpoint.Id);
                auto const height = node != nullptr && node->Height > 0 ? node->Height : 200.0;

                shapes::Rectangle rectangle{};

                rectangle.Width(std::max(2.0, NodeWidth * scale));
                rectangle.Height(std::max(2.0, height * scale));
                rectangle.RadiusX(1);
                rectangle.RadiusY(1);
                rectangle.Fill(node != nullptr && node->IsOffline ? critical : nodeBrush);

                controls::Canvas::SetLeft(rectangle, endpoint.CanvasX * scale);
                controls::Canvas::SetTop(rectangle, endpoint.CanvasY * scale);

                m_minimap.Children().Append(rectangle);
            }

            if (m_scrollViewer != nullptr && m_scrollViewer.ZoomFactor() > 0)
            {
                auto const zoom = static_cast<double>(m_scrollViewer.ZoomFactor());

                shapes::Rectangle viewport{};

                viewport.Width(std::max(4.0, m_scrollViewer.ViewportWidth() / zoom * scale));
                viewport.Height(std::max(4.0, m_scrollViewer.ViewportHeight() / zoom * scale));
                viewport.Stroke(accent);
                viewport.StrokeThickness(1.0);

                controls::Canvas::SetLeft(viewport, m_scrollViewer.HorizontalOffset() / zoom * scale);
                controls::Canvas::SetTop(viewport, m_scrollViewer.VerticalOffset() / zoom * scale);

                m_minimap.Children().Append(viewport);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to draw the canvas map.")
    }
}
