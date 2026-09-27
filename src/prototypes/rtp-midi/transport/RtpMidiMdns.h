// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// PROTOTYPE. The host name behind a remote address, from the rtpMIDI advertisements.
//
// Standalone, so the spike can test it without the service. The repeated announcements of this
// PC's hosts are in midi_dnssd_announcer.h, shared with Network MIDI 2.0.
// ============================================================================

#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <vector>

#include "midi_dnssd_browser.h"
#include "rtpmidi_session.h"

namespace RtpMidiMdns
{
    // The host name of the advertisement that lists this address. Empty when none does, and when
    // advertisements from different hosts both list it, so a stale entry never names the wrong device.
    inline std::wstring FindHostNameForAddress(
        _In_ std::vector<WindowsMidiServicesInternal::MidiDnssdService> const& services,
        _In_ RtpMidi::PeerAddress const& address)
    {
        if (address.Family != 4 && address.Family != 6) return {};

        int const family = address.Family == 4 ? AF_INET : AF_INET6;
        size_t const length = address.Family == 4 ? 4 : 16;

        std::wstring found{};

        for (auto const& service : services)
        {
            if (service.HostName.empty()) continue;

            auto const& listed = address.Family == 4 ? service.IPv4Addresses : service.IPv6Addresses;

            // advertised addresses carry no scope, so a link-local match ignores the connection's
            bool const matches = std::any_of(listed.begin(), listed.end(), [&](std::wstring const& text)
            {
                std::array<uint8_t, 16> bytes{};
                return InetPtonW(family, text.c_str(), bytes.data()) == 1 && memcmp(bytes.data(), address.Bytes.data(), length) == 0;
            });

            if (!matches) continue;

            if (found.empty()) found = service.HostName;
            else if (_wcsicmp(found.c_str(), service.HostName.c_str()) != 0) return {};
        }

        return found;
    }
}
