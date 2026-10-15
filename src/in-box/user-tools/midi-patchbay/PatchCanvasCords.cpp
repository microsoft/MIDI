// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// How the connections' lines are planned and drawn on the canvas. Where they run is worked out in
// CordPath.cpp, which has no XAML in it so the tests can check it.

#include "pch.h"
#include "PatchCanvas.h"
#include "StringResources.h"

namespace midipatchbay
{
    namespace
    {
        // How closely the lines are followed when finding what was dropped on one.
        constexpr int SamplesPerCurve = 24;

        CordPoint ToCord(_In_ foundation::Point const& point) noexcept
        {
            return CordPoint{ point.X, point.Y };
        }

        foundation::Point ToCanvas(_In_ CordPoint const& point) noexcept
        {
            return foundation::Point{ static_cast<float>(point.X), static_cast<float>(point.Y) };
        }

        size_t IndexOf(_In_ std::vector<std::wstring> const& nodeIds, _In_ std::wstring const& nodeId) noexcept
        {
            auto const found = std::find(nodeIds.begin(), nodeIds.end(), nodeId);

            return found == nodeIds.end() ? NoNode : static_cast<size_t>(found - nodeIds.begin());
        }
    }

    // Built again for each path that shows it: a geometry can be the Data of only one path.
    _Use_decl_annotations_
    media::PathGeometry PatchCanvas::MakeCordGeometry(CordPath const& path)
    {
        media::PathFigure figure{};
        figure.StartPoint(ToCanvas(path.Start));

        for (auto const& piece : path.Pieces)
        {
            if (piece.IsStraight)
            {
                media::LineSegment line{};
                line.Point(ToCanvas(piece.End));
                figure.Segments().Append(line);
            }
            else
            {
                media::BezierSegment curve{};
                curve.Point1(ToCanvas(piece.Control1));
                curve.Point2(ToCanvas(piece.Control2));
                curve.Point3(ToCanvas(piece.End));
                figure.Segments().Append(curve);
            }
        }

        media::PathGeometry geometry{};
        geometry.Figures().Append(figure);

        return geometry;
    }

    _Use_decl_annotations_
    std::vector<CordRect> PatchCanvas::NodeBounds(std::vector<std::wstring>& nodeIds) const
    {
        nodeIds.clear();

        std::vector<CordRect> bounds{};

        for (auto const& node : m_nodes)
        {
            // A line may cross a note. Going around them, a wide heading would send lines a long way.
            if (node.IsAnnotation)
            {
                continue;
            }

            auto const origin = NodePosition(node.NodeId);

            if (!origin.has_value())
            {
                continue;
            }

            nodeIds.push_back(node.NodeId);
            bounds.push_back(CordRect{
                origin->X,
                origin->Y,
                origin->X + node.Width,
                origin->Y + HeightOf(node) });
        }

        return bounds;
    }

    _Use_decl_annotations_
    winrt::hstring PatchCanvas::PillTextFor(PatchConnection const& connection) const
    {
        auto const sourceIsBlock = m_patch->IsBlock(connection.SourceId);
        auto const destinationIsBlock = m_patch->IsBlock(connection.DestinationId);

        if (!sourceIsBlock && !destinationIsBlock)
        {
            return connection.SourceGroupIndex == AllGroups &&
                connection.DestinationGroupIndex == AllGroups
                ? resources::GetString(L"ConnectionPillAllGroups")
                : resources::FormatString(L"ConnectionPillFormat",
                    connection.SourceGroupIndex == AllGroups
                        ? std::wstring{ resources::GetString(L"ConnectionPillAny") }
                        : std::to_wstring(connection.SourceGroupIndex + 1),
                    connection.DestinationGroupIndex == AllGroups
                        ? std::wstring{ resources::GetString(L"ConnectionPillSame") }
                        : std::to_wstring(connection.DestinationGroupIndex + 1));
        }

        // Only an endpoint end has a group, so only that end can say one.
        if (!sourceIsBlock && connection.SourceGroupIndex != AllGroups)
        {
            return resources::FormatString(L"ConnectionPillFromGroupFormat", connection.SourceGroupIndex + 1);
        }

        if (!destinationIsBlock && connection.DestinationGroupIndex != AllGroups)
        {
            return resources::FormatString(L"ConnectionPillToGroupFormat", connection.DestinationGroupIndex + 1);
        }

        return {};
    }

