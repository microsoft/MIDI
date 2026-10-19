// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
// The program list query is shared with MIDI Sequencer, in midi-app-shared. These names keep the
// keyboard's own code as it was.

#include "../midi-app-shared/MidiCiProgramListQuery.h"

namespace midikeyboard
{
    using midiapp::GroupProgramsByCategory;
    using midiapp::MidiCiProgramListQuery;
    using midiapp::ProgramCategoryGroup;
    using midiapp::ProgramListEntry;
    using midiapp::ProgramListResult;
}