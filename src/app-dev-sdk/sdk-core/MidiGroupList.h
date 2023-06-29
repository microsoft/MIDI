#pragma once
#include "MidiGroupList.g.h"
#include "MidiListBase.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiGroupList : MidiGroupListT<MidiGroupList, Microsoft::Devices::Midi2::implementation::MidiListBase>
    {
        MidiGroupList() = default;

    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiGroupList : MidiGroupListT<MidiGroupList, implementation::MidiGroupList>
    {
    };
}
