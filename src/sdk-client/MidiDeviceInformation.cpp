#include "pch.h"
#include "MidiDeviceInformation.h"
#include "MidiDeviceInformation.g.cpp"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    winrt::Microsoft::Devices::Midi2::MidiDeviceInformation MidiDeviceInformation::FromDeviceInformation(winrt::Windows::Devices::Enumeration::DeviceInformation const& deviceInformation)
    {
        throw hresult_not_implemented();
    }
}
