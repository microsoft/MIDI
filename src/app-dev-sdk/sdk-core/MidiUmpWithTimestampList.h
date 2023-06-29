#pragma once
#include "MidiUmpWithTimestampList.g.h"
#include "MidiListBase.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiUmpWithTimestampList : MidiUmpWithTimestampListT<MidiUmpWithTimestampList, Microsoft::Devices::Midi2::implementation::MidiListBase>
    {
        MidiUmpWithTimestampList() = default;

    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiUmpWithTimestampList : MidiUmpWithTimestampListT<MidiUmpWithTimestampList, implementation::MidiUmpWithTimestampList>
    {
    };
}
