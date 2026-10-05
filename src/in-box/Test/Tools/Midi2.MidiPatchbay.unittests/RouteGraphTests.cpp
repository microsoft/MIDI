// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "RouteGraphTests.h"
#include "TestMessages.h"

#include "RouteGraph.h"
#include "PatchSerializer.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>

using namespace midipatchbay;
using namespace patchbaytests;

namespace
{
    constexpr uint8_t NoteOn = 0x9;

    struct Output
    {
        std::wstring Device{};
        Message Sent{};
    };

    // Records everything, and runs a throttle's tree straight away instead of pacing it.
    class RecordingSink : public RouteSink
    {
    public:
        explicit RecordingSink(RouteGraph const& graph) :
            m_graph(graph),
            m_states(std::make_unique<std::atomic<uint32_t>[]>((std::max)(graph.States.size(), size_t{ 1 })))
        {
        }

        std::vector<Output> Sent{};
        std::map<std::wstring, uint64_t> Links{};
        std::map<std::wstring, uint64_t> BlocksIn{};
        std::map<std::wstring, uint64_t> BlocksOut{};
        std::map<uint32_t, uint64_t> Throttled{};

        void CountLink(uint32_t cell) noexcept override
        {
            Links[m_graph.Cells[cell]]++;
        }

        void CountBlock(uint32_t cell, bool passed) noexcept override
        {
            BlocksIn[m_graph.Cells[cell]]++;

            if (passed)
            {
                BlocksOut[m_graph.Cells[cell]]++;
            }
        }

        void Send(uint32_t leaf, uint32_t const* words, uint8_t wordCount) noexcept override
        {
            Output output{};
            output.Device = m_graph.Leaves[leaf].DestinationDeviceId;
            std::copy_n(words, wordCount, output.Sent.Words.begin());
            output.Sent.Count = wordCount;

            Sent.push_back(output);
        }

        void Throttle(uint32_t throttle, uint32_t const* words, uint8_t wordCount) noexcept override
        {
            Throttled[throttle]++;

            auto const& entry = m_graph.Throttles[throttle];

            for (uint32_t i = 0; i < entry.EdgeCount; i++)
            {
                RunEdge(m_graph, m_graph.Edges[entry.FirstEdge + i], words, wordCount, *this);
            }
        }

        std::atomic<uint32_t>* StateOf(uint32_t state) noexcept override
        {
            return state < m_graph.States.size() ? &m_states[state] : nullptr;
        }

    private:
        RouteGraph const& m_graph;
        std::unique_ptr<std::atomic<uint32_t>[]> m_states{};
    };

    RecordingSink Arrive(RouteGraph const& graph, std::wstring const& device, Message const& message)
    {
        RecordingSink sink{ graph };

        for (auto const& root : graph.Roots)
        {
            if (root.SourceDeviceId == device)
            {
                RunRoot(graph, root, message.Words.data(), message.Count, sink);
            }
        }

        return sink;
    }

    void AddEndpoint(PatchDocument& patch, std::wstring const& id)
    {
        PatchEndpoint endpoint{};
        endpoint.Id = id;
        endpoint.DisplayName = id;
        patch.Endpoints.push_back(endpoint);
    }

    PatchBlock& AddBlock(PatchDocument& patch, std::wstring const& id, BlockKind kind)
    {
        PatchBlock block{};
        block.Id = id;
        block.Kind = kind;
        block.Settings = DefaultBlockSettings(kind);
        patch.Blocks.push_back(block);
        return patch.Blocks.back();
    }

    PatchConnection& Link(PatchDocument& patch, std::wstring const& id, std::wstring const& from, std::wstring const& to)
    {
        PatchConnection link{};
        link.Id = id;
        link.SourceId = from;
        link.DestinationId = to;
        patch.Connections.push_back(link);
        return patch.Connections.back();
    }

    RoutePatch Input(PatchDocument const& patch, std::wstring const& key = L"k")
    {
        RoutePatch input{};
        input.Key = key;
        input.Patch = &patch;

        for (auto const& endpoint : patch.Endpoints)
        {
            input.DeviceIds[endpoint.Id] = L"dev-" + endpoint.Id;
        }

        return input;
    }

    RouteGraph Compile(PatchDocument const& patch)
    {
        return CompileRoutes({ Input(patch) });
    }

    // One message from a generator, the way the engine's generator thread runs it.
    void Generate(RouteGraph const& graph, RouteGenerator const& generator, Message const& message, RecordingSink& sink)
    {
        sink.CountBlock(generator.Cell, true);

        for (uint32_t i = 0; i < generator.EdgeCount; i++)
        {
            RunEdge(graph, graph.Edges[generator.FirstEdge + i], message.Words.data(), message.Count, sink);
        }
    }

    size_t SentTo(RecordingSink const& sink, std::wstring const& device)
    {
        return static_cast<size_t>(std::count_if(sink.Sent.begin(), sink.Sent.end(),
            [&device](Output const& output) { return output.Device == device; }));
    }
}

