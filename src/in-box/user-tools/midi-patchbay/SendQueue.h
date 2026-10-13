// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <sal.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

#include <midi_send_pacer.h>
#include <midi_send_backlog_pruner.h>

namespace midipatchbay
{
    // What one connection has to send, held until it may go: the connection has a sending speed,
    // or its patch waits for each send to complete. The receive callback must never wait for
    // either, so it adds messages here and the destination's send thread takes them. When the
    // sending speed leaves it far behind, the steps in the middle of controller sweeps are
    // thinned out so it catches up.
    class SendQueue
    {
    public:
        // Minutes of data at MIDI 1.0 wire speed. Past this, new messages are dropped rather than
        // letting a source that never stops use up memory.
        static constexpr size_t MaximumQueuedWords = 1024 * 1024;

        // A multiple of MIDI 1.0 wire speed, 0 for no limit. Called before anything is added.
        void Configure(_In_ uint32_t const speedMultiple, _In_ uint64_t const ticksPerSecond, _In_ bool const skipInBetweenValues) noexcept;

        // Receive thread. A partial message at the end is left out. False when there is no room,
        // and nothing was added.
        bool Push(
            _In_ uint64_t const timestamp,
            _In_reads_(wordCount) uint32_t const* words,
            _In_ uint32_t const wordCount) noexcept;

        // Send thread. Copies out the messages that may go now, all with the same timestamp, and
        // returns how many words that is. When it returns zero, waitTicks is how long until the
        // speed lets the next message go, or zero when nothing is waiting.
        uint32_t Take(
            _In_ uint64_t const now,
            _Out_writes_to_(capacity, return) uint32_t* buffer,
            _In_ uint32_t const capacity,
            _Out_ uint64_t& timestamp,
            _Out_ uint32_t& messageCount,
            _Out_ uint64_t& waitTicks) noexcept;

        uint64_t WaitingMessageCount() const noexcept
        {
            return m_waitingMessages.load(std::memory_order_relaxed);
        }

        // In-between values taken out to catch up
        uint64_t PrunedMessageCount() const noexcept
        {
            return m_prunedMessages.load(std::memory_order_relaxed);
        }

    private:
        // Messages that arrived together, and so share a timestamp
        struct Run
        {
            uint64_t Timestamp{ 0 };
            size_t WordCount{ 0 };
        };

        // Needs m_lock
        void Compact() noexcept;

        // Needs m_lock
        void PruneIfBehind() noexcept;

        std::mutex m_lock{};

        std::vector<uint32_t> m_words{};
        size_t m_readIndex{ 0 };

        std::vector<Run> m_runs{};
        size_t m_runReadIndex{ 0 };

        std::atomic<uint64_t> m_waitingMessages{ 0 };

        ::WindowsMidiServicesInternal::MidiSendPacer m_pacer{};

        // What the waiting messages take on a MIDI 1.0 cable, what may wait before the queue is
        // thinned (0 for never), and how many messages arrived since it last was
        uint64_t m_waitingWireBytes{ 0 };
        uint64_t m_pruneThresholdWireBytes{ 0 };
        uint64_t m_addedSincePrune{ 0 };

        ::WindowsMidiServicesInternal::MidiSendBacklogPruner m_pruner{};
        std::atomic<uint64_t> m_prunedMessages{ 0 };
    };
}
