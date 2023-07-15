#include "pch.h"
#include "MidiGroupEndpointListener.h"
#include "MidiGroupEndpointListener.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::MidiGroup> MidiGroupEndpointListener::IncludeGroups()
    {
        throw hresult_not_implemented();
    }
}
