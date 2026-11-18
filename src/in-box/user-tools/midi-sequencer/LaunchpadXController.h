// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Runs a Novation Launchpad X for the sequencer: puts it in Programmer mode, turns what its pads
// and buttons send into actions, and keeps its LEDs in step with the sequence. It knows nothing
// of Windows MIDI Services: the messages go out through ILaunchpadOutput, which the app connects
// to the device and a test replaces with a recorder. Pure C++20 with no pch.h and no XAML.
//
// Threads: input arrives on the service's thread, and the app updates the LEDs from its own.
// Everything here is behind one lock, and no handler or output is ever called while it's held.

#include <sal.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

#include "LaunchpadXProtocol.h"
#include "LaunchpadXSurface.h"

namespace midisequencer::launchpad
{
    class ILaunchpadOutput
    {
    public:
        virtual ~ILaunchpadOutput() = default;

        // Sends the packets now, in order. False when any of them didn't go.
        [[nodiscard]] virtual bool SendUmpPackets(_In_ std::span<const UmpPacket> packets) noexcept = 0;
    };

    class LaunchpadXController
    {
    public:
        // A note played, let go or pressed harder in Note mode. Called on the thread the input
        // came in on, so the note sounds without waiting for the app's thread. Must not block.
        // The timestamp is the input's, or 0 for a note let go by a mode or layout change.
        using NoteHandler = std::function<void(_In_ SurfaceAction const& action, _In_ uint64_t timestamp)>;

        // Everything else a pad or button asks for: launching, stopping, moving around, switching
        // mode. Called on the input's thread; the app carries them out on its own.
        using ActionHandler = std::function<void(_In_ std::vector<SurfaceAction> actions)>;

        LaunchpadXController(_In_ std::shared_ptr<ILaunchpadOutput> output, _In_ uint8_t group, _In_ NoteHandler notes, _In_ ActionHandler actions);

        LaunchpadXController(LaunchpadXController const&) = delete;
        LaunchpadXController& operator=(LaunchpadXController const&) = delete;

        // Puts the device in Programmer mode and paints every LED. False when that couldn't be sent.
        bool Start(_In_ SurfaceStateSnapshot const& snapshot);

        // Lets go of any notes held in Note mode, then gives the device back its own modes.
        void Stop() noexcept;

        [[nodiscard]] bool IsStarted() const noexcept;
        [[nodiscard]] uint8_t Group() const noexcept { return m_group; }

        // A message from the device. Any thread. Messages on other groups are someone else's.
        void OnInput(_In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount, _In_ uint64_t timestamp) noexcept;

        // The sequence as it is now. Sends the LEDs that changed, or all of them when forced.
        void Update(_In_ SurfaceStateSnapshot snapshot, _In_ bool forceFullRefresh = false) noexcept;

        // From the app, such as its own Clip / Note switch. Notes held are let go.
        void SetMode(_In_ ControllerMode mode) noexcept;
        [[nodiscard]] ControllerMode Mode() const noexcept;

        // The Note mode layout. A change lets go of held notes, since the pads move.
        void SetNoteConfig(_In_ NoteLayoutConfig const& config) noexcept;
        [[nodiscard]] NoteLayoutConfig NoteConfig() const noexcept;

        // The MIDI 2.0 message a Note mode action plays, on group 0 and channel 0: the app moves
        // it to where the track plays. Returns the word count, or 0 for any other action.
        [[nodiscard]] static uint8_t BuildNoteWords(_In_ SurfaceAction const& action, _Out_writes_(2) uint32_t* words) noexcept;

    private:
        void Dispatch(_In_ std::vector<SurfaceAction> actions, _In_ uint64_t timestamp) noexcept;
        bool SendLeds(_In_ std::vector<LedSpec> const& leds) noexcept;

        std::shared_ptr<ILaunchpadOutput> const m_output;
        uint8_t const m_group;
        NoteHandler const m_notes;
        ActionHandler const m_actions;

        mutable std::mutex m_lock{};
        LaunchpadXSurface m_surface{};
        SurfaceStateSnapshot m_snapshot{};
        SysEx7Accumulator m_accumulator{};
        bool m_started{ false };
    };
}
