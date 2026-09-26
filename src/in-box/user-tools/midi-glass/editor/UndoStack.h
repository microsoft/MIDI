// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML, so the unit tests compile it unchanged.

#include <sal.h>
#include <cstddef>
#include <string>
#include <vector>

#include "LayoutModel.h"

namespace glass
{
    // Undo for a layout. States, not instructions.
    //
    // Every entry holds the whole document as it stood before an edit. That is more memory than
    // a delta would be, and it is the right trade here: the classic undo defect is the one that
    // restores nine of the ten things an edit touched, and a state cannot have that defect. A
    // page resize moves every control, a repeat adds a hundred, and a control kind change
    // rewrites the message list — an instruction-shaped model would need a separate case for
    // each, and each case is a place to get it wrong.
    //
    // The cost is bounded by MaximumDepth rather than left to grow.
    class UndoStack
    {
    public:
        // The deepest edit that can still be taken back. Beyond this the oldest is dropped.
        static constexpr size_t MaximumDepth = 100;

        // Starts again from this document. Both stacks go, because undoing past a file that is
        // now open into one that is not would be nonsense.
        void Reset(_In_ LayoutDocument const& document);

        // Records that the document has just changed, and what to call it in the Edit menu.
        void Commit(_In_ LayoutDocument const& document, _In_ std::wstring const& name);

        // The same, for something the customer experiences as one action but which arrives as
        // many: a drag is a hundred small moves and has to be one entry. Consecutive commits
        // carrying the same key fold into the first one. Any other commit, undo, redo or
        // EndCoalescing closes it.
        void CommitCoalesced(
            _In_ LayoutDocument const& document,
            _In_ std::wstring const& name,
            _In_ std::wstring const& coalesceKey);

        // Ends a gesture, so the next edit starts a new entry even if it carries the same key.
        void EndCoalescing() noexcept;

        bool CanUndo() const noexcept { return !m_undo.empty(); }
        bool CanRedo() const noexcept { return !m_redo.empty(); }

        // What the Edit menu says. Empty when there is nothing to do.
        std::wstring UndoName() const;
        std::wstring RedoName() const;

        size_t UndoDepth() const noexcept { return m_undo.size(); }
        size_t RedoDepth() const noexcept { return m_redo.size(); }

        bool Undo(_Out_ LayoutDocument& document);
        bool Redo(_Out_ LayoutDocument& document);

        LayoutDocument const& Current() const noexcept { return m_current; }

    private:
        struct Entry
        {
            LayoutDocument State{};
            std::wstring Name{};
        };

        LayoutDocument m_current{};
        std::vector<Entry> m_undo{};
        std::vector<Entry> m_redo{};

        std::wstring m_openCoalesceKey{};
    };
}
