// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MessagePipeline.h"
#include "AppSettings.h"

namespace midi2monitor
{
    // Data virtualized list for the message grid. Rows are formatted only when XAML asks for
    // them, so a full buffer costs nothing until the customer scrolls to it.
    struct MessageListSource : winrt::implements<MessageListSource,
        collections::IObservableVector<foundation::IInspectable>,
        collections::IVector<foundation::IInspectable>,
        collections::IIterable<foundation::IInspectable>>
    {
        explicit MessageListSource(MessagePipeline& pipeline) noexcept : m_pipeline(pipeline) {}

        void FormattingOptions(TimestampDisplayFormat timestampFormat, bool showMessageNames, double rowFontSize) noexcept;

        // Pulls the latest counts from the pipeline and raises the minimum set of change
        // notifications. Returns true when rows were appended at the end.
        bool Refresh() noexcept;
        void Reset() noexcept;

        bool TryGetSequence(uint32_t index, uint64_t& sequence) noexcept;

        foundation::IInspectable GetAt(uint32_t index);
        uint32_t Size() const noexcept { return static_cast<uint32_t>(m_count); }
        collections::IVectorView<foundation::IInspectable> GetView();
        bool IndexOf(foundation::IInspectable const& value, uint32_t& index) const noexcept;
        uint32_t GetMany(uint32_t startIndex, winrt::array_view<foundation::IInspectable> items);

        void SetAt(uint32_t, foundation::IInspectable const&) { throw winrt::hresult_not_implemented(); }
        void InsertAt(uint32_t, foundation::IInspectable const&) { throw winrt::hresult_not_implemented(); }
        void RemoveAt(uint32_t) { throw winrt::hresult_not_implemented(); }
        void Append(foundation::IInspectable const&) { throw winrt::hresult_not_implemented(); }
        void RemoveAtEnd() { throw winrt::hresult_not_implemented(); }
        void Clear() { throw winrt::hresult_not_implemented(); }
        void ReplaceAll(winrt::array_view<foundation::IInspectable const>) { throw winrt::hresult_not_implemented(); }

        collections::IIterator<foundation::IInspectable> First();

        winrt::event_token VectorChanged(collections::VectorChangedEventHandler<foundation::IInspectable> const& handler)
        {
            return m_vectorChanged.add(handler);
        }

        void VectorChanged(winrt::event_token const& token) noexcept
        {
            m_vectorChanged.remove(token);
        }

        // beyond this many new rows in one refresh it is cheaper to rebuild than to insert
        static constexpr size_t MaximumIncrementalInserts = 64;
        static constexpr size_t MaximumCachedRows = 4096;

    private:
        void RaiseChange(collections::CollectionChange change, uint32_t index) noexcept;

        MessagePipeline& m_pipeline;

        size_t m_count{ 0 };
        uint64_t m_generation{ 0 };
        bool m_hasGeneration{ false };

        TimestampDisplayFormat m_timestampFormat{ TimestampDisplayFormat::Ticks };
        bool m_showMessageNames{ true };
        double m_rowFontSize{ AppSettings::BaseRowFontSize };

        std::unordered_map<uint64_t, foundation::IInspectable> m_cache{};

        winrt::event<collections::VectorChangedEventHandler<foundation::IInspectable>> m_vectorChanged{};
    };
}
