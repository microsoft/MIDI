#pragma once
#include "IMidiEndpointCreateSettings.g.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct IMidiEndpointCreateSettings : IMidiEndpointCreateSettingsT<IMidiEndpointCreateSettings>
    {
        IMidiEndpointCreateSettings() = default;

        hstring TempFoo();
    };
}
