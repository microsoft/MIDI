#pragma once
#include "Enumeration.MidiDeviceManager.g.h"


namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    struct MidiDeviceManager : MidiDeviceManagerT<MidiDeviceManager>
    {
        MidiDeviceManager() = default;

        hstring CreateDevice(hstring const& providerId, hstring const& jsonProperties);
        bool DestroyDevice(hstring const& deviceId);
    };
}
namespace winrt::Windows::Devices::Midi2::Enumeration::factory_implementation
{
    struct MidiDeviceManager : MidiDeviceManagerT<MidiDeviceManager, implementation::MidiDeviceManager>
    {
    };
}
