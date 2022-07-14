#pragma once
#include "MidiEndpointSettings.g.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct MidiEndpointSettings : MidiEndpointSettingsT<MidiEndpointSettings>
    {
        MidiEndpointSettings() = default;

        hstring TempFoo();
    };
}
