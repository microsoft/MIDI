#pragma once
#include "MidiCIPropertyRequestedEventArgs.g.h"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiCIPropertyRequestedEventArgs : MidiCIPropertyRequestedEventArgsT<MidiCIPropertyRequestedEventArgs>
    {
        MidiCIPropertyRequestedEventArgs() = default;

        winrt::Windows::Devices::Midi2::MidiUniqueId UniqueId();
    };
}
