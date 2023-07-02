// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiSessionSettings.h"
#include "MidiSessionSettings.g.cpp"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    winrt::Microsoft::Devices::Midi2::MidiSessionSettings MidiSessionSettings::Default()
    {
        // right now, the default is just empty

        return *(winrt::make_self<implementation::MidiSessionSettings>());
    }
}
