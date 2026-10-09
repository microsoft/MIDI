// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "EndpointCatalog.h"
#include "PatchGraph.h"
#include "PatchModel.h"
#include "RouteEngine.h"
#include "ThemeBrushes.h"

namespace midipatchbay
{
    // Identifies one connection point: a node, a side, and a group (or all groups). A block has
    // one of each side, and its group is always AllGroups.
    struct PortKey
    {
        std::wstring NodeId{};
        bool IsOutput{ false };
        int32_t GroupIndex{ AllGroups };

        bool operator==(PortKey const& other) const noexcept
        {
            return NodeId == other.NodeId &&
                IsOutput == other.IsOutput &&
                GroupIndex == other.GroupIndex;
        }

        std::wstring ToString() const noexcept
        {
            return NodeId + (IsOutput ? L"|out|" : L"|in|") + std::to_wstring(GroupIndex);
        }
    };

    enum class CanvasSelectionKind
    {
        None = 0,
        Endpoint = 1,
        Connection = 2,
        Block = 3,
    };

    // What the palette sets on a drag's data, so the canvas knows what is arriving without
    // waiting for the data itself.
    constexpr wchar_t PaletteBlockKindProperty[] = L"PatchbayBlockKind";
    constexpr wchar_t PaletteEndpointProperty[] = L"PatchbayEndpointId";

    // The node graph surface: builds the visuals, draws the connections, and turns pointer input
    // into requests the window decides on.
    //
    // Deliberately a plain C++ class rather than a XAML control. Everything here is built and
    // measured in code because the connections need exact port positions, and a DataTemplate
    // would put those behind a visual tree walk on every redraw.
    class PatchCanvas
    {
    public:
        struct Callbacks
        {
            std::function<void()> SelectionChanged{};

            // The customer dragged from an output to an input. The window validates it, checks
            // for loops and commits, which is why this hands over a candidate rather than a fact.
            std::function<void(PatchConnection)> ConnectionRequested{};

            // A node was dragged to a new place; the patch is now unsaved.
            std::function<void()> LayoutChanged{};

            // The point is relative to the scroll viewer, so the menu opens where the click
            // happened rather than at the corner of the canvas.
            std::function<void(std::wstring, foundation::Point)> NodeContextMenuRequested{};

            // The customer dragged one end of an existing connection onto a different port.
            // The window validates and commits it, the same as a brand new connection.
            std::function<void(std::wstring, PatchConnection)> ConnectionRetargetRequested{};

            std::function<void()> ViewportChanged{};

            // A block from the palette was dropped. The point is in canvas units; the connection
            // is the one it landed on, if any, so the window can put the block into it.
            std::function<void(BlockKind, foundation::Point, std::wstring)> BlockDropped{};

            // An endpoint from the palette was dropped, by endpoint device id.
            std::function<void(std::wstring, foundation::Point)> EndpointDropped{};

            // A block was double-clicked, which opens its settings.
            std::function<void(std::wstring)> BlockActivated{};
        };

        void Initialize(
            _In_ controls::ScrollViewer const& scrollViewer,
            _In_ controls::Canvas const& surface,
            _In_ controls::Canvas const& minimap,
            _In_ Callbacks callbacks) noexcept;

        void Shutdown() noexcept;

        // Rebuilds every visual. Called when the patch or the live endpoint list changes.
        void Rebuild(
            _In_ PatchDocument const* patch,
            _In_ std::vector<LiveEndpoint> const& liveEndpoints,
            _In_ PatchAnalysis const& analysis) noexcept;

        // Cheap refresh of the things that change often: activity, offline state, muted state.
        // Keyed by link or block id.
        void RefreshStatus(
            _In_ std::unordered_map<std::wstring, RouteStats> const& stats) noexcept;

        CanvasSelectionKind SelectionKind() const noexcept { return m_selectionKind; }

        // The node the inspector shows: the one clicked last.
        std::wstring const& SelectedNodeId() const noexcept { return m_selectedNodeId; }
        std::wstring const& SelectedConnectionId() const noexcept { return m_selectedConnectionId; }

        // Every selected node, the primary one included. Ctrl and a click add or take away one.
        std::vector<std::wstring> const& SelectedNodeIds() const noexcept { return m_selectedNodeIds; }

        void Select(_In_ CanvasSelectionKind kind, _In_ std::wstring const& id) noexcept;

