// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "SendQueue.h"

#include <ump_helpers.h>

namespace internal = ::WindowsMidiServicesInternal;

namespace midipatchbay
{
    namespace
    {
        // Taken words are removed from the front in bulk, not on every take
        constexpr size_t CompactAfterWords = 16 * 1024;
        constexpr size_t CompactAfterRuns = 1024;

        // An empty queue lets go of what a large burst made it allocate
        constexpr size_t KeepWordCapacity = 64 * 1024;
        constexpr size_t KeepRunCapacity = 4 * 1024;
    }

    _Use_decl_annotations_
    void SendQueue::Configure(uint32_t const speedMultiple, uint64_t const ticksPerSecond) noexcept
    {
        try
        {
            std::scoped_lock guard{ m_lock };

            m_pacer.Configure(speedMultiple, ticksPerSecond);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    bool SendQueue::Push(
        uint64_t const timestamp,
        uint32_t const* words,
        uint32_t const wordCount) noexcept
    {
        if (words == nullptr || wordCount == 0)
        {
            return true;
        }

        // Whole messages only, so Take never meets part of one
        uint32_t wholeWords{ 0 };
        uint32_t messageCount{ 0 };

        while (wholeWords < wordCount)
        {
            uint32_t const length = internal::GetUmpLengthInMidiWordsFromFirstWord(words[wholeWords]);

            if (length == 0 || wholeWords + length > wordCount)
            {
                break;
            }

            wholeWords += length;
            messageCount++;
        }

        if (wholeWords == 0)
        {
            return true;
        }

        try
        {
            std::scoped_lock guard{ m_lock };

            if ((m_words.size() - m_readIndex) + wholeWords > MaximumQueuedWords)
            {
                return false;
            }

            auto const extendsLastRun = m_runReadIndex < m_runs.size() && m_runs.back().Timestamp == timestamp;

            if (!extendsLastRun)
            {
                m_runs.push_back(Run{ timestamp, 0 });
            }

            try
            {
                m_words.insert(m_words.end(), words, words + wholeWords);
            }
            catch (...)
            {
                if (!extendsLastRun)
                {
                    m_runs.pop_back();
                }

                return false;
            }

            m_runs.back().WordCount += wholeWords;
            m_waitingMessages.fetch_add(messageCount, std::memory_order_relaxed);

            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    uint32_t SendQueue::Take(
        uint64_t const now,
        uint32_t* buffer,
        uint32_t const capacity,
        uint64_t& timestamp,
        uint32_t& messageCount,
        uint64_t& waitTicks) noexcept
    {
        timestamp = 0;
        messageCount = 0;
        waitTicks = 0;

        if (buffer == nullptr || capacity == 0)
        {
            return 0;
        }

        try
        {
            std::scoped_lock guard{ m_lock };

            if (m_runReadIndex >= m_runs.size())
            {
                return 0;
            }

            auto& run = m_runs[m_runReadIndex];
            uint32_t taken{ 0 };

            timestamp = run.Timestamp;

            while (run.WordCount > 0)
            {
                auto const firstWord = m_words[m_readIndex];
                uint32_t const length = internal::GetUmpLengthInMidiWordsFromFirstWord(firstWord);

                if (length == 0 || length > run.WordCount)
                {
                    // Push takes whole messages only, so this is a guard, not a case. Part of
                    // a message can never be sent, and would hold up everything behind it.
                    m_readIndex += run.WordCount;
                    run.WordCount = 0;
                    break;
                }

                if (taken + length > capacity)
                {
                    break;
                }

                // A message at a time, so a batch never goes faster than the speed allows
                auto const wireBytes = internal::EstimateMidi1WireByteCount(firstWord, length);
                auto const allowedIn = m_pacer.TicksUntilAllowed(wireBytes, now);

                if (allowedIn > 0)
                {
                    if (taken == 0)
                    {
                        waitTicks = allowedIn;
                    }

                    break;
                }

                m_pacer.Charge(wireBytes, now);

                std::copy_n(m_words.data() + m_readIndex, length, buffer + taken);

                taken += length;
                m_readIndex += length;
                run.WordCount -= length;
                messageCount++;
            }

            if (run.WordCount == 0)
            {
                m_runReadIndex++;
            }

            m_waitingMessages.fetch_sub(messageCount, std::memory_order_relaxed);

            Compact();

            return taken;
        }
        catch (...)
        {
            messageCount = 0;
            waitTicks = 0;

            return 0;
        }
    }

    void SendQueue::Compact() noexcept
    {
        if (m_runReadIndex >= m_runs.size())
        {
            // Swapped rather than shrunk, because a swap cannot throw
            if (m_words.capacity() > KeepWordCapacity)
            {
                std::vector<uint32_t>{}.swap(m_words);
            }

            if (m_runs.capacity() > KeepRunCapacity)
            {
                std::vector<Run>{}.swap(m_runs);
            }

            m_words.clear();
            m_readIndex = 0;
            m_runs.clear();
            m_runReadIndex = 0;

            // Empty, so nothing is waiting, whatever any earlier count said
            m_waitingMessages.store(0, std::memory_order_relaxed);

            return;
        }

        if (m_readIndex >= CompactAfterWords && m_readIndex * 2 >= m_words.size())
        {
            m_words.erase(m_words.begin(), m_words.begin() + static_cast<std::ptrdiff_t>(m_readIndex));
            m_readIndex = 0;
        }

        if (m_runReadIndex >= CompactAfterRuns && m_runReadIndex * 2 >= m_runs.size())
        {
            m_runs.erase(m_runs.begin(), m_runs.begin() + static_cast<std::ptrdiff_t>(m_runReadIndex));
            m_runReadIndex = 0;
        }
    }
}