void RouteGraphTests::AChainRoutesTheWayItReads()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"in");
    AddEndpoint(patch, L"out");

    auto& filter = AddBlock(patch, L"nf", BlockKind::NoteFilter);
    filter.Settings.Values.Lowest = 48;
    filter.Settings.Values.Highest = 72;

    AddBlock(patch, L"tr", BlockKind::Transpose).Settings.Transform.TransposeSemitones = 12;

    Link(patch, L"l1", L"in", L"nf");
    Link(patch, L"l2", L"nf", L"tr");
    Link(patch, L"l3", L"tr", L"out").DestinationGroupIndex = 3;

    auto const graph = Compile(patch);

    VERIFY_IS_TRUE(graph.Problems.empty());
    VERIFY_ARE_EQUAL(size_t{ 1 }, graph.Roots.size());

    auto const played = Arrive(graph, L"dev-in", Midi1(0, NoteOn, 0, 60, 100));

    VERIFY_ARE_EQUAL(size_t{ 1 }, played.Sent.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"dev-out" }, played.Sent[0].Device);
    VERIFY_IS_TRUE(played.Sent[0].Sent == Midi1(3, NoteOn, 0, 72, 100));

    VERIFY_ARE_EQUAL(uint64_t{ 1 }, played.Links.at(L"k|l1"));
    VERIFY_ARE_EQUAL(uint64_t{ 1 }, played.Links.at(L"k|l3"));
    VERIFY_ARE_EQUAL(uint64_t{ 1 }, played.BlocksOut.at(L"k|tr"));

    auto const low = Arrive(graph, L"dev-in", Midi1(0, NoteOn, 0, 40, 100));

    VERIFY_IS_TRUE(low.Sent.empty());
    VERIFY_ARE_EQUAL(uint64_t{ 1 }, low.BlocksIn.at(L"k|nf"));
    VERIFY_IS_TRUE(low.BlocksOut.find(L"k|nf") == low.BlocksOut.end());
    VERIFY_IS_TRUE(low.Links.find(L"k|l2") == low.Links.end());
}

void RouteGraphTests::EachPathGetsItsOwnCopy()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"in");
    AddEndpoint(patch, L"out");

    AddBlock(patch, L"split", BlockKind::NoteFilter);
    AddBlock(patch, L"up", BlockKind::Transpose).Settings.Transform.TransposeSemitones = 12;
    AddBlock(patch, L"down", BlockKind::Transpose).Settings.Transform.TransposeSemitones = -12;
    AddBlock(patch, L"join", BlockKind::ChannelMap).Settings.Transform.ChannelMap[0] = 1;

    Link(patch, L"1", L"in", L"split");
    Link(patch, L"2", L"split", L"up");
    Link(patch, L"3", L"split", L"down");
    Link(patch, L"4", L"up", L"join");
    Link(patch, L"5", L"down", L"join");
    Link(patch, L"6", L"join", L"out");

    auto const graph = Compile(patch);

    auto const played = Arrive(graph, L"dev-in", Midi1(0, NoteOn, 0, 60, 100));

    VERIFY_ARE_EQUAL(size_t{ 2 }, played.Sent.size());
    VERIFY_IS_TRUE(played.Sent[0].Sent == Midi1(0, NoteOn, 1, 72, 100));
    VERIFY_IS_TRUE(played.Sent[1].Sent == Midi1(0, NoteOn, 1, 48, 100));

    // The join is run once on each path, and counted as one block.
    auto const joins = std::count_if(graph.Stages.begin(), graph.Stages.end(),
        [](RouteStage const& s) { return s.Kind == RouteStageKind::Block && s.Block == BlockKind::ChannelMap; });

    VERIFY_ARE_EQUAL(ptrdiff_t{ 2 }, joins);
    VERIFY_ARE_EQUAL(uint64_t{ 2 }, played.BlocksIn.at(L"k|join"));

    // Both paths end on the one link into the destination.
    VERIFY_ARE_EQUAL(size_t{ 1 }, graph.Leaves.size());
    VERIFY_ARE_EQUAL(uint64_t{ 2 }, played.Links.at(L"k|6"));
}

void RouteGraphTests::OnlyTheChosenSourceGroupGoesIn()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"in");
    AddEndpoint(patch, L"out");

    Link(patch, L"1", L"in", L"out").SourceGroupIndex = 2;

    auto const graph = Compile(patch);

    VERIFY_ARE_EQUAL(size_t{ 1 }, Arrive(graph, L"dev-in", Midi1(2, NoteOn, 0, 60, 100)).Sent.size());
    VERIFY_ARE_EQUAL(size_t{ 0 }, Arrive(graph, L"dev-in", Midi1(1, NoteOn, 0, 60, 100)).Sent.size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, Arrive(graph, L"dev-in", SysEx7(2)).Sent.size());

    // Without a group nothing says which route a message was meant for.
    VERIFY_ARE_EQUAL(size_t{ 0 }, Arrive(graph, L"dev-in", Utility()).Sent.size());
    VERIFY_ARE_EQUAL(size_t{ 0 }, Arrive(graph, L"dev-in", Stream()).Sent.size());

    // Into all groups, a message keeps its own.
    auto const kept = Arrive(graph, L"dev-in", Midi1(2, NoteOn, 5, 60, 100));
    VERIFY_IS_TRUE(kept.Sent[0].Sent == Midi1(2, NoteOn, 5, 60, 100));
}

