// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include "PadGridChanges.h"
#include "FretboardChanges.h"

#include <algorithm>
#include <iterator>

namespace glass
{
    namespace
    {
        bool IsRoot(_In_ int32_t root) noexcept
        {
            return root >= 0 && root <= 11;
        }

        bool IsOffered(_In_ int32_t scale) noexcept
        {
            return std::any_of(std::begin(PadScaleOrder), std::end(PadScaleOrder),
                [scale](MusicalScale offered) { return static_cast<int32_t>(offered) == scale; });
        }

        int32_t ScaleOrderIndex(_In_ MusicalScale scale) noexcept
        {
            auto const found = std::find(std::begin(PadScaleOrder), std::end(PadScaleOrder), scale);

            return found == std::end(PadScaleOrder) ? 0 : static_cast<int32_t>(found - std::begin(PadScaleOrder));
        }

        int32_t Wrap(_In_ int32_t value, _In_ int32_t count) noexcept
        {
            return ((value % count) + count) % count;
        }

        // The row offset preset a grid is on, in its own units, or -1 when it is between them.
        int32_t RowOffsetIndex(_In_ PadGridSpec const& spec) noexcept
        {
            auto const inKey = PlaysOnlyTheKey(spec);
            auto const& offsets = inKey ? RowOffsetScaleSteps : RowOffsetSemitones;
            auto const current = inKey ? spec.RowSteps : spec.RowInterval;

            for (int32_t index = 0; index < RowOffsetPickerPositions; ++index)
            {
                if (offsets[index] == current)
                {
                    return index;
                }
            }

            return -1;
        }

        // The bottom left pad is the one a change must never push off the note range. For a
        // flowing grid that carries rows on, its row width is not known here; the pads in a row
        // stand in for it.
        bool StaysInRange(_In_ PadGridSpec const& built, _In_ bool hex, _In_ PadGridState const& state) noexcept
        {
            auto const spec = WithGridState(built, hex, state);
            auto const start = spec.StartNote;

            if (start < 0 || start > 127)
            {
                return false;
            }

            return PadNote(spec, hex, 0, 0, std::max(spec.PadsPerRow, 1)) >= 0;
        }

        bool IsGridWithId(_In_ Control const& control) noexcept
        {
            return IsPadGrid(control.Kind) && !control.Id.empty();
        }
    }

    _Use_decl_annotations_
    PadGridState StateOf(PadGridStates const& states, std::wstring const& id)
    {
        auto const found = states.find(id);

        return found == states.end() ? PadGridState{} : found->second;
    }

    _Use_decl_annotations_
    PadGridSpec WithGridState(PadGridSpec spec, bool hex, PadGridState const& state) noexcept
    {
        if (IsRoot(spec.KeyRoot))
        {
            if (IsRoot(state.Root))
            {
                spec.KeyRoot = state.Root;
            }

            if (IsOffered(state.Scale))
            {
                spec.Scale = static_cast<MusicalScale>(state.Scale);
            }

            if (state.InKeyOnly == 0 || state.InKeyOnly == 1)
            {
                spec.Notes = state.InKeyOnly == 1 ? PadNotes::InKey : PadNotes::Chromatic;
            }
        }

        // Continuous makes no sense on hexagons, whose rows always sit half a pad over.
        if (state.RowOffset >= 0 && state.RowOffset < RowOffsetPickerPositions && !(hex && state.RowOffset == 0))
        {
            spec.RowInterval = RowOffsetSemitones[state.RowOffset];
            spec.RowSteps = RowOffsetScaleSteps[state.RowOffset];
        }

        spec.StartNote += state.Octaves * 12;
        spec.ShiftRows = state.Rows;
        spec.ShiftSteps = state.Steps;

        return spec;
    }

    _Use_decl_annotations_
    bool TargetsGrid(PadGridTargets const& targets, std::wstring const& id) noexcept
    {
        if (targets.AllGrids)
        {
            return true;
        }

        return std::find(targets.ControlIds.begin(), targets.ControlIds.end(), id) != targets.ControlIds.end();
    }

