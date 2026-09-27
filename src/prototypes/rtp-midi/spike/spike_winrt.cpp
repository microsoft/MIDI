// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// PROTOTYPE. Registers a DNS-SD instance through WinRT, with the same calls the Network MIDI 2.0
// transport makes (DnssdServiceInstance + RegisterDatagramSocketAsync), so the records the
// system responder sends for that path can be compared with DnsServiceRegister's.
// Kept in its own translation unit so the WinRT headers stay away from the Winsock code.
// ============================================================================

#include "spike_common.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Networking.h>
#include <winrt/Windows.Networking.ServiceDiscovery.Dnssd.h>
#include <winrt/Windows.Networking.Sockets.h>

using namespace winrt::Windows::Networking;
using namespace winrt::Windows::Networking::ServiceDiscovery::Dnssd;
using namespace winrt::Windows::Networking::Sockets;

namespace
{
    char const* StatusName(DnssdRegistrationStatus status)
    {
        switch (status)
        {
        case DnssdRegistrationStatus::Success: return "Success";
        case DnssdRegistrationStatus::InvalidServiceName: return "InvalidServiceName";
        case DnssdRegistrationStatus::ServerError: return "ServerError";
        case DnssdRegistrationStatus::SecurityError: return "SecurityError";
        default: return "?";
        }
    }
}

int RegisterWithWinRt(std::wstring const& fullName, uint16_t port, uint32_t seconds, bool withText)
{
    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    try
    {
        DatagramSocket socket;
        socket.MessageReceived([](DatagramSocket const&, DatagramSocketMessageReceivedEventArgs const&) {});
        socket.BindServiceNameAsync(winrt::to_hstring(port)).get();

        // a null HostName is what the Network MIDI 2.0 transport passes when none is configured
        DnssdServiceInstance instance(fullName, HostName{ nullptr }, port);

        if (withText)
        {
            instance.TextAttributes().Insert(L"UMPEndpointName", L"probe");
            instance.TextAttributes().Insert(L"ProductInstanceId", L"probe");
        }

        auto const started = GetTickCount64();
        auto const result = instance.RegisterDatagramSocketAsync(socket).get();

        Spike::Print("RegisterDatagramSocketAsync \"%s\" port %u: %s after %llu ms%s", Spike::ToUtf8(fullName).c_str(), port,
            StatusName(result.Status()), GetTickCount64() - started, result.HasInstanceNameChanged() ? " (renamed)" : "");

        auto const end = GetTickCount64() + static_cast<uint64_t>(seconds) * 1000;
        while (!Spike::StopRequested() && GetTickCount64() < end) Sleep(50);

        socket.Close();
        Spike::Print("Socket closed.");

        // give the responder a moment to act on the registration going away
        Sleep(1500);
    }
    catch (winrt::hresult_error const& error)
    {
        Spike::Print("WinRT registration failed: 0x%08X %s", static_cast<unsigned>(error.code()), Spike::ToUtf8(std::wstring{ error.message() }).c_str());
        return 1;
    }

    return 0;
}
