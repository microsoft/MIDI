#pragma once
#include "MidiSession.g.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct MidiSession : MidiSessionT<MidiSession>
    {
        MidiSession() = default;

        static winrt::Windows::Devices::Midi::MidiSession Create(hstring const& name, winrt::Windows::Devices::Midi::MidiSessionCreateSettings const& settings);
        static winrt::Windows::Devices::Midi::MidiSession Create(hstring const& name);
        hstring Id();
        hstring Name();
        void Name(hstring const& value);
        hstring ProcessName();
        uint32_t ProcessId();
        winrt::Windows::Foundation::DateTime CreatedTime();
        winrt::Windows::Devices::Midi::MidiSessionLogLevel LogLevel();
        void LogLevel(winrt::Windows::Devices::Midi::MidiSessionLogLevel const& value);
        winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi::IMidiDevice> Devices();
        void Close();
        winrt::Windows::Devices::Midi::IMidiDevice CreateDevice(winrt::Windows::Devices::Midi::IMidiDeviceCreateSettings const& settings);
        winrt::Windows::Devices::Midi::IMidiDevice DestroyDevice(hstring const& deviceId);
        winrt::Windows::Devices::Midi::IMidiEndpoint OpenDevice(hstring const& deviceId, winrt::Windows::Devices::Midi::IMidiDeviceOpenSettings const& settings);
        void CloseDevice(hstring const& deviceId, hstring const& endpointId);
        winrt::Windows::Devices::Midi::IMidiEndpoint CreateEndpoint(hstring const& deviceId, winrt::Windows::Devices::Midi::IMidiEndpointCreateSettings const& settings);
        winrt::Windows::Devices::Midi::IMidiEndpoint DestroyEndpoint(hstring const& deviceId, hstring const& endpointId);
        winrt::Windows::Devices::Midi::IMidiEndpoint OpenEndpoint(hstring const& deviceId, hstring const& endpointId, winrt::Windows::Devices::Midi::IMidiEndpointOpenSettings const& settings);
        void CloseEndpoint(hstring const& deviceId, hstring const& endpointId);
        winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi::IMidiEndpoint> GetAllEndpoints();
        void Panic();
        void Panic(hstring const& deviceId);
        void Panic(hstring const& deviceId, hstring const& endpointId);
    };
}
namespace winrt::Windows::Devices::Midi::factory_implementation
{
    struct MidiSession : MidiSessionT<MidiSession, implementation::MidiSession>
    {
    };
}
