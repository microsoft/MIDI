// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Root and scale pickers: a switch that picks the key of every pad grid with one while the
// layout runs. The pick lives here, so the note a screen reader's press plays moves with it.
// Nothing picked is saved.

#include "pch.h"
#include "LivePlayer.h"
#include "PadGrid.h"

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
        auto const pick = PickAt(picker->Picker, position);

        auto next = m_pickedKey;

        if (pick.Root >= 0)
        {
            next.Root = pick.Root;
        }

        if (pick.Scale >= 0)
        {
            next.Scale = pick.Scale;
        }

        if (next.Root == m_pickedKey.Root && next.Scale == m_pickedKey.Scale)
        {
            return true;
        }

        m_pickedKey = next;

        for (auto& pads : m_padControls)
        {
            pads.StartNote = FirstPadNote(WithPickedKey(pads.Spec, m_pickedKey));
        }

        if (KeyPicked)
        {
            KeyPicked(m_pickedKey);
        }

        return true;
    }

    void LivePlayer::ForgetPickedKey() noexcept
    {
        m_pickedKey = {};

        for (auto& pads : m_padControls)
        {
            pads.StartNote = FirstPadNote(pads.Spec);
        }
    }
}
