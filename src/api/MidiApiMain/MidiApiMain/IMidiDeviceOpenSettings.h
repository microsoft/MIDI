#pragma once
#include "IMidiDeviceOpenSettings.g.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct IMidiDeviceOpenSettings : IMidiDeviceOpenSettingsT<IMidiDeviceOpenSettings>
    {
        IMidiDeviceOpenSettings() = default;

        hstring TempFoo();
    };
}
