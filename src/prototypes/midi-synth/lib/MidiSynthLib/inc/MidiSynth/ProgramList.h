// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#pragma once

#include <string>
#include <vector>

#include "DlsCollection.h"

namespace MidiSynth
{
    // Builds the MIDI-CI Property Exchange ProgramList resource for a sound set.
    //
    // Called once when a sound set loads, never while answering a request, so allocating here is
    // fine. What comes back is a byte blob that replies slice.
    //
    // Drum kits are deliberately left out. In this sound set they are selected by the drum flag on
    // a channel rather than by a bank, so a kit and a melodic instrument share the same bank and
    // program numbers. There is no bankPC that would select a kit, and publishing one would send
    // clients to the wrong sound.
    std::vector<char> BuildProgramListJson(_In_ const DlsCollection& collection);
}
