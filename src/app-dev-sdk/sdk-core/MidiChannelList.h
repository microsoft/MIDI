#pragma once
#include "MidiChannelList.g.h"
#include "MidiListBase.h"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiChannelList : MidiChannelListT<MidiChannelList, Microsoft::Devices::Midi2::implementation::MidiListBase>
    {
        MidiChannelList() = default;

    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiChannelList : MidiChannelListT<MidiChannelList, implementation::MidiChannelList>
    {
    };
}
