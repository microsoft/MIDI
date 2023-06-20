// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once
#include "MidiSysExReceiveProcessor.g.h"

namespace winrt::Microsoft::Devices::Midi2::SysEx::implementation
{
    struct MidiSysExReceiveProcessor : MidiSysExReceiveProcessorT<MidiSysExReceiveProcessor>
    {
        MidiSysExReceiveProcessor() = default;

        void ThisIsHereJustToAllowCppWinRTGenAndCompilation();
    };
}
namespace winrt::Microsoft::Devices::Midi2::SysEx::factory_implementation
{
    struct MidiSysExReceiveProcessor : MidiSysExReceiveProcessorT<MidiSysExReceiveProcessor, implementation::MidiSysExReceiveProcessor>
    {
    };
}
