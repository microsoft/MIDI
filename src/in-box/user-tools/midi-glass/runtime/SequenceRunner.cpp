// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "SequenceRunner.h"

#include <algorithm>

namespace glass
{
    namespace
    {
        uint64_t NowMilliseconds() noexcept
        {
            return static_cast<uint64_t>(::GetTickCount64());
        }

        // Long enough that an idle runner costs nothing, short enough that Stop is never left
        // waiting on a thread that is asleep for a minute.
        constexpr uint32_t IdleWaitMilliseconds = 1000;
    }

    std::shared_ptr<SequenceRunner> SequenceRunner::Create()
    {
        return std::shared_ptr<SequenceRunner>(new SequenceRunner());
    }

    SequenceRunner::~SequenceRunner() noexcept
    {
        Stop();
    }

    _Use_decl_annotations_
    void SequenceRunner::Start(winrt::Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher)
    {
        if (m_running.exchange(true))
        {
            return;
        }

        m_dispatcher = dispatcher;

        // A joined thread rather than a detached one: the runner owns the clock, so a plan can
        // never still be firing into a window that has gone.
        m_clock = std::thread([weak = std::weak_ptr<SequenceRunner>{ shared_from_this() }]()
            {
                if (auto strong = weak.lock())
                {
                    strong->ClockLoop();
                }
            });
    }

