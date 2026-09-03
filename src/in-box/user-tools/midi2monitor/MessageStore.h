// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midi2monitor
{
    enum class RecordKind : uint8_t
    {
        MidiMessage = 0,
        Notice = 1
    };

    enum class MessageTraits : uint8_t
    {
        None = 0x00,
        SystemRealTime = 0x01,
        Clock = 0x02,
        ActiveSense = 0x04
    };

    constexpr MessageTraits operator|(MessageTraits left, MessageTraits right) noexcept
    {
        return static_cast<MessageTraits>(static_cast<uint8_t>(left) | static_cast<uint8_t>(right));
    }

    constexpr bool HasTrait(MessageTraits value, MessageTraits trait) noexcept
    {
        return (static_cast<uint8_t>(value) & static_cast<uint8_t>(trait)) != 0;
    }

    // A single retained row. Free of any XAML type so that ingestion can run at full MIDI rate
    // on the capture worker thread without ever touching the UI.
    struct MessageRecord
    {
        uint64_t Sequence{ 0 };         // monotonic across every record, including notices
        uint64_t MessageIndex{ 0 };     // 1-based display index; counts hidden messages too
        uint64_t Timestamp{ 0 };        // MIDI clock ticks
        uint64_t OffsetTicks{ 0 };      // ticks since monitoring was last started
        uint64_t DeltaTicks{ 0 };       // ticks since the previously captured message
        uint32_t BatchId{ 0 };          // messages delivered in one callback share a batch id

        std::array<uint32_t, 4> Words{};
        uint8_t WordCount{ 0 };

        RecordKind Kind{ RecordKind::MidiMessage };
        MessageTraits Traits{ MessageTraits::None };

        bool AlternateBatch{ false };   // flips each time a new delivery batch starts
        bool HasGroup{ false };
        bool HasChannel{ false };
        uint8_t GroupNumber{ 0 };       // 1-16
        uint8_t ChannelNumber{ 0 };     // 1-16

        winrt::hstring NoticeText{};
        winrt::hstring Comment{};
    };

    struct MessageStoreSnapshot
    {
        size_t VisibleCount{ 0 };
        uint64_t TotalMessageCount{ 0 };
        uint64_t RealTimeMessageCount{ 0 };
        uint64_t DroppedMessageCount{ 0 };

        // bumped only for changes that cannot be described incrementally: clear, filter change
        uint64_t Generation{ 0 };

        // running total of visible rows dropped off the front, so a slide can be reported as
        // removals plus appends rather than a rebuild
        uint64_t VisibleEvictedCount{ 0 };
    };

    // Fixed capacity ring of retained records plus the filtered list of what is currently on
    // screen. This type does no locking of its own: MessagePipeline owns the lock and is the
    // only thing that touches it.
    class MessageStore
    {
    public:
        void Capacity(uint32_t capacity) noexcept;
        uint32_t Capacity() const noexcept { return m_capacity; }

        void Clear() noexcept;

        // returns the sequence number assigned to the new record
        uint64_t Append(MessageRecord&& record) noexcept;

        void HiddenTraits(MessageTraits traits) noexcept;
        MessageTraits HiddenTraits() const noexcept { return m_hiddenTraits; }

        size_t VisibleCount() const noexcept { return m_visibleSequences.size(); }

        bool TryGetVisibleRecord(size_t visibleIndex, MessageRecord& record) const noexcept;
        bool TryGetRecord(uint64_t sequence, MessageRecord& record) const noexcept;
        bool TrySetComment(uint64_t sequence, winrt::hstring const& comment) noexcept;

        // full retained set, oldest first, used for export
        size_t RetainedCount() const noexcept { return m_records.size(); }
        MessageRecord const& RetainedAt(size_t position) const noexcept { return m_records[position]; }

        MessageStoreSnapshot Snapshot() const noexcept;

        void AddDroppedMessages(uint64_t count) noexcept { m_droppedMessageCount += count; }
        uint64_t NextMessageIndex() const noexcept { return m_totalMessageCount + 1; }

    private:
        bool IsVisible(MessageRecord const& record) const noexcept;
        void RebuildVisible() noexcept;
        void EvictOverflow() noexcept;

        std::deque<MessageRecord> m_records{};
        std::deque<uint64_t> m_visibleSequences{};

        uint32_t m_capacity{ 10000 };
        uint64_t m_firstSequence{ 0 };
        uint64_t m_nextSequence{ 0 };
        uint64_t m_totalMessageCount{ 0 };
        uint64_t m_realTimeMessageCount{ 0 };
        uint64_t m_droppedMessageCount{ 0 };
        uint64_t m_generation{ 0 };

        // monotonic for the life of the store, never reset
        uint64_t m_visibleEvictedCount{ 0 };

        MessageTraits m_hiddenTraits{ MessageTraits::Clock | MessageTraits::ActiveSense };
    };
}
