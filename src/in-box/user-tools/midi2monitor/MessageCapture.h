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
        uint16_t SourceSlot{ 0xFFFF };
    };

    // Advanced mode keeps one connection per endpoint, each known by a slot. A slot's mask says
    // which groups to keep: bits 0 to 15 are groups 1 to 16. Messages without a group, such as
    // UMP stream messages, are kept when any group is.
    inline constexpr size_t MaximumSourceSlots = 256;

    // Receives UMPs through the Windows MIDI Services COM extensions. The callback runs on a
    // service worker thread, so it does the smallest possible amount of work: split the
    // incoming buffer into individual UMPs, apply the group and channel filters, and stage the
    // results. MessagePipeline's own worker thread drains the staging buffer; the UI thread is
    // never involved in capture.
    struct MessageCapture : winrt::implements<MessageCapture, IMidiEndpointConnectionMessagesReceivedCallback>
    {
        STDMETHOD(MessagesReceived)(
            _In_ GUID sessionId,
            _In_ GUID connectionId,
            _In_ UINT64 timestamp,
            _In_ UINT32 wordCount,
            _In_reads_(wordCount) UINT32 const* messages) override;

        // group and channel are user-facing numbers (1-16). Zero means "all".
        void SetFilter(_In_ uint8_t groupNumber, _In_ uint8_t channelNumber) noexcept;

        // Advanced mode: messages are kept by their connection's slot and that slot's groups.
        void Advanced(_In_ bool value) noexcept { m_advanced.store(value, std::memory_order_relaxed); }

        // Call before the connection opens, so nothing it sends is missed.
        void MapConnection(_In_ GUID const& connectionId, _In_ uint16_t slot) noexcept;
        void UnmapConnection(_In_ GUID const& connectionId) noexcept;

        void SlotGroups(_In_ uint16_t slot, _In_ uint32_t groupMask) noexcept;

        // When a slot last received anything on a group, as a MIDI clock timestamp. Index 0 to 15
        // is a group; 16 is the endpoint as a whole. Zero means never.
        uint64_t LastActivity(_In_ uint16_t slot, _In_ uint8_t index) const noexcept;

        void Enabled(bool value) noexcept { m_enabled.store(value, std::memory_order_relaxed); }
        bool Enabled() const noexcept { return m_enabled.load(std::memory_order_relaxed); }

        void Drain(std::vector<CapturedMessage>& target) noexcept;

        uint64_t TakeDroppedCount() noexcept { return m_droppedCount.exchange(0, std::memory_order_relaxed); }

        static constexpr size_t MaximumStagedMessages = 200000;

    private:
        std::mutex m_stagingLock{};
        std::vector<CapturedMessage> m_staged{};

        // guarded by m_stagingLock
        std::vector<std::pair<GUID, uint16_t>> m_connections{};

        std::array<std::atomic<uint32_t>, MaximumSourceSlots> m_slotMasks{};
        std::array<std::array<std::atomic<uint64_t>, 17>, MaximumSourceSlots> m_activity{};

        std::atomic<bool> m_advanced{ false };
        std::atomic<bool> m_enabled{ false };
        std::atomic<uint8_t> m_groupFilter{ 0 };
        std::atomic<uint8_t> m_channelFilter{ 0 };
        std::atomic<uint32_t> m_batchId{ 0 };
        std::atomic<uint64_t> m_droppedCount{ 0 };
    };
}
