#pragma once
#include "IMidiEndpointOpenSettings.g.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct IMidiEndpointOpenSettings : IMidiEndpointOpenSettingsT<IMidiEndpointOpenSettings>
    {
        IMidiEndpointOpenSettings() = default;

        hstring TempFoo();
    };
}
