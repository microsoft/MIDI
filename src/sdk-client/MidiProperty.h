#pragma once
#include "MidiProperty.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiProperty : MidiPropertyT<MidiProperty>
    {
        MidiProperty() = default;

        hstring RawJson();
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiProperty : MidiPropertyT<MidiProperty, implementation::MidiProperty>
    {
    };
}
