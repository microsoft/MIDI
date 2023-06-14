// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiSetup.g.h"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiSetup : MidiSetupT<MidiSetup>
    {
        MidiSetup() = default;

        hstring Id();
        void Id(hstring const& value);
        hstring FileName();
        void FileName(hstring const& value);
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiSetup : MidiSetupT<MidiSetup, implementation::MidiSetup>
    {
    };
}
