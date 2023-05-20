#pragma once
#include "MidiDeviceInformation.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiDeviceInformation : MidiDeviceInformationT<MidiDeviceInformation>
    {
        MidiDeviceInformation() = default;

        static winrt::Microsoft::Devices::Midi2::MidiDeviceInformation FromDeviceInformation(winrt::Windows::Devices::Enumeration::DeviceInformation const& deviceInformation);
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiDeviceInformation : MidiDeviceInformationT<MidiDeviceInformation, implementation::MidiDeviceInformation>
    {
    };
}
