// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MessageRowPanel.g.h"
#include "MonitorColumn.h"

namespace midi2monitor
{
    struct ColumnDisplayEntry
    {
        MonitorColumnId Id{ MonitorColumnId::Index };
        bool IsVisible{ true };
    };

    // Shared by the header and every message row. UI thread only.
    class ColumnLayoutState
    {
    public:
        static std::vector<ColumnDisplayEntry>& Entries() noexcept;
        static void ResetToDefault() noexcept;

        static bool AutoHideWhenNarrow() noexcept;
        static void AutoHideWhenNarrow(bool value) noexcept;

        // "0,1,2,-3,4,5,6" style string; a leading minus means the column is hidden
        static std::wstring Serialize();
        static void Deserialize(std::wstring const& value);
    };
}

namespace winrt::midi2monitor::implementation
{
    struct MessageRowPanel : MessageRowPanelT<MessageRowPanel>
    {
        MessageRowPanel() = default;

        foundation::Size MeasureOverride(foundation::Size const& availableSize);
        foundation::Size ArrangeOverride(foundation::Size const& finalSize);

    private:
        struct PlacedColumn
        {
            uint32_t ChildIndex{ 0 };
            double Offset{ 0.0 };
            double Width{ 0.0 };
        };

        std::vector<PlacedColumn> m_placement{};
    };
}

namespace winrt::midi2monitor::factory_implementation
{
    struct MessageRowPanel : MessageRowPanelT<MessageRowPanel, implementation::MessageRowPanel>
    {
    };
}
