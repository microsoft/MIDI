// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "LayoutCard.g.h"
#include "LayoutCardTemplateSelector.g.h"

namespace midiglass
{
    // What a card says about whether the layout can run right now.
    enum class LayoutCardStatus
    {
        Ready = 0,
        DeviceMissing = 1,
        NeedsAttention = 2,
    };

    // Everything a card shows, gathered in one place so building the library is one call per
    // layout rather than a dozen setters.
    struct LayoutCardData
    {
        std::wstring FilePath{};
        std::wstring DisplayName{};
        std::wstring Description{};
        std::wstring DetailText{};
        std::wstring RelativeDate{};
        std::wstring StatusText{};

        LayoutCardStatus Status{ LayoutCardStatus::Ready };

        bool IsFavorite{ false };
        bool IsNewTile{ false };

        // Sort keys, kept beside the display text so the library never re-parses what it shows.
        int64_t LastUsedTicks{ 0 };
        int64_t LastChangedTicks{ 0 };
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
        hstring RelativeDate() const noexcept { return m_relativeDate; }
        hstring StatusText() const noexcept { return m_statusText; }

        media::ImageSource Thumbnail() const noexcept { return m_thumbnail; }
        void Thumbnail(_In_ media::ImageSource const& value) noexcept { m_thumbnail = value; }

        bool IsFavorite() const noexcept { return m_isFavorite; }
        bool IsNewTile() const noexcept { return m_isNewTile; }

        xaml::Visibility FavoriteVisibility() const noexcept
        {
            return m_isFavorite ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        }

        xaml::Visibility DescriptionVisibility() const noexcept
        {
            return m_description.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible;
        }

        xaml::Visibility ReadyVisibility() const noexcept
        {
            return VisibilityFor(::midiglass::LayoutCardStatus::Ready);
        }

        xaml::Visibility WarningVisibility() const noexcept
        {
            return VisibilityFor(::midiglass::LayoutCardStatus::DeviceMissing);
        }

        xaml::Visibility InfoVisibility() const noexcept
        {
            return VisibilityFor(::midiglass::LayoutCardStatus::NeedsAttention);
        }

        hstring FavoriteMenuText() const noexcept { return m_favoriteMenuText; }

        hstring RunAccessibleName() const noexcept { return m_runAccessibleName; }
        hstring EditAccessibleName() const noexcept { return m_editAccessibleName; }
        hstring MoreAccessibleName() const noexcept { return m_moreAccessibleName; }
        hstring CardAccessibleName() const noexcept { return m_cardAccessibleName; }

        // Not projected. The library sorts on these rather than on what it prints.
        int64_t LastUsedTicks() const noexcept { return m_lastUsedTicks; }
        int64_t LastChangedTicks() const noexcept { return m_lastChangedTicks; }

    private:
        xaml::Visibility VisibilityFor(_In_ ::midiglass::LayoutCardStatus status) const noexcept
        {
            return m_status == status ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        }

        hstring m_filePath{};
        hstring m_displayName{};
        hstring m_description{};
        hstring m_detailText{};
        hstring m_relativeDate{};
        hstring m_statusText{};
        hstring m_favoriteMenuText{};
        hstring m_runAccessibleName{};
        hstring m_editAccessibleName{};
        hstring m_moreAccessibleName{};
        hstring m_cardAccessibleName{};

        media::ImageSource m_thumbnail{ nullptr };

        ::midiglass::LayoutCardStatus m_status{ ::midiglass::LayoutCardStatus::Ready };

        bool m_isFavorite{ false };
        bool m_isNewTile{ false };

        int64_t m_lastUsedTicks{ 0 };
        int64_t m_lastChangedTicks{ 0 };
    };

    struct LayoutCardTemplateSelector : LayoutCardTemplateSelectorT<LayoutCardTemplateSelector>
    {
        LayoutCardTemplateSelector() = default;

        xaml::DataTemplate CardTemplate() const noexcept { return m_cardTemplate; }
        void CardTemplate(_In_ xaml::DataTemplate const& value) noexcept { m_cardTemplate = value; }

        xaml::DataTemplate NewTileTemplate() const noexcept { return m_newTileTemplate; }
        void NewTileTemplate(_In_ xaml::DataTemplate const& value) noexcept { m_newTileTemplate = value; }

        xaml::DataTemplate SelectTemplateCore(_In_ foundation::IInspectable const& item);

        xaml::DataTemplate SelectTemplateCore(
            _In_ foundation::IInspectable const& item,
            _In_ xaml::DependencyObject const& container);

    private:
        xaml::DataTemplate m_cardTemplate{ nullptr };
        xaml::DataTemplate m_newTileTemplate{ nullptr };
    };
}

namespace winrt::midiglass::factory_implementation
{
    struct LayoutCard : LayoutCardT<LayoutCard, implementation::LayoutCard>
    {
    };

    struct LayoutCardTemplateSelector :
        LayoutCardTemplateSelectorT<LayoutCardTemplateSelector, implementation::LayoutCardTemplateSelector>
    {
    };
}
