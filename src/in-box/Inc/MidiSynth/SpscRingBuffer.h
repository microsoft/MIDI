// Single producer, single consumer ring buffer. Used to hand MIDI events from a callback thread
// to the audio render thread without a lock, since the render thread must never block.

#pragma once

#include <sal.h>

#include <array>
#include <atomic>
#include <cstddef>

namespace MidiSynth
{
    template <typename T, size_t Capacity>
    class SpscRingBuffer
    {
        static_assert(Capacity > 1, "capacity must leave room for the empty and full distinction");

    public:
        // Called only by the producer thread.
        bool TryPush(_In_ const T& item) noexcept
        {
            const size_t write = m_write.load(std::memory_order_relaxed);
            const size_t next = (write + 1) % Capacity;

            if (next == m_read.load(std::memory_order_acquire))
            {
                return false;
            }

            m_items[write] = item;
            m_write.store(next, std::memory_order_release);
            return true;
        }

        // Called only by the consumer thread.
        bool TryPop(_Out_ T& item) noexcept
        {
            const size_t read = m_read.load(std::memory_order_relaxed);

            if (read == m_write.load(std::memory_order_acquire))
            {
                return false;
            }

            item = m_items[read];
            m_read.store((read + 1) % Capacity, std::memory_order_release);
            return true;
        }

    private:
        std::array<T, Capacity> m_items{};
        std::atomic<size_t> m_write{ 0 };
        std::atomic<size_t> m_read{ 0 };
    };
}
