// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, so the unit tests compile it unchanged.

#include "LfoShape.h"

namespace glass
{
    _Use_decl_annotations_
    double LfoValueAt(LfoSpec const& spec, double phase, double waveValue) noexcept
    {
        return ::midiapp::LfoSweepValue(spec.Wave, phase, waveValue, spec.Lowest, spec.Highest);
    }
}
