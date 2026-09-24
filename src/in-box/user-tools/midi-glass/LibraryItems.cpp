// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "LibraryItems.h"
#include "LayoutCard.g.cpp"
#include "StringResources.h"

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    _Use_decl_annotations_
    void LayoutCard::Update(::midiglass::LayoutCardData const& data)
    {
        m_filePath = hstring{ data.FilePath };
        m_displayName = hstring{ data.DisplayName };
        m_description = hstring{ data.Description };
        m_detailText = hstring{ data.DetailText };
        m_isImported = data.IsImported;

        m_runAccessibleName = resources::FormatString(L"LibraryRunAccessibleFormat", m_displayName);

        m_cardAccessibleName = m_description.empty()
            ? resources::FormatString(L"LibraryCardAccessibleFormat", m_displayName, m_detailText)
            : resources::FormatString(
                L"LibraryCardWithDescriptionAccessibleFormat", m_displayName, m_description, m_detailText);
    }
}
