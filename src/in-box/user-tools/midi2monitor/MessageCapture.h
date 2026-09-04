// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midi2monitor
{
    struct CapturedMessage
    {
        uint64_t Timestamp{ 0 };
        std::array<uint32_t, 4> Words{};
        uint8_t WordCount{ 0 };
        uint32_t BatchId{ 0 };
    };

    // Receives UMPs through the Windows MIDI Services COM extensions. The callback runs on a
    // service worker thread, so it does the smallest possible amount of work: split the
    // incoming buffer into individual UMPs, apply the group and channel filters, and stage the
    // results. MessagePipeline's own worker thread drains the staging buffer; the UI thread is
    // never involved in capture.
    struct MessageCapture : winrt::implements<MessageCapture, IMidiEndpointConnectionMessagesReceivedCallback>
    {
        STDMETHOD(MessagesReceived)(
            GUID sessionId,
            GUID connectionId,
            UINT64 timestamp,
            UINT32 wordCount,
            UINT32 const* messages) override;

        // group and channel are user-facing numbers (1-16). Zero means "all".
        void SetFilter(uint8_t groupNumber, uint8_t channelNumber) noexcept;

        void Enabled(bool value) noexcept { m_enabled.store(value, std::memory_order_relaxed); }
        bool Enabled() const noexcept { return m_enabled.load(std::memory_order_relaxed); }

        void Drain(std::vector<CapturedMessage>& target) noexcept;

        uint64_t TakeDroppedCount() noexcept { return m_droppedCount.exchange(0, std::memory_order_relaxed); }

        static constexpr size_t MaximumStagedMessages = 200000;

    private:
        std::mutex m_stagingLock{};
        std::vector<CapturedMessage> m_staged{};

        std::atomic<bool> m_enabled{ false };
        std::atomic<uint8_t> m_groupFilter{ 0 };
        std::atomic<uint8_t> m_channelFilter{ 0 };
        std::atomic<uint32_t> m_batchId{ 0 };
        std::atomic<uint64_t> m_droppedCount{ 0 };
    };
}