    _Use_decl_annotations_
    CordPath PatchCanvas::PlanDragLine(foundation::Point const& end, std::optional<PortKey> const& target) const
    {
        // When the line is looking for an Out, the pointer's end is the Out. Either way the line
        // leaves an Out to the right and comes into an In from the left.
        auto const anchor = ToCord(m_dragAnchor);
        auto const loose = ToCord(end);
        auto const from = m_dragWantsOutput ? loose : anchor;
        auto const to = m_dragWantsOutput ? anchor : loose;

        // Until it lands on a connection point, the pointer's end has no node to go around things to.
        if (!target.has_value())
        {
            return UsualCordPath(from, to);
        }

        std::vector<std::wstring> nodeIds{};
        auto const nodes = NodeBounds(nodeIds);

        auto const anchorNode = IndexOf(nodeIds, m_dragAnchorNodeId);
        auto const targetNode = IndexOf(nodeIds, target->NodeId);

        return PlanCordPath(from, to, nodes,
            m_dragWantsOutput ? targetNode : anchorNode,
            m_dragWantsOutput ? anchorNode : targetNode);
    }

    void PatchCanvas::RedrawConnections() noexcept
    {
        if (m_patch == nullptr)
        {
            return;
        }

        try
        {
            auto const hide = [](ConnectionVisual& visual)
                {
                    for (auto const& path : { visual.Line, visual.Glow, visual.HitArea, visual.Flash })
                    {
                        if (path != nullptr)
                        {
                            path.Visibility(xaml::Visibility::Collapsed);
                        }
                    }

                    if (visual.Pill != nullptr)
                    {
                        visual.Pill.Visibility(xaml::Visibility::Collapsed);
                    }
                };

            std::vector<std::wstring> nodeIds{};
            auto const nodes = NodeBounds(nodeIds);

            std::unordered_map<std::wstring, size_t> nodeIndexes{};

            for (size_t i = 0; i < nodeIds.size(); i++)
            {
                nodeIndexes.emplace(nodeIds[i], i);
            }

            auto const nodeIndexOf = [&nodeIndexes](std::wstring const& nodeId)
                {
                    auto const found = nodeIndexes.find(nodeId);
                    return found == nodeIndexes.end() ? NoNode : found->second;
                };

            // Every line is planned at once, so lines that find the same way around something can
            // be drawn apart rather than on top of each other.
            std::vector<CordRequest> requests{};
            std::vector<size_t> requestOf(m_connections.size(), NoNode);

            for (size_t i = 0; i < m_connections.size(); i++)
            {
                auto& visual = m_connections[i];
                auto const* connection = m_patch->FindConnection(visual.ConnectionId);

                if (connection == nullptr || visual.Line == nullptr)
                {
                    continue;
                }

                auto const from = PortPoint(PortKey{ connection->SourceId, true, connection->SourceGroupIndex });
                auto const to = PortPoint(PortKey{ connection->DestinationId, false, connection->DestinationGroupIndex });

                visual.Samples.clear();

                if (!from.has_value() || !to.has_value())
                {
                    hide(visual);
                    continue;
                }

                auto const pill = PillTextFor(*connection);

                if (visual.PillText != nullptr)
                {
                    visual.PillText.Text(pill);
                }

                requestOf[i] = requests.size();
                requests.push_back(CordRequest{
                    ToCord(from.value()),
                    ToCord(to.value()),
                    nodeIndexOf(connection->SourceId),
                    nodeIndexOf(connection->DestinationId),
                    !pill.empty() && visual.Pill != nullptr });
            }

            auto const paths = PlanCordPaths(requests, nodes);

            for (size_t i = 0; i < m_connections.size(); i++)
            {
                if (requestOf[i] == NoNode)
                {
                    continue;
                }

                auto& visual = m_connections[i];

                if (requestOf[i] >= paths.size() || paths[requestOf[i]].Pieces.empty())
                {
                    hide(visual);
                    continue;
                }

                auto const& path = paths[requestOf[i]];

                visual.Line.Visibility(xaml::Visibility::Visible);
                visual.Line.Data(MakeCordGeometry(path));

                if (visual.HitArea != nullptr)
                {
                    visual.HitArea.Visibility(xaml::Visibility::Visible);
                    visual.HitArea.Data(MakeCordGeometry(path));
                }

                if (visual.Flash != nullptr)
                {
                    visual.Flash.Visibility(xaml::Visibility::Visible);
                    visual.Flash.Data(MakeCordGeometry(path));
                }

                if (visual.Glow != nullptr)
                {
                    visual.Glow.Data(MakeCordGeometry(path));
                    ApplyConnectionAppearance(visual);
                }

                for (auto const& sample : SampleCordPath(path, SamplesPerCurve))
                {
                    visual.Samples.push_back(ToCanvas(sample));
                }

                if (visual.Pill == nullptr)
                {
                    continue;
                }

                auto const showPill = visual.PillText != nullptr && !visual.PillText.Text().empty();

                visual.Pill.Visibility(showPill ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

                if (showPill)
                {
                    visual.Pill.Measure(foundation::Size{ 400, 40 });

                    auto const size = visual.Pill.DesiredSize();

                    controls::Canvas::SetLeft(visual.Pill, path.LabelPoint.X - size.Width / 2);
                    controls::Canvas::SetTop(visual.Pill, path.LabelPoint.Y - size.Height / 2);
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to draw the connections.")

        RedrawAnswerPaths();
    }
}
