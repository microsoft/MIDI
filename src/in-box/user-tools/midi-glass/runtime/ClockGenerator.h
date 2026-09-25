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

#include <winrt/Microsoft.UI.Dispatching.h>

namespace glass
{
    // The range a generated clock is held to. The same numbers the document model bounds a
    // typed tempo with, repeated here so this class needs nothing from the document layer.
    constexpr double MinimumBeatsPerMinuteForClock = 20.0;
    constexpr double MaximumBeatsPerMinuteForClock = 300.0;

    // MIDI clock at twenty four ticks a quarter note, and the beat the surface draws while it
    // runs.
    //
    // One thread for the whole player rather than one per clock control, the same reasoning the
    // sequence runner uses: several clock controls on a page is a reasonable layout and several
    // threads each waking every few milliseconds during a set is not.
    //
    // !! Timing is the whole point of this class. !! Ticks are placed against a fixed start
    // point rather than by adding an interval to "now" each time, so a late wakeup is caught up
    // on the next tick instead of accumulating. At 120 bpm a tick is 20.8 ms, and a millisecond
    // of drift per tick is a bar and a half out by the end of a three minute song.
    //
    // Sending happens on the dispatcher's thread, because that is the only thread the send table
    // is read from. This thread never touches a connection.
    class ClockGenerator : public std::enable_shared_from_this<ClockGenerator>
    {
    public:
        static std::shared_ptr<ClockGenerator> Create();

        ~ClockGenerator() noexcept;

        ClockGenerator(ClockGenerator const&) = delete;
        ClockGenerator& operator=(ClockGenerator const&) = delete;

        // ---- what the owner supplies. Both called on the dispatcher's thread. ----

        // One system real time byte to everything this control names.
        std::function<void(uint32_t controlIndex, uint8_t status)> SendRealTime{};

        // Where the beat is now, for whatever draws it. Raised once per tick while running and
        // once on stop.
        std::function<void(uint32_t controlIndex, int32_t beatInBar, double phase, bool running)> BeatMoved{};

        void Start(_In_ winrt::Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher);

        void Stop() noexcept;

        // Starts this control's clock, from the top of a bar. Sends start before the first tick
        // when the control asked for transport.
        void Run(
            _In_ uint32_t controlIndex,
            _In_ double beatsPerMinute,
            _In_ bool sendsTransport);

        void CancelFor(_In_ uint32_t controlIndex) noexcept;

        void CancelAll() noexcept;

        bool IsRunning(_In_ uint32_t controlIndex) const noexcept;

        // A tempo knob moved. Takes effect on the next tick and does not restart the bar, so
        // riding the tempo during a set does not throw the downbeat away.
        void SetTempo(_In_ uint32_t controlIndex, _In_ double beatsPerMinute) noexcept;

        // What this control is running at, or zero when it is stopped. What a lamp following
        // the beat reads, and what the editor shows.
        double TempoOf(_In_ uint32_t controlIndex) const noexcept;

        size_t RunningCount() const noexcept;

    private:
        ClockGenerator() = default;

        // Twenty four clock messages to the quarter note, which is what MIDI has always been.
        static constexpr int32_t TicksPerQuarterNote = 24;
        static constexpr int32_t QuarterNotesPerBar = 4;

        static constexpr uint8_t StatusTimingClock = 0xF8;
        static constexpr uint8_t StatusStart = 0xFA;
        static constexpr uint8_t StatusStop = 0xFC;

        struct RunningClock
        {
            uint32_t ControlIndex{ 0 };
            double BeatsPerMinute{ 120.0 };
            bool SendsTransport{ true };

            // Where the bar started, and how many ticks have gone out since. Everything is
            // placed against these two rather than against the previous tick, which is what
            // stops a late wakeup turning into drift.
            uint64_t OriginMicroseconds{ 0 };
            uint64_t TicksSent{ 0 };
        };

        void ClockLoop();
        void Wake() noexcept;

        // Runs the work on the dispatcher, or right here when this already is that thread.
        void RaiseOnDispatcher(_In_ std::function<void(ClockGenerator&)> const& work);

        // Microseconds since the process started, from the steady clock. Not the wall clock:
        // a time change mid set must not move the beat.
        static uint64_t NowMicroseconds() noexcept;

        uint64_t DueMicrosecondsOf(_In_ RunningClock const& clock) const noexcept;

        mutable std::mutex m_lock{};
        std::condition_variable m_wakeup{};

        std::vector<RunningClock> m_clocks{};

        std::thread m_thread{};
        bool m_stopping{ false };

        // A notify that lands between the due check and the wait is otherwise dropped, and the
        // clock stalls for the whole idle wait. Same lost wakeup the sequence runner had.
        bool m_signaled{ false };

        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };
    };
}
