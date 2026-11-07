// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "OverviewTile.g.h"

namespace winrt::miditroubleshooter::implementation
{
    struct OverviewTile : OverviewTileT<OverviewTile>
    {
        OverviewTile();

        int32_t Severity() const noexcept { return m_severity; }
        void Severity(_In_ int32_t value) noexcept;

        winrt::hstring Glyph() const noexcept { return m_glyph; }
        void Glyph(_In_ winrt::hstring const& value) noexcept;

        winrt::hstring Label() const noexcept { return m_label; }
        void Label(_In_ winrt::hstring const& value) noexcept;

        int32_t PillSeverity() const noexcept { return m_pillSeverity; }
        void PillSeverity(_In_ int32_t value) noexcept;

        winrt::hstring PillText() const noexcept { return m_pillText; }
        void PillText(_In_ winrt::hstring const& value) noexcept;

        winrt::hstring Value() const noexcept { return m_value; }
        void Value(_In_ winrt::hstring const& value) noexcept;

        winrt::hstring ValueSuffix() const noexcept { return m_valueSuffix; }
        void ValueSuffix(_In_ winrt::hstring const& value) noexcept;

        bool CompactValue() const noexcept { return m_compactValue; }
        void CompactValue(_In_ bool value) noexcept;

        winrt::hstring Detail() const noexcept { return m_detail; }
        void Detail(_In_ winrt::hstring const& value) noexcept;

        winrt::hstring LinkText() const noexcept { return m_linkText; }
        void LinkText(_In_ winrt::hstring const& value) noexcept;

        xaml::UIElement Extra() const noexcept { return m_extra; }
        void Extra(_In_ xaml::UIElement const& value) noexcept;

        winrt::event_token LinkClick(
            _In_ foundation::TypedEventHandler<miditroubleshooter::OverviewTile, foundation::IInspectable> const& handler);
        void LinkClick(_In_ winrt::event_token const& token) noexcept;

        void OnLinkClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

    private:
        void ApplyState() noexcept;
        void ApplyAccessibleName() noexcept;

        int32_t m_severity{ 3 };
        int32_t m_pillSeverity{ 4 };
        bool m_compactValue{ false };
        winrt::hstring m_glyph{};
        winrt::hstring m_label{};
        winrt::hstring m_pillText{};
        winrt::hstring m_value{};
        winrt::hstring m_valueSuffix{};
        winrt::hstring m_detail{};
        winrt::hstring m_linkText{};
        xaml::UIElement m_extra{ nullptr };

        winrt::event<foundation::TypedEventHandler<miditroubleshooter::OverviewTile, foundation::IInspectable>> m_linkClick;
    };
}

namespace winrt::miditroubleshooter::factory_implementation
{
    struct OverviewTile : OverviewTileT<OverviewTile, implementation::OverviewTile>
    {
    };
}
