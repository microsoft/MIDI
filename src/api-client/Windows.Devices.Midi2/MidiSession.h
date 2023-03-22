#pragma once
#include "MidiSession.g.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiSession : MidiSessionT<MidiSession>
    {
        MidiSession() = default;

        static winrt::Windows::Devices::Midi2::MidiSession CreateSession(hstring const& sessionName);
        hstring Id();
        winrt::Windows::Devices::Midi2::MidiUmpEndpoint ConnectToEndpoint(hstring const& id);
    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiSession : MidiSessionT<MidiSession, implementation::MidiSession>
    {
    };
}
