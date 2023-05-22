#pragma once
#include "Enumeration.MidiDeviceInformationCollection.g.h"


namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    struct MidiDeviceInformationCollection : MidiDeviceInformationCollectionT<MidiDeviceInformationCollection>
    {
        MidiDeviceInformationCollection() = default;

    };
}
namespace winrt::Windows::Devices::Midi2::Enumeration::factory_implementation
{
    struct MidiDeviceInformationCollection : MidiDeviceInformationCollectionT<MidiDeviceInformationCollection, implementation::MidiDeviceInformationCollection>
    {
    };
}
