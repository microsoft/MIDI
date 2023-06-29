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
        winrt::Microsoft::Devices::Midi2::MidiGroupList IncludedGroups();
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
