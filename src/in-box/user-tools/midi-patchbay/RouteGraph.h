// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Turns the patches that are routing into what the engine runs: for every link out of a source
// endpoint, a tree of stages that ends at destination endpoints. Pure, so the tests can prove a
// patch routes the way it reads, without a device or the service.
//
// A block that is fed from two places appears once in each tree, so every path through a patch
// runs on its own copy of the message. A throttle is the exception: it is one queue, whichever
// way messages reach it, and what comes after it is a tree of its own that runs at its pace. A
// generator starts a tree of its own too, because what it sends comes from nowhere.

#include "PatchDocument.h"
#include "StatefulBlocks.h"

#include <atomic>
#include <unordered_map>

namespace midipatchbay
{
    // Caps, so a patch that splits and joins many times cannot grow without bound. A patch past
    // either one does not route, and says so.
    constexpr size_t MaximumRouteStages = 16384;
    constexpr size_t MaximumRouteDepth = 64;

    enum class RouteStageKind : uint8_t
    {
        // Runs a block on the message, and stops it there when the block keeps it out.
        Block = 0,

        // A bypassed block, or a throttle with no limit: counted, and passed on untouched.
        PassThrough = 1,

        // Hands the message to a throttle's queue.
        Throttle = 2,

        // Sends the message to a destination endpoint.
        Leaf = 3,

        // Hands timing to an LFO that follows a clock. Nothing goes on from here.
        ClockInput = 4,
    };

    // A link into a stage. The cell is the link's own counters.
    struct RouteEdge
    {
        uint32_t Stage{ 0 };
        uint32_t LinkCell{ 0 };

        // The way out of a Branch or a Switch the link leaves by.
        uint8_t Way{ 0 };
    };

    // No way back: a message from a throttle or a generator.
    constexpr uint32_t NoReturnLeaf = 0xFFFFFFFF;

    // Where an answer to a message goes, and on which group: the one it came in on.
    struct ReturnPath
    {
        uint32_t Leaf{ NoReturnLeaf };
        uint8_t Group{ 0 };
    };

    struct RouteStage
    {
        RouteStageKind Kind{ RouteStageKind::PassThrough };
        BlockKind Block{ BlockKind::MessageTypeFilter };

        // Block: which settings. Leaf: which leaf. Throttle: which throttle. ClockInput: which of
        // RouteGraph::ClockTargets.
        uint32_t Index{ 0 };

        // The block's counters. Unused for a leaf, which counts on its link.
        uint32_t Cell{ 0 };

        // A stateful step's state, as an index into RouteGraph::States.
        uint32_t State{ 0 };

        // Where this stage sends on to, as a run in RouteGraph::Edges.
        uint32_t FirstEdge{ 0 };
        uint32_t EdgeCount{ 0 };

        // A bypassed Branch or Switch set to send only its first way: that way. -1 is every way.
        int16_t OnlyWay{ -1 };
    };

    // A link out of a source endpoint, and the tree it starts.
    struct RouteRoot
    {
        // Lowercased, the way the engine keys its connections.
        std::wstring SourceDeviceId{};
        bool WaitForSendComplete{ false };
        int32_t SourceGroupIndex{ AllGroups };
        RouteEdge Edge{};

        // Where a MIDI-CI responder's answers go: back to the source. Only made when the patch
        // has a responder in it.
        uint32_t ReplyLeaf{ NoReturnLeaf };
    };

    // A link into a destination endpoint. Each one sends on its own, the way a version 1
    // connection did, however many paths reach it.
    struct RouteLeaf
    {
        std::wstring DestinationDeviceId{};
        bool WaitForSendComplete{ false };
        int32_t DestinationGroupIndex{ AllGroups };
        uint32_t LinkCell{ 0 };
    };

    struct RouteThrottle
    {
        // A multiple of MIDI 1.0 wire speed.
        uint32_t Speed{ 1 };
        uint32_t Cell{ 0 };

        // What the throttle sends on to, once paced.
        uint32_t FirstEdge{ 0 };
        uint32_t EdgeCount{ 0 };
    };

    // A generator, and the tree what it sends goes through. Runs on a thread of its own for as
    // long as its patch routes.
    struct RouteGenerator
    {
        // "patch key|block id". The same in every graph a running patch compiles to, which is how
        // a change somewhere else in the patch leaves a running clock alone.
        std::wstring Key{};

        BlockKind Kind{ BlockKind::ClockGenerator };
        uint32_t Settings{ 0 };
        uint32_t Cell{ 0 };

        // An LFO with anything connected to its In follows that clock, even while the connection
        // is muted or its source is missing, rather than its own tempo.
        bool FollowsClock{ false };

        uint32_t FirstEdge{ 0 };
        uint32_t EdgeCount{ 0 };
    };

    enum class RouteProblemKind : int32_t
    {
        // Steps wired in a circle with no device in it, so a message would go round forever.
        LoopBetweenBlocks = 0,

        // Past MaximumRouteStages or MaximumRouteDepth.
        TooComplex = 1,
    };

    struct RouteProblem
    {
        std::wstring PatchKey{};
        RouteProblemKind Kind{ RouteProblemKind::LoopBetweenBlocks };
    };

