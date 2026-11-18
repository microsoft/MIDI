// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "LaunchpadXController.h"

#include "SequenceModel.h"
#include "SequenceRender.h"

#include <algorithm>
#include <utility>

namespace midisequencer::launchpad
{
    namespace
    {
        [[nodiscard]] bool IsNoteAction(_In_ SurfaceActionKind kind) noexcept
        {
            return kind == SurfaceActionKind::EmitNoteOn ||
                kind == SurfaceActionKind::EmitNoteOff ||
                kind == SurfaceActionKind::EmitPolyAftertouch;
        }

        // The group a message is on, for the message types the device sends.
        [[nodiscard]] bool OnGroup(_In_ uint32_t word0, _In_ uint8_t group) noexcept
        {
            return ((word0 >> 24) & 0x0Fu) == group;
        }
    }

    _Use_decl_annotations_
    LaunchpadXController::LaunchpadXController(std::shared_ptr<ILaunchpadOutput> output, uint8_t group, NoteHandler notes, ActionHandler actions) :
        m_output(std::move(output)),
        m_group(static_cast<uint8_t>(group & 0x0Fu)),
        m_notes(std::move(notes)),
        m_actions(std::move(actions))
    {
    }

    _Use_decl_annotations_
    bool LaunchpadXController::Start(SurfaceStateSnapshot const& snapshot)
    {
        std::vector<LedSpec> leds{};

        {
            std::scoped_lock guard{ m_lock };
            m_snapshot = snapshot;
            m_accumulator.Reset();
            m_surface.InvalidateLeds();
            leds = m_surface.BuildLedFeedback(m_snapshot, true);
        }

        if (!m_output->SendUmpPackets(BuildProgrammerModePackets(m_group, true)))
        {
            return false;
        }

        {
            std::scoped_lock guard{ m_lock };
            m_started = true;
        }

        SendLeds(leds);
        return true;
    }

    void LaunchpadXController::Stop() noexcept
    {
        try
        {
            std::vector<SurfaceAction> releases{};
            bool wasStarted = false;

            {
                std::scoped_lock guard{ m_lock };
                wasStarted = std::exchange(m_started, false);
                m_surface.ReleaseHeldNotes(releases);
                m_accumulator.Reset();
            }

            Dispatch(std::move(releases), 0);

            if (wasStarted)
            {
                (void)m_output->SendUmpPackets(BuildProgrammerModePackets(m_group, false));
            }
        }
        catch (...)
        {
        }
    }

    bool LaunchpadXController::IsStarted() const noexcept
    {
        std::scoped_lock guard{ m_lock };
        return m_started;
    }

    _Use_decl_annotations_
    void LaunchpadXController::OnInput(uint32_t const* words, uint8_t wordCount, uint64_t timestamp) noexcept
    {
        try
        {
            if (words == nullptr || wordCount == 0 || !OnGroup(words[0], m_group))
            {
                return;
            }

            std::vector<SurfaceAction> actions{};

            {
                std::scoped_lock guard{ m_lock };

                if (!m_started)
                {
                    return;
                }

                InputEvent event{};

                if (!TryParseInputUmp(words, wordCount, event, &m_accumulator))
                {
                    return;
                }

                m_surface.HandleInput(event, m_snapshot, actions);
            }

            Dispatch(std::move(actions), timestamp);
        }
        catch (...)
        {
            // Called on the service's thread, which must never see an exception.
        }
    }

    _Use_decl_annotations_
    void LaunchpadXController::Update(SurfaceStateSnapshot snapshot, bool forceFullRefresh) noexcept
    {
        try
        {
            std::vector<LedSpec> leds{};

            {
                std::scoped_lock guard{ m_lock };
                m_snapshot = std::move(snapshot);

                if (!m_started)
                {
                    return;
                }

                leds = m_surface.BuildLedFeedback(m_snapshot, forceFullRefresh);
            }

            SendLeds(leds);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void LaunchpadXController::SetMode(ControllerMode mode) noexcept
    {
        try
        {
            std::vector<SurfaceAction> releases{};

            {
                std::scoped_lock guard{ m_lock };
                m_surface.SetMode(mode, releases);
            }

            Dispatch(std::move(releases), 0);
        }
        catch (...)
        {
        }
    }

    ControllerMode LaunchpadXController::Mode() const noexcept
    {
        std::scoped_lock guard{ m_lock };
        return m_surface.Mode();
    }

    _Use_decl_annotations_
    void LaunchpadXController::SetNoteConfig(NoteLayoutConfig const& config) noexcept
    {
        try
        {
            std::vector<SurfaceAction> releases{};

            {
                std::scoped_lock guard{ m_lock };
                m_surface.SetNoteConfig(config, releases);
            }

            Dispatch(std::move(releases), 0);
        }
        catch (...)
        {
        }
    }

    NoteLayoutConfig LaunchpadXController::NoteConfig() const noexcept
    {
        std::scoped_lock guard{ m_lock };
        return m_surface.NoteConfig();
    }

    _Use_decl_annotations_
    uint8_t LaunchpadXController::BuildNoteWords(SurfaceAction const& action, uint32_t* words) noexcept
    {
        words[0] = 0;
        words[1] = 0;

        Note note{};
        note.Number = static_cast<uint8_t>(action.midiNote & 0x7Fu);

        switch (action.kind)
        {
        case SurfaceActionKind::EmitNoteOn:
            // A note-on with no velocity would be a note-off to a MIDI 1.0 device.
            note.Velocity = static_cast<uint16_t>(ScaleUp(std::clamp<uint32_t>(action.velocity, 1, 127), 7, 16));
            BuildNoteOn(note, 0, words);
            return 2;

        case SurfaceActionKind::EmitNoteOff:
            BuildNoteOff(note, 0, words);
            return 2;

        case SurfaceActionKind::EmitPolyAftertouch:
            words[0] = 0x40A00000u | (static_cast<uint32_t>(note.Number) << 8);
            words[1] = ScaleUp(static_cast<uint32_t>(action.velocity & 0x7Fu), 7, 32);
            return 2;

        default:
            return 0;
        }
    }

    _Use_decl_annotations_
    void LaunchpadXController::Dispatch(std::vector<SurfaceAction> actions, uint64_t timestamp) noexcept
    {
        try
        {
            std::vector<SurfaceAction> others{};

            for (auto const& action : actions)
            {
                if (IsNoteAction(action.kind))
                {
                    if (m_notes)
                    {
                        m_notes(action, timestamp);
                    }
                }
                else
                {
                    others.push_back(action);
                }
            }

            if (!others.empty() && m_actions)
            {
                m_actions(std::move(others));
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    bool LaunchpadXController::SendLeds(std::vector<LedSpec> const& leds) noexcept
    {
        try
        {
            if (leds.empty())
            {
                return true;
            }

            if (m_output->SendUmpPackets(BuildLedSysExPackets(m_group, leds)))
            {
                return true;
            }
        }
        catch (...)
        {
        }

        // Some of them may not have reached the device, so the next update paints them all.
        try
        {
            std::scoped_lock guard{ m_lock };
            m_surface.InvalidateLeds();
        }
        catch (...)
        {
        }

        return false;
    }
}
