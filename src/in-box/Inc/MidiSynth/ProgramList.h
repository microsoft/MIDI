// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#pragma once

#include <string>
#include <vector>

#include "DlsCollection.h"

namespace MidiSynth
{
    enum class ProgramListKind
    {
        Melodic,
        DrumKits
    };

    // Builds the MIDI-CI Property Exchange ProgramList resource for a sound set.
    //
    // Called once when a sound set loads, never while answering a request, so allocating here is
    // fine. What comes back is a byte blob that replies slice.
    //
    // Melodic instruments and drum kits are two separate lists because in this sound set a kit and
    // a melodic instrument can share the same bank and program numbers: what tells them apart is
    // whether the channel is a drum channel. A channel's ChannelList entry links to only the one
    // list that applies to it, so a client is never offered a bankPC that would select the wrong
    // sound.
    std::vector<char> BuildProgramListJson(
        _In_ const DlsCollection& collection,
        _In_ ProgramListKind kind);
}
