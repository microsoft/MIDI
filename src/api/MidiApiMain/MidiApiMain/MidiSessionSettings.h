#pragma once
#include "MidiSessionSettings.g.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct MidiSessionSettings : MidiSessionSettingsT<MidiSessionSettings>
    {
        //MidiSessionSettings() = default;

        hstring TempFoo();
    };
}
namespace winrt::Windows::Devices::Midi::factory_implementation
{
    struct MidiSessionSettings : MidiSessionSettingsT<MidiSessionSettings, implementation::MidiSessionSettings>
    {
    };
}
