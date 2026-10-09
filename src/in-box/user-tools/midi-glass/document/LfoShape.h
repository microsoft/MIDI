// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML. The arithmetic an LFO runs on is the same arithmetic the
// control draws itself with, so it lives here where both can reach it and where it is tested
// without a window. The waves and the noise are shared with MIDI Patchbay's LFO step, in
// midi-app-shared; this adds what belongs to an LFO control.

#include <sal.h>
#include <cstdint>

#include "LayoutModel.h"
#include "LfoWave.h"

namespace glass
{
    using ::midiapp::LfoWaveOrder;
    using ::midiapp::LfoRateChoices;
    using ::midiapp::LfoWaveRepeats;
    using ::midiapp::LfoWaveAt;
    using ::midiapp::LfoNoise;

    // The value an LFO actually sends, after the sweep's two ends are applied. Lowest above
    // highest turns the wave upside down, which falls out of the arithmetic rather than needing
    // its own switch.
    double LfoValueAt(_In_ LfoSpec const& spec, _In_ double phase, _In_ double waveValue) noexcept;
}
