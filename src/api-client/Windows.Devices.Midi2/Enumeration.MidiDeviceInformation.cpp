#include "pch.h"
#include "Enumeration.MidiDeviceInformation.h"
#include "Enumeration.MidiDeviceInformation.g.cpp"


namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    hstring MidiDeviceInformation::Id()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Collections::PropertySet MidiDeviceInformation::Properties()
    {
        throw hresult_not_implemented();
    }
}
