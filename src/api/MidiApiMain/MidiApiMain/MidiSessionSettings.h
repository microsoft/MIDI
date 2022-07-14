#pragma once
#include "MidiSessionSettings.g.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct MidiSessionSettings : MidiSessionSettingsT<MidiSessionSettings>
    {
        MidiSessionSettings() = default;

        hstring TempFoo();
    };
}
