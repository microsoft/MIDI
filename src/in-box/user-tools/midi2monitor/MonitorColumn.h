// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MonitorColumn.g.h"

namespace midi2monitor
{
    // Canonical order. Row and header XAML declare their cells in exactly this order, after the
    // comment gutter, and MessageRowPanel reorders them for display.
    enum class MonitorColumnId : int32_t
    {
        Index = 0,
        Timestamp = 1,
        Data = 2,
        Group = 3,
        Channel = 4,
        Decoded = 5,
        Delta = 6
    };

    inline constexpr int32_t MonitorColumnCount = 7;
    inline constexpr double CommentGutterWidth = 26.0;

    struct MonitorColumnMetrics
    {
        double Width;           // ignored when IsStar
        bool IsStar;
        bool CanHide;
        int32_t DropPriority;   // lower drops first when the window is too narrow; -1 never drops
    };

    constexpr MonitorColumnMetrics GetColumnMetrics(MonitorColumnId id) noexcept
    {
        switch (id)
        {
        case MonitorColumnId::Index:     return { 64.0, false, true, 4 };
        case MonitorColumnId::Timestamp: return { 168.0, false, true, 3 };
        case MonitorColumnId::Data:      return { 304.0, false, false, -1 };
        case MonitorColumnId::Group:     return { 56.0, false, true, 2 };
        case MonitorColumnId::Channel:   return { 64.0, false, true, 1 };
        case MonitorColumnId::Decoded:   return { 220.0, true, true, -1 };
        case MonitorColumnId::Delta:     return { 120.0, false, true, 0 };
        default:                         return { 80.0, false, true, 0 };
        }
    }
}

namespace winrt::midi2monitor::implementation
{
    struct MonitorColumn : MonitorColumnT<MonitorColumn>
    {
        MonitorColumn() = default;

        MonitorColumn(int32_t columnId, winrt::hstring const& header, bool isVisible) :
            m_columnId(columnId),
            m_header(header),
            m_isVisible(isVisible)
        {
        }

        int32_t ColumnId() const noexcept { return m_columnId; }
        winrt::hstring Header() const noexcept { return m_header; }

        bool IsVisible() const noexcept { return m_isVisible; }
        void IsVisible(bool value);

        bool CanHide() const noexcept
        {
            return ::midi2monitor::GetColumnMetrics(
                static_cast<::midi2monitor::MonitorColumnId>(m_columnId)).CanHide;
        }

        bool CanMove() const noexcept { return true; }

        winrt::event_token PropertyChanged(xaml::Data::PropertyChangedEventHandler const& handler)
        {
            return m_propertyChanged.add(handler);
        }

        void PropertyChanged(winrt::event_token const& token) noexcept
        {
            m_propertyChanged.remove(token);
        }

    private:
        int32_t m_columnId{ 0 };
        winrt::hstring m_header{};
        bool m_isVisible{ true };

        winrt::event<xaml::Data::PropertyChangedEventHandler> m_propertyChanged{};
    };
}

namespace winrt::midi2monitor::factory_implementation
{
    struct MonitorColumn : MonitorColumnT<MonitorColumn, implementation::MonitorColumn>
    {
    };
}
