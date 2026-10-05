// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Pure: no precompiled header, so the unit tests compile this file exactly as it ships.

#include <windows.h>

#include "RouteGraph.h"

#include <algorithm>
#include <unordered_set>

// Uses PVOID and UNREFERENCED_PARAMETER, so it comes after windows.h.
#include <ump_helpers.h>

namespace internal = ::WindowsMidiServicesInternal;

namespace midipatchbay
{
    namespace
    {
        constexpr uint8_t MaximumWordsPerMessage = 4;

        enum class Expansion
        {
            // Reaches at least one destination that is here now.
            Live,

            // Reaches nothing that is here now, so it is left out.
            Dead,

            // The patch cannot route at all.
            Refused,
        };

        std::wstring LowerCopy(_In_ std::wstring value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });

            return value;
        }

        // What an LFO that follows a clock listens to: timing clock, start and song position.
        bool IsClockMessage(_In_ uint32_t word) noexcept
        {
            if ((word >> 28) != static_cast<uint32_t>(UmpMessageType::System))
            {
                return false;
            }

            auto const status = (word >> 16) & 0xFF;

            return status == 0xF8 || status == 0xFA || status == 0xF2;
        }

        class PatchCompiler
        {
        public:
            PatchCompiler(_Inout_ RouteGraph& graph, _In_ RoutePatch const& input) :
                m_graph(graph),
                m_input(input),
                m_patch(*input.Patch)
            {
            }

            bool Run(_Out_ RouteProblemKind& problem)
            {
                problem = RouteProblemKind::LoopBetweenBlocks;

                // A muted link carries nothing, so it is as if it were not drawn.
                for (auto const& link : m_patch.Connections)
                {
                    if (!link.Muted)
                    {
                        m_outgoing[link.SourceId].push_back(&link);
                    }
                }

                for (auto const& endpoint : m_patch.Endpoints)
                {
                    auto const* device = DeviceIdOf(endpoint.Id);

                    if (device == nullptr)
                    {
                        continue;
                    }

                    auto const links = m_outgoing.find(endpoint.Id);

                    if (links == m_outgoing.end())
                    {
                        continue;
                    }

                    for (auto const* link : links->second)
                    {
                        std::vector<std::wstring> path{};
                        RouteEdge edge{};

                        auto const result = ExpandLink(*link, path, 0, edge);

                        if (result == Expansion::Refused)
                        {
                            problem = m_problem;
                            return false;
                        }

                        if (result == Expansion::Dead)
                        {
                            continue;
                        }

                        RouteRoot root{};
                        root.SourceDeviceId = LowerCopy(*device);
                        root.WaitForSendComplete = m_patch.WaitForSendComplete;
                        root.SourceGroupIndex = link->SourceGroupIndex;
                        root.Edge = edge;

                        m_graph.Roots.push_back(std::move(root));
                    }
                }

                // A generator is a source of its own. Bypassed, or leading nowhere that is here
                // now, it does not run at all.
                for (auto const& block : m_patch.Blocks)
                {
                    if (!IsGenerator(block.Kind) || block.Bypassed)
                    {
                        continue;
                    }

                    std::vector<std::wstring> path{ block.Id };
                    std::vector<RouteEdge> edges{};

                    if (ExpandOutgoing(block.Id, path, 1, edges) == Expansion::Refused)
                    {
                        problem = m_problem;
                        return false;
                    }

                    if (edges.empty())
                    {
                        continue;
                    }

                    RouteGenerator generator{};
                    generator.Key = m_input.Key + L'|' + block.Id;
                    generator.Kind = block.Kind;
                    generator.Settings = SettingsFor(block);
                    generator.Cell = CellFor(block.Id);
                    generator.FollowsClock = HasInput(block.Kind) &&
                        std::any_of(m_patch.Connections.begin(), m_patch.Connections.end(),
                            [&block](PatchConnection const& link) { return link.DestinationId == block.Id; });
                    generator.FirstEdge = static_cast<uint32_t>(m_graph.Edges.size());
                    generator.EdgeCount = static_cast<uint32_t>(edges.size());

                    m_graph.Edges.insert(m_graph.Edges.end(), edges.begin(), edges.end());
                    m_graph.Generators.push_back(std::move(generator));
                }

                return true;
            }

        private:
            std::wstring const* DeviceIdOf(_In_ std::wstring const& endpointId) const
            {
                auto const found = m_input.DeviceIds.find(endpointId);

                if (found == m_input.DeviceIds.end() || found->second.empty())
                {
                    return nullptr;
                }

                return &found->second;
            }

            uint32_t CellFor(_In_ std::wstring const& elementId)
            {
                auto const found = m_cells.find(elementId);

                if (found != m_cells.end())
                {
                    return found->second;
                }

                auto const cell = static_cast<uint32_t>(m_graph.Cells.size());

                m_graph.Cells.push_back(m_input.Key + L'|' + elementId);
                m_cells.emplace(elementId, cell);

                return cell;
            }

            uint32_t SettingsFor(_In_ PatchBlock const& block)
            {
                auto const found = m_settings.find(block.Id);

                if (found != m_settings.end())
                {
                    return found->second;
                }

                auto const index = static_cast<uint32_t>(m_graph.Settings.size());

                m_graph.Settings.push_back(block.Settings);
                m_settings.emplace(block.Id, index);

                return index;
            }

            uint32_t StateFor(_In_ PatchBlock const& block)
            {
                auto const found = m_states.find(block.Id);

                if (found != m_states.end())
                {
                    return found->second;
                }

                auto const index = static_cast<uint32_t>(m_graph.States.size());

                m_graph.States.push_back(CellFor(block.Id));
                m_states.emplace(block.Id, index);

                return index;
            }

            uint32_t ClockTargetFor(_In_ PatchBlock const& block)
            {
                auto const found = m_clockTargets.find(block.Id);

                if (found != m_clockTargets.end())
                {
                    return found->second;
                }

                auto const index = static_cast<uint32_t>(m_graph.ClockTargets.size());

                m_graph.ClockTargets.push_back(m_input.Key + L'|' + block.Id);
                m_clockTargets.emplace(block.Id, index);

                return index;
            }

            Expansion ExpandOutgoing(
                _In_ std::wstring const& nodeId,
                _Inout_ std::vector<std::wstring>& path,
                _In_ size_t depth,
                _Inout_ std::vector<RouteEdge>& edges)
            {
                auto const links = m_outgoing.find(nodeId);

                if (links == m_outgoing.end())
                {
                    return Expansion::Live;
                }

                for (auto const* link : links->second)
                {
                    RouteEdge edge{};

                    auto const result = ExpandLink(*link, path, depth, edge);

                    if (result == Expansion::Refused)
                    {
                        return Expansion::Refused;
                    }

                    if (result == Expansion::Live)
                    {
                        edges.push_back(edge);
                    }
                }

                return Expansion::Live;
            }

            uint32_t AddStage(_In_ RouteStage stage, _In_ std::vector<RouteEdge> const& edges)
            {
                stage.FirstEdge = static_cast<uint32_t>(m_graph.Edges.size());
                stage.EdgeCount = static_cast<uint32_t>(edges.size());

                m_graph.Edges.insert(m_graph.Edges.end(), edges.begin(), edges.end());
                m_graph.Stages.push_back(stage);

                return static_cast<uint32_t>(m_graph.Stages.size() - 1);
            }

            Expansion ExpandLink(
                _In_ PatchConnection const& link,
                _Inout_ std::vector<std::wstring>& path,
                _In_ size_t depth,
                _Out_ RouteEdge& edge)
            {
                edge = RouteEdge{};

                if (depth >= MaximumRouteDepth || m_graph.Stages.size() >= MaximumRouteStages)
                {
                    m_problem = RouteProblemKind::TooComplex;
                    return Expansion::Refused;
                }

                auto const* block = m_patch.FindBlock(link.DestinationId);

                if (block == nullptr)
                {
                    return ExpandDestination(link, edge);
                }

                // Nothing goes into MIDI clock or MIDI Time Code, and only timing into an LFO. A
                // link into one that can't take it came from a file, and carries nothing. A clock
                // input goes no further, so it is never part of a circle.
                if (IsGenerator(block->Kind))
                {
                    if (!HasInput(block->Kind) || block->Bypassed)
                    {
                        return Expansion::Dead;
                    }

                    RouteStage stage{};
                    stage.Kind = RouteStageKind::ClockInput;
                    stage.Block = block->Kind;
                    stage.Index = ClockTargetFor(*block);
                    stage.Cell = CellFor(block->Id);

                    edge.Stage = AddStage(stage, {});
                    edge.LinkCell = CellFor(link.Id);

                    return Expansion::Live;
                }

                if (std::find(path.begin(), path.end(), block->Id) != path.end())
                {
                    m_problem = RouteProblemKind::LoopBetweenBlocks;
                    return Expansion::Refused;
                }

                auto const paced = block->Kind == BlockKind::Throttle &&
                    !block->Bypassed &&
                    block->Settings.SendSpeedLimit != 0;

                if (paced)
                {
                    uint32_t stage{ 0 };

                    auto const result = ExpandThrottle(*block, path, depth, stage);

                    if (result != Expansion::Live)
                    {
                        return result;
                    }

                    edge.Stage = stage;
                    edge.LinkCell = CellFor(link.Id);
                    return Expansion::Live;
                }

                path.push_back(block->Id);

                std::vector<RouteEdge> next{};
                auto const result = ExpandOutgoing(block->Id, path, depth + 1, next);

                path.pop_back();

                if (result == Expansion::Refused)
                {
                    return Expansion::Refused;
                }

                if (next.empty())
                {
                    return Expansion::Dead;
                }

                // A block that changes nothing only needs counting.
                auto const runs = !block->Bypassed &&
                    block->Kind != BlockKind::Throttle &&
                    !BlockChangesNothing(block->Kind, block->Settings);

                RouteStage stage{};
                stage.Kind = runs ? RouteStageKind::Block : RouteStageKind::PassThrough;
                stage.Block = block->Kind;
                stage.Index = runs ? SettingsFor(*block) : 0;
                stage.Cell = CellFor(block->Id);
                stage.State = runs && block->Kind == BlockKind::ClockDivider ? StateFor(*block) : 0;

                edge.Stage = AddStage(stage, next);
                edge.LinkCell = CellFor(link.Id);

                return Expansion::Live;
            }

            Expansion ExpandDestination(_In_ PatchConnection const& link, _Out_ RouteEdge& edge)
            {
                edge = RouteEdge{};

                auto const* device = DeviceIdOf(link.DestinationId);

                if (device == nullptr)
                {
                    return Expansion::Dead;
                }

                // One leaf for each link into a destination, however many paths reach it.
                auto const found = m_leafStages.find(link.Id);
                uint32_t stage{ 0 };

                if (found != m_leafStages.end())
                {
                    stage = found->second;
                }
                else
                {
                    RouteLeaf leaf{};
                    leaf.DestinationDeviceId = LowerCopy(*device);
                    leaf.WaitForSendComplete = m_patch.WaitForSendComplete;
                    leaf.DestinationGroupIndex = link.DestinationGroupIndex;
                    leaf.LinkCell = CellFor(link.Id);

                    m_graph.Leaves.push_back(std::move(leaf));

                    RouteStage leafStage{};
                    leafStage.Kind = RouteStageKind::Leaf;
                    leafStage.Index = static_cast<uint32_t>(m_graph.Leaves.size() - 1);

                    stage = AddStage(leafStage, {});
                    m_leafStages.emplace(link.Id, stage);
                }

                edge.Stage = stage;
                edge.LinkCell = CellFor(link.Id);

                return Expansion::Live;
            }

            // A throttle is one queue however messages reach it, so what comes after it is
            // worked out once, the first time it is reached. A loop through it is found then,
            // because the path at that point leads into it.
            Expansion ExpandThrottle(
                _In_ PatchBlock const& block,
                _Inout_ std::vector<std::wstring>& path,
                _In_ size_t depth,
                _Out_ uint32_t& stage)
            {
                stage = 0;

                auto const done = m_throttleStages.find(block.Id);

                if (done != m_throttleStages.end())
                {
                    stage = done->second;
                    return Expansion::Live;
                }

                if (m_deadThrottles.count(block.Id) != 0)
                {
                    return Expansion::Dead;
                }

                path.push_back(block.Id);

                std::vector<RouteEdge> next{};
                auto const result = ExpandOutgoing(block.Id, path, depth + 1, next);

                path.pop_back();

                if (result == Expansion::Refused)
                {
                    return Expansion::Refused;
                }

                if (next.empty())
                {
                    m_deadThrottles.insert(block.Id);
                    return Expansion::Dead;
                }

                RouteThrottle throttle{};
                throttle.Speed = block.Settings.SendSpeedLimit;
                throttle.Cell = CellFor(block.Id);
                throttle.FirstEdge = static_cast<uint32_t>(m_graph.Edges.size());
                throttle.EdgeCount = static_cast<uint32_t>(next.size());

                m_graph.Edges.insert(m_graph.Edges.end(), next.begin(), next.end());
                m_graph.Throttles.push_back(throttle);

                RouteStage throttleStage{};
                throttleStage.Kind = RouteStageKind::Throttle;
                throttleStage.Block = BlockKind::Throttle;
                throttleStage.Index = static_cast<uint32_t>(m_graph.Throttles.size() - 1);
                throttleStage.Cell = throttle.Cell;

                stage = AddStage(throttleStage, {});
                m_throttleStages.emplace(block.Id, stage);

                return Expansion::Live;
            }

            RouteGraph& m_graph;
            RoutePatch const& m_input;
            PatchDocument const& m_patch;

            std::unordered_map<std::wstring, std::vector<PatchConnection const*>> m_outgoing{};
            std::unordered_map<std::wstring, uint32_t> m_cells{};
            std::unordered_map<std::wstring, uint32_t> m_settings{};
            std::unordered_map<std::wstring, uint32_t> m_states{};
            std::unordered_map<std::wstring, uint32_t> m_clockTargets{};
            std::unordered_map<std::wstring, uint32_t> m_leafStages{};
            std::unordered_map<std::wstring, uint32_t> m_throttleStages{};
            std::unordered_set<std::wstring> m_deadThrottles{};

            RouteProblemKind m_problem{ RouteProblemKind::LoopBetweenBlocks };
        };

        // Everything that decides how a patch routes, so an unchanged plan is left running.
        std::wstring PatchSignature(_In_ RoutePatch const& input)
        {
            auto const& patch = *input.Patch;

            std::vector<std::wstring> parts{};

            for (auto const& endpoint : patch.Endpoints)
            {
                auto const found = input.DeviceIds.find(endpoint.Id);

                parts.push_back(L"e " + endpoint.Id + L'=' +
                    (found == input.DeviceIds.end() ? std::wstring{} : LowerCopy(found->second)));
            }

            for (auto const& block : patch.Blocks)
            {
                parts.push_back(L"b " + block.Id + (block.Bypassed ? L" off " : L" on ") +
                    BlockSettingsSignature(block.Kind, block.Settings));
            }

            for (auto const& link : patch.Connections)
            {
                if (link.Muted)
                {
                    continue;
                }

                parts.push_back(L"l " + link.Id + L' ' +
                    link.SourceId + L'/' + std::to_wstring(link.SourceGroupIndex) + L'>' +
                    link.DestinationId + L'/' + std::to_wstring(link.DestinationGroupIndex));
            }

            std::sort(parts.begin(), parts.end());

            std::wstring signature{ input.Key };
            signature += patch.WaitForSendComplete ? L"|wait\n" : L"|now\n";

            for (auto const& part : parts)
            {
                signature += part;
                signature += L'\n';
            }

            return signature;
        }
    }

    _Use_decl_annotations_
    RouteGraph CompileRoutes(std::vector<RoutePatch> const& patches) noexcept
    {
        RouteGraph graph{};

        try
        {
            std::vector<std::wstring> signatures{};

            for (auto const& input : patches)
            {
                if (input.Patch == nullptr)
                {
                    continue;
                }

                // A patch either routes whole or not at all, so what it added is taken back when
                // it cannot.
                auto const settings = graph.Settings.size();
                auto const cells = graph.Cells.size();
                auto const stages = graph.Stages.size();
                auto const edges = graph.Edges.size();
                auto const roots = graph.Roots.size();
                auto const leaves = graph.Leaves.size();
                auto const throttles = graph.Throttles.size();
                auto const generators = graph.Generators.size();
                auto const states = graph.States.size();
                auto const clockTargets = graph.ClockTargets.size();

                PatchCompiler compiler{ graph, input };
                RouteProblemKind problem{};

                if (!compiler.Run(problem))
                {
                    graph.Settings.resize(settings);
                    graph.Cells.resize(cells);
                    graph.Stages.resize(stages);
                    graph.Edges.resize(edges);
                    graph.Roots.resize(roots);
                    graph.Leaves.resize(leaves);
                    graph.Throttles.resize(throttles);
                    graph.Generators.resize(generators);
                    graph.States.resize(states);
                    graph.ClockTargets.resize(clockTargets);

                    graph.Problems.push_back(RouteProblem{ input.Key, problem });

                    signatures.push_back(input.Key + L"|refused " + std::to_wstring(static_cast<int32_t>(problem)));
                    continue;
                }

                signatures.push_back(PatchSignature(input));
            }

            std::sort(signatures.begin(), signatures.end());

            for (auto const& signature : signatures)
            {
                graph.Signature += signature;
                graph.Signature += L"\n\n";
            }
        }
        catch (...)
        {
            // Nothing routes rather than half of something.
            graph = RouteGraph{};
            graph.Signature = L"failed";
        }

        return graph;
    }

    _Use_decl_annotations_
    void RunEdge(
        RouteGraph const& graph,
        RouteEdge const& edge,
        uint32_t const* words,
        uint8_t wordCount,
        RouteSink& sink) noexcept
    {
        if (words == nullptr || wordCount == 0 || wordCount > MaximumWordsPerMessage ||
            edge.Stage >= graph.Stages.size())
        {
            return;
        }

        sink.CountLink(edge.LinkCell);

        auto const& stage = graph.Stages[edge.Stage];

        uint32_t copy[MaximumWordsPerMessage]{};
        std::copy_n(words, wordCount, copy);

        switch (stage.Kind)
        {
        case RouteStageKind::Leaf:
        {
            auto const& leaf = graph.Leaves[stage.Index];

            // Messages into one group take that group, whatever group they came from.
            if (leaf.DestinationGroupIndex != AllGroups && internal::MessageHasGroupField(copy[0]))
            {
                copy[0] = internal::GetFirstWordWithNewGroup(copy[0], static_cast<uint8_t>(leaf.DestinationGroupIndex));
            }

            sink.Send(stage.Index, copy, wordCount);
            return;
        }

        case RouteStageKind::Throttle:
            sink.CountBlock(stage.Cell, true);
            sink.Throttle(stage.Index, copy, wordCount);
            return;

        case RouteStageKind::ClockInput:
            if (IsClockMessage(copy[0]))
            {
                sink.Clock(stage.Index, copy, wordCount);
            }
            return;

        case RouteStageKind::Block:
        {
            bool passed{ false };

            if (stage.Block == BlockKind::ClockDivider)
            {
                auto* count = sink.StateOf(stage.State);

                passed = count == nullptr ||
                    DivideClock(graph.Settings[stage.Index].ClockDivision, *count, copy, wordCount);
            }
            else
            {
                passed = ProcessBlock(stage.Block, graph.Settings[stage.Index], copy, wordCount);
            }

            sink.CountBlock(stage.Cell, passed);

            if (!passed)
            {
                return;
            }

            break;
        }

        default:
            sink.CountBlock(stage.Cell, true);
            break;
        }

        for (uint32_t i = 0; i < stage.EdgeCount; i++)
        {
            RunEdge(graph, graph.Edges[stage.FirstEdge + i], copy, wordCount, sink);
        }
    }

    _Use_decl_annotations_
    void RunRoot(
        RouteGraph const& graph,
        RouteRoot const& root,
        uint32_t const* words,
        uint8_t wordCount,
        RouteSink& sink) noexcept
    {
        if (words == nullptr || wordCount == 0 || !internal::MessageHasGroupField(words[0]))
        {
            return;
        }

        if (root.SourceGroupIndex != AllGroups &&
            root.SourceGroupIndex != static_cast<int32_t>(internal::GetGroupIndexFromFirstWord(words[0])))
        {
            return;
        }

        RunEdge(graph, root.Edge, words, wordCount, sink);
    }
}