void RouteGraphTests::NothingIsBuiltForAnAbsentDestination()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"in");
    AddEndpoint(patch, L"out");
    AddEndpoint(patch, L"spare");

    AddBlock(patch, L"tr", BlockKind::Transpose).Settings.Transform.TransposeSemitones = 2;

    Link(patch, L"1", L"in", L"tr");
    Link(patch, L"2", L"tr", L"out");

    auto input = Input(patch);
    input.DeviceIds.erase(L"out");

    auto const absent = CompileRoutes({ input });

    VERIFY_IS_TRUE(absent.Roots.empty());
    VERIFY_IS_TRUE(absent.Stages.empty());
    VERIFY_IS_TRUE(absent.Problems.empty());

    Link(patch, L"3", L"tr", L"spare");

    auto again = Input(patch);
    again.DeviceIds.erase(L"out");

    auto const partly = CompileRoutes({ again });

    VERIFY_ARE_EQUAL(size_t{ 1 }, partly.Roots.size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, partly.Leaves.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"dev-spare" }, partly.Leaves[0].DestinationDeviceId);

    auto noSource = Input(patch);
    noSource.DeviceIds.erase(L"in");

    VERIFY_IS_TRUE(CompileRoutes({ noSource }).Roots.empty());
}

void RouteGraphTests::ALoopBetweenBlocksStopsOnlyThatPatch()
{
    PatchDocument looped{};
    AddEndpoint(looped, L"in");
    AddEndpoint(looped, L"out");
    AddBlock(looped, L"a", BlockKind::Transpose).Settings.Transform.TransposeSemitones = 1;
    AddBlock(looped, L"b", BlockKind::Transpose).Settings.Transform.TransposeSemitones = 1;

    Link(looped, L"1", L"in", L"a");
    Link(looped, L"2", L"a", L"b");
    Link(looped, L"3", L"b", L"a");
    Link(looped, L"4", L"b", L"out");

    PatchDocument plain{};
    AddEndpoint(plain, L"in");
    AddEndpoint(plain, L"out");
    Link(plain, L"1", L"in", L"out");

    auto const graph = CompileRoutes({ Input(looped, L"A"), Input(plain, L"B") });

    VERIFY_ARE_EQUAL(size_t{ 1 }, graph.Problems.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"A" }, graph.Problems[0].PatchKey);
    VERIFY_IS_TRUE(graph.Problems[0].Kind == RouteProblemKind::LoopBetweenBlocks);

    VERIFY_ARE_EQUAL(size_t{ 1 }, graph.Roots.size());

    for (auto const& cell : graph.Cells)
    {
        VERIFY_ARE_EQUAL(size_t{ 0 }, cell.find(L"B|"));
    }

    VERIFY_ARE_EQUAL(size_t{ 1 }, Arrive(graph, L"dev-in", Midi1(0, NoteOn, 0, 60, 100)).Sent.size());

    // Muting the link that closes the circle is enough to route again.
    looped.Connections[2].Muted = true;

    VERIFY_IS_TRUE(CompileRoutes({ Input(looped, L"A") }).Problems.empty());
}

void RouteGraphTests::AThrottleIsOneQueue()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"in1");
    AddEndpoint(patch, L"in2");
    AddEndpoint(patch, L"out");
    AddBlock(patch, L"slow", BlockKind::Throttle).Settings.SendSpeedLimit = 2;

    Link(patch, L"1", L"in1", L"slow");
    Link(patch, L"2", L"in2", L"slow");
    Link(patch, L"3", L"slow", L"out");

    auto const graph = Compile(patch);

    VERIFY_ARE_EQUAL(size_t{ 1 }, graph.Throttles.size());
    VERIFY_ARE_EQUAL(2u, graph.Throttles[0].Speed);
    VERIFY_ARE_EQUAL(size_t{ 2 }, graph.Roots.size());
    VERIFY_ARE_EQUAL(graph.Roots[0].Edge.Stage, graph.Roots[1].Edge.Stage);
    VERIFY_IS_TRUE(graph.Stages[graph.Roots[0].Edge.Stage].Kind == RouteStageKind::Throttle);

    auto const played = Arrive(graph, L"dev-in2", Midi1(0, NoteOn, 0, 60, 100));

    VERIFY_ARE_EQUAL(uint64_t{ 1 }, played.Throttled.at(0));
    VERIFY_ARE_EQUAL(size_t{ 1 }, played.Sent.size());

    // With no limit, or bypassed, there is nothing to pace.
    patch.Blocks[0].Settings.SendSpeedLimit = 0;
    VERIFY_IS_TRUE(Compile(patch).Throttles.empty());

    patch.Blocks[0].Settings.SendSpeedLimit = 4;
    patch.Blocks[0].Bypassed = true;
    VERIFY_IS_TRUE(Compile(patch).Throttles.empty());
    VERIFY_ARE_EQUAL(size_t{ 1 }, Arrive(Compile(patch), L"dev-in1", Midi1(0, NoteOn, 0, 60, 100)).Sent.size());
}

