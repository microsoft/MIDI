#pragma once
#include "MidiSessionCreateSettings.g.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct MidiSessionCreateSettings : MidiSessionCreateSettingsT<MidiSessionCreateSettings>
    {
        MidiSessionCreateSettings() = default;

        winrt::Windows::Devices::Midi::MidiSessionLogLevel LogLevel();
        void LogLevel(winrt::Windows::Devices::Midi::MidiSessionLogLevel const& value);

        bool MessageProcessorsEnabled();
        void MessageProcessorsEnabled(bool const& value);

    };
}
