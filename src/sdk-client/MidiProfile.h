#pragma once
#include "MidiProfile.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiProfile : MidiProfileT<MidiProfile>
    {
        MidiProfile() = default;

        hstring RawJson();
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiProfile : MidiProfileT<MidiProfile, implementation::MidiProfile>
    {
    };
}
