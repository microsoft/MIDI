// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Undo by change. MIDI Patchbay keeps a whole copy of the patch for each step, which a sequence
// with a hundred thousand notes can't afford, so each edit here records only what it changed and
// how to put it back.

#include <sal.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "SequenceModel.h"

namespace midisequencer
{
    // One reversible edit. Apply is redo; Revert is undo. Both find what they change by id, so a
    // change still lands on the right clip after other edits moved things around.
    class SequenceChange
    {
    public:
        virtual ~SequenceChange() = default;

        virtual void Apply(_Inout_ Sequence& sequence) = 0;
        virtual void Revert(_Inout_ Sequence& sequence) = 0;

        // Roughly how much memory the change holds, so the history can stay under a ceiling.
        virtual size_t Cost() const noexcept = 0;
    };

    // Notes taken out of and put into one clip. Notes are matched by value, so two identical notes
    // are interchangeable, which is what they are.
    std::unique_ptr<SequenceChange> MakeNoteChange(
        _In_ std::wstring clipId,
        _In_ std::vector<Note> removed,
        _In_ std::vector<Note> added);

    // Anything small enough to keep a before and after copy of: a track's settings, a clip's
    // length, the tempo map, the tags. The locator finds the value in the sequence each time.
    template<typename TValue>
    class SnapshotChange final : public SequenceChange
    {
    public:
        using Locator = std::function<TValue*(Sequence&)>;

        SnapshotChange(_In_ Locator locate, _In_ TValue before, _In_ TValue after, _In_ size_t cost) :
            m_locate(std::move(locate)), m_before(std::move(before)), m_after(std::move(after)), m_cost(cost)
        {
        }

        void Apply(_Inout_ Sequence& sequence) override
        {
            if (auto value = m_locate(sequence); value != nullptr)
            {
                *value = m_after;
            }
        }

        void Revert(_Inout_ Sequence& sequence) override
        {
            if (auto value = m_locate(sequence); value != nullptr)
            {
                *value = m_before;
            }
        }

        size_t Cost() const noexcept override { return m_cost; }

    private:
        Locator m_locate;
        TValue m_before;
        TValue m_after;
        size_t m_cost{ 0 };
    };

    class UndoStack
    {
    public:
        // Keeps the history under about this many bytes by forgetting the oldest steps.
        explicit UndoStack(_In_ size_t maximumCost = 256u * 1024 * 1024) noexcept :
            m_maximumCost(maximumCost)
        {
        }

        // Records changes that were already applied, as one step with a name for the Edit menu.
        // Anything that could be redone is forgotten: a new edit starts a new future.
        void Commit(_In_ std::wstring name, _In_ std::vector<std::unique_ptr<SequenceChange>> changes);

        // Applies the changes, then records them.
        void ApplyAndCommit(
            _Inout_ Sequence& sequence,
            _In_ std::wstring name,
            _In_ std::vector<std::unique_ptr<SequenceChange>> changes);

        bool Undo(_Inout_ Sequence& sequence);
        bool Redo(_Inout_ Sequence& sequence);

        bool CanUndo() const noexcept { return !m_undo.empty(); }
        bool CanRedo() const noexcept { return !m_redo.empty(); }

        std::wstring const& UndoName() const noexcept;
        std::wstring const& RedoName() const noexcept;

        size_t UndoCount() const noexcept { return m_undo.size(); }
        size_t TotalCost() const noexcept { return m_cost; }

        void Clear() noexcept;

    private:
        struct Step
        {
            std::wstring Name{};
            std::vector<std::unique_ptr<SequenceChange>> Changes{};
            size_t Cost{ 0 };
        };

        void Trim() noexcept;

        std::vector<Step> m_undo{};
        std::vector<Step> m_redo{};
        size_t m_cost{ 0 };
        size_t m_maximumCost{ 0 };
    };
}
