#include "pch.h"
#include "MidiChannelEndpointListener.h"
#include "MidiChannelEndpointListener.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    winrt::Microsoft::Devices::Midi2::MidiGroup MidiChannelEndpointListener::IncludeGroup()
    {
        throw hresult_not_implemented();
    }
    void MidiChannelEndpointListener::IncludeGroup(winrt::Microsoft::Devices::Midi2::MidiGroup const& value)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::MidiChannel> MidiChannelEndpointListener::IncludeChannels()
    {
        throw hresult_not_implemented();
    }
}
