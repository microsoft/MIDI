#pragma once
#include "MidiFunctionBlockList.g.h"
#include "MidiListBase.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiFunctionBlockList : MidiFunctionBlockListT<MidiFunctionBlockList, Microsoft::Devices::Midi2::implementation::MidiListBase>
    {
        MidiFunctionBlockList() = default;

    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiFunctionBlockList : MidiFunctionBlockListT<MidiFunctionBlockList, implementation::MidiFunctionBlockList>
    {
    };
}
