#pragma once
#include "MidiDeviceInformationList.g.h"
#include "MidiListBase.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiDeviceInformationList : MidiDeviceInformationListT<MidiDeviceInformationList, Microsoft::Devices::Midi2::implementation::MidiListBase>
    {
        MidiDeviceInformationList() = default;

    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiDeviceInformationList : MidiDeviceInformationListT<MidiDeviceInformationList, implementation::MidiDeviceInformationList>
    {
    };
}
