// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, so the unit tests compile it unchanged.

#include "UndoStack.h"

namespace glass
{
    _Use_decl_annotations_
    void UndoStack::Reset(LayoutDocument const& document)
    {
        m_current = document;
        m_undo.clear();
        m_redo.clear();
        m_openCoalesceKey.clear();
    }

    _Use_decl_annotations_
    void UndoStack::Commit(LayoutDocument const& document, std::wstring const& name)
    {
        m_undo.push_back({ m_current, name });

        if (m_undo.size() > MaximumDepth)
        {
            m_undo.erase(m_undo.begin());
        }

        // A new edit is the end of the branch that was undone. Keeping it would let a redo
        // reinstate something built on a document that no longer exists.
        m_redo.clear();
        m_openCoalesceKey.clear();

        m_current = document;
    }

    _Use_decl_annotations_
    void UndoStack::CommitCoalesced(
        LayoutDocument const& document,
        std::wstring const& name,
        std::wstring const& coalesceKey)
    {
        if (!coalesceKey.empty() && coalesceKey == m_openCoalesceKey && !m_undo.empty())
        {
            // Still the same gesture. The state before it started is the one to keep.
            m_current = document;
            return;
        }

        Commit(document, name);

        m_openCoalesceKey = coalesceKey;
    }

    void UndoStack::EndCoalescing() noexcept
    {
        m_openCoalesceKey.clear();
    }

    std::wstring UndoStack::UndoName() const
    {
        return m_undo.empty() ? std::wstring{} : m_undo.back().Name;
    }

    std::wstring UndoStack::RedoName() const
    {
        return m_redo.empty() ? std::wstring{} : m_redo.back().Name;
    }

    _Use_decl_annotations_
    bool UndoStack::Undo(LayoutDocument& document)
    {
        if (m_undo.empty())
        {
            return false;
        }

        auto entry = m_undo.back();
        m_undo.pop_back();

        m_redo.push_back({ m_current, entry.Name });

        m_current = std::move(entry.State);
        m_openCoalesceKey.clear();

        document = m_current;

        return true;
    }

    _Use_decl_annotations_
    bool UndoStack::Redo(LayoutDocument& document)
    {
        if (m_redo.empty())
        {
            return false;
        }

        auto entry = m_redo.back();
        m_redo.pop_back();

        m_undo.push_back({ m_current, entry.Name });

        m_current = std::move(entry.State);
        m_openCoalesceKey.clear();

        document = m_current;

        return true;
    }
}