void RouteGraphTests::MutedLinksAndBypassedBlocks()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"in");
    AddEndpoint(patch, L"out");

    auto& transpose = AddBlock(patch, L"tr", BlockKind::Transpose);
    transpose.Settings.Transform.TransposeSemitones = 12;
    transpose.Bypassed = true;

    Link(patch, L"1", L"in", L"tr");
    Link(patch, L"2", L"tr", L"out");

    auto& muted = Link(patch, L"3", L"in", L"out");
    muted.DestinationGroupIndex = 5;
    muted.Muted = true;

    auto const graph = Compile(patch);
    auto const played = Arrive(graph, L"dev-in", Midi1(0, NoteOn, 0, 60, 100));

    VERIFY_ARE_EQUAL(size_t{ 1 }, played.Sent.size());
    VERIFY_IS_TRUE(played.Sent[0].Sent == Midi1(0, NoteOn, 0, 60, 100));
    VERIFY_ARE_EQUAL(uint64_t{ 1 }, played.BlocksIn.at(L"k|tr"));
    VERIFY_ARE_EQUAL(uint64_t{ 1 }, played.BlocksOut.at(L"k|tr"));
    VERIFY_IS_TRUE(played.Links.find(L"k|3") == played.Links.end());
}

void RouteGraphTests::APatchTooDeepDoesNotRoute()
{
    auto chain = [](int length)
    {
        PatchDocument patch{};
        AddEndpoint(patch, L"in");
        AddEndpoint(patch, L"out");

        std::wstring previous{ L"in" };

        for (int i = 0; i < length; i++)
        {
            auto const id = L"b" + std::to_wstring(i);

            AddBlock(patch, id, BlockKind::Transpose).Settings.Transform.TransposeSemitones = 1;
            Link(patch, L"l" + std::to_wstring(i), previous, id);

            previous = id;
        }

        Link(patch, L"last", previous, L"out");
        return patch;
    };

    auto const fine = chain(60);
    auto const graph = Compile(fine);

    VERIFY_IS_TRUE(graph.Problems.empty());

    auto const played = Arrive(graph, L"dev-in", Midi1(0, NoteOn, 0, 0, 100));
    VERIFY_ARE_EQUAL(size_t{ 1 }, played.Sent.size());
    VERIFY_IS_TRUE(played.Sent[0].Sent == Midi1(0, NoteOn, 0, 60, 100));

    auto const deep = chain(70);
    auto const refused = Compile(deep);

    VERIFY_ARE_EQUAL(size_t{ 1 }, refused.Problems.size());
    VERIFY_IS_TRUE(refused.Problems[0].Kind == RouteProblemKind::TooComplex);
    VERIFY_IS_TRUE(refused.Roots.empty());
    VERIFY_IS_TRUE(refused.Stages.empty());
}

void RouteGraphTests::TheSignatureFollowsWhatRoutes()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"in");
    AddEndpoint(patch, L"out");
    AddBlock(patch, L"tr", BlockKind::Transpose).Settings.Transform.TransposeSemitones = 2;
    Link(patch, L"1", L"in", L"tr");
    Link(patch, L"2", L"tr", L"out");

    auto const first = Compile(patch).Signature;

    VERIFY_IS_FALSE(first.empty());
    VERIFY_ARE_EQUAL(first, Compile(patch).Signature);

    // Where a node sits on the canvas does not change how it routes.
    patch.Blocks[0].CanvasX = 500;
    VERIFY_ARE_EQUAL(first, Compile(patch).Signature);

    patch.Blocks[0].Settings.Transform.TransposeSemitones = 3;
    auto const second = Compile(patch).Signature;
    VERIFY_ARE_NOT_EQUAL(first, second);

    patch.Connections[1].Muted = true;
    VERIFY_ARE_NOT_EQUAL(second, Compile(patch).Signature);
    patch.Connections[1].Muted = false;

    auto moved = Input(patch);
    moved.DeviceIds[L"out"] = L"dev-elsewhere";
    VERIFY_ARE_NOT_EQUAL(second, CompileRoutes({ moved }).Signature);

    patch.WaitForSendComplete = true;
    VERIFY_ARE_NOT_EQUAL(second, Compile(patch).Signature);
}

