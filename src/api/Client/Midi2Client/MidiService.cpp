#include "pch.h"
#include "MidiService.h"
#include "MidiService.g.cpp"

namespace winrt::Windows::Devices::Midi2::implementation
{
    winrt::Windows::Devices::Midi2::MidiServicePingResponseSummary MidiService::PingService(uint8_t /* pingCount*/)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::Devices::Midi2::MidiTransportInformation> MidiService::GetInstalledTransports()
    {
        throw hresult_not_implemented();
    }
}
