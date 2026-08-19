// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MessageStore.h"
#include "AppSettings.h"

namespace midi2monitor
{
    _Use_decl_annotations_
    void MessageStore::Capacity(uint32_t capacity) noexcept
    {
        m_capacity = std::clamp(capacity,
            AppSettings::MinimumRetainedMessageCount,
            AppSettings::MaximumRetainedMessageCount);

        EvictOverflow();
    }

    void MessageStore::Clear() noexcept
    {
        m_records.clear();
        m_visibleSequences.clear();
        m_firstSequence = m_nextSequence;
        m_totalMessageCount = 0;
        m_realTimeMessageCount = 0;
        m_droppedMessageCount = 0;
        m_generation++;
    }

    _Use_decl_annotations_
    void MessageStore::HiddenTraits(MessageTraits traits) noexcept
    {
        if (m_hiddenTraits == traits)
        {
            return;
        }

        m_hiddenTraits = traits;
        RebuildVisible();
    }

    _Use_decl_annotations_
    bool MessageStore::IsVisible(MessageRecord const& record) const noexcept
    {
        if (record.Kind != RecordKind::MidiMessage)
        {
            return true;
        }

        return (static_cast<uint8_t>(record.Traits) & static_cast<uint8_t>(m_hiddenTraits)) == 0;
    }

    void MessageStore::RebuildVisible() noexcept
    {
        m_visibleSequences.clear();

        for (auto const& record : m_records)
        {
            if (IsVisible(record))
            {
                m_visibleSequences.push_back(record.Sequence);
            }
        }

        m_generation++;
    }

    void MessageStore::EvictOverflow() noexcept
    {
        bool evicted{ false };

        while (m_records.size() > m_capacity)
        {
            m_records.pop_front();
            m_firstSequence++;
            evicted = true;
        }

        if (!evicted)
        {
            return;
        }

        while (!m_visibleSequences.empty() && m_visibleSequences.front() < m_firstSequence)
        {
            m_visibleSequences.pop_front();
        }

        m_generation++;
    }

    _Use_decl_annotations_
    uint64_t MessageStore::Append(MessageRecord&& record) noexcept
    {
        auto const sequence = m_nextSequence++;
        record.Sequence = sequence;

        if (record.Kind == RecordKind::MidiMessage)
        {
            m_totalMessageCount++;

            if (HasTrait(record.Traits, MessageTraits::SystemRealTime))
            {
                m_realTimeMessageCount++;
            }
        }

        if (m_records.empty())
        {
            m_firstSequence = sequence;
        }

        auto const visible = IsVisible(record);

        m_records.push_back(std::move(record));

        if (visible)
        {
            m_visibleSequences.push_back(sequence);
        }

        EvictOverflow();

        return sequence;
    }

    _Use_decl_annotations_
    bool MessageStore::TryGetVisibleRecord(size_t visibleIndex, MessageRecord& record) const noexcept
    {
        if (visibleIndex >= m_visibleSequences.size())
        {
            return false;
        }

        auto const sequence = m_visibleSequences[visibleIndex];

        if (sequence < m_firstSequence)
        {
            return false;
        }

        auto const position = static_cast<size_t>(sequence - m_firstSequence);

        if (position >= m_records.size())
        {
            return false;
        }

        record = m_records[position];
        return true;
    }

    _Use_decl_annotations_
    bool MessageStore::TryGetRecord(uint64_t sequence, MessageRecord& record) const noexcept
    {
        if (sequence < m_firstSequence)
        {
            return false;
        }

        auto const position = static_cast<size_t>(sequence - m_firstSequence);

        if (position >= m_records.size())
        {
            return false;
        }

        record = m_records[position];
        return true;
    }

    _Use_decl_annotations_
    bool MessageStore::TrySetComment(uint64_t sequence, winrt::hstring const& comment) noexcept
    {
        if (sequence < m_firstSequence)
        {
            return false;
        }

        auto const position = static_cast<size_t>(sequence - m_firstSequence);

        if (position >= m_records.size())
        {
            return false;
        }

        m_records[position].Comment = comment;
        return true;
    }

    MessageStoreSnapshot MessageStore::Snapshot() const noexcept
    {
        MessageStoreSnapshot snapshot{};

        snapshot.VisibleCount = m_visibleSequences.size();
        snapshot.TotalMessageCount = m_totalMessageCount;
        snapshot.RealTimeMessageCount = m_realTimeMessageCount;
        snapshot.DroppedMessageCount = m_droppedMessageCount;
        snapshot.Generation = m_generation;

        return snapshot;
    }
}
