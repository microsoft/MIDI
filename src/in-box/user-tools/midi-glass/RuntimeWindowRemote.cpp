// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The runtime window's half of MIDI remote control: drawing the controls a rule turned off or
// hid, and letting go of anything a finger was holding on them. Which controls those are is
// LivePlayer's to say.

#include "pch.h"
#include "RuntimeWindow.xaml.h"

namespace winrt::midiglass::implementation
{
    void RuntimeWindow::ApplyControlStates()
    {
        try
        {
            if (m_player == nullptr || m_closing)
            {
                return;
            }

            // Only after a fresh build, where everything starts on and in sight, so only the
            // controls a rule changed need touching.
            auto const& states = m_player->ControlStates();

            if (!states.AnyChanged())
            {
                return;
            }

            for (size_t item = 0; item < m_renderer.ItemCount(); ++item)
            {
                if (!states.TakesInput(m_renderer.ControlIndexOf(item)))
                {
                    ApplyControlState(item);
                }
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show which controls are turned off.")
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnControlStatesChanged(std::vector<uint32_t> const& controlIndexes)
    {
        try
        {
            if (m_player == nullptr || m_closing)
            {
                return;
            }

            for (auto const controlIndex : controlIndexes)
            {
                size_t item{ 0 };

                // A control on another page is drawn the right way when that page is built.
                if (m_renderer.TryFindItem(controlIndex, item))
                {
                    ApplyControlState(item);
                }
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show a control a rule turned off.")
    }

    _Use_decl_annotations_
    void RuntimeWindow::ApplyControlState(size_t itemIndex)
    {
        auto const& states = m_player->ControlStates();
        auto const controlIndex = m_renderer.ControlIndexOf(itemIndex);
        auto const enabled = states.IsEnabled(controlIndex);
        auto const shown = states.IsShown(controlIndex);

        // Let go first, while the control still takes input, so a held note ends.
        if (!enabled || !shown)
        {
            m_input.ReleaseItem(itemIndex);
        }

        m_renderer.SetControlState(itemIndex, enabled, shown);

        if (auto const element = m_renderer.ElementAt(itemIndex))
        {
            element.IsHitTestVisible(m_renderer.TakesHitsAt(itemIndex) && enabled && shown);
        }
    }
}
