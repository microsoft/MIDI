#include "pch.h"
#include "MidiSession.h"
#include "MidiSession.g.cpp"


namespace winrt::Windows::Devices::Midi::implementation
{
    winrt::Windows::Devices::Midi::MidiSession MidiSession::Create(hstring const& name, winrt::Windows::Devices::Midi::MidiSessionCreateSettings const& settings)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi::MidiSession MidiSession::Create(hstring const& name)
    {
        throw hresult_not_implemented();
    }
    hstring MidiSession::Id()
    {
        throw hresult_not_implemented();
    }
    hstring MidiSession::Name()
    {
        throw hresult_not_implemented();
    }
    void MidiSession::Name(hstring const& value)
    {
        throw hresult_not_implemented();
    }
    hstring MidiSession::ProcessName()
    {
        throw hresult_not_implemented();
    }
    uint32_t MidiSession::ProcessId()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::DateTime MidiSession::CreatedTime()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi::MidiSessionLogLevel MidiSession::LogLevel()
    {
        throw hresult_not_implemented();
    }
    void MidiSession::LogLevel(winrt::Windows::Devices::Midi::MidiSessionLogLevel const& value)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi::IMidiDevice> MidiSession::Devices()
    {
        throw hresult_not_implemented();
    }
    void MidiSession::Close()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi::IMidiDevice MidiSession::CreateDevice(winrt::Windows::Devices::Midi::IMidiDeviceCreateSettings const& settings)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi::IMidiDevice MidiSession::DestroyDevice(hstring const& deviceId)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi::IMidiEndpoint MidiSession::OpenDevice(hstring const& deviceId, winrt::Windows::Devices::Midi::IMidiDeviceOpenSettings const& settings)
    {
        throw hresult_not_implemented();
    }
    void MidiSession::CloseDevice(hstring const& deviceId, hstring const& endpointId)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi::IMidiEndpoint MidiSession::CreateEndpoint(hstring const& deviceId, winrt::Windows::Devices::Midi::IMidiEndpointCreateSettings const& settings)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi::IMidiEndpoint MidiSession::DestroyEndpoint(hstring const& deviceId, hstring const& endpointId)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi::IMidiEndpoint MidiSession::OpenEndpoint(hstring const& deviceId, hstring const& endpointId, winrt::Windows::Devices::Midi::IMidiEndpointOpenSettings const& settings)
    {
        throw hresult_not_implemented();
    }
    void MidiSession::CloseEndpoint(hstring const& deviceId, hstring const& endpointId)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi::IMidiEndpoint> MidiSession::GetAllEndpoints()
    {
        throw hresult_not_implemented();
    }
    void MidiSession::Panic()
    {
        throw hresult_not_implemented();
    }
    void MidiSession::Panic(hstring const& deviceId)
    {
        throw hresult_not_implemented();
    }
    void MidiSession::Panic(hstring const& deviceId, hstring const& endpointId)
    {
        throw hresult_not_implemented();
    }
}