void RouteGraphTests::AConvertedPatchRoutesLikeVersion1()
{
    auto const patch = ReadPatchJson(LR"({
        "fileVersion": 1,
        "name": "Two ways",
        "endpoints": [
            { "id": "kb", "displayName": "Keyboard", "match": {} },
            { "id": "syn", "displayName": "Synth", "match": {} },
            { "id": "drm", "displayName": "Drums", "match": {} }
        ],
        "connections": [
            { "id": "c1", "sourceEndpointId": "kb", "sourceGroup": -1, "destinationEndpointId": "syn", "destinationGroup": 2,
              "filter": { "active": true, "channels": 3, "systemMessages": 1007 },
              "transform": { "active": true, "valueScale": "percent", "transposeSemitones": 12 },
              "sendSpeedLimit": 2 },
            { "id": "c2", "sourceEndpointId": "kb", "sourceGroup": 1, "destinationEndpointId": "drm", "destinationGroup": 4 }
        ]
    })", L"fallback");

    VERIFY_IS_TRUE(patch.has_value());

    auto input = Input(patch.value());
    auto const graph = CompileRoutes({ input });

    VERIFY_IS_TRUE(graph.Problems.empty());
    VERIFY_ARE_EQUAL(size_t{ 1 }, graph.Throttles.size());

    // Group 2, channel 2, as people count them: transposed into group 3 of the synth, and as it
    // was into group 5 of the drums.
    auto const both = Arrive(graph, L"dev-kb", Midi1(1, NoteOn, 1, 60, 100));

    VERIFY_ARE_EQUAL(size_t{ 2 }, both.Sent.size());

    auto const synth = std::find_if(both.Sent.begin(), both.Sent.end(), [](Output const& o) { return o.Device == L"dev-syn"; });
    auto const drums = std::find_if(both.Sent.begin(), both.Sent.end(), [](Output const& o) { return o.Device == L"dev-drm"; });

    VERIFY_IS_TRUE(synth != both.Sent.end());
    VERIFY_IS_TRUE(drums != both.Sent.end());
    VERIFY_IS_TRUE(synth->Sent == Midi1(2, NoteOn, 1, 72, 100));
    VERIFY_IS_TRUE(drums->Sent == Midi1(4, NoteOn, 1, 60, 100));

    // Group 0 reaches only the synth, and clock never does.
    VERIFY_ARE_EQUAL(size_t{ 1 }, Arrive(graph, L"dev-kb", Midi1(0, NoteOn, 1, 60, 100)).Sent.size());
    VERIFY_ARE_EQUAL(size_t{ 0 }, Arrive(graph, L"dev-kb", System(0, 0xF8)).Sent.size());

    auto const clock = Arrive(graph, L"dev-kb", System(1, 0xF8));
    VERIFY_ARE_EQUAL(size_t{ 1 }, clock.Sent.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"dev-drm" }, clock.Sent[0].Device);
}

void RouteGraphTests::TheAgentGuideSplitExampleRoutes()
{
    // "A complete example" in the guide.
    auto const patch = ReadPatchJson(LR"({
  "fileVersion": 2,
  "name": "Keyboard split",
  "description": "Bass below middle C, pad from middle C up.",
  "activateAtStartup": false,
  "endpoints": [
    { "id": "keyboard", "displayName": "Keyboard", "match": { "transportSuppliedEndpointName": "Keyboard" }, "matchMode": "endpointName", "x": 60, "y": 100 },
    { "id": "bass", "displayName": "Bass synth", "match": { "transportSuppliedEndpointName": "Bass synth" }, "matchMode": "endpointName", "x": 1520, "y": 20 },
    { "id": "pad", "displayName": "Pad synth", "match": { "transportSuppliedEndpointName": "Pad synth" }, "matchMode": "endpointName", "x": 1520, "y": 220 }
  ],
  "blocks": [
    { "id": "no-clock", "type": "messageTypeFilter", "name": "Keep out clock", "x": 400, "y": 130, "settings": { "systemMessages": 751 } },
    { "id": "low-notes", "type": "noteFilter", "name": "Below middle C", "x": 680, "y": 40, "settings": { "mode": "range", "action": "letThrough", "lowest": 0, "highest": 59 } },
    { "id": "high-notes", "type": "noteFilter", "name": "Middle C and up", "x": 680, "y": 220, "settings": { "mode": "range", "action": "letThrough", "lowest": 60, "highest": 127 } },
    { "id": "octave-down", "type": "transpose", "name": "Octave down", "x": 960, "y": 220, "settings": { "transposeSemitones": -12 } },
    { "id": "softer", "type": "velocity", "name": "Softer", "x": 1240, "y": 220, "settings": { "valueScale": "percent", "velocityCurve": 1 } }
  ],
  "connections": [
    { "id": "keyboard-in", "source": "keyboard", "sourceGroup": -1, "destination": "no-clock" },
    { "id": "to-low", "source": "no-clock", "destination": "low-notes" },
    { "id": "to-high", "source": "no-clock", "destination": "high-notes" },
    { "id": "low-to-bass", "source": "low-notes", "destination": "bass", "destinationGroup": -1 },
    { "id": "high-down", "source": "high-notes", "destination": "octave-down" },
    { "id": "down-softer", "source": "octave-down", "destination": "softer" },
    { "id": "softer-to-pad", "source": "softer", "destination": "pad", "destinationGroup": -1 }
  ]
})", L"fallback");

    VERIFY_IS_TRUE(patch.has_value());
    VERIFY_ARE_EQUAL(size_t{ 3 }, patch->Endpoints.size());
    VERIFY_ARE_EQUAL(size_t{ 5 }, patch->Blocks.size());
    VERIFY_ARE_EQUAL(size_t{ 7 }, patch->Connections.size());
    VERIFY_IS_FALSE(patch->ActivateAtStartup);

    auto const graph = CompileRoutes({ Input(patch.value()) });

    VERIFY_IS_TRUE(graph.Problems.empty());

    // Below middle C reaches the bass as it was played.
    auto const low = Arrive(graph, L"dev-keyboard", Midi1(0, NoteOn, 0, 59, 100));

    VERIFY_ARE_EQUAL(size_t{ 1 }, low.Sent.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"dev-bass" }, low.Sent[0].Device);
    VERIFY_IS_TRUE(low.Sent[0].Sent == Midi1(0, NoteOn, 0, 59, 100));

    // Middle C and up reaches the pad, an octave lower and played softer.
    auto const high = Arrive(graph, L"dev-keyboard", Midi1(0, NoteOn, 0, 60, 100));

    VERIFY_ARE_EQUAL(size_t{ 1 }, high.Sent.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"dev-pad" }, high.Sent[0].Device);

    auto const word = high.Sent[0].Sent.Words[0];

    VERIFY_ARE_EQUAL(uint32_t{ 48 }, (word >> 8) & 0x7F);
    VERIFY_IS_LESS_THAN(word & 0x7F, uint32_t{ 100 });
    VERIFY_IS_GREATER_THAN(word & 0x7F, uint32_t{ 0 });

    // Clock and active sensing reach neither synth. Start reaches both.
    VERIFY_ARE_EQUAL(size_t{ 0 }, Arrive(graph, L"dev-keyboard", System(0, 0xF8)).Sent.size());
    VERIFY_ARE_EQUAL(size_t{ 0 }, Arrive(graph, L"dev-keyboard", System(0, 0xFE)).Sent.size());
    VERIFY_ARE_EQUAL(size_t{ 2 }, Arrive(graph, L"dev-keyboard", System(0, 0xFA)).Sent.size());
}