    _Use_decl_annotations_
    PadGridState ChangedState(PadGridSpec const& built, bool hex, PadGridState state, PadGridFunction function) noexcept
    {
        auto const now = WithGridState(built, hex, state);
        auto const hasKey = IsRoot(built.KeyRoot);
        auto next = state;

        auto const rowOrRight = [&](bool rows, int32_t direction)
            {
                if (rows)
                {
                    next.Rows += direction;
                }
                else
                {
                    auto const inKey = PlaysOnlyTheKey(now);
                    auto const right = std::clamp(inKey ? now.RightSteps : now.RightInterval, MinimumRightInterval, MaximumPadInterval);

                    next.Steps += direction * right;
                }
            };

        switch (function)
        {
        case PadGridFunction::OctaveUp:     next.Octaves++; break;
        case PadGridFunction::OctaveDown:   next.Octaves--; break;
        case PadGridFunction::RowUp:        rowOrRight(true, 1); break;
        case PadGridFunction::RowDown:      rowOrRight(true, -1); break;
        case PadGridFunction::ShiftRight:   rowOrRight(false, 1); break;
        case PadGridFunction::ShiftLeft:    rowOrRight(false, -1); break;
        case PadGridFunction::SemitoneUp:   next.Steps++; break;
        case PadGridFunction::SemitoneDown: next.Steps--; break;

        case PadGridFunction::InKeyOnlyOn:
        case PadGridFunction::InKeyOnlyOff:
        case PadGridFunction::InKeyOnlyToggle:
        {
            if (!hasKey)
            {
                return state;
            }

            auto const wanted = function == PadGridFunction::InKeyOnlyOn ? 1
                : function == PadGridFunction::InKeyOnlyOff ? 0
                : (PlaysOnlyTheKey(now) ? 0 : 1);

            if (wanted == (PlaysOnlyTheKey(now) ? 1 : 0))
            {
                return state;
            }

            next.InKeyOnly = wanted;

            // A step counts semitones one way and notes of the scale the other, so a shift
            // along the row means something else once the notes change. It goes back to zero.
            next.Steps = 0;
            break;
        }

        case PadGridFunction::NextRoot:
        case PadGridFunction::PreviousRoot:
            if (!hasKey)
            {
                return state;
            }

            next.Root = Wrap(now.KeyRoot + (function == PadGridFunction::NextRoot ? 1 : -1), 12);
            break;

        case PadGridFunction::NextScale:
        case PadGridFunction::PreviousScale:
        {
            if (!hasKey)
            {
                return state;
            }

            constexpr auto count = static_cast<int32_t>(std::size(PadScaleOrder));

            auto const index = Wrap(ScaleOrderIndex(now.Scale) + (function == PadGridFunction::NextScale ? 1 : -1), count);

            next.Scale = static_cast<int32_t>(PadScaleOrder[static_cast<size_t>(index)]);
            break;
        }

        case PadGridFunction::NextRowOffset:
        {
            // The next preset above where the grid is, round to the first again. Hexagons skip
            // continuous.
            auto const first = hex ? 1 : 0;
            auto const inKey = PlaysOnlyTheKey(now);
            auto const& offsets = inKey ? RowOffsetScaleSteps : RowOffsetSemitones;
            auto const current = inKey ? now.RowSteps : now.RowInterval;

            auto index = first;

            for (int32_t candidate = first; candidate < RowOffsetPickerPositions; ++candidate)
            {
                if (offsets[candidate] > current)
                {
                    index = candidate;
                    break;
                }
            }

            next.RowOffset = index;
            break;
        }

        case PadGridFunction::Reset:
            return PadGridState{};

        default:
            return state;
        }

        return StaysInRange(built, hex, next) ? next : state;
    }

    _Use_decl_annotations_
    bool ApplyPadGridFunction(
        LayoutDocument const& document,
        PadGridTargets const& targets,
        PadGridFunction function,
        PadGridStates& states)
    {
        auto changed = false;

        for (auto const& page : document.Pages)
        {
            for (auto const& control : page.Controls)
            {
                // A fretboard has a capo and a neck rather than octaves and rows.
                if (IsFretboard(control.Kind) && !control.Id.empty() && TargetsGrid(targets, control.Id))
                {
                    if (!ReachesFretboards(function))
                    {
                        continue;
                    }

                    auto const before = StateOf(states, control.Id);
                    auto const after = ChangedFretState(control.Fretboard, before, function);

                    if (after == before)
                    {
                        continue;
                    }

                    if (after == PadGridState{})
                    {
                        states.erase(control.Id);
                    }
                    else
                    {
                        states[control.Id] = after;
                    }

                    changed = true;
                    continue;
                }

                if (!IsGridWithId(control) || !TargetsGrid(targets, control.Id))
                {
                    continue;
                }

                auto const before = StateOf(states, control.Id);
                auto const after = ChangedState(control.Pads, IsHexPadGrid(control.Kind), before, function);

                if (after == before)
                {
                    continue;
                }

                if (after == PadGridState{})
                {
                    states.erase(control.Id);
                }
                else
                {
                    states[control.Id] = after;
                }

                changed = true;
            }
        }

        return changed;
    }

