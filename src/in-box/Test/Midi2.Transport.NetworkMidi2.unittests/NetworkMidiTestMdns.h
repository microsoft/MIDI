// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Minimal mDNS / DNS-SD client used only by the Network MIDI 2.0 protocol tests.
//
// This exists because Windows.Devices.Enumeration's DNS-SD enumeration does not
// return results for this process, and because a conformance test should not depend
// on the platform's discovery stack to find the thing it is testing. Speaking mDNS
// directly also works inside CI containers, where device enumeration often does not.
//
// This parses untrusted network data. Every read is bounds-checked.
// ============================================================================

#pragma once

namespace NetworkMidiTest
{
    struct MdnsDiscoveredHost
    {
        std::wstring InstanceName;      // "pmb-wopr2-midisrv-01"
        std::wstring FullName;          // "pmb-wopr2-midisrv-01._midi2._udp.local"
        std::wstring TargetHostName;    // "pmb-wopr2.local"
        std::vector<std::wstring> IPv4Addresses;
        uint16_t Port{ 0 };
        std::map<std::wstring, std::wstring> TextAttributes;

        std::wstring TextAttribute(_In_ std::wstring const& key) const
        {
            auto it = TextAttributes.find(key);

            return it == TextAttributes.end() ? std::wstring{ } : it->second;
        }
    };

    // Sends a PTR query for the supplied service type on every local IPv4 interface and
    // collects responses until the timeout expires. Never throws.
    std::vector<MdnsDiscoveredHost> DiscoverMdnsServices(
        _In_ std::string const& serviceType,
        _In_ std::chrono::milliseconds const timeout);
}
