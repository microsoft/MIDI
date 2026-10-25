// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#ifndef MIDI_BLE_WORKER_WAKEUP_H
#define MIDI_BLE_WORKER_WAKEUP_H

#include <windows.h>
#include <wil/resource.h>

// Wakes a worker thread which sleeps on an event whenever its queue is empty.
//
// The worker looks for work and resets the event while holding its queue lock, and a producer
// queues under that same lock, so new work cannot be missed. A stop request is a flag rather
// than something in the queue. Raised outside the lock, it could land after the worker last
// looked at the flag but before it reset the event, and the worker would then sleep through it.
// The thread join in shutdown would never return. So the stop is signaled under the queue lock,
// and the worker checks for it under that lock before resetting the event.
//
// No Bluetooth, service or COM dependency, so it is unit tested on its own.
class MidiBleWorkerWakeup
{
public:
    // After queuing work. The queue lock may or may not be held.
    void Signal() noexcept
    {
        m_event.SetEvent();
    }

    // For a stop request, after the stop flag is set. Takes the queue lock, so it must never be
    // called while that lock is held.
    void SignalStop(_In_ wil::srwlock& queueLock) noexcept
    {
        auto lock = queueLock.lock_exclusive();

        m_event.SetEvent();
    }

    // Called with the queue lock held, when the worker found nothing to do. Returns false when the
    // worker has to stop instead of sleeping, and leaves the event alone in that case.
    bool PrepareToSleep(_In_ bool const stopRequested) noexcept
    {
        if (stopRequested)
        {
            return false;
        }

        m_event.ResetEvent();

        return true;
    }

    // Called after the queue lock is released. True when signaled, false on a timeout.
    bool Sleep(_In_ DWORD const timeoutMilliseconds) noexcept
    {
        return m_event.wait(timeoutMilliseconds);
    }

    bool IsSignaled() const noexcept
    {
        return m_event.is_signaled();
    }

private:
    wil::slim_event_manual_reset m_event;
};

#endif
