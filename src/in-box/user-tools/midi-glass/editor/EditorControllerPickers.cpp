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
                : picks == SwitchPicks::RootNote ? RootPickerSlice
                : ScalePickerSlice;

            return positions * slice + PickerEnds;
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

        auto const wantsRoot = !HasKeyPicker(m_document, SwitchPicks::RootNote);
        auto const wantsScale = !HasKeyPicker(m_document, SwitchPicks::Scale);
        auto const count = (wantsRoot ? size_t{ 1 } : size_t{ 0 }) + (wantsScale ? size_t{ 1 } : size_t{ 0 });

        if (count == 0 || page->Controls.size() + count > MaximumControlsPerPage)
        {
            return added;
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
}
