// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MessageListSource.h"
#include "MidiMessageViewModel.h"

namespace midi2monitor
{
    namespace
    {
        struct VectorChangedArgs : winrt::implements<VectorChangedArgs, collections::IVectorChangedEventArgs>
        {
            VectorChangedArgs(collections::CollectionChange change, uint32_t index) noexcept :
                m_change(change),
                m_index(index)
            {
            }

            collections::CollectionChange CollectionChange() const noexcept { return m_change; }
            uint32_t Index() const noexcept { return m_index; }

        private:
            collections::CollectionChange m_change;
            uint32_t m_index;
        };

        struct ListIterator : winrt::implements<ListIterator, collections::IIterator<foundation::IInspectable>>
        {
            ListIterator(winrt::com_ptr<MessageListSource> const& source) noexcept :
                m_source(source)
            {
            }

            foundation::IInspectable Current() const
            {
                if (m_position >= m_source->Size())
                {
                    throw winrt::hresult_out_of_bounds();
                }

                return m_source->GetAt(m_position);
            }

            bool HasCurrent() const noexcept { return m_position < m_source->Size(); }

            bool MoveNext() noexcept
            {
                if (m_position < m_source->Size())
                {
                    m_position++;
                }

                return HasCurrent();
            }

            uint32_t GetMany(winrt::array_view<foundation::IInspectable> items)
            {
                auto const copied = m_source->GetMany(m_position, items);
                m_position += copied;
                return copied;
            }

        private:
            winrt::com_ptr<MessageListSource> m_source;
            uint32_t m_position{ 0 };
        };

        // Forwards to the live source rather than copying, so asking for a view of a full
        // buffer never materializes thousands of rows.
        struct ListView : winrt::implements<ListView,
            collections::IVectorView<foundation::IInspectable>,
            collections::IIterable<foundation::IInspectable>>
        {
            ListView(winrt::com_ptr<MessageListSource> const& source) noexcept :
                m_source(source)
            {
            }

            foundation::IInspectable GetAt(uint32_t index) const { return m_source->GetAt(index); }
            uint32_t Size() const noexcept { return m_source->Size(); }

            bool IndexOf(foundation::IInspectable const& value, uint32_t& index) const noexcept
            {
                return m_source->IndexOf(value, index);
            }

            uint32_t GetMany(uint32_t startIndex, winrt::array_view<foundation::IInspectable> items) const
            {
                return m_source->GetMany(startIndex, items);
            }

            collections::IIterator<foundation::IInspectable> First() const
            {
                return winrt::make<ListIterator>(m_source);
            }

        private:
            winrt::com_ptr<MessageListSource> m_source;
        };
    }

    _Use_decl_annotations_
    void MessageListSource::FormattingOptions(TimestampDisplayFormat timestampFormat, bool showMessageNames, double rowFontSize) noexcept
    {
        if (m_timestampFormat == timestampFormat &&
            m_showMessageNames == showMessageNames &&
            m_rowFontSize == rowFontSize)
        {
            return;
        }

        m_timestampFormat = timestampFormat;
        m_showMessageNames = showMessageNames;
        m_rowFontSize = rowFontSize;

        Reset();
    }

