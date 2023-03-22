#pragma once
#include "Enumeration.MidiDeviceWatcher.g.h"


namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    struct MidiDeviceWatcher : MidiDeviceWatcherT<MidiDeviceWatcher>
    {
        MidiDeviceWatcher() = default;

        void Start();
        void Stop();
    };
}
namespace winrt::Windows::Devices::Midi2::Enumeration::factory_implementation
{
    struct MidiDeviceWatcher : MidiDeviceWatcherT<MidiDeviceWatcher, implementation::MidiDeviceWatcher>
    {
    };
}
