// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "MessageWatch.h"

#include <algorithm>
#include <cstdio>

namespace midipatchbay
{
    namespace
    {
        static_assert((WatchBufferCapacity & (WatchBufferCapacity - 1)) == 0, "The capacity must be a power of two.");

        constexpr uint64_t SlotMask = WatchBufferCapacity - 1;

        // Message types, from the top four bits of the first word.
        constexpr uint32_t UtilityType = 0x0;
        constexpr uint32_t SystemType = 0x1;

        constexpr uint32_t TimingClockStatus = 0xF8;
        constexpr uint32_t ActiveSensingStatus = 0xFE;

        constexpr uint32_t JitterReductionClockStatus = 0x1;
        constexpr uint32_t JitterReductionTimestampStatus = 0x2;
    }

    WatchBuffer::WatchBuffer() :
        m_slots(std::make_unique<Slot[]>(WatchBufferCapacity))
    {
        for (uint64_t i = 0; i < WatchBufferCapacity; i++)
        {
            m_slots[i].Sequence.store(i, std::memory_order_relaxed);
        }
    }

    uint32_t WatchBuffer::Lease() noexcept
    {
        m_dropped.store(0, std::memory_order_relaxed);

        auto next = m_lease.load(std::memory_order_relaxed) + 1;

        // 0 is never a lease, so a target that was never given one matches nothing.
        if (next == 0)
        {
            next = 1;
        }

        m_lease.store(next, std::memory_order_release);

        return next;
    }

    uint32_t WatchBuffer::CurrentLease() const noexcept
    {
        return m_lease.load(std::memory_order_acquire);
    }

    _Use_decl_annotations_
    bool WatchBuffer::Push(
        uint32_t lease,
        uint16_t link,
        uint64_t timestamp,
        uint32_t const* words,
        uint8_t wordCount) noexcept
    {
        if (words == nullptr || wordCount == 0)
        {
            return false;
        }

        // Each thread claims a slot of its own, so several can put in at once. Nothing here
        // waits on another thread: a slot the window hasn't emptied yet means the buffer is full.
        auto position = m_putPosition.load(std::memory_order_relaxed);
        Slot* slot{ nullptr };

        for (;;)
        {
            slot = &m_slots[position & SlotMask];

            auto const sequence = slot->Sequence.load(std::memory_order_acquire);
            auto const difference = static_cast<int64_t>(sequence - position);

            if (difference == 0)
            {
                if (m_putPosition.compare_exchange_weak(position, position + 1, std::memory_order_relaxed))
                {
                    break;
                }
            }
            else if (difference < 0)
            {
                if (lease == m_lease.load(std::memory_order_relaxed))
                {
                    m_dropped.fetch_add(1, std::memory_order_relaxed);
                }

                return false;
            }
            else
            {
                position = m_putPosition.load(std::memory_order_relaxed);
            }
        }

        auto const count = (std::min)(wordCount, static_cast<uint8_t>(slot->Message.Words.size()));

        slot->Lease = lease;
        slot->Message.Timestamp = timestamp;
        slot->Message.WordCount = count;
        slot->Message.Link = link;
        slot->Message.FromTest = false;
        slot->Message.Words = {};

        std::copy_n(words, count, slot->Message.Words.begin());

        slot->Sequence.store(position + 1, std::memory_order_release);

        return true;
    }

    _Use_decl_annotations_
    size_t WatchBuffer::Drain(WatchedMessage* messages, size_t capacity) noexcept
    {
        if (messages == nullptr)
        {
            return 0;
        }

        auto const lease = m_lease.load(std::memory_order_acquire);

        size_t taken{ 0 };

        // Bounded, so a buffer full of an earlier window's messages can't hold this thread.
        for (uint64_t looked = 0; taken < capacity && looked < WatchBufferCapacity; looked++)
        {
            auto& slot = m_slots[m_takePosition & SlotMask];

            auto const sequence = slot.Sequence.load(std::memory_order_acquire);

            // Empty, or a thread is still writing it. Either way, the rest waits for next time.
            if (static_cast<int64_t>(sequence - (m_takePosition + 1)) < 0)
            {
                break;
            }

            if (slot.Lease == lease)
            {
                messages[taken++] = slot.Message;
            }

            slot.Sequence.store(m_takePosition + WatchBufferCapacity, std::memory_order_release);
            m_takePosition++;
        }

        return taken;
    }

