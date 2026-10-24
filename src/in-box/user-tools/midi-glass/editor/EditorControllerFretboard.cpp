// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, so the unit tests compile it unchanged.
//
// Fretboards in the editor: their settings, and the buttons that change them while the layout
// runs.

#include "EditorController.h"
#include "ControlFactory.h"
#include "Fretboard.h"

#include <algorithm>

namespace glass
{
    namespace
    {
        // The same sizes the grid buttons use, so the two rows line up when they share a page.
        constexpr double FretButtonWidth = 88.0;
        constexpr double FretButtonHeight = 44.0;
        constexpr double FretReadoutWidth = 120.0;
        constexpr double FretGap = 12.0;
        constexpr double FretLabelRoom = 24.0;

        // The tuning picker stands beside the fretboard, one tuning to a row.
        constexpr double TuningPickerWidth = 160.0;
        constexpr double TuningPickerSlice = 30.0;
        constexpr double TuningPickerEnds = 16.0;

        bool SameFretboard(_In_ FretboardSpec const& a, _In_ FretboardSpec const& b) noexcept
        {
            return a.Tuning == b.Tuning &&
                a.Strings == b.Strings &&
                a.Frets == b.Frets &&
                a.FirstFret == b.FirstFret &&
                a.LastFret == b.LastFret &&
                a.Capo == b.Capo &&
                a.Strum == b.Strum &&
                a.Play == b.Play &&
                a.Sustain == b.Sustain &&
                a.KeyRoot == b.KeyRoot &&
                a.Scale == b.Scale &&
                a.OutOfKey == b.OutOfKey &&
                a.BendRangeSemitones == b.BendRangeSemitones &&
                a.Slide == b.Slide &&
                a.HammerOns == b.HammerOns &&
                a.Output == b.Output;
        }
    }

    _Use_decl_annotations_
    bool EditorController::SetControlFretboard(std::wstring const& id, FretboardSpec const& fretboard)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || control->Kind != ControlKind::Fretboard)
        {
            return false;
        }

        // A list emptied by removing strings is still a fretboard with one string, not one that
        // quietly turns back into standard tuning.
        if (fretboard.Strings.empty())
        {
            return false;
        }

        auto wanted = NormalizedFretboard(fretboard);

        if (SameFretboard(wanted, control->Fretboard))
        {
            return false;
        }

        // Everything but the fields this build does not know, which stay as they were.
        wanted.Unknown = control->Fretboard.Unknown;
        control->Fretboard = std::move(wanted);

        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    std::vector<std::wstring> EditorController::AddFretboardButtons(
        std::wstring const& fretboardId,
        FretboardButtonLabels const& labels)
    {
        std::vector<std::wstring> added{};

        auto* const page = MutablePage();

        if (page == nullptr)
        {
            return added;
        }

        auto const fretboard = std::find_if(page->Controls.begin(), page->Controls.end(),
            [&fretboardId](Control const& control) { return control.Id == fretboardId; });

        if (fretboard == page->Controls.end() || !IsFretboard(fretboard->Kind))
        {
            return added;
        }

        // A tuning picker, then a row of five buttons and a readout.
        constexpr size_t Count = 7;

        if (page->Controls.size() + Count > MaximumControlsPerPage)
        {
            return added;
        }

        PadGridTargets const targets{ false, { fretboardId } };

        // Copied out, because adding to the page can move the fretboard in memory.
        auto const boardX = fretboard->X;
        auto const boardY = fretboard->Y;
        auto const boardWidth = fretboard->Width;
        auto const boardHeight = fretboard->Height;

        auto const pageWidth = static_cast<double>(m_document.PageWidth);
        auto const pageHeight = static_cast<double>(m_document.PageHeight);

        auto const place = [&](Control& control, double x, double y, double width, double height)
            {
                control.Width = std::min(width, pageWidth);
                control.Height = std::min(height, pageHeight);
                control.X = std::clamp(x, 0.0, std::max(pageWidth - control.Width, 0.0));
                control.Y = std::clamp(y, 0.0, std::max(pageHeight - control.Height, 0.0));
            };

        // The tuning picker, standing beside the fretboard on whichever side has the room.
        {
            auto control = MakeNewControl(
                ControlKind::Switch, boardX, boardY, m_document.PageWidth, m_document.PageHeight, std::wstring{}, *page);

            control.Messages.clear();
            control.Feedback = FeedbackBinding{};
            control.Label = labels.Tuning;
            control.Switch.Picks = SwitchPicks::Tuning;
            control.Switch.Targets = targets;

            auto const right = boardX + boardWidth + FretGap;
            auto const x = right + TuningPickerWidth <= pageWidth ? right : boardX - FretGap - TuningPickerWidth;
            auto const height = std::max(boardHeight, FretTuningPresetCount * TuningPickerSlice + TuningPickerEnds);

            place(control, x, boardY, TuningPickerWidth, height);

            added.push_back(control.Id);
            page->Controls.push_back(std::move(control));
        }

        // The row under the fretboard, or over it when the page ends.
        struct Planned
        {
            ControlKind Kind{ ControlKind::Button };
            double Width{ FretButtonWidth };
            PadGridFunction Function{ PadGridFunction::CapoUp };
            std::wstring const* Label{ nullptr };
        };

        Planned const planned[]
        {
            { ControlKind::Button, FretButtonWidth, PadGridFunction::CapoDown, &labels.CapoDown },
            { ControlKind::Readout, FretReadoutWidth, PadGridFunction::CapoUp, &labels.Capo },
            { ControlKind::Button, FretButtonWidth, PadGridFunction::CapoUp, &labels.CapoUp },
            { ControlKind::Button, FretButtonWidth, PadGridFunction::DownTheNeck, &labels.DownTheNeck },
            { ControlKind::Button, FretButtonWidth, PadGridFunction::UpTheNeck, &labels.UpTheNeck },
            { ControlKind::Button, FretButtonWidth, PadGridFunction::MuteStrings, &labels.Mute },
        };

        auto const below = boardY + boardHeight + FretGap + FretLabelRoom;
        auto const y = below + FretButtonHeight <= pageHeight ? below : boardY - FretGap - FretButtonHeight;

        auto rowWidth = -FretGap;

        for (auto const& plan : planned)
        {
            rowWidth += plan.Width + FretGap;
        }

        auto x = std::clamp(boardX, 0.0, std::max(pageWidth - FretGap - rowWidth, 0.0));

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
                ControlMessage row{};

                row.Trigger = MessageTrigger::TurnsOn;
                row.Kind = MessageKind::PadGrid;
                row.PadFunction = plan.Function;
                row.PadTargets = targets;

                control.Messages.push_back(std::move(row));
            }
            else
            {
                control.Readout.Source = ReadoutSource::Capo;
                control.Readout.Targets = targets;
            }

            place(control, x, y, plan.Width, FretButtonHeight);

            x += control.Width + FretGap;

            added.push_back(control.Id);
            page->Controls.push_back(std::move(control));
        }

        Commit(EditNames::Add);

        return added;
    }
}
