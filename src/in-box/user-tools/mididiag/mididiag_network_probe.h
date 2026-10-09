// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

// Network facts that come from the IP Helper and DNS-SD APIs. Nothing here calls the MIDI
// service.

#pragma once

namespace mididiag::netprobe
{
    enum class AddressKind
    {
        IPv4,
        IPv6LinkLocal,
        IPv6UniqueLocal,
        IPv6Global,
    };

    struct AdapterAddress
    {
        // as Windows writes it, without a zone
        std::wstring Text{};
        AddressKind Kind{ AddressKind::IPv4 };

        // an IPv6 address Windows makes up for privacy and replaces about once a day
        bool Temporary{ false };

        // a 169.254 address, which Windows gives itself when no DHCP server answers
        bool Autoconfigured{ false };

        // still works, but Windows no longer picks it for new connections
        bool Deprecated{ false };
    };

    struct AdapterInfo
    {
        // "{1A946373-4577-440A-BBB9-493FDF6C2A26}"
        std::wstring Id{};
        uint32_t InterfaceIndex{ 0 };

        // "Ethernet 3", the name Windows Settings shows
        std::wstring Name{};
        std::wstring Description{};

        // ethernet, wifi, ppp or other
        std::wstring Kind{};

        bool IsUp{ false };
        bool SupportsMulticast{ false };

        // false for a virtual adapter, such as one a VPN or Hyper-V adds
        bool IsHardware{ false };

        bool DhcpEnabled{ false };
        uint32_t Metric{ 0 };

        std::vector<AdapterAddress> Addresses{};
    };

    // Every adapter but loopback and tunnels, which is the set the network MIDI transports look
    // at. Like the transports, it leaves out addresses Windows is still checking for duplicates.
    // False when Windows would not list them.
    bool TryGetAdapters(_Out_ std::vector<AdapterInfo>& adapters) noexcept;

    struct UdpPortUser
    {
        uint32_t ProcessId{ 0 };
        uint32_t IPv4Sockets{ 0 };
        uint32_t IPv6Sockets{ 0 };
    };

    // The processes with a UDP socket on this local port. False when Windows would not list them.
    bool TryGetUdpPortUsers(_In_ uint16_t const port, _Out_ std::vector<UdpPortUser>& users) noexcept;

    struct ServiceLookupResult
    {
        bool Answered{ false };

        // the Win32 error: 0 when answered, ERROR_TIMEOUT when nothing answered in time
        uint32_t Status{ 0 };

        std::wstring HostName{};
        uint16_t Port{ 0 };
        uint64_t ElapsedMilliseconds{ 0 };
    };

    // Looks up a DNS-SD service instance, such as "Studio._midi2._udp.local". This PC answers
    // for the services it advertises, so no answer for one of its own hosts means the
    // advertisement is gone. An answer does not prove that other computers can hear it.
    ServiceLookupResult LookUpServiceInstance(_In_ std::wstring const& fullName, _In_ uint32_t const timeoutMilliseconds) noexcept;
}
