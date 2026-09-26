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

#include "LayoutModel.h"
#include "LfoShape.h"

namespace glass
{
    // The sweeps every running LFO control on a layout is making.
    //
    // Built the same way ClockGenerator is, and for the same reasons: one thread for the whole
    // player rather than one per control, samples placed against a fixed origin so a late
    // wakeup is caught up rather than accumulated, and the value raised on the dispatcher's
    // thread because that is the only thread a connection is ever touched from.
    //
    // A DispatcherQueueTimer would have been shorter and is wrong here. The system timer runs at
    // about fifteen milliseconds unless something has asked for better, so a twenty five
    // millisecond sweep would arrive in lumps and a triangle would come out with a stagger in it.
    class LfoGenerator : public std::enable_shared_from_this<LfoGenerator>
    {
    public:
        static std::shared_ptr<LfoGenerator> Create();

        ~LfoGenerator() noexcept;

        LfoGenerator(LfoGenerator const&) = delete;
        LfoGenerator& operator=(LfoGenerator const&) = delete;

        // Where the sweep is now. Raised on the dispatcher's thread, once per sample while
        // running and once more on stop. `phase` is where in the cycle it is, 0 to 1, for
        // whatever draws the bead; `running` goes false on the last one.
        std::function<void(uint32_t controlIndex, double value, double phase, bool running)> ValueMoved{};

        void Start(_In_ winrt::Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher);

        void Stop() noexcept;

        // Starts this control's sweep from the top of its cycle.
        void Run(
            _In_ uint32_t controlIndex,
            _In_ LfoSpec const& spec,
            _In_ double beatsPerMinute);

        void CancelFor(_In_ uint32_t controlIndex) noexcept;
        void CancelAll() noexcept;

        bool IsRunning(_In_ uint32_t controlIndex) const noexcept;

        // The layout's tempo changed. Takes effect on the next sample and keeps the phase it
        // had reached, so speeding a sweep up does not jump it back to the start.
        void SetTempo(_In_ double beatsPerMinute) noexcept;

        size_t RunningCount() const noexcept;

    private:
        LfoGenerator() = default;

        struct RunningLfo
        {
            uint32_t ControlIndex{ 0 };
            LfoSpec Spec{};

            double BeatsPerMinute{ 120.0 };

            // Where the sweep started and how many samples have gone out since. Everything is
            // placed against these two rather than against the previous sample.
            uint64_t OriginMicroseconds{ 0 };
            uint64_t SamplesSent{ 0 };

            // How far through the cycle the origin already was. Set when the tempo changes, so
            // the wave carries on from where it was instead of restarting.
            double PhaseAtOrigin{ 0.0 };

            LfoNoise Noise{};
        };

        void SweepLoop();
        void Wake() noexcept;

        void RaiseOnDispatcher(_In_ std::function<void(LfoGenerator&)> const& work);

        static uint64_t NowMicroseconds() noexcept;

        // Microseconds between two samples of this sweep.
        static uint64_t IntervalOf(_In_ RunningLfo const& lfo) noexcept;

        static uint64_t DueMicrosecondsOf(_In_ RunningLfo const& lfo) noexcept;

        // Where in the cycle the sample at this index falls, 0 to 1.
        static double PhaseOf(_In_ RunningLfo const& lfo, _In_ uint64_t sampleIndex) noexcept;

        mutable std::mutex m_lock{};
        std::condition_variable m_wakeup{};

        std::vector<RunningLfo> m_sweeps{};

        std::thread m_thread{};
        bool m_stopping{ false };
        bool m_signaled{ false };

        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };
    };
}
