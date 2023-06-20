// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once
#include "MidiSysEx8Sender.g.h"

namespace winrt::Microsoft::Devices::Midi2::SysEx::implementation
{
    struct MidiSysEx8Sender : MidiSysEx8SenderT<MidiSysEx8Sender>
    {
        MidiSysEx8Sender() = default;

        void ThisIsHereJustToAllowCppWinRTGenAndCompilation();
    };
}
namespace winrt::Microsoft::Devices::Midi2::SysEx::factory_implementation
{
    struct MidiSysEx8Sender : MidiSysEx8SenderT<MidiSysEx8Sender, implementation::MidiSysEx8Sender>
    {
    };
}
