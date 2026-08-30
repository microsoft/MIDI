// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <functional>
#include <memory>
#include <utility>

class ThreadpoolWork
{
public:
    ThreadpoolWork()
    {
        m_TpWork = CreateThreadpoolWork(&Callback, this, nullptr);
    }

    ~ThreadpoolWork()
    {
        PTP_WORK work = nullptr;

        {
            // prevent any additional work from being added
            auto lock = m_Lock.lock();
            work = m_TpWork;
            m_TpWork = nullptr;
        }

        if (work)
        {
            WaitForThreadpoolWorkCallbacks(work, FALSE);
            CloseThreadpoolWork(work);
        }

        // We're now single threaded, finish any outstanding
        // callbacks
        for (auto& f : m_CallbackLambdas)
        {
            if (f)
            {
                f();
            }
        }

        m_CallbackLambdas.clear();
    }

    void Submit(std::function<void()> fn)
    {
        auto lock = m_Lock.lock();
        if (m_TpWork)
        {
            m_CallbackLambdas.emplace_back(std::move(fn));
            SubmitThreadpoolWork(m_TpWork);
        }
    }

private:
    static void CALLBACK Callback(PTP_CALLBACK_INSTANCE, void* ctx, PTP_WORK)\
    {
        auto This = static_cast<ThreadpoolWork*>(ctx);
        std::vector<std::function<void()>> to_run;

        // Atomically take all queued tasks
        {
            auto lock = This->m_Lock.lock();
            to_run.swap(This->m_CallbackLambdas); // fn_ becomes empty while we run what's captured
        }

        // Run outside the lock
        for (auto& f : to_run) {
            if (f) f();
        }
    }

    PTP_WORK m_TpWork {nullptr};
    wil::critical_section m_Lock;
    std::vector<std::function<void()>> m_CallbackLambdas;
};

