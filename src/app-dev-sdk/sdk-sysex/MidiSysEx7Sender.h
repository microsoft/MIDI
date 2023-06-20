// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once
#include "MidiSysEx7Sender.g.h"

namespace winrt::Microsoft::Devices::Midi2::SysEx::implementation
{
    struct MidiSysEx7Sender : MidiSysEx7SenderT<MidiSysEx7Sender>
    {
        MidiSysEx7Sender() = default;

        void ThisIsHereJustToAllowCppWinRTGenAndCompilation();
    };
}
namespace winrt::Microsoft::Devices::Midi2::SysEx::factory_implementation
{
    struct MidiSysEx7Sender : MidiSysEx7SenderT<MidiSysEx7Sender, implementation::MidiSysEx7Sender>
    {
    };
}
