// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include <sal.h>
#include <string>

#include "LayoutModel.h"

namespace glass
{
    // A layout somebody can run the moment they have picked a device: eight faders, eight knobs
    // and eight pads, on one page, all pointing at the one device.
    //
    // This exists because the first thing a new customer needs is not a blank canvas. It is also
    // how the runtime is proved end to end before there is an editor to build anything with.
    LayoutDocument BuildStarterLayout(
        _In_ std::wstring const& layoutName,
        _In_ std::wstring const& deviceName,
        _In_ midiapp::EndpointMatch const& match,
        _In_ midiapp::EndpointMatchMode matchMode) noexcept;

    // What the starter layout sends, written down so the end to end test knows what to look for.
    constexpr uint32_t StarterFaderCount = 8;
    constexpr uint32_t StarterKnobCount = 8;
    constexpr uint32_t StarterPadCount = 8;

    // Volume through to controller 14, the range every DAW maps first.
    constexpr uint32_t StarterFirstFaderController = 7;
    constexpr uint32_t StarterFirstKnobController = 16;

    // The bottom octave of the General MIDI drum map, which is where a pad grid usually lands.
    constexpr uint32_t StarterFirstPadNote = 36;
}
