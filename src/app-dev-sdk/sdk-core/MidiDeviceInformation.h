#pragma once
#include "MidiDeviceInformation.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiDeviceInformation : MidiDeviceInformationT<MidiDeviceInformation>
    {
        MidiDeviceInformation() = default;

        static winrt::Microsoft::Devices::Midi2::MidiDeviceInformation CreateFromId(hstring const& deviceId);
        static winrt::Microsoft::Devices::Midi2::MidiDeviceInformation FromDeviceInformation(winrt::Windows::Devices::Enumeration::DeviceInformation const& deviceInformation);
        static winrt::Windows::Devices::Enumeration::DeviceWatcher CreateWatcher();
        static winrt::Windows::Devices::Enumeration::DeviceWatcher CreateWatcher(winrt::Microsoft::Devices::Midi2::MidiEndpointDataFormat const& midiEndpointDataFormat);
        hstring Id();
        hstring Name();
        hstring Description();
        hstring HasPersistentUniqueIdentifier();
        hstring UniqueIdentifier();
        winrt::Microsoft::Devices::Midi2::MidiEndpointDataFormat EndpointDataFormat();
        bool HasGroupTerminalBlocks();
        winrt::Microsoft::Devices::Midi2::MidiGroupTerminalBlockList GroupTerminalBlocks();
        winrt::Windows::Devices::Enumeration::DeviceThumbnail DeviceThumbnail();
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiDeviceInformation : MidiDeviceInformationT<MidiDeviceInformation, implementation::MidiDeviceInformation>
    {
    };
}
