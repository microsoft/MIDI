#pragma once
#include "IMidiEndpointSettings.g.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct IMidiEndpointSettings : IMidiEndpointSettingsT<IMidiEndpointSettings>
    {
        IMidiEndpointSettings() = default;

        hstring TempFoo();
    };
}