        // Selects several nodes at once, for example what was just pasted.
        void SelectNodes(_In_ std::vector<std::wstring> const& nodeIds) noexcept;

        void ClearSelection() noexcept;

        // Moves the viewport so everything on the canvas is in view, at the largest zoom that fits
        // up to the ceiling. Automatic fits stay at 100% or less so a small patch is not blown up.
        void FitToContent(_In_ float maximumZoom = 1.0f) noexcept;

        // Lays the nodes out left to right in the order messages flow.
        void AutoArrange() noexcept;

        // Redraws one annotation from the patch as it is now, for text that is still being typed.
        void RefreshAnnotation(_In_ std::wstring const& blockId) noexcept;

        // For a node that was just added: its width is only known once it is built, so a spot
        // picked beforehand can land on another node. Moves it right until it does not.
        void MoveClearOfOtherNodes(_In_ std::wstring const& nodeId) noexcept;

        // Scrolls only as far as it takes to show all of a node, at the zoom there is now.
        void BringIntoView(_In_ std::wstring const& nodeId) noexcept;

        // Lets palette drops land on something laid over the canvas, such as the hint on an empty
        // patch, as well as on the canvas itself.
        void AcceptDrops(_In_ xaml::UIElement const& element) noexcept;

        void UpdateMinimap() noexcept;

        // The middle of what is in view, in canvas units, for something added without a drop.
        foundation::Point ViewCenter() const noexcept;

        // The connection nearest the point, in canvas units, when one is close enough to hit.
        std::wstring ConnectionAt(_In_ foundation::Point const& point) const noexcept;

        // Where a traced message went: its nodes and links stand out, the steps that kept it out
        // are marked, and everything else fades. It stays through a rebuild until ClearTrace.
        void ShowTrace(
            _In_ std::vector<std::wstring> const& nodeIds,
            _In_ std::vector<std::wstring> const& linkIds,
            _In_ std::vector<std::wstring> const& keptOutIds) noexcept;

        void ClearTrace() noexcept;

        bool IsShowingTrace() const noexcept { return m_tracing; }

        // Nodes grow from here to fit their longest name.
        static constexpr double MinimumNodeWidth = 252.0;
        static constexpr double BlockNodeWidth = 208.0;
        static constexpr double CanvasMargin = 280.0;

        // Must match the scroll viewer's MinZoomFactor and MaxZoomFactor in MainWindow.xaml.
        static constexpr float MinimumZoom = 0.1f;
        static constexpr float MaximumZoom = 4.0f;

        // Category colors for the blocks, so the palette and the inspector match the canvas.
        static media::Brush CategoryBrush(_In_ BlockCategory category, _In_ double opacity = 1.0) noexcept;

        // "#RRGGBB" to a color and back, for annotation colors.
        static std::optional<winrt::Windows::UI::Color> ParseColorCode(_In_ std::wstring_view code) noexcept;
        static std::wstring ColorCode(_In_ winrt::Windows::UI::Color const& color);

        // Sets text in an annotation's font, size, style and color. Empty text shows the hint.
        static void ApplyAnnotationLook(
            _In_ controls::TextBlock const& text,
            _In_ AnnotationSettings const& settings) noexcept;

        // The picture the customer chose for an endpoint, decoded at this height. Null for none.
        static media::ImageSource LoadEndpointImage(_In_ std::wstring const& path, _In_ int32_t pixelHeight) noexcept;

    private:
        struct PortVisual
        {
            PortKey Key{};
            double OffsetX{ 0 };    // from the node origin, to the center of the dot
            double OffsetY{ 0 };
            shapes::Ellipse Dot{ nullptr };
            controls::Button Row{ nullptr };
            controls::TextBlock Label{ nullptr };
            media::Brush LabelBrush{ nullptr };

            // The hover wash on an endpoint's row. A step's point has none.
            shapes::Rectangle Highlight{ nullptr };
        };

        struct NodeVisual
        {
            std::wstring NodeId{};
            bool IsBlock{ false };
            bool IsAnnotation{ false };
            BlockCategory Category{ BlockCategory::Filter };

            // What the canvas positions. The card is drawn behind the node's content and its edge
            // over it, because the connection points hang half over the edge.
            controls::Grid Root{ nullptr };
            shapes::Rectangle Card{ nullptr };
            shapes::Rectangle Edge{ nullptr };

