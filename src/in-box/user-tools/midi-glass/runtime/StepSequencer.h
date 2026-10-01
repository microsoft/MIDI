// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <sal.h>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <winrt/Microsoft.UI.Dispatching.h>

#include "LayoutModel.h"

namespace glass
{
    // Keeps time for every step sequencer running on a layout.
    //
    // Built the way LfoGenerator and ClockGenerator are, for the same reasons: one thread for the
    // whole player, every step placed against a fixed origin so a late wakeup is caught up rather
    // than carried forward, and everything raised on the dispatcher's thread, which is the only
    // thread a connection is touched from.
    //
    // It plays nothing itself. It says when a step starts and when that step's note should end,
    // and the player turns those into notes. The player also remembers which note is sounding,
    // on the same thread that sends it, so a stop can always end it: a sequencer that leaves a
    // note hanging when it is stopped is worse than no sequencer.
    class StepSequencer : public std::enable_shared_from_this<StepSequencer>
    {
    public:
        static std::shared_ptr<StepSequencer> Create();

        ~StepSequencer() noexcept;

        StepSequencer(StepSequencer const&) = delete;
        StepSequencer& operator=(StepSequencer const&) = delete;

        // A step started (`starts` true) or the note of the step that started last should end
        // (`starts` false). Raised on the dispatcher's thread. `run` names the run that raised
        // it, because one queued just before a stop can still arrive after it; the caller
        // compares it with the number Run returned to tell the two apart.
        std::function<void(uint32_t controlIndex, uint64_t run, int32_t stepIndex, bool starts)> StepChanged{};

        void Start(_In_ winrt::Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher);

        void Stop() noexcept;

        // Starts this control's sequence from its first step, now. Returns the run's number.
        uint64_t Run(
            _In_ uint32_t controlIndex,
            _In_ StepsSpec const& spec,
            _In_ double beatsPerMinute);

        // Nothing more is raised for this control's run. The note it may have left sounding is
        // the caller's to end, because the caller is the one that knows it.
        void CancelFor(_In_ uint32_t controlIndex) noexcept;
        void CancelAll() noexcept;

    private:
        StepSequencer() = default;

        struct RunningSequence
        {
            uint32_t ControlIndex{ 0 };
            uint64_t Run{ 0 };
            StepsSpec Spec{};

            double StepMicroseconds{ 0.0 };

            // Every step is placed against this, not against the step before it.
            uint64_t OriginMicroseconds{ 0 };

            // How many steps have started since the origin, which is also where the pattern is.
            uint64_t StepsStarted{ 0 };

            // The note of the last step that started, and when it ends.
            bool NoteEndPending{ false };
            uint64_t NoteEndMicroseconds{ 0 };
            int32_t SoundingStep{ -1 };

            uint32_t RandomState{ 1 };
        };

        struct Due
        {
            uint32_t ControlIndex{ 0 };
            uint64_t Run{ 0 };
            int32_t StepIndex{ 0 };
            bool Starts{ false };
        };

        void TimeLoop();
        void Wake() noexcept;

        static uint64_t NowMicroseconds() noexcept;
        static uint64_t NextStepMicrosecondsOf(_In_ RunningSequence const& sequence) noexcept;

        mutable std::mutex m_lock{};
        std::condition_variable m_wakeup{};

        std::vector<RunningSequence> m_sequences{};
        uint64_t m_nextRun{ 1 };

        std::thread m_thread{};
        bool m_stopping{ false };
        bool m_signaled{ false };

        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };
    };
}
