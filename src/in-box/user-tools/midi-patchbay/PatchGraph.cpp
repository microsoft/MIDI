// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "PatchGraph.h"

namespace midipatchbay
{
    namespace
    {
        // A vertex is "messages present at this endpoint, on this group, on this side".
        // Keeping the two sides apart is what stops a plain two-way link between a keyboard and
        // a synth from being reported as a loop: material sent to a device only comes back if
        // that device echoes it, and that is modeled as an explicit edge below.
        constexpr int32_t SideOut = 0;
        constexpr int32_t SideIn = 1;

        struct Edge
        {
            int32_t To{ 0 };

            // Empty for an echo edge inside a device.
            std::wstring ConnectionId{};

            // The endpoint reached after taking this edge.
            size_t EndpointIndex{ 0 };

            bool IsAssumedEcho{ false };
        };

        struct Builder
        {
            std::vector<std::wstring> EndpointIds{};
            std::vector<std::wstring> EndpointNames{};
            std::unordered_map<std::wstring, size_t> IndexById{};
            std::vector<std::vector<Edge>> Adjacency{};

            int32_t Vertex(_In_ size_t endpointIndex, _In_ int32_t groupIndex, _In_ int32_t side) const noexcept
            {
                return static_cast<int32_t>(endpointIndex) * (MaximumGroupCount * 2) + groupIndex * 2 + side;
            }

            size_t VertexCount() const noexcept
            {
                return EndpointIds.size() * MaximumGroupCount * 2;
            }
        };

        LiveEndpoint const* FindLive(
            _In_ std::vector<LiveEndpoint> const& liveEndpoints,
            _In_ std::wstring const& endpointDeviceId) noexcept
        {
            if (endpointDeviceId.empty())
            {
                return nullptr;
            }

            auto it = std::find_if(liveEndpoints.begin(), liveEndpoints.end(),
                [&endpointDeviceId](LiveEndpoint const& e)
                {
                    return ::CompareStringOrdinal(
                        e.EndpointDeviceId.c_str(), -1, endpointDeviceId.c_str(), -1, TRUE) == CSTR_EQUAL;
                });

            return it == liveEndpoints.end() ? nullptr : &(*it);
        }

        // Expands a connection into the concrete group pairs it carries.
        void ForEachGroupPair(
            _In_ PatchConnection const& connection,
            _In_ std::function<void(int32_t, int32_t)> const& callback)
        {
            if (connection.SourceGroupIndex == AllGroups)
            {
                for (int32_t group = 0; group < MaximumGroupCount; group++)
                {
                    // "all groups" into a specific group folds everything onto that group;
                    // into "all groups" it passes the group through untouched
                    callback(group, connection.DestinationGroupIndex == AllGroups
                        ? group
                        : connection.DestinationGroupIndex);
                }

                return;
            }

            callback(connection.SourceGroupIndex, connection.DestinationGroupIndex == AllGroups
                ? connection.SourceGroupIndex
                : connection.DestinationGroupIndex);
        }

