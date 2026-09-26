// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <sal.h>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "ActionPlan.h"

namespace glass
{
    // Plays the plans the action layer built: system exclusive dumps, raw UMP, and sequences of
    // notes with real gaps between them.
    //
    // Everything that has no wait in front of it runs on the caller's thread, which is the
    // pointer handler, so a button that sends one dump has no more latency than a button that
    // sends one note. Only a wait puts a plan on the clock.
    //
    // The clock is one thread for the whole player, not one per sequence. Twenty buttons holding
    // twenty sequences is a real layout, and twenty threads each waking every few milliseconds
    // is not a reasonable way to spend a laptop's battery during a set.
    //
    // Sending happens on the dispatcher's thread, because that is the only thread the send table
    // is read from. The clock thread never touches a connection.
    class SequenceRunner : public std::enable_shared_from_this<SequenceRunner>
    {
    public:
        static std::shared_ptr<SequenceRunner> Create();

        ~SequenceRunner() noexcept;

        SequenceRunner(SequenceRunner const&) = delete;
        SequenceRunner& operator=(SequenceRunner const&) = delete;

        // ---- what the owner supplies. Called on the dispatcher's thread. ----

        std::function<void(uint32_t controlIndex, int32_t destinationIndex, uint32_t const* words, uint32_t wordCount)> Send{};
        std::function<void(uint32_t controlIndex, double value)> SetControlValue{};
        std::function<void(uint32_t pageIndex)> GoToPage{};

        void Start(_In_ winrt::Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher);

        // Stops the clock and drops everything still waiting. Safe to call twice.
        void Stop() noexcept;

        // Plays this plan for this control. A control already playing starts again from the top,
        // which is what pressing a button twice means.
        void Run(_In_ uint32_t controlIndex, _In_ ActionPlan const& plan);

        // Drops whatever this control was still playing, without sending anything else. The
        // messages already out stay out; nothing is unwound, because unwinding somebody's
        // synthesizer state is a guess.
        void CancelFor(_In_ uint32_t controlIndex) noexcept;

        void CancelAll() noexcept;

        // Whether this control already has a plan running. What tells a button pressed a second
        // time that it means stop rather than start again.
        bool IsRunning(_In_ uint32_t controlIndex) const noexcept;

        // How many plans are still on the clock. The runtime window shows it, and a test can
        // assert a plan finished rather than sleeping and hoping.
        size_t PendingCount() const noexcept;

    private:
        SequenceRunner() = default;

        struct PendingRun
        {
            uint32_t ControlIndex{ 0 };

            // Copied rather than referenced: the document is re-prepared under a running plan
            // every time an edit happens in Try mode.
            std::vector<PlanAction> Actions{};

            size_t NextAction{ 0 };
            uint64_t DueMilliseconds{ 0 };

            // Starts again from the top at the end. An arpeggio held under a finger does; a
            // patch recall does not.
            bool Loops{ false };

            // Handed to the dispatcher and not yet back. Without it the clock re-posts a due run
            // on every pass while the UI thread is busy.
            bool Dispatched{ false };
        };

        // Runs actions until a wait or the end. Returns true when the run is finished.
        bool Advance(_Inout_ PendingRun& run);

        void ClockLoop();

        mutable std::mutex m_lock{};
        std::condition_variable m_wake{};

        // A notify that arrives while the clock thread is between checking and waiting would be
        // dropped without this, and every step of every sequence would stall for as long as the
        // idle wait instead of for the gap it asked for.
        bool m_signaled{ false };

        std::vector<std::shared_ptr<PendingRun>> m_pending{};

        std::thread m_clock{};
        std::atomic<bool> m_running{ false };

        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };
    };
}
