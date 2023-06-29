#pragma once
#include "MidiTransportInformationList.g.h"
#include "MidiListBase.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiTransportInformationList : MidiTransportInformationListT<MidiTransportInformationList, Microsoft::Devices::Midi2::implementation::MidiListBase>
    {
        MidiTransportInformationList() = default;

    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiTransportInformationList : MidiTransportInformationListT<MidiTransportInformationList, implementation::MidiTransportInformationList>
    {
    };
}
