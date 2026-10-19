// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// What this PC knows about the drivers a DPC latency check found: the driver file, the company
// that made it, its description and version, and the devices that use it. That's what turns
// "nvlddmkm.sys" into "the NVIDIA graphics driver, version 32.0.15.6094, used by the NVIDIA
// GeForce RTX 4090".

#include "LatencyModel.h"

#include <string>

namespace miditroubleshooter::latency
{
    // Fills in the details of every module the results mention. A module only gets them when
    // the file on this PC is the very file the trace saw, so a trace from another PC, or from
    // before a driver update, keeps just the names. Blocking: it reads driver files and asks
    // Plug and Play about every device, so call it from a background thread.
    void AddDriverDetails(_Inout_ Results& results) noexcept;

    // The Win32 path for a path as a kernel trace or a service records it, such as
    // \SystemRoot\System32\drivers\x.sys. Empty when there isn't one.
    std::wstring Win32PathFromTracePath(_In_ std::wstring const& tracePath);
}
