// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Pure: the unit tests compile files that use this, so it includes what it needs itself.

#include <sal.h>

#include <atomic>
#include <thread>

namespace midipatchbay
{
    // Held for one message at most, by the threads that MIDI arrives on.
    class SpinGuard
    {
    public:
        explicit SpinGuard(_Inout_ std::atomic_flag& flag) noexcept :
            m_flag(flag)
        {
            while (m_flag.test_and_set(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
        }

        ~SpinGuard() noexcept
        {
            m_flag.clear(std::memory_order_release);
        }

        SpinGuard(SpinGuard const&) = delete;
        SpinGuard& operator=(SpinGuard const&) = delete;

    private:
        std::atomic_flag& m_flag;
    };
}
