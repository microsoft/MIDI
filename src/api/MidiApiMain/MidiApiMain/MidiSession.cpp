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
    hstring MidiSession::ProcessName()
    {
        throw hresult_not_implemented();
    }
    uint32_t MidiSession::ProcessId()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::DateTime MidiSession::OpenedAt()
    {
        throw hresult_not_implemented();
    }
    void MidiSession::Close()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi::IMidiEndpoint MidiSession::OpenEndpoint(hstring const& deviceId, hstring const& endpointId, winrt::Windows::Devices::Midi::MidiEndpointSettings const& settings)
    {
        throw hresult_not_implemented();
    }
    void MidiSession::Panic()
    {
        throw hresult_not_implemented();
    }
}
