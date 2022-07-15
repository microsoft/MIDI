#include "pch.h"
#include "MidiSessionSettings.h"
#include "MidiSessionSettings.g.cpp"

namespace winrt::Windows::Devices::Midi::implementation
{
    winrt::Windows::Devices::Midi::MidiSessionLogLevel MidiSessionSettings::LogLevel()
    {
        throw hresult_not_implemented();
    }
    void MidiSessionSettings::LogLevel(winrt::Windows::Devices::Midi::MidiSessionLogLevel const& value)
    {
        throw hresult_not_implemented();
    }
}
