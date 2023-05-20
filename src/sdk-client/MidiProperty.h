#pragma once
#include "Properties.MidiProperty.g.h"


namespace winrt::Microsoft::Devices::Midi2::Properties::implementation
{
    struct MidiProperty : MidiPropertyT<MidiProperty>
    {
        MidiProperty() = default;

        hstring RawJson();
    };
}
namespace winrt::Microsoft::Devices::Midi2::Properties::factory_implementation
{
    struct MidiProperty : MidiPropertyT<MidiProperty, implementation::MidiProperty>
    {
    };
}
