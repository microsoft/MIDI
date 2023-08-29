#include "pch.h"
#include "MidiCIPropertyRequestedEventArgs.h"
#include "MidiCIPropertyRequestedEventArgs.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    winrt::Windows::Devices::Midi2::MidiUniqueId MidiCIPropertyRequestedEventArgs::UniqueId()
    {
        throw hresult_not_implemented();
    }
}
