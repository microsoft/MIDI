// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Which of a remote device's addresses the Network MIDI 2.0 and RTP-MIDI clients try, and in
// what order.
//
// A device can have IPv4 and IPv6 addresses, and an IPv6 device usually has several. Nothing in
// DNS-SD says which one to use, so the addresses are put in the order Windows puts the addresses
// of any name it looks up. That order comes from the prefix policy table (Get-NetPrefixPolicy),
// which an administrator can change, for example to prefer IPv4. When an address does not
// answer, the next attempt goes to the next one.
//
// Declarations only, for the same reason as midi_network_adapters.h: the implementation needs
// winsock2.h ahead of windows.h.
// ============================================================================

#pragma once

#include <cstdint>
#include <stop_token>
#include <string>
#include <vector>

#include <sal.h>

namespace WindowsMidiServicesInternal
{
    // The addresses in the order Windows would try them, without the ones this PC has no route
    // to. Each is text like "192.168.1.10" or "fe80::1%14", and comes back exactly as given. A
    // link-local IPv6 address without the %scope of its adapter is left out, because there is no
    // telling which adapter reaches it. If Windows can't rank the rest, or ranks none of them,
    // they come back in the order given.
    std::vector<std::wstring> SortMidiNetworkAddresses(_In_ std::vector<std::wstring> const& addresses);

    // Looks up a host name with every name service Windows has, multicast DNS for ".local"
    // names included, and returns its addresses as text in the form above. Empty when the name
    // isn't found within timeoutSeconds, or once stopToken is signaled.
    std::vector<std::wstring> ResolveMidiNetworkHostName(
        _In_ std::wstring const& hostName,
        _In_ uint32_t const timeoutSeconds,
        _In_ std::stop_token const& stopToken);

    // The address to invite next. The one the last session opened on goes first, if it is still
    // listed, then the others in order. Each attempt nobody answered moves on to the next address,
    // and after the last one it starts over. Empty when there are no addresses.
    std::wstring ChooseMidiNetworkAddress(
        _In_ std::vector<std::wstring> const& addresses,
        _In_ std::wstring const& connectedAddress,
        _In_ uint32_t const unansweredAttempts);

    // True when the attempt after this many unanswered ones goes to an address which hasn't had
    // its turn yet, so it can go at once rather than wait to try again.
    bool IsNextMidiNetworkAddressUntried(
        _In_ uint32_t const unansweredAttempts,
        _In_ uint32_t const addressCount) noexcept;
}
