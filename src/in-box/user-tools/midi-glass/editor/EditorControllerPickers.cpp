// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, so the unit tests compile it unchanged.
//
// Root and scale pickers: switches that pick the key of every pad grid while a layout runs.

#include "EditorController.h"
#include "ControlFactory.h"
#include "PadGrid.h"
#include "PadGridChanges.h"
#include "Fretboard.h"

#include <algorithm>

namespace glass
{
    namespace
    {
        // Along a picker, enough for each name to print whole: a note is a letter or two, and a
        // scale is a word. Standing on end, every name has the picker's whole width.
        constexpr double RootPickerSlice = 30.0;
        constexpr double ScalePickerSlice = 80.0;
        constexpr double StandingPickerSlice = 30.0;
        constexpr double PickerEnds = 16.0;

        constexpr double PickerThickness = 44.0;
        constexpr double StandingPickerWidth = 128.0;

        // Between a grid and a picker put next to it, and above a picker for its label.
        constexpr double PickerGap = 12.0;
        constexpr double LabelRoom = 24.0;

        double PickerLength(_In_ SwitchPicks picks, _In_ int32_t positions, _In_ bool across) noexcept
        {
            auto const slice = !across ? StandingPickerSlice
                : picks == SwitchPicks::RootNote || picks == SwitchPicks::Octave ? RootPickerSlice
                : ScalePickerSlice;

            return positions * slice + PickerEnds;
        }

        // A grid button: a plain button with one pad grid row, run when it is pressed.
        constexpr double GridButtonWidth = 88.0;
        constexpr double GridButtonHeight = 44.0;
        constexpr double GridWideWidth = 176.0;

        // Wide enough for a key, an octave and a row offset at a size that can be read.
        constexpr double GridReadoutWidth = 260.0;

        ControlMessage PadGridRow(_In_ PadGridFunction function, _In_ PadGridTargets const& targets)
        {
            ControlMessage row{};

            row.Trigger = MessageTrigger::TurnsOn;
            row.Kind = MessageKind::PadGrid;
            row.PadFunction = function;
            row.PadTargets = targets;

            return row;
        }
    }

