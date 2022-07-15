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
        void Name(hstring const& value);
        hstring ProcessName();
        uint32_t ProcessId();
        winrt::Windows::Foundation::DateTime CreatedTime();
        winrt::Windows::Devices::Midi::MidiSessionLogLevel LogLevel();
        void LogLevel(winrt::Windows::Devices::Midi::MidiSessionLogLevel const& value);
        void Close();
        winrt::Windows::Devices::Midi::IMidiDevice AddDevice(winrt::Windows::Devices::Midi::IMidiDeviceSettings const& settings);
        winrt::Windows::Devices::Midi::IMidiEndpoint AddEndpoint(hstring const& deviceId, winrt::Windows::Devices::Midi::IMidiEndpointSettings const& settings);
        winrt::Windows::Devices::Midi::IMidiDevice RemoveDevice(hstring const& deviceId);
        winrt::Windows::Devices::Midi::IMidiEndpoint RemoveEndpoint(hstring const& deviceId, hstring const& endpointId);
        winrt::Windows::Devices::Midi::IMidiDevice GetDevice(hstring const& deviceId);
        winrt::Windows::Devices::Midi::IMidiDevice GetEndpoint(hstring const& deviceId, hstring const& endpointId);
        winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi::IMidiDevice> GetAllDevices();
        winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi::IMidiEndpoint> GetAllEndpoints();
        winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi::IMidiEndpoint> GetDeviceEndpoints(hstring const& deviceId);
        winrt::Windows::Devices::Midi::IMidiEndpoint OpenEndpoint(hstring const& deviceId, hstring const& endpointId, winrt::Windows::Devices::Midi::IMidiEndpointSettings const& settings);
        void CloseEndpoint(hstring const& deviceId, hstring const& endpointId);
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
