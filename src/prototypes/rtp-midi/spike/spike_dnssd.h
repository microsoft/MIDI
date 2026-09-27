// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// PROTOTYPE. DNS-SD for AppleMIDI through the Windows mDNS stack.
//
// Discovery uses the repository's shared MidiDnssdBrowser (DnsServiceBrowse), the same code the
// Network MIDI 2.0 transport and SDK use. Advertising uses DnsServiceRegister, because the
// sockets here are Winsock and WinRT's DnssdServiceInstance needs a DatagramSocket.
// ============================================================================

#pragma once

#include "spike_common.h"
#include "spike_net.h"

#include <condition_variable>

#include "midi_dnssd_browser.h"

namespace Spike
{
    constexpr wchar_t AppleMidiServiceType[] = L"_apple-midi._udp.local";

    inline std::wstring LocalHostName()
    {
        wchar_t name[256]{};
        DWORD size = ARRAYSIZE(name);

        if (!GetComputerNameExW(ComputerNameDnsHostname, name, &size)) return L"localhost.local";

        return std::wstring{ name } + L".local";
    }

    class DnssdAdvertiser
    {
    public:
        ~DnssdAdvertiser() { Unregister(); }

        // Blocks until the Windows DNS client reports the result or the timeout passes.
        bool Register(std::wstring const& instanceLabel, uint16_t port, DWORD timeoutMilliseconds,
            std::wstring const& serviceType = AppleMidiServiceType,
            std::vector<std::pair<std::wstring, std::wstring>> const& text = {})
        {
            m_requestedLabel = instanceLabel;
            m_fullName = instanceLabel + L"." + serviceType;
            m_hostName = LocalHostName();

            std::vector<PCWSTR> keys;
            std::vector<PCWSTR> values;
            for (auto const& [key, value] : text)
            {
                keys.push_back(key.c_str());
                values.push_back(value.c_str());
            }

            m_instance = DnsServiceConstructInstance(
                m_fullName.c_str(),
                m_hostName.c_str(),
                nullptr,
                nullptr,
                port,
                0,
                0,
                static_cast<DWORD>(keys.size()),
                keys.empty() ? nullptr : keys.data(),
                values.empty() ? nullptr : values.data());

            if (m_instance == nullptr)
            {
                m_status = GetLastError();
                return false;
            }

            m_request = DNS_SERVICE_REGISTER_REQUEST{};
            m_request.Version = DNS_QUERY_REQUEST_VERSION1;
            m_request.InterfaceIndex = 0;
            m_request.pServiceInstance = m_instance;
            m_request.pRegisterCompletionCallback = &DnssdAdvertiser::Completed;
            m_request.pQueryContext = this;
            m_request.hCredentials = nullptr;
            m_request.unicastEnabled = FALSE;

            m_completed = false;

            auto const status = DnsServiceRegister(&m_request, &m_cancel);

            if (status != DNS_REQUEST_PENDING)
            {
                m_status = status;
                DnsServiceFreeInstance(m_instance);
                m_instance = nullptr;
                return false;
            }

            m_registered = true;

            auto lock = std::unique_lock{ m_lock };
            m_changed.wait_for(lock, std::chrono::milliseconds(timeoutMilliseconds), [this]() { return m_completed; });

            return m_completed && m_status == ERROR_SUCCESS;
        }

        void Unregister()
        {
            if (!m_registered) return;
            m_registered = false;

            if (!m_completed) DnsServiceRegisterCancel(&m_cancel);

            // the goodbye for the records goes out from the DNS client service, not from here
            m_completed = false;
            DnsServiceDeRegister(&m_request, nullptr);

            auto lock = std::unique_lock{ m_lock };
            m_changed.wait_for(lock, std::chrono::milliseconds(2000), [this]() { return m_completed; });

            if (m_instance != nullptr)
            {
                DnsServiceFreeInstance(m_instance);
                m_instance = nullptr;
            }
        }

        DWORD Status() const { return m_status; }
        std::wstring const& HostName() const { return m_hostName; }
        std::wstring const& RequestedName() const { return m_fullName; }
        std::wstring const& RegisteredName() const { return m_registeredName; }

    private:
        static VOID WINAPI Completed(DWORD status, PVOID context, PDNS_SERVICE_INSTANCE instance)
        {
            auto self = static_cast<DnssdAdvertiser*>(context);

            {
                auto lock = std::scoped_lock{ self->m_lock };

                self->m_status = status;

                if (instance != nullptr && instance->pszInstanceName != nullptr)
                {
                    self->m_registeredName = instance->pszInstanceName;
                }

                self->m_completed = true;
            }

            if (instance != nullptr) DnsServiceFreeInstance(instance);

            self->m_changed.notify_all();
        }

