#include "pch.h"
#include "MidiSession.h"
#include "MidiSession.g.cpp"


namespace winrt::Windows::Devices::Midi::implementation
{
    winrt::Windows::Devices::Midi::MidiSession MidiSession::Create(hstring const& name, winrt::Windows::Devices::Midi::MidiSessionSettings const& settings)
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
    void MidiSession::Close()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi::IMidiDevice MidiSession::AddDevice(winrt::Windows::Devices::Midi::IMidiDeviceSettings const& settings)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi::IMidiEndpoint MidiSession::AddEndpoint(hstring const& deviceId, winrt::Windows::Devices::Midi::IMidiEndpointSettings const& settings)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi::IMidiDevice MidiSession::RemoveDevice(hstring const& deviceId)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi::IMidiEndpoint MidiSession::RemoveEndpoint(hstring const& deviceId, hstring const& endpointId)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi::IMidiDevice MidiSession::GetDevice(hstring const& deviceId)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi::IMidiDevice MidiSession::GetEndpoint(hstring const& deviceId, hstring const& endpointId)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi::IMidiDevice> MidiSession::GetAllDevices()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi::IMidiEndpoint> MidiSession::GetAllEndpoints()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi::IMidiEndpoint> MidiSession::GetDeviceEndpoints(hstring const& deviceId)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi::IMidiEndpoint MidiSession::OpenEndpoint(hstring const& deviceId, hstring const& endpointId, winrt::Windows::Devices::Midi::IMidiEndpointSettings const& settings)
    {
        throw hresult_not_implemented();
    }
    void MidiSession::CloseEndpoint(hstring const& deviceId, hstring const& endpointId)
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
