#pragma once
#include "MidiSession.g.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct MidiSession : MidiSessionT<MidiSession>
    {
        MidiSession() = default;

        static winrt::Windows::Devices::Midi::MidiSession Create(hstring const& name, winrt::Windows::Devices::Midi::MidiSessionSettings const& settings);
        static winrt::Windows::Devices::Midi::MidiSession Create(hstring const& name);
        hstring Id();
        hstring Name();
        hstring ProcessName();
        uint32_t ProcessId();
        winrt::Windows::Foundation::DateTime OpenedAt();
        void Close();
        winrt::Windows::Devices::Midi::IMidiEndpoint OpenEndpoint(hstring const& deviceId, hstring const& endpointId, winrt::Windows::Devices::Midi::MidiEndpointSettings const& settings);
        void Panic();
    };
}
namespace winrt::Windows::Devices::Midi::factory_implementation
{
    struct MidiSession : MidiSessionT<MidiSession, implementation::MidiSession>
    {
    };
}
