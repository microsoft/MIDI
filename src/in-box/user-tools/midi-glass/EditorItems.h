// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "EditorItem.g.h"
#include "MonitorItem.g.h"

namespace midiglass
{
    // Everything a palette tile or an outline row shows, gathered in one place so building a
    // list is one call per row rather than a dozen setters.
    struct EditorItemData
    {
        std::wstring Key{};
        std::wstring DisplayName{};
        std::wstring Detail{};
        std::wstring Glyph{};
        std::wstring Badge{};

        double IndentPixels{ 0.0 };
        bool IsOutsidePage{ false };
    };
}

namespace winrt::midiglass::implementation
{
    struct EditorItem : EditorItemT<EditorItem>
    {
        EditorItem() = default;

        void Update(_In_ ::midiglass::EditorItemData const& data);

        hstring Key() const noexcept { return m_key; }
        hstring DisplayName() const noexcept { return m_displayName; }
        hstring Detail() const noexcept { return m_detail; }
        hstring Glyph() const noexcept { return m_glyph; }
        hstring Badge() const noexcept { return m_badge; }

        foundation::IInspectable Art() const noexcept { return m_art; }
        void Art(_In_ foundation::IInspectable const& value) noexcept { m_art = value; }

        xaml::Thickness Indent() const noexcept { return m_indent; }

        xaml::Visibility DetailVisibility() const noexcept
        {
            return m_detail.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible;
        }

        xaml::Visibility BadgeVisibility() const noexcept
        {
            return m_badge.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible;
        }

        bool IsOutsidePage() const noexcept { return m_isOutsidePage; }

        xaml::Visibility OutsideVisibility() const noexcept
        {
            return m_isOutsidePage ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        }

    private:
        hstring m_key{};
        hstring m_displayName{};
        hstring m_detail{};
        hstring m_glyph{};
        hstring m_badge{};

        foundation::IInspectable m_art{ nullptr };

        xaml::Thickness m_indent{ 0, 0, 0, 0 };
        bool m_isOutsidePage{ false };
    };

    struct MonitorItem : MonitorItemT<MonitorItem>
    {
        MonitorItem() = default;

        void Update(
            _In_ std::wstring const& elapsed,
            _In_ std::wstring const& words,
            _In_ std::wstring const& meaning,
            _In_ std::wstring const& destination);

        hstring Elapsed() const noexcept { return m_elapsed; }
        hstring Words() const noexcept { return m_words; }
        hstring Meaning() const noexcept { return m_meaning; }
        hstring Destination() const noexcept { return m_destination; }
        hstring AutomationName() const noexcept { return m_automationName; }

    private:
        hstring m_elapsed{};
        hstring m_words{};
        hstring m_meaning{};
        hstring m_destination{};
        hstring m_automationName{};
    };
}

namespace winrt::midiglass::factory_implementation
{
    struct EditorItem : EditorItemT<EditorItem, implementation::EditorItem>
    {
    };

    struct MonitorItem : MonitorItemT<MonitorItem, implementation::MonitorItem>
    {
    };
}
