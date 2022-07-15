#pragma once
#include "MidiSessionSettings.g.h"

namespace winrt::Windows::Devices::Midi::implementation
{
    struct MidiSessionSettings : MidiSessionSettingsT<MidiSessionSettings>
    {
        MidiSessionSettings() = default;

        winrt::Windows::Devices::Midi::MidiSessionLogLevel LogLevel();
        void LogLevel(winrt::Windows::Devices::Midi::MidiSessionLogLevel const& value);
    };
}
