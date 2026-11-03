// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Frames drawn at an even pace while something plays, shared by Windows MIDI Sequencer and the
// MIDI File Player (#1324). A DispatcherTimer fires on the
// system timer's tick, so on a PC left at 15.6 ms a 30 ms timer fires at 31 and 47 ms by turns
// and the playhead moves in uneven steps. This waits on a high-resolution waitable timer on its
// own thread, which wakes within about a millisecond whatever the system timer's period is, and
// asks the UI thread for a frame. It never changes the system's timer period.
//
// At most one frame waits on the UI thread at a time, so a busy UI thread skips frames rather
// than falling further behind.

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>

#include <winrt/Microsoft.UI.Dispatching.h>

namespace midiapp
{
    class FrameTicker
    {
    public:
        // tick runs on the queue's thread. It's called only while the ticker runs.
        FrameTicker(
            _In_ winrt::Microsoft::UI::Dispatching::DispatcherQueue queue,
            _In_ std::chrono::microseconds interval,
            _In_ std::function<void()> tick);

        ~FrameTicker();

        FrameTicker(FrameTicker const&) = delete;
        FrameTicker& operator=(FrameTicker const&) = delete;

        void Start();
        void Stop() noexcept;
        bool IsRunning() const noexcept;

    private:
        // Shared with frames already queued, which may run after Stop or after the ticker is gone.
        struct State
        {
            std::atomic<bool> Running{ false };
            std::atomic<bool> FramePending{ false };
            std::function<void()> Tick{};
        };

        void ThreadMain() noexcept;

        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_queue{ nullptr };
        std::chrono::microseconds m_interval{};
        std::shared_ptr<State> m_state{};
        std::thread m_thread{};
    };
}
