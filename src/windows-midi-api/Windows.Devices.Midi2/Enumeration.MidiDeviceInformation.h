#pragma once
#include "Enumeration.MidiDeviceInformation.g.h"


namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    struct MidiDeviceInformation : MidiDeviceInformationT<MidiDeviceInformation>
    {
        MidiDeviceInformation() = default;

        hstring Id();
        winrt::Windows::Foundation::Collections::PropertySet Properties();
    };
}
namespace winrt::Windows::Devices::Midi2::Enumeration::factory_implementation
{
    struct MidiDeviceInformation : MidiDeviceInformationT<MidiDeviceInformation, implementation::MidiDeviceInformation>
    {
    };
}
