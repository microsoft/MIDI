// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "LayoutCard.g.h"

namespace midiglass
{
    // Everything a card shows, gathered in one place so building the library is one call per
    // layout rather than a dozen setters.
    struct LayoutCardData
    {
        std::wstring FilePath{};
        std::wstring DisplayName{};
        std::wstring Description{};
        std::wstring DetailText{};
        bool IsImported{ false };
    };
}

namespace winrt::midiglass::implementation
{
    struct LayoutCard : LayoutCardT<LayoutCard>
    {
        LayoutCard() = default;

        void Update(_In_ ::midiglass::LayoutCardData const& data);

        hstring FilePath() const noexcept { return m_filePath; }
        hstring DisplayName() const noexcept { return m_displayName; }
        hstring Description() const noexcept { return m_description; }
        hstring DetailText() const noexcept { return m_detailText; }

        media::ImageSource Thumbnail() const noexcept { return m_thumbnail; }
        void Thumbnail(_In_ media::ImageSource const& value) noexcept { m_thumbnail = value; }

        xaml::Visibility DescriptionVisibility() const noexcept
        {
            return m_description.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible;
        }

        xaml::Visibility ImportedVisibility() const noexcept
        {
            return m_isImported ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        }

        hstring RunAccessibleName() const noexcept { return m_runAccessibleName; }
        hstring CardAccessibleName() const noexcept { return m_cardAccessibleName; }

    private:
        hstring m_filePath{};
        hstring m_displayName{};
        hstring m_description{};
        hstring m_detailText{};
        hstring m_runAccessibleName{};
        hstring m_cardAccessibleName{};

        media::ImageSource m_thumbnail{ nullptr };

        bool m_isImported{ false };
    };
}

namespace winrt::midiglass::factory_implementation
{
    struct LayoutCard : LayoutCardT<LayoutCard, implementation::LayoutCard>
    {
    };
}