            controls::TextBlock NameText{ nullptr };
            controls::TextBlock SubtitleText{ nullptr };
            controls::TextBlock AnnotationText{ nullptr };
            shapes::Ellipse StatusDot{ nullptr };

            // A block that is bypassed is drawn with a dashed outline, which a Border cannot do.
            shapes::Rectangle DashedOutline{ nullptr };
            bool IsBypassed{ false };

            std::vector<PortVisual> Ports{};
            double Width{ MinimumNodeWidth };
            double Height{ 0 };
            bool IsOffline{ false };
        };

        struct ConnectionVisual
        {
            std::wstring ConnectionId{};
            shapes::Path Line{ nullptr };

            // Drawn under the line, wider and translucent, so a selected connection reads as
            // lifted rather than merely thicker.
            shapes::Path Glow{ nullptr };

            // Invisible and much thicker than the line, because a two pixel cord is unreasonable
            // to expect anyone to hit.
            shapes::Path HitArea{ nullptr };

            controls::Grid Pill{ nullptr };
            shapes::Rectangle PillShape{ nullptr };
            controls::TextBlock PillText{ nullptr };
            bool IsLoopMuted{ false };
            bool IsMuted{ false };

            // Points along the curve, for finding which connection something was dropped on.
            std::vector<foundation::Point> Samples{};
        };

        void BuildNode(
            _In_ PatchEndpoint const& endpoint,
            _In_ LiveEndpoint const* live,
            _In_ std::optional<LiveEndpoint> const& suggestion) noexcept;

        void BuildBlockNode(_In_ PatchBlock const& block) noexcept;

        void BuildAnnotationNode(_In_ PatchBlock const& block) noexcept;

        // Pressing, right-clicking and double-clicking work the same on every kind of node.
        void AttachNodeHandlers(_In_ xaml::UIElement const& root, _In_ std::wstring const& nodeId, _In_ bool isBlock) noexcept;

        // The same for every connection point: hover, press to drag, click for the keyboard.
        void AttachPortHandlers(_In_ controls::Button const& row, _In_ PortKey const& key) noexcept;

        void BuildConnections(_In_ PatchAnalysis const& analysis) noexcept;

        void MeasurePorts(_Inout_ NodeVisual& node) noexcept;

        void RedrawConnections() noexcept;

        void UpdateExtent() noexcept;

        void ApplyNodeAppearance(_In_ NodeVisual& node) noexcept;
        void ApplyConnectionAppearance(_In_ ConnectionVisual& visual) noexcept;

        NodeVisual* FindNode(_In_ std::wstring const& nodeId) noexcept;
        NodeVisual const* FindNode(_In_ std::wstring const& nodeId) const noexcept;

        // Where a node sits, endpoint or block alike.
        std::optional<foundation::Point> NodePosition(_In_ std::wstring const& nodeId) const noexcept;
        void SetNodePosition(_In_ std::wstring const& nodeId, _In_ double x, _In_ double y) noexcept;

        bool IsNodeSelected(_In_ std::wstring const& nodeId) const noexcept;

        std::optional<foundation::Point> PortPoint(_In_ PortKey const& key) noexcept;

        void OnNodePointerPressed(_In_ std::wstring const& nodeId, _In_ input::PointerRoutedEventArgs const& args) noexcept;
        void OnSurfacePointerMoved(_In_ input::PointerRoutedEventArgs const& args) noexcept;
        void OnSurfacePointerReleased(_In_ input::PointerRoutedEventArgs const& args) noexcept;

        // Saves where the dragged nodes ended up, the same as letting go of the button.
        void FinishNodeDrag() noexcept;

        void OnDragOver(_In_ xaml::DragEventArgs const& args) noexcept;
        void OnDrop(_In_ xaml::DragEventArgs const& args) noexcept;
        void SetDropTarget(_In_ std::wstring const& connectionId) noexcept;

        // Shared by the move and the release, so a drag that delivered no useful move events
        // still ends up where the pointer actually was.
        void ApplyDragPosition(_In_ foundation::Point const& position) noexcept;

        void BeginConnectionDrag(_In_ PortKey const& key) noexcept;

        // Picks up one end of an existing connection so it can be dropped somewhere else.
        void BeginRetargetDrag(_In_ std::wstring const& connectionId, _In_ bool movingSource) noexcept;

        void UpdateConnectionDrag(_In_ foundation::Point const& point) noexcept;
        void EndConnectionDrag() noexcept;

