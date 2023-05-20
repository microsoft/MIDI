#pragma once
#include "Profiles.MidiProfile.g.h"


namespace winrt::Microsoft::Devices::Midi2::Profiles::implementation
{
    struct MidiProfile : MidiProfileT<MidiProfile>
    {
        MidiProfile() = default;

        hstring RawJson();
    };
}
namespace winrt::Microsoft::Devices::Midi2::Profiles::factory_implementation
{
    struct MidiProfile : MidiProfileT<MidiProfile, implementation::MidiProfile>
    {
    };
}