    uint64_t WatchBuffer::Dropped() const noexcept
    {
        return m_dropped.load(std::memory_order_relaxed);
    }

    _Use_decl_annotations_
    bool WatchFan::Add(WatchTarget const& target) noexcept
    {
        if (Count >= Targets.size() || target.Buffer == nullptr)
        {
            return false;
        }

        Targets[Count++] = target;
        return true;
    }

    _Use_decl_annotations_
    void WatchFan::Deliver(uint64_t timestamp, uint32_t const* words, uint8_t wordCount) const noexcept
    {
        for (size_t i = 0; i < Count && i < Targets.size(); i++)
        {
            auto const& target = Targets[i];

            if (target.Buffer != nullptr)
            {
                target.Buffer->Push(target.Lease, target.Link, timestamp, words, wordCount);
            }
        }
    }

    _Use_decl_annotations_
    WatchedKind ClassifyWatched(uint32_t word0) noexcept
    {
        auto const type = word0 >> 28;

        if (type == SystemType)
        {
            auto const status = (word0 >> 16) & 0xFF;

            if (status == TimingClockStatus)
            {
                return WatchedKind::Clock;
            }

            if (status == ActiveSensingStatus)
            {
                return WatchedKind::ActiveSensing;
            }
        }
        else if (type == UtilityType)
        {
            auto const status = (word0 >> 20) & 0x0F;

            if (status == JitterReductionClockStatus || status == JitterReductionTimestampStatus)
            {
                return WatchedKind::Clock;
            }
        }

        return WatchedKind::Other;
    }

    _Use_decl_annotations_
    std::wstring FormatWatchedWords(std::array<uint32_t, 4> const& words, uint8_t wordCount)
    {
        std::wstring text{};

        auto const count = (std::min)(static_cast<size_t>(wordCount), words.size());

        for (size_t i = 0; i < count; i++)
        {
            wchar_t word[9]{};
            swprintf_s(word, L"%08X", words[i]);

            if (!text.empty())
            {
                text += L' ';
            }

            text += word;
        }

        return text;
    }

    _Use_decl_annotations_
    double WatchMilliseconds(uint64_t timestamp, uint64_t origin, uint64_t ticksPerSecond) noexcept
    {
        if (ticksPerSecond == 0)
        {
            return 0.0;
        }

        auto const milliseconds = [ticksPerSecond](uint64_t ticks)
            {
                return static_cast<double>(ticks / ticksPerSecond) * 1000.0 +
                    static_cast<double>(ticks % ticksPerSecond) * 1000.0 / static_cast<double>(ticksPerSecond);
            };

        return timestamp >= origin
            ? milliseconds(timestamp - origin)
            : -milliseconds(origin - timestamp);
    }

    _Use_decl_annotations_
    int64_t WatchMicroseconds(uint64_t timestamp, uint64_t origin, uint64_t ticksPerSecond) noexcept
    {
        if (ticksPerSecond == 0)
        {
            return 0;
        }

        constexpr uint64_t PerSecond = 1'000'000;

        auto const ticks = timestamp >= origin ? timestamp - origin : origin - timestamp;

        // Whole seconds and the rest apart, so a long session never overflows.
        auto const microseconds = (ticks / ticksPerSecond) * PerSecond +
            ((ticks % ticksPerSecond) * PerSecond + ticksPerSecond / 2) / ticksPerSecond;

        return timestamp >= origin
            ? static_cast<int64_t>(microseconds)
            : -static_cast<int64_t>(microseconds);
    }

    _Use_decl_annotations_
    WatchHistory::WatchHistory(size_t limit) noexcept :
        m_limit((std::max)(limit, size_t{ 1 }))
    {
    }

    _Use_decl_annotations_
    size_t WatchHistory::Add(WatchedMessage const& message)
    {
        m_rows.push_back(WatchRow{ m_nextSequence++, message });

        if (m_rows.size() > m_limit)
        {
            m_rows.pop_front();
            return 1;
        }

        return 0;
    }

    _Use_decl_annotations_
    WatchRow const* WatchHistory::Find(uint64_t sequence) const noexcept
    {
        if (m_rows.empty() || sequence < m_rows.front().Sequence)
        {
            return nullptr;
        }

        auto const index = sequence - m_rows.front().Sequence;

        return index < m_rows.size() ? &m_rows[static_cast<size_t>(index)] : nullptr;
    }

    void WatchHistory::Clear() noexcept
    {
        m_rows.clear();
    }
}
