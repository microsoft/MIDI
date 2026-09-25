// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "EditorItems.h"
#include "EditorItem.g.cpp"
#include "MonitorItem.g.cpp"

namespace winrt::midiglass::implementation
{
    _Use_decl_annotations_
    void EditorItem::Update(::midiglass::EditorItemData const& data)
    {
        m_key = hstring{ data.Key };
        m_displayName = hstring{ data.DisplayName };
        m_detail = hstring{ data.Detail };
        m_glyph = hstring{ data.Glyph };
        m_badge = hstring{ data.Badge };
        m_isOutsidePage = data.IsOutsidePage;
        m_indent = xaml::ThicknessHelper::FromLengths(data.IndentPixels, 0, 0, 0);
    }

    _Use_decl_annotations_
    void MonitorItem::Update(
        std::wstring const& elapsed,
        std::wstring const& words,
        std::wstring const& meaning,
        std::wstring const& destination)
    {
        m_elapsed = hstring{ elapsed };
        m_words = hstring{ words };
        m_meaning = hstring{ meaning };
        m_destination = hstring{ destination };

        // One line, because a screen reader reading four columns of hex a cell at a time tells
        // nobody anything.
        std::wstring spoken{ elapsed };

        if (!meaning.empty())
        {
            spoken += L". ";
            spoken += meaning;
        }

        if (!destination.empty())
        {
            spoken += L". ";
            spoken += destination;
        }

        m_automationName = hstring{ spoken };
    }
}
