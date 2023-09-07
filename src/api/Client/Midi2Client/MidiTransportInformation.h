#pragma once
#include "MidiTransportInformation.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiTransportInformation : MidiTransportInformationT<MidiTransportInformation>
    {
        MidiTransportInformation() = default;

        hstring Id();
        hstring Name();
        hstring ShortName();
        hstring IconPath();
        hstring Author();
        hstring ServicePluginFileName();
        bool IsRuntimeCreatable();
    };
}
