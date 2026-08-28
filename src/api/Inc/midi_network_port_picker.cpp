// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// See midi_network_port_picker.h. winsock2.h has to come before windows.h, which is why this
// lives in its own translation unit rather than in the header.
// ============================================================================

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>

#include <algorithm>
#include <memory>
#include <random>
#include <vector>

#include "midi_network_port_picker.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

namespace WindowsMidiServicesInternal
{
    namespace
    {
        // Assignments inside the generated range which are in real use on the kind of machine
        // this runs on, so a generated port never lands on one.
        struct ReservedRange { uint16_t Low; uint16_t High; };

        constexpr ReservedRange ReservedRanges[] =
        {
            { 41794, 41795 },   // Crestron control
            { 44818, 44818 },   // EtherNet/IP
            { 47808, 47823 },   // BACnet/IP
        };

        bool IsReserved(_In_ uint16_t const port)
        {
            for (auto const& range : ReservedRanges)
            {
                if (port >= range.Low && port <= range.High) return true;
            }

            return false;
        }

        // Winsock has to be up for the bind probe. The service and the SDK both already start it
        // elsewhere, but this must not depend on that.
        struct WinsockScope
        {
            bool Started{ false };

            WinsockScope()
            {
                WSADATA data{ };

                Started = (WSAStartup(MAKEWORD(2, 2), &data) == 0);
            }

            ~WinsockScope()
            {
                if (Started) WSACleanup();
            }
        };

        // Everything the OS reports as bound, whatever the owning process and whether or not that
        // socket would allow another bind on top of it.
        bool IsPortInUdpTable(_In_ uint16_t const port)
        {
            ULONG size{ 0 };

            if (GetExtendedUdpTable(nullptr, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0) != ERROR_INSUFFICIENT_BUFFER)
            {
                return false;
            }

            auto buffer = std::make_unique<uint8_t[]>(size);

            if (GetExtendedUdpTable(buffer.get(), &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0) != NO_ERROR)
            {
                return false;
            }

            auto const table = reinterpret_cast<MIB_UDPTABLE_OWNER_PID const*>(buffer.get());

            for (DWORD i = 0; i < table->dwNumEntries; i++)
            {
                // dwLocalPort is in network byte order
                if (ntohs(static_cast<u_short>(table->table[i].dwLocalPort)) == port)
                {
                    return true;
                }
            }

            return false;
        }

        bool CanBindPort(_In_ uint16_t const port)
        {
            auto socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

            if (socketHandle == INVALID_SOCKET) return false;

            // Stricter than the host's own bind, so this can only ever be over cautious: it also
            // refuses a port some other socket has left open to sharing.
            BOOL exclusive{ TRUE };
            setsockopt(socketHandle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                reinterpret_cast<char const*>(&exclusive), sizeof(exclusive));

            sockaddr_in address{ };
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = INADDR_ANY;
            address.sin_port = htons(port);

            auto const result = bind(socketHandle, reinterpret_cast<sockaddr const*>(&address), sizeof(address));

            closesocket(socketHandle);

            // WSAEACCES is what a port held by a LocalSystem service reports to a user process.
            return result != SOCKET_ERROR;
        }
    }


    _Use_decl_annotations_
    bool IsUdpPortAvailable(uint16_t const port)
    {
        if (port == 0) return false;

        WinsockScope winsock;

        if (!winsock.Started) return false;

        if (IsPortInUdpTable(port)) return false;

        return CanBindPort(port);
    }


    _Use_decl_annotations_
    bool TryGenerateAvailableHostPort(uint16_t& port)
    {
        port = 0;

        WinsockScope winsock;

        if (!winsock.Started) return false;

        std::random_device seed;
        std::mt19937 generator{ seed() };
        std::uniform_int_distribution<int> distribution{
            MidiNetworkGeneratedPortRangeLow, MidiNetworkGeneratedPortRangeHigh };

        // Random rather than sequential so two machines set up from the same image, or two hosts
        // on one machine, do not converge on the same number.
        constexpr int maxAttempts = 64;

        for (int attempt = 0; attempt < maxAttempts; attempt++)
        {
            auto const candidate = static_cast<uint16_t>(distribution(generator));

            if (IsReserved(candidate)) continue;

            if (IsPortInUdpTable(candidate)) continue;

            if (!CanBindPort(candidate)) continue;

            port = candidate;

            return true;
        }

        return false;
    }
}
