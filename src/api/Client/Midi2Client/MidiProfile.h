#pragma once
#include "MidiProfile.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiProfile : MidiProfileT<MidiProfile>
    {
        MidiProfile() = default;

    };
}