        Builder BuildGraph(
            _In_ PatchDocument const& patch,
            _In_ std::vector<LiveEndpoint> const& liveEndpoints,
            _In_ bool includeAssumedEcho)
        {
            Builder builder{};

            for (auto const& endpoint : patch.Endpoints)
            {
                builder.IndexById[endpoint.Id] = builder.EndpointIds.size();
                builder.EndpointIds.push_back(endpoint.Id);
                builder.EndpointNames.push_back(endpoint.DisplayName);
            }

            builder.Adjacency.resize(builder.VertexCount());

            for (auto const& connection : patch.Connections)
            {
                auto const sourceIt = builder.IndexById.find(connection.SourceEndpointId);
                auto const destinationIt = builder.IndexById.find(connection.DestinationEndpointId);

                if (sourceIt == builder.IndexById.end() || destinationIt == builder.IndexById.end())
                {
                    continue;
                }

                // a muted connection carries nothing, so it cannot close a circle
                if (connection.Muted)
                {
                    continue;
                }

                ForEachGroupPair(connection, [&](int32_t sourceGroup, int32_t destinationGroup)
                    {
                        Edge edge{};
                        edge.To = builder.Vertex(destinationIt->second, destinationGroup, SideIn);
                        edge.ConnectionId = connection.Id;
                        edge.EndpointIndex = destinationIt->second;

                        builder.Adjacency[static_cast<size_t>(
                            builder.Vertex(sourceIt->second, sourceGroup, SideOut))].push_back(std::move(edge));
                    });
            }

            // The echo edges: what turns "sent to a device" back into "available from a device".
            for (size_t index = 0; index < patch.Endpoints.size(); index++)
            {
                auto const& endpoint = patch.Endpoints[index];
                auto const* live = FindLive(liveEndpoints, endpoint.Match.EndpointDeviceId);

                auto const isLoopback = live != nullptr && live->IsLoopback;

                if (!isLoopback && !includeAssumedEcho)
                {
                    continue;
                }

                size_t echoTargetIndex = index;

                // A MIDI 2.0 loopback is a pair: what goes into one side comes out of the other.
                if (isLoopback && live != nullptr && !live->LoopbackPartnerEndpointId.empty())
                {
                    auto partner = std::find_if(patch.Endpoints.begin(), patch.Endpoints.end(),
                        [live](PatchEndpoint const& e)
                        {
                            return ::CompareStringOrdinal(
                                e.Match.EndpointDeviceId.c_str(), -1,
                                live->LoopbackPartnerEndpointId.c_str(), -1, TRUE) == CSTR_EQUAL;
                        });

                    if (partner == patch.Endpoints.end())
                    {
                        // the other side of the pair is not on this canvas, so nothing routed
                        // here can come back through it
                        continue;
                    }

                    echoTargetIndex = static_cast<size_t>(std::distance(patch.Endpoints.begin(), partner));
                }

                for (int32_t group = 0; group < MaximumGroupCount; group++)
                {
                    Edge edge{};
                    edge.To = builder.Vertex(echoTargetIndex, group, SideOut);
                    edge.EndpointIndex = echoTargetIndex;
                    edge.IsAssumedEcho = !isLoopback;

                    builder.Adjacency[static_cast<size_t>(
                        builder.Vertex(index, group, SideIn))].push_back(std::move(edge));
                }
            }

            return builder;
        }

        // Iterative depth first search with an explicit stack: a hand-edited patch could nest
        // deeply enough to matter, and recursion here would be on the UI thread.
        bool FindCycle(_In_ Builder const& graph, _Out_ std::vector<Edge const*>& cycle)
        {
            cycle.clear();

            enum class Color { White, Gray, Black };

            std::vector<Color> color(graph.Adjacency.size(), Color::White);
            std::vector<size_t> nextEdge(graph.Adjacency.size(), 0);
            std::vector<int32_t> vertexStack{};
            std::vector<Edge const*> edgeStack{};

            for (size_t start = 0; start < graph.Adjacency.size(); start++)
            {
                if (color[start] != Color::White)
                {
                    continue;
                }

                vertexStack.clear();
                edgeStack.clear();

                vertexStack.push_back(static_cast<int32_t>(start));
                color[start] = Color::Gray;

                while (!vertexStack.empty())
                {
                    auto const current = static_cast<size_t>(vertexStack.back());

                    if (nextEdge[current] < graph.Adjacency[current].size())
                    {
                        auto const& edge = graph.Adjacency[current][nextEdge[current]];
                        nextEdge[current]++;

                        auto const target = static_cast<size_t>(edge.To);

                        if (color[target] == Color::Gray)
                        {
                            // found the back edge; unwind to where the circle started
                            edgeStack.push_back(&edge);

                            size_t first = 0;

                            for (size_t i = 0; i < vertexStack.size(); i++)
                            {
                                if (static_cast<size_t>(vertexStack[i]) == target)
                                {
                                    first = i;
                                    break;
                                }
                            }

                            for (size_t i = first; i < edgeStack.size(); i++)
                            {
                                cycle.push_back(edgeStack[i]);
                            }

                            return true;
                        }

                        if (color[target] == Color::White)
                        {
                            color[target] = Color::Gray;
                            vertexStack.push_back(edge.To);
                            edgeStack.push_back(&edge);
                        }
                    }
                    else
                    {
                        color[current] = Color::Black;
                        vertexStack.pop_back();

                        if (!edgeStack.empty())
                        {
                            edgeStack.pop_back();
                        }
                    }
                }
            }

            return false;
        }