    void MessageListSource::Reset() noexcept
    {
        try
        {
            auto const snapshot = m_pipeline.Snapshot();

            m_generation = snapshot.Generation;
            m_hasGeneration = true;
            m_count = snapshot.VisibleCount;
            m_visibleEvicted = snapshot.VisibleEvictedCount;
            m_cache.clear();

            RaiseChange(collections::CollectionChange::Reset, 0);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to reset the message list.")
    }

    bool MessageListSource::Refresh() noexcept
    {
        try
        {
            auto const snapshot = m_pipeline.Snapshot();

            auto const seenEver = m_count + static_cast<size_t>(m_visibleEvicted);
            auto const snapshotSeenEver = snapshot.VisibleCount + static_cast<size_t>(snapshot.VisibleEvictedCount);

            // a clear, a filter change or anything that ran backwards cannot be described
            // incrementally, so the list is rebuilt
            if (!m_hasGeneration ||
                snapshot.Generation != m_generation ||
                snapshot.VisibleEvictedCount < m_visibleEvicted ||
                snapshotSeenEver < seenEver)
            {
                auto const grew = snapshot.VisibleCount > m_count;

                m_generation = snapshot.Generation;
                m_hasGeneration = true;
                m_count = snapshot.VisibleCount;
                m_visibleEvicted = snapshot.VisibleEvictedCount;
                m_cache.clear();

                RaiseChange(collections::CollectionChange::Reset, 0);

                return grew;
            }

            auto const removed = static_cast<size_t>(snapshot.VisibleEvictedCount - m_visibleEvicted);
            auto const appended = snapshotSeenEver - seenEver;

            if (removed == 0 && appended == 0)
            {
                return false;
            }

            m_visibleEvicted = snapshot.VisibleEvictedCount;

            if (removed + appended > MaximumIncrementalInserts)
            {
                m_count = snapshot.VisibleCount;
                m_cache.clear();
                RaiseChange(collections::CollectionChange::Reset, 0);

                return appended > 0;
            }

            // XAML reads Size() from inside these handlers, so the count moves one at a time
            for (size_t i = 0; i < removed; i++)
            {
                m_count--;
                RaiseChange(collections::CollectionChange::ItemRemoved, 0);
            }

            for (size_t i = 0; i < appended; i++)
            {
                m_count++;
                RaiseChange(collections::CollectionChange::ItemInserted, static_cast<uint32_t>(m_count - 1));
            }

            return appended > 0;
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to refresh the message list.")

        return false;
    }

    _Use_decl_annotations_
    void MessageListSource::RaiseChange(collections::CollectionChange change, uint32_t index) noexcept
    {
        try
        {
            m_vectorChanged(*this, winrt::make<VectorChangedArgs>(change, index));
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to raise a message list change notification.")
    }

    _Use_decl_annotations_
    bool MessageListSource::TryGetSequence(uint32_t index, uint64_t& sequence) noexcept
    {
        MessageRecord record{};

        if (!m_pipeline.TryGetVisibleRecord(index, record))
        {
            return false;
        }

        sequence = record.Sequence;
        return true;
    }

    _Use_decl_annotations_
    foundation::IInspectable MessageListSource::GetAt(uint32_t index)
    {
        if (index >= m_count)
        {
            throw winrt::hresult_out_of_bounds();
        }

        MessageRecord record{};

        if (!m_pipeline.TryGetVisibleRecord(index, record))
        {
            // the row was evicted between the refresh and the request. An empty placeholder is
            // better than an exception escaping into XAML layout.
            return winrt::make<winrt::midi2monitor::implementation::MidiMessageViewModel>(
                MessageRecord{}, m_timestampFormat, m_showMessageNames, m_rowFontSize);
        }

        auto const cached = m_cache.find(record.Sequence);

        if (cached != m_cache.end())
        {
            return cached->second;
        }

        auto item = winrt::make<winrt::midi2monitor::implementation::MidiMessageViewModel>(
            record, m_timestampFormat, m_showMessageNames, m_rowFontSize);

        if (m_cache.size() >= MaximumCachedRows)
        {
            m_cache.clear();
        }

        m_cache.emplace(record.Sequence, item);

        return item;
    }

    collections::IVectorView<foundation::IInspectable> MessageListSource::GetView()
    {
        return winrt::make<ListView>(get_strong());
    }

    _Use_decl_annotations_
    bool MessageListSource::IndexOf(foundation::IInspectable const& value, uint32_t& index) const noexcept
    {
        index = 0;

        auto const item = value.try_as<winrt::midi2monitor::MidiMessageViewModel>();

        if (item == nullptr)
        {
            return false;
        }

        for (uint32_t i = 0; i < static_cast<uint32_t>(m_count); i++)
        {
            MessageRecord record{};

            if (m_pipeline.TryGetVisibleRecord(i, record) && record.Sequence == item.Sequence())
            {
                index = i;
                return true;
            }
        }

        return false;
    }

    _Use_decl_annotations_
    uint32_t MessageListSource::GetMany(uint32_t startIndex, winrt::array_view<foundation::IInspectable> items)
    {
        if (startIndex >= m_count)
        {
            return 0;
        }

        auto const available = static_cast<uint32_t>(m_count) - startIndex;
        auto const copyCount = std::min(available, items.size());

        for (uint32_t i = 0; i < copyCount; i++)
        {
            items[i] = GetAt(startIndex + i);
        }

        return copyCount;
    }

    collections::IIterator<foundation::IInspectable> MessageListSource::First()
    {
        return winrt::make<ListIterator>(get_strong());
    }
}
