// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "SynthList.h"

namespace midisoundmodule
{
    // Says why the app can't start with this synth list, and returns once the customer closes it.
    // It is a Windows task dialog rather than XAML because it shows before XAML starts, and there
    // must not be a window: the window saves the synth list when it closes.
    void ShowSynthListProblem(_In_ SynthList const& list, _In_ std::wstring const& path) noexcept;
}