    void SequenceRunner::Stop() noexcept
    {
        if (!m_running.exchange(false))
        {
            return;
        }

        try
        {
            {
                std::lock_guard guard{ m_lock };
                m_pending.clear();
                m_signaled = true;
            }

            m_wake.notify_all();

            if (m_clock.joinable())
            {
                m_clock.join();
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void SequenceRunner::Run(uint32_t controlIndex, ActionPlan const& plan)
    {
        if (plan.Actions.empty())
        {
            return;
        }

        try
        {
            auto run = std::make_shared<PendingRun>();

            run->ControlIndex = controlIndex;
            run->Actions = plan.Actions;
            run->Loops = plan.Loops;

            {
                std::lock_guard guard{ m_lock };

                // Pressing a button twice means start again, not run two copies side by side.
                m_pending.erase(
                    std::remove_if(
                        m_pending.begin(),
                        m_pending.end(),
                        [controlIndex](std::shared_ptr<PendingRun> const& existing)
                        { return existing->ControlIndex == controlIndex; }),
                    m_pending.end());
            }

            // Everything before the first wait goes out right here, on the pointer handler's
            // thread, so a button carrying one dump is as immediate as one carrying one note.
            if (Advance(*run))
            {
                return;
            }

            {
                std::lock_guard guard{ m_lock };
                m_pending.push_back(std::move(run));
                m_signaled = true;
            }

            m_wake.notify_all();
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void SequenceRunner::CancelFor(uint32_t controlIndex) noexcept
    {
        try
        {
            std::lock_guard guard{ m_lock };

            m_pending.erase(
                std::remove_if(
                    m_pending.begin(),
                    m_pending.end(),
                    [controlIndex](std::shared_ptr<PendingRun> const& existing)
                    { return existing->ControlIndex == controlIndex; }),
                m_pending.end());
        }
        catch (...)
        {
        }
    }

    void SequenceRunner::CancelAll() noexcept
    {
        try
        {
            std::lock_guard guard{ m_lock };
            m_pending.clear();
        }
        catch (...)
        {
        }
    }

    size_t SequenceRunner::PendingCount() const noexcept
    {
        try
        {
            std::lock_guard guard{ m_lock };
            return m_pending.size();
        }
        catch (...)
        {
            return 0;
        }
    }

    _Use_decl_annotations_
    bool SequenceRunner::IsRunning(uint32_t controlIndex) const noexcept
    {
        try
        {
            std::lock_guard guard{ m_lock };

            for (auto const& run : m_pending)
            {
                if (run->ControlIndex == controlIndex)
                {
                    return true;
                }
            }

            return false;
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    bool SequenceRunner::Advance(PendingRun& run)
    {
        while (run.NextAction < run.Actions.size())
        {
            auto const& action = run.Actions[run.NextAction];

            if (action.Kind == ActionKind::Wait)
            {
                ++run.NextAction;

                if (action.WaitMilliseconds != 0)
                {
                    run.DueMilliseconds = NowMilliseconds() + action.WaitMilliseconds;
                    return false;
                }

                continue;
            }

            try
            {
                switch (action.Kind)
                {
                case ActionKind::Send:
                    if (Send && action.DestinationIndex >= 0 && !action.Words.empty())
                    {
                        Send(
                            run.ControlIndex,
                            action.DestinationIndex,
                            action.Words.data(),
                            static_cast<uint32_t>(action.Words.size()));
                    }
                    break;

                case ActionKind::SetControlValue:
                    if (SetControlValue && action.TargetControlIndex >= 0)
                    {
                        SetControlValue(static_cast<uint32_t>(action.TargetControlIndex), action.TargetValue);
                    }
                    break;

                case ActionKind::GoToPage:
                    if (GoToPage && action.TargetPageIndex >= 0)
                    {
                        GoToPage(static_cast<uint32_t>(action.TargetPageIndex));
                    }
                    break;

                default:
                    break;
                }
            }
            catch (...)
            {
            }

            ++run.NextAction;
        }

        if (!run.Loops)
        {
            return true;
        }

        // Round again. The floor is what stops a plan with no waits in it spinning the clock
        // thread and the dispatcher at whatever rate the machine can manage.
        run.NextAction = 0;
        run.DueMilliseconds = NowMilliseconds() + ActionPlanSet::MinimumLoopMilliseconds;

        return false;
    }

    void SequenceRunner::ClockLoop()
    {
        while (m_running)
        {
            std::vector<std::shared_ptr<PendingRun>> due{};

            {
                std::unique_lock guard{ m_lock };

                auto const now = NowMilliseconds();

                uint64_t soonest{ now + IdleWaitMilliseconds };

                for (auto const& run : m_pending)
                {
                    // A run already handed to the dispatcher is not due again until it comes
                    // back. Without this the clock re-posts it on every pass while the UI thread
                    // is busy, and one slow frame becomes a thousand work items.
                    if (run->Dispatched)
                    {
                        continue;
                    }

                    if (run->DueMilliseconds <= now)
                    {
                        run->Dispatched = true;
                        due.push_back(run);
                    }
                    else
                    {
                        soonest = (std::min)(soonest, run->DueMilliseconds);
                    }
                }

                if (due.empty())
                {
                    auto const waitFor = soonest <= now
                        ? uint64_t{ 1 }
                        : (std::min)(soonest - now, uint64_t{ IdleWaitMilliseconds });

                    // The predicate is what makes this safe. Without it, a run coming back from
                    // the dispatcher between the check above and this call is a lost wakeup, and
                    // the next step of the sequence waits a whole second for nothing.
                    m_wake.wait_for(
                        guard,
                        std::chrono::milliseconds{ waitFor },
                        [this]() { return m_signaled || !m_running; });

                    m_signaled = false;
                }
            }

            if (!m_running || due.empty() || m_dispatcher == nullptr)
            {
                continue;
            }

            for (auto const& run : due)
            {
                std::weak_ptr<SequenceRunner> weak{ weak_from_this() };

                // Sending happens on the dispatcher's thread, because that is the only thread
                // the send table is read from. The clock thread never touches a connection.
                auto const queued = m_dispatcher.TryEnqueue([weak, run]()
                    {
                        auto strong = weak.lock();

                        if (strong == nullptr)
                        {
                            return;
                        }

                        {
                            // A cancel between the post and here means this run is gone, and
                            // nothing more of it may reach the wire.
                            std::lock_guard guard{ strong->m_lock };

                            if (std::find(strong->m_pending.begin(), strong->m_pending.end(), run) ==
                                strong->m_pending.end())
                            {
                                return;
                            }
                        }

                        auto const finished = strong->Advance(*run);

                        std::lock_guard guard{ strong->m_lock };

                        auto const found = std::find(
                            strong->m_pending.begin(), strong->m_pending.end(), run);

                        if (found == strong->m_pending.end())
                        {
                            return;
                        }

                        if (finished)
                        {
                            strong->m_pending.erase(found);
                        }
                        else
                        {
                            run->Dispatched = false;
                        }

                        strong->m_signaled = true;
                        strong->m_wake.notify_all();
                    });

                if (!queued)
                {
                    std::lock_guard guard{ m_lock };
                    run->Dispatched = false;
                }
            }
        }
    }
}
