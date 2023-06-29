#pragma once
#include "MidiGroupTerminalBlockList.g.h"
#include "MidiListBase.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiGroupTerminalBlockList : MidiGroupTerminalBlockListT<MidiGroupTerminalBlockList, Microsoft::Devices::Midi2::implementation::MidiListBase>
    {
        MidiGroupTerminalBlockList() = default;

    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiGroupTerminalBlockList : MidiGroupTerminalBlockListT<MidiGroupTerminalBlockList, implementation::MidiGroupTerminalBlockList>
    {
    };
}
