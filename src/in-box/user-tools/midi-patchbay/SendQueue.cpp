// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Pure: no precompiled header, so the unit tests compile this file exactly as it ships.

#include <windows.h>

#include "SendQueue.h"

#include <algorithm>

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

        // A pass reads the whole backlog, so another waits until this many more messages, or a
        // quarter of the backlog, have arrived
        constexpr uint64_t PruneAfterNewMessages = 32;
    }

    _Use_decl_annotations_
    void SendQueue::Configure(uint32_t const speedMultiple, uint64_t const ticksPerSecond, bool const skipInBetweenValues) noexcept
    {
        try
        {
            std::scoped_lock guard{ m_lock };

            m_pacer.Configure(speedMultiple, ticksPerSecond);
            m_pruneThresholdWireBytes = skipInBetweenValues ? internal::MidiSendBacklogPruneThreshold(speedMultiple) : 0;
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
        uint64_t wireBytes{ 0 };

        while (wholeWords < wordCount)
        {
            uint32_t const length = internal::GetUmpLengthInMidiWordsFromFirstWord(words[wholeWords]);

            if (length == 0 || wholeWords + length > wordCount)
            {
                break;
            }

            wireBytes += internal::EstimateMidi1WireByteCount(words[wholeWords], length);
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
            m_waitingWireBytes += wireBytes;
            m_addedSincePrune += messageCount;

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

            PruneIfBehind();

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
                m_waitingWireBytes -= (std::min)(m_waitingWireBytes, static_cast<uint64_t>(wireBytes));

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
                m_pruner.Release();
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
            m_waitingWireBytes = 0;
            m_addedSincePrune = 0;

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

    void SendQueue::PruneIfBehind() noexcept
    {
        if (m_pruneThresholdWireBytes == 0 || m_waitingWireBytes <= m_pruneThresholdWireBytes)
        {
            return;
        }

        if (m_addedSincePrune < (std::max)(PruneAfterNewMessages, m_waitingMessages.load(std::memory_order_relaxed) / 4))
        {
            return;
        }

        m_addedSincePrune = 0;

        auto const keepOneIn = internal::ChooseMidiSendBacklogKeepOneIn(m_waitingWireBytes, m_pruneThresholdWireBytes);

        // every message is at least a word
        if (!m_pruner.Begin(keepOneIn, m_words.size() - m_readIndex))
        {
            return;
        }

        size_t position{ m_readIndex };

        for (size_t run = m_runReadIndex; run < m_runs.size(); run++)
        {
            auto const end = position + m_runs[run].WordCount;

            while (position < end)
            {
                uint32_t const length = internal::GetUmpLengthInMidiWordsFromFirstWord(m_words[position]);

                if (length == 0 || position + length > end)
                {
                    // Push takes whole messages only, so this is a guard, not a case
                    return;
                }

                m_pruner.AddUmp(m_words.data() + position, length);
                position += length;
            }
        }

        if (m_pruner.PrunedCount() == 0)
        {
            return;
        }

        size_t read{ m_readIndex };
        size_t write{ m_readIndex };
        size_t keptRuns{ m_runReadIndex };
        size_t index{ 0 };
        uint64_t removedMessages{ 0 };
        uint64_t removedWireBytes{ 0 };

        for (size_t run = m_runReadIndex; run < m_runs.size(); run++)
        {
            auto const end = read + m_runs[run].WordCount;
            size_t keptWords{ 0 };

            while (read < end)
            {
                uint32_t const length = internal::GetUmpLengthInMidiWordsFromFirstWord(m_words[read]);

                if (m_pruner.IsPruned(index))
                {
                    removedMessages++;
                    removedWireBytes += internal::EstimateMidi1WireByteCount(m_words[read], length);
                }
                else
                {
                    if (write != read)
                    {
                        std::copy_n(m_words.begin() + static_cast<std::ptrdiff_t>(read), length, m_words.begin() + static_cast<std::ptrdiff_t>(write));
                    }

                    write += length;
                    keptWords += length;
                }

                read += length;
                index++;
            }

            // A run that lost every message goes, and the ones after it keep their timestamps
            if (keptWords > 0)
            {
                m_runs[keptRuns] = Run{ m_runs[run].Timestamp, keptWords };
                keptRuns++;
            }
        }

        m_words.erase(m_words.begin() + static_cast<std::ptrdiff_t>(write), m_words.end());
        m_runs.erase(m_runs.begin() + static_cast<std::ptrdiff_t>(keptRuns), m_runs.end());

        m_waitingMessages.fetch_sub(removedMessages, std::memory_order_relaxed);
        m_waitingWireBytes -= (std::min)(m_waitingWireBytes, removedWireBytes);
        m_prunedMessages.fetch_add(removedMessages, std::memory_order_relaxed);
    }
}
