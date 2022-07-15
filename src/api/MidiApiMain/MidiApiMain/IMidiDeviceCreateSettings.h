#pragma once
#include "IMidiDeviceCreateSettings.g.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct IMidiDeviceCreateSettings : IMidiDeviceCreateSettingsT<IMidiDeviceCreateSettings>
    {
        IMidiDeviceCreateSettings() = default;

        hstring TempFoo();
    };
}
