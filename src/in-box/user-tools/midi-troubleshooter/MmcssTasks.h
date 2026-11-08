// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "RegistryRepair.h"

namespace miditroubleshooter
{
    // Checks the Multimedia Class Scheduler Service (MMCSS) "Audio" and "Pro Audio" tasks.
    // Tweak guides and "optimizer" apps often edit these, and an invalid value makes Windows
    // drop the task when it starts. Apps that ask for a dropped task can still open MIDI ports
    // but get no input. See https://microsoft.github.io/MIDI/kb/mmcss-task-settings/
    //
    // Also reports the MaxThreads value, which changes how many threads each app can register.
    //
    // Read-only. Each task is checked by registering the calling thread with it and reverting
    // straight away, so call this from a background thread.
    std::vector<RegistryEntryInfo> ScanMmcssTasks() noexcept;
}
