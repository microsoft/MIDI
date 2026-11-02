// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "FrameTicker.h"

namespace midiapp
{
    _Use_decl_annotations_
    FrameTicker::FrameTicker(
        winrt::Microsoft::UI::Dispatching::DispatcherQueue queue,
        std::chrono::microseconds interval,
        std::function<void()> tick) :
        m_queue(std::move(queue)),
        m_interval(interval),
        m_state(std::make_shared<State>())
    {
        m_state->Tick = std::move(tick);
    }

    FrameTicker::~FrameTicker()
    {
        Stop();
    }

    bool FrameTicker::IsRunning() const noexcept
    {
        return m_state->Running.load();
    }

    void FrameTicker::Start()
    {
        if (m_state->Running.exchange(true))
        {
            return;
        }

        // Stop always waits for the thread, so none is left from an earlier run.
        m_state->FramePending = false;

        try
        {
            m_thread = std::thread([this]() { ThreadMain(); });
        }
        catch (...)
        {
            m_state->Running = false;
            throw;
        }
    }

    void FrameTicker::Stop() noexcept
    {
        m_state->Running = false;

        try
        {
            // It wakes within one interval and sees Running is false.
            if (m_thread.joinable() && m_thread.get_id() != std::this_thread::get_id())
            {
                m_thread.join();
            }
        }
        catch (...)
        {
        }
    }

    void FrameTicker::ThreadMain() noexcept
    {
        auto const timer = ::CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        auto const state = m_state;
        auto const queue = m_queue;
        auto const interval = m_interval;

        while (state->Running.load())
        {
            if (timer != nullptr)
            {
                LARGE_INTEGER due{};
                due.QuadPart = -static_cast<LONGLONG>(interval.count()) * 10;
                ::SetWaitableTimerEx(timer, &due, 0, nullptr, nullptr, nullptr, 0);
                ::WaitForSingleObject(timer, 1000);
            }
            else
            {
                ::Sleep(static_cast<DWORD>(std::max<int64_t>(1, interval.count() / 1000)));
            }

            if (!state->Running.load() || state->FramePending.exchange(true))
            {
                continue;
            }

            try
            {
                auto const queued = queue.TryEnqueue([state]()
                {
                    state->FramePending = false;

                    if (state->Running.load() && state->Tick)
                    {
                        try
                        {
                            state->Tick();
                        }
                        catch (...)
                        {
                        }
                    }
                });

                if (!queued)
                {
                    state->FramePending = false;
                }
            }
            catch (...)
            {
                state->FramePending = false;
            }
        }

        if (timer != nullptr)
        {
            ::CloseHandle(timer);
        }
    }
}