    _Use_decl_annotations_
    bool ApplyPick(LayoutDocument const& document, Control const& picker, int32_t position, PadGridStates& states)
    {
        if (!IsKeyPicker(picker) || position < 0 || position >= SwitchPositionCount(picker))
        {
            return false;
        }

        auto const picks = picker.Switch.Picks;
        auto const pick = PickAt(picker, position);
        auto changed = false;

        for (auto const& page : document.Pages)
        {
            for (auto const& control : page.Controls)
            {
                // A fretboard takes a root and a scale for its dots, and a tuning.
                if (IsFretboard(control.Kind) && !control.Id.empty() && TargetsGrid(picker.Switch.Targets, control.Id))
                {
                    auto const before = StateOf(states, control.Id);
                    auto after = before;
                    auto const hasKey = HasFretKey(control.Fretboard);

                    if (picks == SwitchPicks::RootNote && hasKey && IsRoot(pick.Root))
                    {
                        after.Root = pick.Root;
                    }
                    else if (picks == SwitchPicks::Scale && hasKey && IsOffered(pick.Scale))
                    {
                        after.Scale = pick.Scale;
                    }
                    else if (picks == SwitchPicks::Tuning)
                    {
                        after.Tuning = static_cast<int32_t>(TuningAtPosition(position));
                    }

                    if (after != before)
                    {
                        states[control.Id] = after;
                        changed = true;
                    }

                    continue;
                }

                if (!IsGridWithId(control) || !TargetsGrid(picker.Switch.Targets, control.Id))
                {
                    continue;
                }

                auto const hex = IsHexPadGrid(control.Kind);
                auto const before = StateOf(states, control.Id);
                auto after = before;

                switch (picks)
                {
                case SwitchPicks::RootNote:
                    if (IsRoot(control.Pads.KeyRoot) && IsRoot(pick.Root))
                    {
                        after.Root = pick.Root;
                    }
                    break;

                case SwitchPicks::Scale:
                    if (IsRoot(control.Pads.KeyRoot) && IsOffered(pick.Scale))
                    {
                        after.Scale = pick.Scale;
                    }
                    break;

                case SwitchPicks::InKeyOnly:
                    if (IsRoot(control.Pads.KeyRoot) &&
                        PlaysOnlyTheKey(WithGridState(control.Pads, hex, before)) != (position == 1))
                    {
                        after.InKeyOnly = position;
                        after.Steps = 0;
                    }
                    break;

                case SwitchPicks::RowOffset:
                    if (!(hex && position == 0))
                    {
                        after.RowOffset = position;
                    }
                    break;

                case SwitchPicks::Octave:
                    after.Octaves = position - OctavePickerReach;
                    break;

                default:
                    break;
                }

                if (after == before || !StaysInRange(control.Pads, hex, after))
                {
                    continue;
                }

                if (after == PadGridState{})
                {
                    states.erase(control.Id);
                }
                else
                {
                    states[control.Id] = after;
                }

                changed = true;
            }
        }

        return changed;
    }

    _Use_decl_annotations_
    Control const* ShownGrid(LayoutDocument const& document, PadGridTargets const& targets, bool needsKey) noexcept
    {
        for (auto const& page : document.Pages)
        {
            for (auto const& control : page.Controls)
            {
                if (IsGridWithId(control) &&
                    TargetsGrid(targets, control.Id) &&
                    (!needsKey || IsRoot(control.Pads.KeyRoot)))
                {
                    return &control;
                }
            }
        }

        return nullptr;
    }

