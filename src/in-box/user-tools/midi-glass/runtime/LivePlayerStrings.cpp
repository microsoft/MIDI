// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Fretboards: a finger on a string, a strum, the neck strip and the mute row, turned into notes
// and bends on each string's channel.

#include "pch.h"
#include "LivePlayer.h"
#include "FretboardChanges.h"

namespace glass
{
    _Use_decl_annotations_
    LivePlayer::FretEntry* LivePlayer::FindFretControl(uint32_t controlIndex) noexcept
    {
        for (auto& entry : m_fretControls)
        {
            if (entry.ControlIndex == controlIndex)
            {
                return &entry;
            }
        }

        return nullptr;
    }

    _Use_decl_annotations_
    void LivePlayer::SendStringActions(FretEntry& entry, std::span<StringAction const> actions) noexcept
    {
        auto const output = entry.Voices.Spec().Output;

        for (auto const& action : actions)
        {
            // How far a bend reaches goes ahead of the first note on each channel, so the bend
            // that comes with the note already lands where it should.
            if (action.Kind == StringActionKind::Bend)
            {
                StringSend setup{};

                if (StringSetupFor(output, action.String, setup))
                {
                    auto const bit = 1u << (setup.ChannelOffset & 0x1F);

                    if ((entry.ToldSensitivity & bit) == 0)
                    {
                        entry.ToldSensitivity |= bit;

                        SendPrepared(entry.ControlIndex, m_engine.EvaluateString(entry.ControlIndex, setup, m_sends));
                    }
                }
            }

            // Straight out, in order, and never through a throttle, the same as a pad grid.
            SendPrepared(
                entry.ControlIndex,
                m_engine.EvaluateString(entry.ControlIndex, StringSendFor(output, action), m_sends));
        }

        if (StringsSounding && !actions.empty())
        {
            uint32_t sounding{ 0 };

            for (int32_t string = 0; string < MaximumFretStrings; ++string)
            {
                if (entry.Voices.IsSounding(string))
                {
                    sounding |= 1u << string;
                }
            }

            StringsSounding(entry.ControlIndex, sounding);
        }
    }

    _Use_decl_annotations_
    void LivePlayer::FretTouched(uint32_t controlIndex, FretTouch const& touch)
    {
        auto* const entry = FindFretControl(controlIndex);

        if (entry == nullptr)
        {
            return;
        }

        // A fretboard that is off or out of sight starts nothing; a finger lifting still ends
        // its string.
        if ((touch.Phase == FretTouchPhase::Down || touch.Phase == FretTouchPhase::Strum) && !TakesInput(controlIndex))
        {
            return;
        }

        std::array<StringAction, MaximumStringActions> actions{};
        uint32_t count{ 0 };

        switch (touch.Phase)
        {
        case FretTouchPhase::Down:
            count = entry->Voices.Press(touch.Touch, touch.String, touch.Fret, touch.FretPosition, touch.Velocity, actions);
            break;

        case FretTouchPhase::Move:
            count = entry->Voices.Move(touch.Touch, touch.Fret, touch.FretPosition, touch.Bend, actions);
            break;

        case FretTouchPhase::Up:
            count = entry->Voices.Release(touch.Touch, actions);
            break;

        case FretTouchPhase::Strum:
            count = entry->Voices.Strum(touch.String, touch.Velocity, actions);
            break;

        default:
            break;
        }

        SendStringActions(*entry, std::span<StringAction const>{ actions.data(), count });
    }

    _Use_decl_annotations_
    void LivePlayer::NeckMoved(uint32_t controlIndex, int32_t firstFret)
    {
        auto const* const entry = FindFretControl(controlIndex);

        if (entry == nullptr || entry->ControlId.empty() || !TakesInput(controlIndex))
        {
            return;
        }

        auto const before = StateOf(m_gridStates, entry->ControlId);
        auto const after = MovedNeckState(entry->Spec, before, firstFret);

        if (after == before)
        {
            return;
        }

        m_gridStates[entry->ControlId] = after;

        AnnounceGridChanges();
    }

    void LivePlayer::ConfigureFretboards()
    {
        for (auto& entry : m_fretControls)
        {
            auto const before = entry.Voices.Spec().Output;
            auto wanted = WithFretState(entry.Spec, StateOf(m_gridStates, entry.ControlId));

            // New strings are a new instrument. What the old ones were sounding stops first, or
            // a string the new tuning doesn't have would ring with nothing left to stop it.
            if (wanted.Strings != entry.Voices.Spec().Strings)
            {
                std::array<StringAction, MaximumFretStrings> actions{};

                auto const count = entry.Voices.MuteAll(actions);

                SendStringActions(entry, std::span<StringAction const>{ actions.data(), count });
            }

            entry.Voices.Configure(wanted);

            // A different output is a different set of channels to tell.
            if (entry.Voices.Spec().Output != before)
            {
                entry.ToldSensitivity = 0;
            }
        }
    }

    void LivePlayer::ReleaseAllFretboards() noexcept
    {
        for (auto& entry : m_fretControls)
        {
            std::array<StringAction, MaximumFretStrings> actions{};

            auto const count = entry.Voices.ReleaseAll(actions);

            SendStringActions(entry, std::span<StringAction const>{ actions.data(), count });
        }
    }

    _Use_decl_annotations_
    void LivePlayer::MuteFretboards(PadGridTargets const& targets) noexcept
    {
        for (auto& entry : m_fretControls)
        {
            if (!TargetsGrid(targets, entry.ControlId))
            {
                continue;
            }

            std::array<StringAction, MaximumFretStrings> actions{};

            auto const count = entry.Voices.MuteAll(actions);

            SendStringActions(entry, std::span<StringAction const>{ actions.data(), count });
        }
    }
}
