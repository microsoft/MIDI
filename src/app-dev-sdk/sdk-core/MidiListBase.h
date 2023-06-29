#pragma once
#include "MidiListBase.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiListBase : MidiListBaseT<MidiListBase>
    {
        MidiListBase() = default;

    };
}