void RouteGraphTests::TheAgentGuideMaskExampleRoutes()
{
    // The message mask example in the guide, as the only step between two devices.
    auto const patch = ReadPatchJson(LR"({
  "fileVersion": 2,
  "name": "No volume on channel 1",
  "activateAtStartup": false,
  "endpoints": [
    { "id": "keyboard", "displayName": "Keyboard", "match": { "transportSuppliedEndpointName": "Keyboard" }, "matchMode": "endpointName", "x": 60, "y": 60 },
    { "id": "synth", "displayName": "Synth", "match": { "transportSuppliedEndpointName": "Synth" }, "matchMode": "endpointName", "x": 680, "y": 60 }
  ],
  "blocks": [
    {
      "id": "no-volume", "type": "messageMaskFilter", "name": "No volume on channel 1", "x": 400, "y": 90,
      "settings": {
        "words": 1,
        "action": "keepOut",
        "conditions": [
          { "word": 0, "highBit": 31, "lowBit": 28, "match": "exactly", "value": 2 },
          { "word": 0, "highBit": 23, "lowBit": 20, "match": "exactly", "value": 11 },
          { "word": 0, "highBit": 19, "lowBit": 16, "match": "exactly", "value": 0 },
          { "word": 0, "highBit": 14, "lowBit": 8, "match": "exactly", "value": 7 }
        ]
      }
    }
  ],
  "connections": [
    { "id": "in", "source": "keyboard", "destination": "no-volume" },
    { "id": "out", "source": "no-volume", "destination": "synth" }
  ]
})", L"fallback");

    VERIFY_IS_TRUE(patch.has_value());
    VERIFY_ARE_EQUAL(size_t{ 1 }, patch->Blocks.size());
    VERIFY_ARE_EQUAL(size_t{ 4 }, patch->Blocks[0].Settings.Mask.Conditions.size());

    auto const graph = CompileRoutes({ Input(patch.value()) });

    VERIFY_IS_TRUE(graph.Problems.empty());

    constexpr uint8_t ControlChange = 0xB;

    // Volume on channel 1 is kept out. Volume on channel 2, pan on channel 1, notes, and a
    // MIDI 2.0 volume, which is a bigger message, all get through.
    VERIFY_ARE_EQUAL(size_t{ 0 }, Arrive(graph, L"dev-keyboard", Midi1(0, ControlChange, 0, 7, 100)).Sent.size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, Arrive(graph, L"dev-keyboard", Midi1(0, ControlChange, 1, 7, 100)).Sent.size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, Arrive(graph, L"dev-keyboard", Midi1(0, ControlChange, 0, 10, 100)).Sent.size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, Arrive(graph, L"dev-keyboard", Midi1(0, NoteOn, 0, 7, 100)).Sent.size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, Arrive(graph, L"dev-keyboard", Midi2(0, ControlChange, 0, 7, 0, 0x80000000u)).Sent.size());
}

