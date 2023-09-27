#pragma once
#include "MidiVirtualDevice.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiVirtualDevice : MidiVirtualDeviceT<MidiVirtualDevice>
    {
        MidiVirtualDevice() = default;

        void Start();
    };
}
