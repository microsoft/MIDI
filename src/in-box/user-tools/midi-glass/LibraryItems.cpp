// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "LibraryItems.h"
#include "LayoutCard.g.cpp"
#include "LayoutCardTemplateSelector.g.cpp"
#include "StringResources.h"

namespace resources = ::midiglass::resources;

namespace
{
    // The spoken name puts a period after each piece, so a piece ending in one would get two.
    winrt::hstring WithoutClosingPeriod(_In_ winrt::hstring const& text)
    {
        std::wstring_view view{ text };

        while (!view.empty() && (view.back() == L'.' || view.back() == L' ' || view.back() == L'\t' ||
            view.back() == L'\r' || view.back() == L'\n'))
        {
            view.remove_suffix(1);
        }

        return view.empty() ? text : winrt::hstring{ view };
    }
}

namespace winrt::midiglass::implementation
{
    _Use_decl_annotations_
    void LayoutCard::Update(::midiglass::LayoutCardData const& data)
    {
        m_filePath = hstring{ data.FilePath };
        m_displayName = hstring{ data.DisplayName };
        m_description = hstring{ data.Description };
        m_detailText = hstring{ data.DetailText };
        m_relativeDate = hstring{ data.RelativeDate };
        m_statusText = hstring{ data.StatusText };
        m_status = data.Status;
        m_isFavorite = data.IsFavorite;
        m_isNewTile = data.IsNewTile;
        m_isFromNewerVersion = data.IsFromNewerVersion;
        m_signerName = data.SignerName;
        m_lastUsedTicks = data.LastUsedTicks;
        m_lastChangedTicks = data.LastChangedTicks;

        if (m_isNewTile)
        {
            m_cardAccessibleName = resources::GetString(L"NewTileAccessibleName");
            return;
        }

        m_favoriteMenuText = resources::GetString(
            m_isFavorite ? L"CardMenuRemoveFavorite" : L"CardMenuAddFavorite");

        m_runAccessibleName = resources::FormatString(L"LibraryRunAccessibleFormat", m_displayName);
        m_editAccessibleName = resources::FormatString(L"LibraryEditAccessibleFormat", m_displayName);
        m_moreAccessibleName = resources::FormatString(L"LibraryMoreAccessibleFormat", m_displayName);

        // A card shows four separate pieces of text but only the container's name is spoken, so
        // everything a sighted customer can read has to be in here.
        auto const name = WithoutClosingPeriod(m_displayName);
        auto const description = WithoutClosingPeriod(m_description);
        auto const status = WithoutClosingPeriod(m_statusText);
        auto const date = WithoutClosingPeriod(m_relativeDate);

        m_cardAccessibleName = m_description.empty()
            ? resources::FormatString(
                L"LibraryCardAccessibleFormat", name, status, date, m_detailText)
            : resources::FormatString(
                L"LibraryCardWithDescriptionAccessibleFormat",
                name, description, status, date, m_detailText);
    }

    _Use_decl_annotations_
    xaml::DataTemplate LayoutCardTemplateSelector::SelectTemplateCore(foundation::IInspectable const& item)
    {
        auto const card = item.try_as<midiglass::LayoutCard>();

        return (card != nullptr && card.IsNewTile()) ? m_newTileTemplate : m_cardTemplate;
    }

    _Use_decl_annotations_
    xaml::DataTemplate LayoutCardTemplateSelector::SelectTemplateCore(
        foundation::IInspectable const& item,
        xaml::DependencyObject const& container)
    {
        UNREFERENCED_PARAMETER(container);

        return SelectTemplateCore(item);
    }
}
