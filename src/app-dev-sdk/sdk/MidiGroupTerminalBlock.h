// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiGroupTerminalBlock.g.h"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiGroupTerminalBlock : MidiGroupTerminalBlockT<MidiGroupTerminalBlock>
    {
        MidiGroupTerminalBlock() = default;

        hstring Id();
        hstring Name();
        winrt::Microsoft::Devices::Midi2::MidiGroupTerminalBlockDirection Direction();
        winrt::Microsoft::Devices::Midi2::MidiGroupTerminalBlockProtocol Protocol();
        winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::MidiGroup> IncludedGroups();
        uint16_t MaxDeviceInputBandwidthIn4KBSecondUnits();
        uint16_t MaxDeviceOutputBandwidthIn4KBSecondUnits();
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiGroupTerminalBlock : MidiGroupTerminalBlockT<MidiGroupTerminalBlock, implementation::MidiGroupTerminalBlock>
    {
    };
}