void RouteGraphTests::AGeneratorStartsATreeOfItsOwn()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"drums");
    AddEndpoint(patch, L"synth");

    AddBlock(patch, L"clock", BlockKind::ClockGenerator);
    AddBlock(patch, L"half", BlockKind::ClockDivider);

    Link(patch, L"1", L"clock", L"drums").DestinationGroupIndex = 2;
    Link(patch, L"2", L"clock", L"half");
    Link(patch, L"3", L"half", L"synth");

    auto const graph = Compile(patch);

    VERIFY_IS_TRUE(graph.Problems.empty());
    VERIFY_ARE_EQUAL(size_t{ 0 }, graph.Roots.size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, graph.Generators.size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, graph.States.size());

    auto const& generator = graph.Generators[0];

    VERIFY_ARE_EQUAL(std::wstring{ L"k|clock" }, generator.Key);
    VERIFY_IS_TRUE(generator.Kind == BlockKind::ClockGenerator);
    VERIFY_ARE_EQUAL(uint32_t{ 2 }, generator.EdgeCount);
    VERIFY_ARE_EQUAL(std::wstring{ L"k|clock" }, graph.Cells[generator.Cell]);

    // Every pulse reaches the drums, on group 3. Every other one reaches the synth.
    RecordingSink sink{ graph };

    for (int i = 0; i < 4; i++)
    {
        Generate(graph, generator, System(0, 0xF8), sink);
    }

    VERIFY_ARE_EQUAL(size_t{ 4 }, SentTo(sink, L"dev-drums"));
    VERIFY_ARE_EQUAL(size_t{ 2 }, SentTo(sink, L"dev-synth"));
    VERIFY_IS_TRUE(sink.Sent[0].Sent == System(2, 0xF8));

    VERIFY_ARE_EQUAL(uint64_t{ 4 }, sink.BlocksOut.at(L"k|clock"));
    VERIFY_ARE_EQUAL(uint64_t{ 4 }, sink.BlocksIn.at(L"k|half"));
    VERIFY_ARE_EQUAL(uint64_t{ 2 }, sink.BlocksOut.at(L"k|half"));

    // A new tempo changes what routes, so a running patch picks it up.
    auto faster = patch;
    faster.Blocks[0].Settings.Clock.BeatsPerMinute = 140;

    VERIFY_ARE_NOT_EQUAL(graph.Signature, Compile(faster).Signature);
}

void RouteGraphTests::AGeneratorRunsOnlyWhenItLeadsSomewhere()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"synth");
    AddEndpoint(patch, L"gone");

    AddBlock(patch, L"bypassed", BlockKind::ClockGenerator).Bypassed = true;
    AddBlock(patch, L"alone", BlockKind::TimeCodeGenerator);
    AddBlock(patch, L"muted", BlockKind::LfoGenerator);
    AddBlock(patch, L"absent", BlockKind::ClockGenerator);

    Link(patch, L"1", L"bypassed", L"synth");
    Link(patch, L"2", L"muted", L"synth").Muted = true;
    Link(patch, L"3", L"absent", L"gone");

    auto input = Input(patch);
    input.DeviceIds.erase(L"gone");

    auto const graph = CompileRoutes({ input });

    VERIFY_IS_TRUE(graph.Problems.empty());
    VERIFY_ARE_EQUAL(size_t{ 0 }, graph.Generators.size());

    // Taken out of bypass, the first one runs, with its own settings.
    patch.Blocks[0].Bypassed = false;
    patch.Blocks[0].Settings.Clock.BeatsPerMinute = 90;

    auto const running = CompileRoutes({ input });

    VERIFY_ARE_EQUAL(size_t{ 1 }, running.Generators.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"k|bypassed" }, running.Generators[0].Key);
    VERIFY_ARE_EQUAL(90.0, running.Settings[running.Generators[0].Settings].Clock.BeatsPerMinute);
}

void RouteGraphTests::NothingGoesIntoAGenerator()
{
    // Drawn by hand, a link into a generator carries nothing, and the generator still runs.
    PatchDocument built{};
    AddEndpoint(built, L"keys");
    AddEndpoint(built, L"synth");
    AddBlock(built, L"lfo", BlockKind::LfoGenerator);
    Link(built, L"in", L"keys", L"lfo");
    Link(built, L"out", L"lfo", L"synth");

    auto const graph = Compile(built);

    VERIFY_IS_TRUE(graph.Problems.empty());
    VERIFY_ARE_EQUAL(size_t{ 0 }, graph.Roots.size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, graph.Generators.size());
    VERIFY_ARE_EQUAL(size_t{ 0 }, Arrive(graph, L"dev-keys", Midi1(0, NoteOn, 0, 60, 100)).Sent.size());

    // From a file, the link is left out when the patch is read.
    auto const patch = ReadPatchJson(LR"({
  "fileVersion": 2,
  "name": "Into a clock",
  "endpoints": [
    { "id": "keys", "displayName": "Keys", "match": { "transportSuppliedEndpointName": "Keys" }, "matchMode": "endpointName", "x": 60, "y": 20 },
    { "id": "synth", "displayName": "Synth", "match": { "transportSuppliedEndpointName": "Synth" }, "matchMode": "endpointName", "x": 680, "y": 20 }
  ],
  "blocks": [
    { "id": "clock", "type": "clockGenerator", "x": 400, "y": 50, "settings": {} }
  ],
  "connections": [
    { "id": "in", "source": "keys", "destination": "clock" },
    { "id": "out", "source": "clock", "destination": "synth" }
  ]
})", L"fallback");

    VERIFY_IS_TRUE(patch.has_value());
    VERIFY_ARE_EQUAL(size_t{ 1 }, patch->Connections.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"out" }, patch->Connections[0].Id);
}