        // The drop target, by proximity. A captured pointer never raises PointerEntered on the
        // row being dragged onto, so hover cannot be used to find it.
        std::optional<PortKey> FindPortNear(_In_ foundation::Point const& point, _In_ bool wantOutput) noexcept;

        // Dragging is not reachable from the keyboard, so a connection can also be made by
        // choosing the Out point and then the In point.
        void OnPortClicked(_In_ PortKey const& key) noexcept;
        void ClearArmedPort() noexcept;
        void ApplyPortAppearance(_In_ PortVisual& port) noexcept;
        void RefreshPortAppearance() noexcept;
        void FocusCanvas() noexcept;

        // Abandons whatever drag is in progress without committing it.
        void CancelDrags() noexcept;
        void RequestConnection(_In_ PortKey const& source, _In_ PortKey const& destination) noexcept;
        void RequestRetarget(
            _In_ std::wstring const& connectionId,
            _In_ bool movingSource,
            _In_ PortKey const& port) noexcept;

        static media::Brush ThemeBrush(_In_ std::wstring_view key, _In_ winrt::Windows::UI::Color fallback) noexcept;

        // The part of the canvas in view, in overview coordinates.
        foundation::Rect MinimapViewportRect() const noexcept;

        void OnMinimapPointerPressed(_In_ input::PointerRoutedEventArgs const& args) noexcept;
        void OnMinimapPointerMoved(_In_ input::PointerRoutedEventArgs const& args) noexcept;
        void OnMinimapPointerReleased(_In_ input::PointerRoutedEventArgs const& args) noexcept;
        void PanToMinimapPoint(_In_ foundation::Point const& point) noexcept;

        controls::ScrollViewer m_scrollViewer{ nullptr };
        controls::Canvas m_surface{ nullptr };
        controls::Canvas m_connectionLayer{ nullptr };
        controls::Canvas m_nodeLayer{ nullptr };
        controls::Canvas m_overlayLayer{ nullptr };
        controls::Canvas m_minimap{ nullptr };

        shapes::Path m_dragLine{ nullptr };

        Callbacks m_callbacks{};

        PatchDocument const* m_patch{ nullptr };
        std::vector<LiveEndpoint> m_liveEndpoints{};

        std::vector<NodeVisual> m_nodes{};
        std::vector<ConnectionVisual> m_connections{};
        std::unordered_set<std::wstring> m_loopMutedConnectionIds{};

        CanvasSelectionKind m_selectionKind{ CanvasSelectionKind::None };
        std::wstring m_selectedNodeId{};
        std::wstring m_selectedConnectionId{};
        std::vector<std::wstring> m_selectedNodeIds{};

        // The connection a palette drag would land on, shown so the customer knows before
        // letting go.
        std::wstring m_dropTargetConnectionId{};

        // A trace being shown, by node and link id.
        bool m_tracing{ false };
        std::unordered_set<std::wstring> m_traceNodeIds{};
        std::unordered_set<std::wstring> m_traceLinkIds{};
        std::unordered_set<std::wstring> m_traceKeptOutIds{};

        // node drag: every selected node moves together
        bool m_draggingNode{ false };
        foundation::Point m_dragStartPointer{};
        std::vector<std::pair<std::wstring, foundation::Point>> m_dragStartPositions{};

        // connection drag
        bool m_draggingConnection{ false };
        bool m_dragMoved{ false };
        bool m_suppressNextPortClick{ false };
        PortKey m_dragSourcePort{};

        // Where the line being dragged is pinned, and which side the drop has to land on.
        foundation::Point m_dragAnchor{};
        bool m_dragWantsOutput{ false };

        bool m_retargeting{ false };
        std::wstring m_retargetConnectionId{};
        bool m_retargetMovingSource{ false };

        std::optional<PortKey> m_hoverPort{};
        std::optional<PortKey> m_armedPort{};

        // Which connection point the pointer is over, whether or not a drag is under way.
        std::optional<PortKey> m_hoverRowPort{};

        foundation::Size m_extent{ 0, 0 };

        // Scale the overview was last drawn at, so a drag on it maps back to the same place.
        double m_minimapScale{ 0 };
        bool m_panningMinimap{ false };
        foundation::Point m_minimapGrabOffset{};

        bool m_initialized{ false };
        bool m_shuttingDown{ false };
    };
}
