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
        m_lastVisibleBySource.clear();
        m_foldUpdates.clear();
        m_firstSequence = m_nextSequence;
        m_totalMessageCount = 0;
        m_realTimeMessageCount = 0;
        m_droppedMessageCount = 0;
        m_hiddenMessageCount = 0;
        m_foldedMessageCount = 0;
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
    void MessageStore::Filter(::midiapp::MessageFilter const& filter) noexcept
    {
        try
        {
            if (::midiapp::FilterSignature(filter) == ::midiapp::FilterSignature(m_filter))
            {
                return;
            }
        }
        catch (...)
        {
            // compare failed; rebuild anyway
        }

        m_filter = filter;
        RebuildVisible();
    }

    _Use_decl_annotations_
    void MessageStore::FoldRepeats(bool value) noexcept
    {
        if (m_foldRepeats == value)
        {
            return;
        }

        m_foldRepeats = value;
        RebuildVisible();
    }

    std::vector<std::pair<uint64_t, uint32_t>> MessageStore::TakeFoldUpdates() noexcept
    {
        std::vector<std::pair<uint64_t, uint32_t>> updates{};

        try
        {
            updates.reserve(m_foldUpdates.size());

            for (auto const& entry : m_foldUpdates)
            {
                updates.push_back(entry);
            }
        }
        catch (...)
        {
            updates.clear();
        }

        m_foldUpdates.clear();

        return updates;
    }

    _Use_decl_annotations_
    bool MessageStore::IsVisible(MessageRecord const& record) const noexcept
    {
        if (record.Kind != RecordKind::MidiMessage)
        {
            return true;
        }

        if ((static_cast<uint8_t>(record.Traits) & static_cast<uint8_t>(m_hiddenTraits)) != 0)
        {
            return false;
        }

        return m_filter.Allows(record.Words.data(), record.WordCount);
    }

    _Use_decl_annotations_
    MessageRecord* MessageStore::RecordAt(uint64_t sequence) noexcept
    {
        if (sequence < m_firstSequence)
        {
            return nullptr;
        }

        auto const position = static_cast<size_t>(sequence - m_firstSequence);

        if (position >= m_records.size())
        {
            return nullptr;
        }

        return &m_records[position];
    }

    _Use_decl_annotations_
    bool MessageStore::TryFold(MessageRecord& record) noexcept
    {
        if (!m_foldRepeats || record.Kind != RecordKind::MidiMessage)
        {
            return false;
        }

        auto const last = m_lastVisibleBySource.find(record.SourceSlot);

        if (last == m_lastVisibleBySource.end())
        {
            return false;
        }

        auto* const target = RecordAt(last->second);

        if (target == nullptr ||
            target->Kind != RecordKind::MidiMessage ||
            target->WordCount != record.WordCount ||
            !std::equal(record.Words.begin(), record.Words.begin() + record.WordCount, target->Words.begin()))
        {
            return false;
        }

        target->RepeatCount++;
        record.Folded = true;

        try
        {
            m_foldUpdates[target->Sequence] = target->RepeatCount;
        }
        catch (...)
        {
            // the row shows the new count the next time it is drawn
        }

        return true;
    }

    void MessageStore::RebuildVisible() noexcept
    {
        m_visibleSequences.clear();
        m_lastVisibleBySource.clear();
        m_foldUpdates.clear();
        m_hiddenMessageCount = 0;
        m_foldedMessageCount = 0;

        for (auto& record : m_records)
        {
            record.Folded = false;
            record.RepeatCount = 0;
        }

        for (auto& record : m_records)
        {
            if (!IsVisible(record))
            {
                m_hiddenMessageCount++;
                continue;
            }

            if (TryFold(record))
            {
                m_foldedMessageCount++;
                continue;
            }

            m_visibleSequences.push_back(record.Sequence);

            if (record.Kind == RecordKind::MidiMessage)
            {
                try
                {
                    m_lastVisibleBySource[record.SourceSlot] = record.Sequence;
                }
                catch (...)
                {
                    // this source just doesn't fold into this row
                }
            }
            else
            {
                // nothing folds across a notice, such as listening stopping and starting
                m_lastVisibleBySource.clear();
            }
        }

        // a full rebuild already shows every count
        m_foldUpdates.clear();
        m_generation++;
    }

    void MessageStore::EvictOverflow() noexcept
    {
        bool evicted{ false };

        while (m_records.size() > m_capacity)
        {
            // the counts are of what is kept, so what leaves comes off them
            auto const& oldest = m_records.front();

            if (oldest.Kind == RecordKind::MidiMessage)
            {
                if (oldest.Folded)
                {
                    m_foldedMessageCount -= (m_foldedMessageCount > 0) ? 1 : 0;
                }
                else if (!IsVisible(oldest))
                {
                    m_hiddenMessageCount -= (m_hiddenMessageCount > 0) ? 1 : 0;
                }
            }

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
            m_visibleEvictedCount++;
        }
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

        auto visible = IsVisible(record);

        if (!visible)
        {
            m_hiddenMessageCount++;
        }
        else if (TryFold(record))
        {
            m_foldedMessageCount++;
            visible = false;
        }

        auto const isMessage = record.Kind == RecordKind::MidiMessage;
        auto const slot = record.SourceSlot;

        m_records.push_back(std::move(record));

        if (visible)
        {
            m_visibleSequences.push_back(sequence);

            if (isMessage)
            {
                try
                {
                    m_lastVisibleBySource[slot] = sequence;
                }
                catch (...)
                {
                    // this source just doesn't fold into this row
                }
            }
            else
            {
                // nothing folds across a notice, such as listening stopping and starting
                m_lastVisibleBySource.clear();
            }
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
        snapshot.HiddenMessageCount = m_hiddenMessageCount;
        snapshot.FoldedMessageCount = m_foldedMessageCount;
        snapshot.Generation = m_generation;
        snapshot.VisibleEvictedCount = m_visibleEvictedCount;

        return snapshot;
    }
}
