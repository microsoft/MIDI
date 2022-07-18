#include "pch.h"
#include "MidiSessionCreateSettings.h"
#include "MidiSessionCreateSettings.g.cpp"


namespace winrt::Windows::Devices::Midi::implementation
{
    winrt::Windows::Devices::Midi::MidiSessionLogLevel MidiSessionCreateSettings::LogLevel()
    {
        throw hresult_not_implemented();
    }
    void MidiSessionCreateSettings::LogLevel(winrt::Windows::Devices::Midi::MidiSessionLogLevel const& value)
    {
        throw hresult_not_implemented();
    }


    bool MidiSessionCreateSettings::MessageProcessorsEnabled()
    {
        throw hresult_not_implemented();
    }
    void MidiSessionCreateSettings::MessageProcessorsEnabled(bool const& value)
    {
        throw hresult_not_implemented();
    }


}