    _Use_decl_annotations_
    bool EditorController::SetSwitchPicks(std::wstring const& id, SwitchPicks picks)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || control->Kind != ControlKind::Switch || control->Switch.Picks == picks)
        {
            return false;
        }

        control->Switch.Picks = picks;

        if (picks == SwitchPicks::Scale && control->Switch.Scales.size() < static_cast<size_t>(MinimumSwitchPositions))
        {
            control->Switch.Scales = DefaultPickerScales();
        }

        // Twelve names squeezed into a three position switch are twelve smudges. It grows along
        // the way it already runs, and stays on the page.
        if (picks != SwitchPicks::Nothing)
        {
            auto const across = control->Width >= control->Height;
            auto const room = static_cast<double>(across ? m_document.PageWidth : m_document.PageHeight);
            auto const wanted = std::min(PickerLength(picks, SwitchPositionCount(*control), across), room);

            auto& length = across ? control->Width : control->Height;
            auto& start = across ? control->X : control->Y;

            if (length < wanted)
            {
                length = wanted;
                start = std::clamp(start, 0.0, std::max(room - length, 0.0));
            }
        }

        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetSwitchScales(std::wstring const& id, std::vector<MusicalScale> const& scales)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || control->Kind != ControlKind::Switch)
        {
            return false;
        }

        std::vector<MusicalScale> wanted{};

        for (auto const scale : PadScaleOrder)
        {
            if (std::find(scales.begin(), scales.end(), scale) != scales.end())
            {
                wanted.push_back(scale);
            }
        }

        if (wanted.size() < static_cast<size_t>(MinimumSwitchPositions) || wanted == control->Switch.Scales)
        {
            return false;
        }

        control->Switch.Scales = std::move(wanted);
        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    std::vector<std::wstring> EditorController::AddKeyPickers(
        std::wstring const& gridId,
        std::wstring const& rootLabel,
        std::wstring const& scaleLabel)
    {
        std::vector<std::wstring> added{};

        auto* const page = MutablePage();

        if (page == nullptr)
        {
            return added;
        }

        auto const grid = std::find_if(page->Controls.begin(), page->Controls.end(),
            [&gridId](Control const& control) { return control.Id == gridId; });

        if (grid == page->Controls.end() || !IsPadGrid(grid->Kind))
        {
            return added;
        }

        auto const wantsRoot = !HasPickerFor(m_document, SwitchPicks::RootNote, gridId);
        auto const wantsScale = !HasPickerFor(m_document, SwitchPicks::Scale, gridId);
        auto const count = (wantsRoot ? size_t{ 1 } : size_t{ 0 }) + (wantsScale ? size_t{ 1 } : size_t{ 0 });

        if (count == 0 || page->Controls.size() + count > MaximumControlsPerPage)
        {
            return added;
        }

        // The first pickers on a layout change every grid, the way they always have. Once some
        // grid has its own, new ones change only this grid, so the two don't fight.
        PadGridTargets targetsFor[2]{};

        for (auto const picks : { SwitchPicks::RootNote, SwitchPicks::Scale })
        {
            if (HasKeyPicker(m_document, picks))
            {
                targetsFor[picks == SwitchPicks::RootNote ? 0 : 1] = PadGridTargets{ false, { gridId } };
            }
        }

        // Copied out, because adding to the page can move the grid in memory.
        auto const gridX = grid->X;
        auto const gridY = grid->Y;
        auto const gridWidth = grid->Width;
        auto const gridHeight = grid->Height;

        auto const pageWidth = static_cast<double>(m_document.PageWidth);
        auto const pageHeight = static_cast<double>(m_document.PageHeight);

        auto const make = [&](SwitchPicks picks, std::wstring const& label, double x, double y, double width, double height)
            {
                auto control = MakeNewControl(
                    ControlKind::Switch, x, y, m_document.PageWidth, m_document.PageHeight, std::wstring{}, *page);

                // A picker sends nothing, so the rows a new switch arrives with would only wait.
                control.Messages.clear();
                control.Label = label;
                control.Switch.Picks = picks;
                control.Switch.Targets = targetsFor[picks == SwitchPicks::RootNote ? 0 : 1];

                if (picks == SwitchPicks::Scale)
                {
                    control.Switch.Scales = DefaultPickerScales();
                }

                control.Width = std::min(width, pageWidth);
                control.Height = std::min(height, pageHeight);
                control.X = std::clamp(x, 0.0, std::max(pageWidth - control.Width, 0.0));
                control.Y = std::clamp(y, 0.0, std::max(pageHeight - control.Height, 0.0));

                added.push_back(control.Id);
                page->Controls.push_back(std::move(control));
            };

        if (wantsRoot)
        {
            // Along the top of the grid and as wide, or along the bottom with no room above.
            auto const width = std::max(gridWidth, PickerLength(SwitchPicks::RootNote, RootPickerPositions, true));
            auto const above = gridY - PickerGap - PickerThickness;
            auto const y = above >= LabelRoom ? above : gridY + gridHeight + PickerGap + LabelRoom;

            make(SwitchPicks::RootNote, rootLabel, gridX, y, width, PickerThickness);
        }

        if (wantsScale)
        {
            // Standing beside the grid, one scale to a row, on whichever side has the room.
            auto const scales = static_cast<int32_t>(DefaultPickerScales().size());
            auto const height = std::max(gridHeight, PickerLength(SwitchPicks::Scale, scales, false));
            auto const right = gridX + gridWidth + PickerGap;
            auto const x = right + StandingPickerWidth <= pageWidth ? right : gridX - PickerGap - StandingPickerWidth;

            make(SwitchPicks::Scale, scaleLabel, x, gridY, StandingPickerWidth, height);
        }

        Commit(EditNames::Add);

        return added;
    }

    namespace
    {
        // Only ids of pad grids, each once, in the order given.
        PadGridTargets CleanTargets(_In_ LayoutDocument const& document, _In_ PadGridTargets const& targets)
        {
            PadGridTargets clean{};

            clean.AllGrids = targets.AllGrids;

            if (targets.AllGrids)
            {
                return clean;
            }

            for (auto const& id : targets.ControlIds)
            {
                auto const* const control = document.FindControl(id);

                if (control != nullptr && (IsPadGrid(control->Kind) || IsFretboard(control->Kind)) &&
                    std::find(clean.ControlIds.begin(), clean.ControlIds.end(), id) == clean.ControlIds.end())
                {
                    clean.ControlIds.push_back(id);
                }
            }

            return clean;
        }
    }

    _Use_decl_annotations_
    bool EditorController::SetSwitchTargets(std::wstring const& id, PadGridTargets const& targets)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || control->Kind != ControlKind::Switch)
        {
            return false;
        }

        auto wanted = CleanTargets(m_document, targets);

        if (wanted == control->Switch.Targets)
        {
            return false;
        }

        control->Switch.Targets = std::move(wanted);
        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetReadout(std::wstring const& id, ReadoutSpec const& readout)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || control->Kind != ControlKind::Readout)
        {
            return false;
        }

        auto const source = static_cast<int32_t>(readout.Source);
        auto const wantedSource = source >= 0 && source <= static_cast<int32_t>(ReadoutSource::Tuning)
            ? readout.Source
            : ReadoutSource::Value;

        auto wantedTargets = CleanTargets(m_document, readout.Targets);

        if (wantedSource == control->Readout.Source && wantedTargets == control->Readout.Targets)
        {
            return false;
        }

        control->Readout.Source = wantedSource;
        control->Readout.Targets = std::move(wantedTargets);
        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    std::vector<std::wstring> EditorController::AddGridButtons(
        std::wstring const& gridId,
        GridButtonChoices const& choices,
        GridButtonLabels const& labels)
    {
        std::vector<std::wstring> added{};

        auto* const page = MutablePage();

        if (page == nullptr)
        {
            return added;
        }

        auto const grid = std::find_if(page->Controls.begin(), page->Controls.end(),
            [&gridId](Control const& control) { return control.Id == gridId; });

        if (grid == page->Controls.end() || !IsPadGrid(grid->Kind))
        {
            return added;
        }

        // What goes in the row, left to right: its kind, its width and what it does.
        struct Planned
        {
            ControlKind Kind{ ControlKind::Button };
            double Width{ GridButtonWidth };
            PadGridFunction Function{ PadGridFunction::OctaveUp };
            std::wstring const* Label{ nullptr };
        };

        std::vector<Planned> planned{};

        if (choices.Octave)
        {
            planned.push_back({ ControlKind::Button, GridButtonWidth, PadGridFunction::OctaveDown, &labels.OctaveDown });
            planned.push_back({ ControlKind::Button, GridButtonWidth, PadGridFunction::OctaveUp, &labels.OctaveUp });
        }

        if (choices.Rows)
        {
            planned.push_back({ ControlKind::Button, GridButtonWidth, PadGridFunction::RowDown, &labels.RowDown });
            planned.push_back({ ControlKind::Button, GridButtonWidth, PadGridFunction::RowUp, &labels.RowUp });
        }

        if (choices.InKeyOnly)
        {
            planned.push_back({ ControlKind::Switch, GridWideWidth, PadGridFunction::InKeyOnlyToggle, &labels.InKeyOnly });
        }

        if (choices.Readout)
        {
            planned.push_back({ ControlKind::Readout, GridReadoutWidth, PadGridFunction::OctaveUp, &labels.Readout });
        }

        if (choices.Reset)
        {
            planned.push_back({ ControlKind::Button, GridButtonWidth, PadGridFunction::Reset, &labels.Reset });
        }

        if (planned.empty() || page->Controls.size() + planned.size() > MaximumControlsPerPage)
        {
            return added;
        }

        PadGridTargets const targets{ false, { gridId } };

        // Copied out, because adding to the page can move the grid in memory.
        auto const gridX = grid->X;
        auto const gridY = grid->Y;
        auto const gridHeight = grid->Height;

        auto const pageWidth = static_cast<double>(m_document.PageWidth);
        auto const pageHeight = static_cast<double>(m_document.PageHeight);

        // Under the grid, with room above each for its label, or over it when the page ends.
        auto const below = gridY + gridHeight + PickerGap + LabelRoom;
        auto y = below + GridButtonHeight <= pageHeight ? below : gridY - PickerGap - GridButtonHeight;

        // The whole row starts far enough left to stay on the page, a gap in from its edge.
        auto rowWidth = -PickerGap;

        for (auto const& plan : planned)
        {
            rowWidth += std::min(plan.Width, pageWidth) + PickerGap;
        }

        auto const left = std::clamp(gridX, 0.0, std::max(pageWidth - PickerGap - rowWidth, 0.0));

        // Lower still while the row would land on something already on the page.
        {
            auto const blocked = [&](double top)
                {
                    return std::any_of(page->Controls.begin(), page->Controls.end(), [&](Control const& other)
                        {
                            return other.Kind != ControlKind::Panel &&
                                other.X < left + rowWidth && other.X + other.Width > left &&
                                other.Y < top + GridButtonHeight && other.Y + other.Height > top - LabelRoom;
                        });
                };

            for (auto top = y; top + GridButtonHeight + PickerGap <= pageHeight; top += GridButtonHeight + LabelRoom + PickerGap)
            {
                if (!blocked(top))
                {
                    y = top;
                    break;
                }
            }
        }

        auto x = left;

        for (auto const& plan : planned)
        {
            auto control = MakeNewControl(
                plan.Kind, x, y, m_document.PageWidth, m_document.PageHeight, std::wstring{}, *page);

            // Nothing here sends MIDI, so the rows a new control arrives with would only wait.
            control.Messages.clear();
            control.Feedback = FeedbackBinding{};
            control.Label = *plan.Label;

            if (plan.Kind == ControlKind::Button)
            {
                control.Messages.push_back(PadGridRow(plan.Function, targets));
            }
            else if (plan.Kind == ControlKind::Switch)
            {
                control.Switch.Picks = SwitchPicks::InKeyOnly;
                control.Switch.Targets = targets;
            }
            else
            {
                control.Readout.Source = ReadoutSource::PadSummary;
                control.Readout.Targets = targets;
            }

            control.Width = std::min(plan.Width, pageWidth);
            control.Height = std::min(GridButtonHeight, pageHeight);
            control.X = std::clamp(x, 0.0, std::max(pageWidth - control.Width, 0.0));
            control.Y = std::clamp(y, 0.0, std::max(pageHeight - control.Height, 0.0));

            x += control.Width + PickerGap;

            added.push_back(control.Id);
            page->Controls.push_back(std::move(control));
        }

        Commit(EditNames::Add);

        return added;
    }
}
