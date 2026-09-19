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
    // Identifies one connection point: an endpoint node, a side, and a group (or all groups).
    struct PortKey
    {
        std::wstring EndpointId{};
        bool IsOutput{ false };
        int32_t GroupIndex{ AllGroups };

        bool operator==(PortKey const& other) const noexcept
        {
            return EndpointId == other.EndpointId &&
                IsOutput == other.IsOutput &&
                GroupIndex == other.GroupIndex;
        }

        std::wstring ToString() const noexcept
        {
            return EndpointId + (IsOutput ? L"|out|" : L"|in|") + std::to_wstring(GroupIndex);
        }
    };

    enum class CanvasSelectionKind
    {
        None = 0,
        Endpoint = 1,
        Connection = 2,
    };

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

            std::function<void(std::wstring)> EndpointContextMenuRequested{};

            std::function<void()> ViewportChanged{};
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
        void RefreshStatus(
            _In_ std::unordered_map<std::wstring, RouteStats> const& stats) noexcept;

        CanvasSelectionKind SelectionKind() const noexcept { return m_selectionKind; }
        std::wstring const& SelectedEndpointId() const noexcept { return m_selectedEndpointId; }
        std::wstring const& SelectedConnectionId() const noexcept { return m_selectedConnectionId; }

        void Select(_In_ CanvasSelectionKind kind, _In_ std::wstring const& id) noexcept;
        void ClearSelection() noexcept;

        // Moves the viewport so every node is in view, at the largest zoom that fits.
        void FitToContent() noexcept;

        // Lays the nodes out in two columns, sources on the left and everything they feed on
        // the right, which is the shape almost every patch ends up in by hand anyway.
        void AutoArrange() noexcept;

        void UpdateMinimap() noexcept;

        // Extent of the content, which is what makes the canvas bigger than the window.
        foundation::Size ContentExtent() const noexcept { return m_extent; }

        static constexpr double NodeWidth = 252.0;
        static constexpr double CanvasMargin = 280.0;

    private:
        struct PortVisual
        {
            PortKey Key{};
            double OffsetX{ 0 };    // from the node origin, to the center of the dot
            double OffsetY{ 0 };
            shapes::Ellipse Dot{ nullptr };
            controls::Button Row{ nullptr };
        };

        struct NodeVisual
        {
            std::wstring EndpointId{};
            controls::Border Root{ nullptr };
            controls::TextBlock NameText{ nullptr };
            controls::TextBlock SubtitleText{ nullptr };
            shapes::Ellipse StatusDot{ nullptr };
            controls::Border AlertPanel{ nullptr };
            std::vector<PortVisual> Ports{};
            double Width{ NodeWidth };
            double Height{ 0 };
            bool IsOffline{ false };
        };

        struct ConnectionVisual
        {
            std::wstring ConnectionId{};
            shapes::Path Line{ nullptr };
            controls::Border Pill{ nullptr };
            controls::TextBlock PillText{ nullptr };
            bool IsLoopMuted{ false };
            bool IsMuted{ false };
        };

        void BuildNode(
            _In_ PatchEndpoint const& endpoint,
            _In_ LiveEndpoint const* live,
            _In_ std::optional<LiveEndpoint> const& suggestion) noexcept;

        void BuildConnections(_In_ PatchAnalysis const& analysis) noexcept;

        void MeasurePorts(_Inout_ NodeVisual& node) noexcept;

        void RedrawConnections() noexcept;

        void UpdateExtent() noexcept;

        void ApplyNodeAppearance(_In_ NodeVisual& node) noexcept;
        void ApplyConnectionAppearance(_In_ ConnectionVisual& visual) noexcept;

        NodeVisual* FindNode(_In_ std::wstring const& endpointId) noexcept;

        std::optional<foundation::Point> PortPoint(_In_ PortKey const& key) noexcept;

        void OnNodePointerPressed(_In_ std::wstring const& endpointId, _In_ input::PointerRoutedEventArgs const& args) noexcept;
        void OnSurfacePointerMoved(_In_ input::PointerRoutedEventArgs const& args) noexcept;
        void OnSurfacePointerReleased(_In_ input::PointerRoutedEventArgs const& args) noexcept;

        void BeginConnectionDrag(_In_ PortKey const& key) noexcept;
        void UpdateConnectionDrag(_In_ foundation::Point const& point) noexcept;
        void EndConnectionDrag() noexcept;

        // Dragging is not reachable from the keyboard, so a connection can also be made by
        // choosing the Out point and then the In point.
        void OnPortClicked(_In_ PortKey const& key) noexcept;
        void ClearArmedPort() noexcept;
        void ApplyPortAppearance(_In_ PortVisual& port) noexcept;
        void RequestConnection(_In_ PortKey const& source, _In_ PortKey const& destination) noexcept;

        static media::Brush ThemeBrush(_In_ std::wstring_view key, _In_ winrt::Windows::UI::Color fallback) noexcept;

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
        std::wstring m_selectedEndpointId{};
        std::wstring m_selectedConnectionId{};

        // node drag
        bool m_draggingNode{ false };
        std::wstring m_dragNodeId{};
        foundation::Point m_dragStartPointer{};
        double m_dragStartX{ 0 };
        double m_dragStartY{ 0 };

        // connection drag
        bool m_draggingConnection{ false };
        bool m_dragMoved{ false };
        bool m_suppressNextPortClick{ false };
        PortKey m_dragSourcePort{};
        std::optional<PortKey> m_hoverPort{};
        std::optional<PortKey> m_armedPort{};

        foundation::Size m_extent{ 0, 0 };

        bool m_initialized{ false };
        bool m_shuttingDown{ false };
    };
}