    // One patch to route.
    struct RoutePatch
    {
        // Unique among the patches, and the start of every counter name.
        std::wstring Key{};

        PatchDocument const* Patch{ nullptr };

        // Endpoint node id to endpoint device id. An endpoint that is not here now is left out,
        // and so is everything that only leads to it.
        std::unordered_map<std::wstring, std::wstring> DeviceIds{};
    };

    // A stateful step: the cell of the block it is for, and what kind it is.
    struct RouteState
    {
        uint32_t Cell{ 0 };
        BlockKind Kind{ BlockKind::MessageTypeFilter };
    };

    struct RouteGraph
    {
        std::vector<BlockSettings> Settings{};

        // "patch key|element id", one per link and block that can count.
        std::vector<std::wstring> Cells{};

        std::vector<RouteStage> Stages{};
        std::vector<RouteEdge> Edges{};
        std::vector<RouteRoot> Roots{};
        std::vector<RouteLeaf> Leaves{};
        std::vector<RouteThrottle> Throttles{};
        std::vector<RouteGenerator> Generators{};

        // One for each stateful step. One for each step rather than each path, so the engine can
        // keep it when the graph changes.
        std::vector<RouteState> States{};

        // The LFOs that clock inputs feed, by generator key. One that isn't running gets nothing.
        std::vector<std::wstring> ClockTargets{};

        // Each patch's memories, as "patch key|name" with the name in lower case. The engine keeps
        // them for as long as the app runs, whatever happens to the graph.
        std::vector<std::wstring> Memories{};

        std::vector<RouteProblem> Problems{};

        // Equal for two graphs that route the same way, so applying an unchanged plan does
        // nothing.
        std::wstring Signature{};
    };

    // Muted links are left out. So is a patch with a loop between its blocks, or one too large
    // to route, each reported in Problems.
    RouteGraph CompileRoutes(_In_ std::vector<RoutePatch> const& patches) noexcept;

    // The endpoints a MIDI-CI responder sends its answers to, in the order the patch lists them:
    // each one whose messages reach it, other than through a throttle or from a generator. None
    // while it is bypassed.
    std::vector<std::wstring> EndpointsAnsweredBy(
        _In_ PatchDocument const& patch,
        _In_ std::wstring const& responderId) noexcept;

    // Told about everything one message does on its way through.
    class RouteSink
    {
    public:
        virtual ~RouteSink() noexcept = default;

        virtual void CountLink(_In_ uint32_t cell) noexcept = 0;
        virtual void CountBlock(_In_ uint32_t cell, _In_ bool passed) noexcept = 0;

        // The message as it leaves for the destination, its group already changed when the link
        // into the destination names one.
        virtual void Send(
            _In_ uint32_t leaf,
            _In_reads_(wordCount) uint32_t const* words,
            _In_ uint8_t wordCount) noexcept = 0;

        virtual void Throttle(
            _In_ uint32_t throttle,
            _In_reads_(wordCount) uint32_t const* words,
            _In_ uint8_t wordCount) noexcept = 0;

        // A stateful step's state. Null passes the message on as it is.
        virtual BlockState* StateOf(_In_ uint32_t state) noexcept = 0;

        // Timing clock, start, continue, stop or song position on its way into an LFO that
        // follows it.
        virtual void Clock(
            _In_ uint32_t target,
            _In_reads_(wordCount) uint32_t const* words,
            _In_ uint8_t wordCount) noexcept = 0;

        // A memory as this message sees it. It is read once for each message and is then the same
        // wherever the message goes, so a change made on another thread never lands halfway.
        virtual LogicValue Memory(_In_ uint32_t memory) noexcept = 0;

        // Changes a memory the way a Set memory step says. The message that changes it sees the
        // new value from here on, and so does every message after it.
        virtual void ChangeMemory(
            _In_ uint32_t memory,
            _In_ SetMemorySettings const& settings,
            _In_ LogicValue const& source) noexcept = 0;
    };

    // Sends one message down one link and everything after it. Every branch works on its own
    // copy, because blocks change the words in place, and carries the tags set on its way there.
    // Runs on the service callback thread: no allocation, no locks and nothing that throws, and no
    // deeper than MaximumRouteDepth. A MIDI-CI responder is the exception: it takes a lock and
    // allocates while it answers, which only MIDI-CI makes it do.
    void RunEdge(
        _In_ RouteGraph const& graph,
        _In_ RouteEdge const& edge,
        _In_reads_(wordCount) uint32_t const* words,
        _In_ uint8_t wordCount,
        _Inout_ RouteSink& sink,
        _In_ ReturnPath const& from = ReturnPath{},
        _In_opt_ TagLink const* tags = nullptr) noexcept;

    // A message arriving from a root's source endpoint. Messages without a group are never
    // routed, as before: nothing says which route they were meant for.
    void RunRoot(
        _In_ RouteGraph const& graph,
        _In_ RouteRoot const& root,
        _In_reads_(wordCount) uint32_t const* words,
        _In_ uint8_t wordCount,
        _Inout_ RouteSink& sink) noexcept;
}