    _Use_decl_annotations_
    int32_t PickerPositionFor(LayoutDocument const& document, Control const& picker, PadGridStates const& states) noexcept
    {
        if (!IsKeyPicker(picker))
        {
            return -1;
        }

        auto const picks = picker.Switch.Picks;
        auto const needsKey = picks == SwitchPicks::RootNote || picks == SwitchPicks::Scale || picks == SwitchPicks::InKeyOnly;

        // A tuning picker, and a root or scale picker that targets only fretboards, show a
        // fretboard.
        if (picks == SwitchPicks::Tuning ||
            ((picks == SwitchPicks::RootNote || picks == SwitchPicks::Scale) &&
                ShownGrid(document, picker.Switch.Targets, true) == nullptr))
        {
            auto const* const fretboard = ShownFretboard(document, picker.Switch.Targets, picks != SwitchPicks::Tuning);

            if (fretboard == nullptr)
            {
                return -1;
            }

            try
            {
                auto const spec = WithFretState(fretboard->Fretboard, StateOf(states, fretboard->Id));

                if (picks == SwitchPicks::Tuning)
                {
                    auto const tuning = MatchingTuning(spec.Strings);

                    return tuning == FretTuning::Custom ? -1 : static_cast<int32_t>(tuning);
                }

                return PickerPosition(picker, { spec.KeyRoot, static_cast<int32_t>(spec.Scale) });
            }
            catch (...)
            {
                return -1;
            }
        }

        auto const* const grid = ShownGrid(document, picker.Switch.Targets, needsKey);

        if (grid == nullptr)
        {
            return -1;
        }

        try
        {
            auto const state = StateOf(states, grid->Id);
            auto const hex = IsHexPadGrid(grid->Kind);
            auto const spec = WithGridState(grid->Pads, hex, state);

            switch (picks)
            {
            case SwitchPicks::RootNote:
            case SwitchPicks::Scale:
                return PickerPosition(picker, { spec.KeyRoot, static_cast<int32_t>(spec.Scale) });

            case SwitchPicks::InKeyOnly:
                return PlaysOnlyTheKey(spec) ? 1 : 0;

            case SwitchPicks::RowOffset:
                return RowOffsetIndex(spec);

            case SwitchPicks::Octave:
            {
                auto const position = state.Octaves + OctavePickerReach;

                return position >= 0 && position < OctavePickerPositions ? position : -1;
            }

            default:
                return -1;
            }
        }
        catch (...)
        {
            return -1;
        }
    }

    _Use_decl_annotations_
    double PickerValueFor(LayoutDocument const& document, Control const& picker, PadGridStates const& states) noexcept
    {
        auto const position = PickerPositionFor(document, picker, states);

        return position >= 0 ? SwitchValueOf(position, SwitchPositionCount(picker)) : -1.0;
    }

    _Use_decl_annotations_
    bool HasPickerFor(LayoutDocument const& document, SwitchPicks picks, std::wstring const& gridId) noexcept
    {
        for (auto const& page : document.Pages)
        {
            for (auto const& control : page.Controls)
            {
                if (IsKeyPicker(control) && control.Switch.Picks == picks && TargetsGrid(control.Switch.Targets, gridId))
                {
                    return true;
                }
            }
        }

        return false;
    }

    _Use_decl_annotations_
    PadGridReading ReadPadGrid(LayoutDocument const& document, PadGridTargets const& targets, PadGridStates const& states) noexcept
    {
        PadGridReading reading{};

        auto const* grid = ShownGrid(document, targets, true);

        if (grid == nullptr)
        {
            grid = ShownGrid(document, targets, false);
        }

        if (grid == nullptr)
        {
            return reading;
        }

        try
        {
            auto const state = StateOf(states, grid->Id);
            auto const spec = WithGridState(grid->Pads, IsHexPadGrid(grid->Kind), state);

            reading.HasGrid = true;
            reading.HasKey = IsRoot(spec.KeyRoot);
            reading.Root = reading.HasKey ? spec.KeyRoot : 0;
            reading.Scale = spec.Scale;
            reading.InKeyOnly = PlaysOnlyTheKey(spec);
            reading.Octaves = state.Octaves;
            reading.RowOffset = reading.InKeyOnly ? spec.RowSteps : spec.RowInterval;
        }
        catch (...)
        {
            reading = PadGridReading{};
        }

        return reading;
    }

    _Use_decl_annotations_
    bool ReadsAPadGrid(Control const& control) noexcept
    {
        return control.Kind == ControlKind::Readout && control.Readout.Source != ReadoutSource::Value;
    }

    _Use_decl_annotations_
    std::wstring SignedNumberText(int32_t value)
    {
        if (value > 0)
        {
            return L"+" + std::to_wstring(value);
        }

        if (value < 0)
        {
            return L"\u2212" + std::to_wstring(-value);
        }

        return L"0";
    }
}
