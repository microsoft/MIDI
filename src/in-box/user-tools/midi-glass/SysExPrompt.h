// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The question asked before a layout from somewhere else may send system exclusive. The rules
// for when it's asked are in document\SysExReview.h. A Windows task dialog rather than a XAML
// one, because a layout can run as a toolbar a few buttons high, and a XAML dialog only fits
// inside its own window.

#include "LayoutModel.h"

namespace midiglass::sysexprompt
{
    // Names the devices and asks. Blocks while it's open, so call it from the UI thread once the
    // window is showing. True when the person allowed it, which is then remembered. The file path
    // only names the layout when it has no name of its own.
    bool AskToAllowSysEx(
        _In_ HWND owner,
        _In_ glass::LayoutDocument const& document,
        _In_ std::wstring const& filePath);
}
