// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "SummaryBanner.g.h"

namespace winrt::miditroubleshooter::implementation
{
    struct SummaryBanner : SummaryBannerT<SummaryBanner>
    {
        SummaryBanner();

        int32_t Severity() const noexcept { return m_severity; }
        void Severity(_In_ int32_t value) noexcept;

        winrt::hstring Title() const noexcept { return m_title; }
        void Title(_In_ winrt::hstring const& value) noexcept;

        winrt::hstring Message() const noexcept { return m_message; }
        void Message(_In_ winrt::hstring const& value) noexcept;

        winrt::hstring Glyph() const noexcept { return m_glyph; }
        void Glyph(_In_ winrt::hstring const& value) noexcept;

        xaml::UIElement Chips() const noexcept { return m_chips; }
        void Chips(_In_ xaml::UIElement const& value) noexcept;

        xaml::UIElement Action() const noexcept { return m_action; }
        void Action(_In_ xaml::UIElement const& value) noexcept;

    private:
        void ApplyState() noexcept;

        int32_t m_severity{ 4 };
        winrt::hstring m_title{};
        winrt::hstring m_message{};
        winrt::hstring m_glyph{};
        xaml::UIElement m_chips{ nullptr };
        xaml::UIElement m_action{ nullptr };
    };
}

namespace winrt::miditroubleshooter::factory_implementation
{
    struct SummaryBanner : SummaryBannerT<SummaryBanner, implementation::SummaryBanner>
    {
    };
}