        LoopFinding DescribeCycle(
            _In_ Builder const& graph,
            _In_ std::vector<Edge const*> const& cycle,
            _In_ LoopSeverity severity)
        {
            LoopFinding finding{};

            finding.Severity = severity;

            for (auto const* edge : cycle)
            {
                if (edge == nullptr)
                {
                    continue;
                }

                if (edge->EndpointIndex < graph.EndpointIds.size())
                {
                    auto const& id = graph.EndpointIds[edge->EndpointIndex];
                    auto const& name = graph.EndpointNames[edge->EndpointIndex];

                    if (finding.EndpointIds.empty() || finding.EndpointIds.back() != id)
                    {
                        finding.EndpointIds.push_back(id);
                        finding.EndpointNames.push_back(name);
                    }

                    if (edge->IsAssumedEcho &&
                        std::find(finding.AssumedEchoNames.begin(), finding.AssumedEchoNames.end(), name) ==
                            finding.AssumedEchoNames.end())
                    {
                        finding.AssumedEchoNames.push_back(name);
                    }
                }

                if (!edge->ConnectionId.empty() &&
                    std::find(finding.ConnectionIds.begin(), finding.ConnectionIds.end(), edge->ConnectionId) ==
                        finding.ConnectionIds.end())
                {
                    finding.ConnectionIds.push_back(edge->ConnectionId);
                }
            }

            return finding;
        }
    }

    _Use_decl_annotations_
    PatchAnalysis AnalyzePatch(
        PatchDocument const& patch,
        std::vector<LiveEndpoint> const& liveEndpoints) noexcept
    {
        PatchAnalysis analysis{};

        try
        {
            // Pass one: only what is certain. A patch copy is used so muting a connection that
            // closes a circle can expose a second, independent circle behind it.
            PatchDocument working{ patch };

            for (int attempt = 0; attempt < 16; attempt++)
            {
                auto const graph = BuildGraph(working, liveEndpoints, false);

                std::vector<Edge const*> cycle{};

                if (!FindCycle(graph, cycle) || cycle.empty())
                {
                    break;
                }

                auto finding = DescribeCycle(graph, cycle, LoopSeverity::Certain);

                if (finding.ConnectionIds.empty())
                {
                    break;
                }

                // Muting the last connection on the circle is what the customer just drew in the
                // common case, and it is the one whose removal restores the previous good state.
                auto const& victim = finding.ConnectionIds.back();

                analysis.LoopMutedConnectionIds.insert(victim);
                analysis.Loops.push_back(std::move(finding));

                if (auto* connection = working.FindConnection(victim))
                {
                    connection->Muted = true;
                }
                else
                {
                    break;
                }
            }

            // Pass two: what would happen if the hardware on the circle echoes. Anything already
            // reported as certain is muted in the working copy, so it cannot be reported twice.
            {
                auto const graph = BuildGraph(working, liveEndpoints, true);

                std::vector<Edge const*> cycle{};

                if (FindCycle(graph, cycle) && !cycle.empty())
                {
                    auto finding = DescribeCycle(graph, cycle, LoopSeverity::Possible);

                    if (!finding.ConnectionIds.empty() && !finding.AssumedEchoNames.empty())
                    {
                        analysis.Loops.push_back(std::move(finding));
                    }
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to analyze the patch for loops.")

        return analysis;
    }

    _Use_decl_annotations_
    bool WouldCreateCertainLoop(
        PatchDocument const& patch,
        PatchConnection const& proposed,
        std::vector<LiveEndpoint> const& liveEndpoints) noexcept
    {
        try
        {
            PatchDocument working{ patch };
            working.Connections.push_back(proposed);

            auto const graph = BuildGraph(working, liveEndpoints, false);

            std::vector<Edge const*> cycle{};

            return FindCycle(graph, cycle);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to test a connection for loops.")

        return false;
    }
}
