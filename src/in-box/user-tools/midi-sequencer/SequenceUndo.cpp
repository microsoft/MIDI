// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SequenceUndo.h"

#include <algorithm>

namespace midisequencer
{
    namespace
    {
        bool NoteOrder(_In_ Note const& a, _In_ Note const& b) noexcept
        {
            return a.Tick != b.Tick ? a.Tick < b.Tick : a.Number < b.Number;
        }

        void RemoveNotes(_Inout_ std::vector<Note>& notes, _In_ std::vector<Note> const& remove)
        {
            for (auto const& note : remove)
            {
                // Notes are sorted, so the search starts where this note would be.
                auto first = std::lower_bound(notes.begin(), notes.end(), note, NoteOrder);
                auto found = std::find(first, notes.end(), note);

                if (found != notes.end())
                {
                    notes.erase(found);
                }
            }
        }

        void AddNotes(_Inout_ std::vector<Note>& notes, _In_ std::vector<Note> const& add)
        {
            for (auto const& note : add)
            {
                notes.insert(std::upper_bound(notes.begin(), notes.end(), note, NoteOrder), note);
            }
        }

        class NoteChange final : public SequenceChange
        {
        public:
            NoteChange(_In_ std::wstring clipId, _In_ std::vector<Note> removed, _In_ std::vector<Note> added) :
                m_clipId(std::move(clipId)), m_removed(std::move(removed)), m_added(std::move(added))
            {
            }

            void Apply(_Inout_ Sequence& sequence) override
            {
                if (auto clip = FindClip(sequence, m_clipId); clip != nullptr)
                {
                    RemoveNotes(clip->Notes, m_removed);
                    AddNotes(clip->Notes, m_added);
                }
            }

            void Revert(_Inout_ Sequence& sequence) override
            {
                if (auto clip = FindClip(sequence, m_clipId); clip != nullptr)
                {
                    RemoveNotes(clip->Notes, m_added);
                    AddNotes(clip->Notes, m_removed);
                }
            }

            size_t Cost() const noexcept override
            {
                return sizeof(*this) + (m_removed.size() + m_added.size()) * sizeof(Note);
            }

        private:
            std::wstring m_clipId{};
            std::vector<Note> m_removed{};
            std::vector<Note> m_added{};
        };

        size_t StepCost(_In_ std::vector<std::unique_ptr<SequenceChange>> const& changes) noexcept
        {
            size_t cost{ 0 };

            for (auto const& change : changes)
            {
                cost += change != nullptr ? change->Cost() : 0;
            }

            return cost;
        }

        std::wstring const EmptyName{};
    }

    _Use_decl_annotations_
    std::unique_ptr<SequenceChange> MakeNoteChange(std::wstring clipId, std::vector<Note> removed, std::vector<Note> added)
    {
        return std::make_unique<NoteChange>(std::move(clipId), std::move(removed), std::move(added));
    }

    _Use_decl_annotations_
    void UndoStack::Commit(std::wstring name, std::vector<std::unique_ptr<SequenceChange>> changes)
    {
        std::erase_if(changes, [](std::unique_ptr<SequenceChange> const& change) { return change == nullptr; });

        if (changes.empty())
        {
            return;
        }

        for (auto const& step : m_redo)
        {
            m_cost -= std::min(m_cost, step.Cost);
        }

        m_redo.clear();

        Step step{};
        step.Name = std::move(name);
        step.Cost = StepCost(changes);
        step.Changes = std::move(changes);

        m_cost += step.Cost;
        m_undo.push_back(std::move(step));

        Trim();
    }

    _Use_decl_annotations_
    void UndoStack::ApplyAndCommit(Sequence& sequence, std::wstring name, std::vector<std::unique_ptr<SequenceChange>> changes)
    {
        for (auto const& change : changes)
        {
            if (change != nullptr)
            {
                change->Apply(sequence);
            }
        }

        Commit(std::move(name), std::move(changes));
    }

    _Use_decl_annotations_
    bool UndoStack::Undo(Sequence& sequence)
    {
        if (m_undo.empty())
        {
            return false;
        }

        auto step = std::move(m_undo.back());
        m_undo.pop_back();

        // Backwards, so a step that changed the same thing twice ends where it started.
        for (auto change = step.Changes.rbegin(); change != step.Changes.rend(); ++change)
        {
            (*change)->Revert(sequence);
        }

        m_redo.push_back(std::move(step));
        return true;
    }

    _Use_decl_annotations_
    bool UndoStack::Redo(Sequence& sequence)
    {
        if (m_redo.empty())
        {
            return false;
        }

        auto step = std::move(m_redo.back());
        m_redo.pop_back();

        for (auto const& change : step.Changes)
        {
            change->Apply(sequence);
        }

        m_undo.push_back(std::move(step));
        return true;
    }

    std::wstring const& UndoStack::UndoName() const noexcept
    {
        return m_undo.empty() ? EmptyName : m_undo.back().Name;
    }

    std::wstring const& UndoStack::RedoName() const noexcept
    {
        return m_redo.empty() ? EmptyName : m_redo.back().Name;
    }

    void UndoStack::Clear() noexcept
    {
        m_undo.clear();
        m_redo.clear();
        m_cost = 0;
    }

    void UndoStack::Trim() noexcept
    {
        // The newest step is always kept, however big, so the last edit can be undone.
        while (m_cost > m_maximumCost && m_undo.size() > 1)
        {
            m_cost -= std::min(m_cost, m_undo.front().Cost);
            m_undo.erase(m_undo.begin());
        }
    }
}
