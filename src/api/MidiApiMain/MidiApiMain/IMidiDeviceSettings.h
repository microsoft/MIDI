#pragma once
#include "IMidiDeviceSettings.g.h"

namespace winrt::Windows::Devices::Midi::implementation
{
    struct IMidiDeviceSettings : IMidiDeviceSettingsT<IMidiDeviceSettings>
    {
        IMidiDeviceSettings() = default;

        hstring TempFoo();
    };
}
