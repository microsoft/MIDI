// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The sound a track asks its device for before it plays: a program change, with a bank when it
// has one. It lives in the track's start-up messages, which go out each time playback starts. A
// track without one leaves the device's sound alone. No XAML here, so the tests compile it
// unchanged (design section 26).

#include <sal.h>

#include <cstdint>
#include <string>
#include <vector>

#include "SequenceModel.h"

namespace midisequencer
{
    struct TrackSound
    {
        // False: the track sends no program change, and the device keeps whatever sound it has.
        bool Set{ false };

        // 0 to 127, as sent. People count programs from 1.
        uint8_t Program{ 0 };

        bool HasBank{ false };
        uint8_t BankMsb{ 0 };
        uint8_t BankLsb{ 0 };

        bool operator==(TrackSound const&) const = default;
    };

    // Longer names from a device are cut, so a damaged file can't grow without end.
    inline constexpr size_t MaximumSoundNameLength = 128;

    // What the start-up messages choose: the last program change, MIDI 1.0 or MIDI 2.0, and the
    // bank it selects, from its own bank or from the bank select controllers before it.
    TrackSound StartupSound(_In_ std::vector<ClipEvent> const& startup) noexcept;

    // Puts the sound first in the start-up messages, so it goes out before the volume and the
    // rest. Any program change and bank select already there go. The other messages stay, in
    // their order. A sound that isn't set only removes them.
    void SetStartupSound(_Inout_ std::vector<ClipEvent>& startup, _In_ TrackSound const& sound, _In_ uint8_t channel);

    // The sound as one MIDI 2.0 program change, with its bank when it has one. The engine sends a
    // MIDI 1.0 device bank select and program change instead.
    ClipEvent SoundMessage(_In_ TrackSound const& sound, _In_ uint8_t channel) noexcept;

    // True for a program change or a bank select controller, the messages SetStartupSound owns.
    bool IsSoundMessage(_In_ ClipEvent const& message) noexcept;
}
