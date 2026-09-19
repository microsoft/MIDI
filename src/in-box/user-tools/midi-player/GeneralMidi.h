// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <cstdint>
#include <sal.h>
#include <string>

namespace midiplayer
{
    // ==============================================================================================
    // THESE STRINGS ARE DELIBERATELY NOT LOCALIZED, AND THEY ARE NOT IN A .resw. THIS IS NOT AN
    // OVERSIGHT. Do not "fix" it.
    //
    // A program name shown next to a track can come from three places, and the player shows
    // whichever is available, in this order:
    //
    //   1. The file's own instrument or program name, written by whoever made the file.
    //   2. The device's own name for that bank and program, fetched over MIDI-CI Property
    //      Exchange from its ProgramList resource (M2-107-UM). The in-box synth answers these.
    //   3. The General MIDI names below, when neither of the above exists.
    //
    // The first two arrive as text from outside this application and are displayed exactly as
    // they were supplied. If the third were translated, the name beside a track would change
    // language depending on which device happened to be selected, and a customer comparing their
    // file against their instrument's front panel would see two different words for one sound.
    // Leaving all three untranslated is what keeps them comparable.
    //
    // These are also the terms the General MIDI specification (RP-003) itself defines. They are
    // identifiers for sounds in the way that a note is called C# rather than being translated,
    // and every sequencer, workstation and instrument presents them in this form.
    // ==============================================================================================

    // programNumber is 0-127 as it appears on the wire. Returns an empty string when the number is
    // out of range.
    std::wstring GeneralMidiProgramName(uint8_t programNumber) noexcept;

    // Channel 10 is percussion, where the program selects a kit rather than an instrument.
    std::wstring GeneralMidiDrumKitName(uint8_t programNumber) noexcept;

    // The instrument family, for grouping a long list. "Piano", "Guitar", "Strings" and so on.
    std::wstring GeneralMidiFamilyName(uint8_t programNumber) noexcept;

    inline constexpr uint8_t PercussionChannelIndex = 9;    // channel 10, counted from 1
}