        std::wstring m_requestedLabel;
        std::wstring m_fullName;
        std::wstring m_hostName;
        std::wstring m_registeredName;

        PDNS_SERVICE_INSTANCE m_instance{ nullptr };
        DNS_SERVICE_REGISTER_REQUEST m_request{};
        DNS_SERVICE_CANCEL m_cancel{};

        std::mutex m_lock;
        std::condition_variable m_changed;
        bool m_completed{ false };
        bool m_registered{ false };
        DWORD m_status{ ERROR_SUCCESS };
    };

    // Picks the address most likely to be reachable: IPv4 on one of our own subnets first, then any
    // routable IPv4, then IPv6. A link-local IPv6 address is useless without its interface.
    inline bool ChooseServiceAddress(WindowsMidiServicesInternal::MidiDnssdService const& service, RtpMidi::PeerAddress& chosen, std::string& why)
    {
        std::vector<RtpMidi::PeerAddress> ipv4;

        for (auto const& text : service.IPv4Addresses)
        {
            RtpMidi::PeerAddress address{};
            if (TryParseAddress(text, service.Port, address)) ipv4.push_back(address);
        }

        for (auto const& address : ipv4)
        {
            if (address.Bytes[0] == 169 && address.Bytes[1] == 254) continue;
            chosen = address;
            why = "first routable IPv4 address";
            return true;
        }

        if (!ipv4.empty())
        {
            chosen = ipv4.front();
            why = "IPv4 link-local address";
            return true;
        }

        for (auto const& text : service.IPv6Addresses)
        {
            RtpMidi::PeerAddress address{};
            if (!TryParseAddress(text, service.Port, address)) continue;
            if (address.Bytes[0] == 0xFE && (address.Bytes[1] & 0xC0) == 0x80) continue;

            chosen = address;
            why = "routable IPv6 address";
            return true;
        }

        why = "no usable address in the advertisement";
        return false;
    }

    inline void PrintService(WindowsMidiServicesInternal::MidiDnssdService const& service, char const* verb)
    {
        std::string v4;
        for (auto const& address : service.IPv4Addresses) { if (!v4.empty()) v4 += ", "; v4 += ToUtf8(address); }

        std::string v6;
        for (auto const& address : service.IPv6Addresses) { if (!v6.empty()) v6 += ", "; v6 += ToUtf8(address); }

        std::string txt;
        for (auto const& entry : service.TextAttributes) { if (!txt.empty()) txt += ", "; txt += ToUtf8(entry.first) + "=" + ToUtf8(entry.second); }

        Print("  %s  \"%s\"", verb, ToUtf8(service.ServiceInstanceName).c_str());
        Print("        host %s  port %u", ToUtf8(service.HostName).c_str(), service.Port);
        Print("        IPv4 [%s]", v4.c_str());
        Print("        IPv6 [%s]", v6.c_str());
        Print("        TXT  [%s]", txt.c_str());
    }

    // Browses until an instance with this label turns up and resolves, or the timeout passes.
    inline bool FindAppleMidiService(std::wstring const& instanceLabel, DWORD timeoutMilliseconds, WindowsMidiServicesInternal::MidiDnssdService& found)
    {
        WindowsMidiServicesInternal::MidiDnssdBrowser browser;

        std::mutex lock;
        std::condition_variable changed;
        bool haveIt = false;

        auto const matches = [&](WindowsMidiServicesInternal::MidiDnssdService const& service)
        {
            return _wcsicmp(service.ServiceInstanceName.c_str(), instanceLabel.c_str()) == 0 && service.IsResolved();
        };

        auto const consider = [&](WindowsMidiServicesInternal::MidiDnssdService const& service)
        {
            if (!matches(service)) return;

            auto guard = std::scoped_lock{ lock };
            found = service;
            haveIt = !service.IPv4Addresses.empty() || !service.IPv6Addresses.empty();
            changed.notify_all();
        };

        auto const status = browser.Start(
            AppleMidiServiceType,
            [&](WindowsMidiServicesInternal::MidiDnssdService const& service) { consider(service); },
            [&](WindowsMidiServicesInternal::MidiDnssdService const& service, uint32_t) { consider(service); },
            [&](std::wstring const&, std::wstring const&) {});

        if (FAILED(status))
        {
            Print("DnsServiceBrowse failed: 0x%08X", static_cast<unsigned>(status));
            return false;
        }

        {
            auto guard = std::unique_lock{ lock };
            changed.wait_for(guard, std::chrono::milliseconds(timeoutMilliseconds), [&]() { return haveIt; });
        }

        browser.Stop();

        return haveIt;
    }
}
