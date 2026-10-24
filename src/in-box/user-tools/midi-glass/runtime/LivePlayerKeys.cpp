// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Pickers and grid buttons: switches and pad grid rows that change pad grids while the layout
// runs. What has changed lives here, so the note a screen reader's press plays moves with it.
// Nothing changed is saved.

#include "pch.h"
#include "LivePlayer.h"
#include "PadGrid.h"
#include "PadGridChanges.h"

namespace glass
{
    _Use_decl_annotations_
    bool LivePlayer::PickFromSwitch(uint32_t controlIndex, double value)
    {
        auto const picker = std::find_if(m_pickerControls.begin(), m_pickerControls.end(),
            [controlIndex](PickerEntry const& entry) { return entry.ControlIndex == controlIndex; });

        if (picker == m_pickerControls.end())
        {
            return false;
        }

        auto const position = SwitchPositionAt(value, SwitchPositionCount(picker->Picker));

        if (ApplyPick(m_document, picker->Picker, position, m_gridStates))
        {
            AnnounceGridChanges();
        }

        return true;
    }

    _Use_decl_annotations_
    void LivePlayer::ChangePadGrids(PlanAction const& action)
    {
        // A mute changes nothing about the fretboards; it stops what they are sounding.
        if (action.PadFunction == PadGridFunction::MuteStrings)
        {
            MuteFretboards(action.PadTargets);
            return;
        }

        if (ApplyPadGridFunction(m_document, action.PadTargets, action.PadFunction, m_gridStates))
        {
            AnnounceGridChanges();
        }
    }

    void LivePlayer::AnnounceGridChanges()
    {
        ConfigureFretboards();

        for (auto& pads : m_padControls)
        {
            auto const spec = WithGridState(pads.Spec, pads.Hex, StateOf(m_gridStates, pads.ControlId));
            auto const first = PadNote(spec, pads.Hex, 0, 0, std::max(spec.PadsPerRow, 1));

            pads.StartNote = first >= 0 ? first : FirstPadNote(spec);
        }

        if (PadGridsChanged)
        {
            PadGridsChanged(m_gridStates);
        }
    }

    void LivePlayer::ForgetGridChanges() noexcept
    {
        m_gridStates.clear();

        try
        {
            ConfigureFretboards();
        }
        catch (...)
        {
        }

        for (auto& pads : m_padControls)
        {
            pads.StartNote = FirstPadNote(pads.Spec);
        }
    }
}
