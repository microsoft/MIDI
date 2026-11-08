// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MessageWatch.h"
#include "PatchModel.h"
#include "RouteGraph.h"
#include "SendQueue.h"

namespace midipatchbay
{
    // What one link or block has done since its patch started routing, keyed by
    // "patch key|element id".
    struct RouteStats
    {
        // A link: messages that went along it. A block: messages it let through.
        uint64_t MessagesForwarded{ 0 };

        // A block: messages it kept out.
        uint64_t MessagesKeptOut{ 0 };

        // A link into a destination: sends the service turned down.
        uint64_t SendFailures{ 0 };

        // A throttle, or a link into a destination in a patch that waits for each send.
        uint64_t MessagesWaiting{ 0 };
        uint64_t MessagesDropped{ 0 };

        // A throttle: in-between values of controller sweeps it skipped to catch up.
        uint64_t MessagesThinned{ 0 };

        bool IsActive{ false };
    };

    // Owns the session, the open connections and the forwarding.
    //
    // Receiving and sending both go through the COM extensions, and a message is walked through
    // its patch on the service callback thread: no queue, no worker, no allocation once the
    // graph is applied. The timestamp the service delivered is the timestamp sent on, so a
    // message scheduled for the future stays scheduled rather than being flattened to "now".
    //
    // Two things hold messages back, and neither may make the callback thread wait. A throttle
    // has a queue and a thread of its own, which runs what comes after it at its pace. A patch
    // that waits for each send to complete queues what reaches a destination, and a send thread
    // for that destination sends it on.
    //
    // A generator also has a thread of its own. It sends ahead of time, each message carrying the
    // timestamp it is meant to play at, so its timing comes from the service and not from when the
    // thread happens to wake.
    //
    // Applying a new graph changes only what it has to. A connection that is still wanted stays
    // open, and a generator whose patch still routes keeps running, so an edit to one patch never
    // restarts another patch's clock or drops the messages it already scheduled.
    //
    // Everything here must be called from a background thread. The SDK's session and connection
    // calls block on the service, and blocking the STA UI thread hangs the app. The counts and
    // the last error can be read from any thread, and never wait for a graph being applied.
    class RouteEngine
    {
    public:
        static RouteEngine& Current() noexcept;

        ~RouteEngine() noexcept;

        // Replaces the whole routing table. A graph that routes the same way as the running one
        // is a no-op, so an unrelated device arriving does not interrupt anything.
        void Apply(_In_ RouteGraph graph) noexcept;

        void Shutdown() noexcept;

        std::unordered_map<std::wstring, RouteStats> Stats() const noexcept;

        winrt::hstring LastErrorMessage() const noexcept;

        // What a MIDI-CI responder step has been doing, by its "patch key|block id". Nothing when
        // it isn't routing.
        std::optional<::midipatchbay::CiResponderSnapshot> CiResponderStatus(_In_ std::wstring const& cell) const noexcept;

        // A patch's memory as it is now, by the patch's key and the memory's name. Nothing when no
        // routing has used it since the app started.
        std::optional<LogicValue> MemoryValue(_In_ std::wstring const& patchKey, _In_ std::wstring const& name) const noexcept;

        // What a Branch or a Switch tested last, by its "patch key|block id". Nothing when it
        // isn't routing.
        std::optional<LogicValue> LastTestedValue(_In_ std::wstring const& cell) const noexcept;

        // Plays one note on an endpoint so a mapping can be heard. Reuses the open connection
        // when the patch is already routing, and otherwise opens one for the length of the note.
        // Blocks for the duration, so it has to be called from a background thread.
        bool SendTestNote(
            _In_ std::wstring const& endpointDeviceId,
            _In_ int32_t groupIndex,
            _In_ uint8_t noteIndex) noexcept;

        // Starts copying what goes along links into a buffer for a connection monitor window.
        // Each cell is "patch key|link id", and what goes along cells[i] is marked as link i. A
        // cell that isn't routing now is watched from when it is. Routing never waits for the
        // window. Any thread; never waits for a graph being applied.
        WatchLease StartWatching(_In_ std::vector<std::wstring> const& cells) noexcept;

        // The same window, watching other links. The marks stay with the place in the list.
        void ChangeWatching(_In_ uint32_t id, _In_ std::vector<std::wstring> const& cells) noexcept;

        // The buffer goes back for the next window. The window has stopped taking from it.
        void StopWatching(_In_ uint32_t id) noexcept;

    private:
        RouteEngine() noexcept = default;

        struct Counters;
        struct Leaf;
        struct Context;
        struct SourceHub;
        struct HubPlan;
        struct ThrottleRunner;
        struct DestinationSender;
        struct Runtime;
        struct Connection;
        struct GeneratorPlan;
        class GeneratorRunner;

        // A connection, and whether its sends wait to complete. Each patch's setting gets
        // connections of its own, so one patch's setting never changes another's.
        using ConnectionKey = std::pair<std::wstring, bool>;

        // Stops every generator, lets what they scheduled play, and closes every connection.
        void TearDownLocked() noexcept;

        // Swaps in what the window reads, and hands back the graph it replaced.
        std::shared_ptr<Runtime> Publish(_In_ std::shared_ptr<Runtime> runtime) noexcept;

        void SetLastError(_In_ winrt::hstring const& message) noexcept;

        void CloseConnection(_Inout_ Connection& connection) noexcept;

        // One connection monitor window, and the links it watches.
        struct Watch
        {
            uint32_t Id{ 0 };
            WatchBuffer* Buffer{ nullptr };
            uint32_t Lease{ 0 };
            std::vector<std::wstring> Cells{};
        };

        // Points each of the graph's watched links at the windows watching it. Under m_publishLock.
        void InstallWatchesLocked(_Inout_ Runtime& runtime) noexcept;

        // Guarded by m_publishLock. A buffer is never freed while the app runs, because a routing
        // thread can still hold one for a moment after its window has gone. One that is let go
        // waits here for the next window.
        std::vector<std::unique_ptr<WatchBuffer>> m_watchBuffers{};
        std::vector<WatchBuffer*> m_freeWatchBuffers{};
        std::vector<Watch> m_watches{};
        uint32_t m_nextWatchId{ 1 };

        // Held for the whole of Apply and Shutdown, and while a test note finds its connection.
        std::mutex m_lock{};

        // Held only to read or replace what the window asks about.
        mutable std::mutex m_publishLock{};

        midi2::MidiSession m_session{ nullptr };

        // Keyed by lowercased endpoint device id, so one endpoint is only ever opened once for
        // each setting even when several patches use it. Kept from one graph to the next.
        std::map<ConnectionKey, std::unique_ptr<Connection>> m_connections{};

        // Keyed by "patch key|block id", and kept from one graph to the next.
        std::map<std::wstring, std::unique_ptr<GeneratorRunner>> m_generators{};

        // Each clock divider's count, keyed by its cell, so a change elsewhere does not restart it.
        std::unordered_map<std::wstring, std::shared_ptr<::midipatchbay::BlockState>> m_blockStates{};

        // Every memory any patch has used, by "patch key|name". Kept for as long as the app runs,
        // whether or not the patch is routing. Guarded by m_publishLock, because the window reads it.
        std::unordered_map<std::wstring, std::shared_ptr<std::atomic<uint64_t>>> m_memories{};

        // Shared with the source hubs and generators, so a callback that is still running when the
        // graph is replaced finishes on the graph it started with.
        std::shared_ptr<Runtime> m_runtime{};

        std::wstring m_signature{};
        winrt::hstring m_lastError{};
    };
}