void RouteGraphTests::TheAgentGuideClockExampleRoutes()
{
    // The MIDI clock example in the guide.
    auto const patch = ReadPatchJson(LR"({
  "fileVersion": 2,
  "name": "Studio clock",
  "description": "One clock at 96 BPM, and half time for the arpeggiator.",
  "activateAtStartup": false,
  "endpoints": [
    { "id": "drums", "displayName": "Drum machine", "match": { "transportSuppliedEndpointName": "Drum machine" }, "matchMode": "endpointName", "x": 680, "y": 20 },
    { "id": "sequencer", "displayName": "Sequencer", "match": { "transportSuppliedEndpointName": "Sequencer" }, "matchMode": "endpointName", "x": 680, "y": 180 },
    { "id": "arp", "displayName": "Arpeggiator", "match": { "transportSuppliedEndpointName": "Arpeggiator" }, "matchMode": "endpointName", "x": 680, "y": 340 }
  ],
  "blocks": [
    { "id": "clock", "type": "clockGenerator", "name": "Studio clock", "x": 60, "y": 180, "settings": { "beatsPerMinute": 96, "sendStartStop": true } },
    { "id": "half-time", "type": "clockDivider", "name": "Half time", "x": 400, "y": 370, "settings": { "divideBy": 2 } }
  ],
  "connections": [
    { "id": "to-drums", "source": "clock", "destination": "drums", "destinationGroup": -1 },
    { "id": "to-sequencer", "source": "clock", "destination": "sequencer", "destinationGroup": -1 },
    { "id": "to-half-time", "source": "clock", "destination": "half-time" },
    { "id": "to-arp", "source": "half-time", "destination": "arp", "destinationGroup": -1 }
  ]
})", L"fallback");

    VERIFY_IS_TRUE(patch.has_value());
    VERIFY_ARE_EQUAL(size_t{ 3 }, patch->Endpoints.size());
    VERIFY_ARE_EQUAL(size_t{ 2 }, patch->Blocks.size());
    VERIFY_ARE_EQUAL(size_t{ 4 }, patch->Connections.size());

    auto const graph = CompileRoutes({ Input(patch.value()) });

    VERIFY_IS_TRUE(graph.Problems.empty());
    VERIFY_ARE_EQUAL(size_t{ 0 }, graph.Roots.size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, graph.Generators.size());

    auto const& clock = graph.Generators[0];

    VERIFY_ARE_EQUAL(96.0, graph.Settings[clock.Settings].Clock.BeatsPerMinute);
    VERIFY_IS_TRUE(graph.Settings[clock.Settings].Clock.SendStartStop);

    // Start reaches all three. Then every pulse reaches the drums and the sequencer, and every
    // other one the arpeggiator.
    RecordingSink sink{ graph };

    Generate(graph, clock, System(0, 0xFA), sink);

    VERIFY_ARE_EQUAL(size_t{ 3 }, sink.Sent.size());

    for (int i = 0; i < 4; i++)
    {
        Generate(graph, clock, System(0, 0xF8), sink);
    }

    VERIFY_ARE_EQUAL(size_t{ 5 }, SentTo(sink, L"dev-drums"));
    VERIFY_ARE_EQUAL(size_t{ 5 }, SentTo(sink, L"dev-sequencer"));
    VERIFY_ARE_EQUAL(size_t{ 3 }, SentTo(sink, L"dev-arp"));
}

void RouteGraphTests::TheAgentGuideLfoExampleReads()
{
    // The LFO example in the guide, as the only step, sending to one device.
    auto const patch = ReadPatchJson(LR"({
  "fileVersion": 2,
  "name": "Filter sweep",
  "activateAtStartup": false,
  "endpoints": [
    { "id": "synth", "displayName": "Synth", "match": { "transportSuppliedEndpointName": "Synth" }, "matchMode": "endpointName", "x": 400, "y": 270 }
  ],
  "blocks": [
    { "id": "filter-sweep", "type": "lfoGenerator", "name": "Filter sweep", "x": 60, "y": 300, "settings": { "wave": "triangle", "beatsPerCycle": 8, "beatsPerMinute": 120, "lowestPercent": 20, "highestPercent": 80, "message": "controlChange", "channel": 0, "number": 74 } }
  ],
  "connections": [
    { "id": "out", "source": "filter-sweep", "destination": "synth" }
  ]
})", L"fallback");

    VERIFY_IS_TRUE(patch.has_value());
    VERIFY_ARE_EQUAL(size_t{ 1 }, patch->Blocks.size());

    auto const& lfo = patch->Blocks[0].Settings.Lfo;

    VERIFY_IS_TRUE(lfo.Wave == midiapp::LfoWave::Triangle);
    VERIFY_ARE_EQUAL(8.0, lfo.BeatsPerCycle);
    VERIFY_ARE_EQUAL(120.0, lfo.BeatsPerMinute);
    VERIFY_ARE_EQUAL(2000, lfo.LowestHundredths);
    VERIFY_ARE_EQUAL(8000, lfo.HighestHundredths);
    VERIFY_IS_TRUE(lfo.Target.Kind == midiapp::ValueMessageKind::ControlChange);
    VERIFY_ARE_EQUAL(74u, lfo.Target.Number);
    VERIFY_ARE_EQUAL(0, static_cast<int>(lfo.Target.Channel));
    VERIFY_IS_FALSE(lfo.Target.Midi1Protocol);
    VERIFY_IS_TRUE(lfo.ReturnsToMiddle);

    auto const graph = CompileRoutes({ Input(patch.value()) });

    VERIFY_IS_TRUE(graph.Problems.empty());
    VERIFY_ARE_EQUAL(size_t{ 1 }, graph.Generators.size());
    VERIFY_IS_TRUE(graph.Generators[0].Kind == BlockKind::LfoGenerator);
}
