// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Finding OSC apps on the network, and telling them about this PC's OSC connections, by DNS-SD
// (mDNS). Uses the same browser, registration and announcement workaround as the Network MIDI
// 2.0 and RTP-MIDI transports, in Inc.
//
// Browsing is done by the Windows DNS client, so nothing listens here for it. Advertising is a
// registration with the DNS client too, plus the follow-up announcement the transports send for
// the DNS client, which goes out from a socket of this app's on port 5353. So a port is only
// advertised while Windows Firewall lets the app in, which is also the only time an app on
// another device could reach it.

#include "OscConnection.h"

#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

namespace midipatchbay
{
    // An OSC app found on the network.
    struct OscFoundApp
    {
        // What it calls itself: TouchOSC Mk1 says "<name> [<model>] (TouchOSC)".
        std::wstring Name{};

        // Where to send to: its first IPv4 address, or its .local name when it has none.
        std::wstring Host{};

        // The port it listens on.
        uint16_t Port{ 0 };
    };

    // A port this PC should advertise, and the name to advertise it under.
    struct OscAdvertisement
    {
        uint16_t Port{ 0 };
        std::wstring Label{};
    };

    class OscDiscovery
    {
    public:
        static OscDiscovery& Current() noexcept;

        // What's been found so far, sorted by name, leaving out this PC's own advertisements.
        // Starts looking the first time it's asked, so the list fills over the next few seconds.
        std::vector<OscFoundApp> FoundApps() noexcept;

        // The ports to advertise now. Returns at once: registering takes a second or more, so a
        // thread of its own does it.
        void Advertise(_In_ std::vector<OscAdvertisement> wanted) noexcept;

        // Withdraws every advertisement and stops looking.
        void Shutdown() noexcept;

        // The names this PC is advertised under now.
        std::vector<std::wstring> AdvertisedLabels() const noexcept;

    private:
        OscDiscovery() noexcept = default;

        struct Browser;
        struct Advertised;

        void Work(_In_ std::stop_token stopToken) noexcept;

        mutable std::mutex m_lock{};
        std::condition_variable_any m_changed{};

        std::shared_ptr<Browser> m_browser{};

        std::vector<OscAdvertisement> m_wanted{};
        uint64_t m_wantedVersion{ 0 };
        std::vector<std::wstring> m_advertisedLabels{};
        bool m_shutDown{ false };

        std::jthread m_worker{};
    };
}
