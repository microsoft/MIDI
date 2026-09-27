// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// PROTOTYPE. A passive mDNS watcher, to see exactly which records a responder sends.
//
// Joins 224.0.0.251 on every IPv4 interface with a shared port 5353 socket, so it sees the same
// multicast traffic as the other responders on this PC, including this PC's own announcements.
// It never sends anything. Everything it reads is untrusted and parsed with bounds checks.
// ============================================================================

#pragma once

#include "spike_common.h"

#include <iphlpapi.h>

#include <cctype>
#include <map>

#pragma comment(lib, "iphlpapi.lib")

namespace Spike
{
    namespace Mdns
    {
        // Reads a possibly compressed DNS name. pos moves past the name as it appears in place.
        inline bool ReadName(uint8_t const* message, size_t size, size_t& pos, std::string& name)
        {
            name.clear();

            size_t cursor = pos;
            bool jumped = false;
            int jumps = 0;

            for (;;)
            {
                if (cursor >= size) return false;

                auto const length = message[cursor];

                if (length == 0)
                {
                    if (!jumped) pos = cursor + 1;
                    return true;
                }

                if ((length & 0xC0) == 0xC0)
                {
                    if (cursor + 1 >= size) return false;

                    auto const target = (static_cast<size_t>(length & 0x3F) << 8) | message[cursor + 1];
                    if (!jumped) pos = cursor + 2;
                    jumped = true;

                    if (++jumps > 32 || target >= size) return false;
                    cursor = target;
                    continue;
                }

                if ((length & 0xC0) != 0) return false;
                if (cursor + 1 + length > size) return false;

                if (!name.empty()) name += '.';

                for (size_t i = 0; i < length; i++)
                {
                    auto const c = static_cast<char>(message[cursor + 1 + i]);
                    if (c == '.') name += "\\.";
                    else name += c;
                }

                if (name.size() > 1024) return false;

                cursor += 1u + length;
            }
        }

        inline char const* TypeName(uint16_t type)
        {
            switch (type)
            {
            case 1: return "A";
            case 12: return "PTR";
            case 16: return "TXT";
            case 28: return "AAAA";
            case 33: return "SRV";
            case 47: return "NSEC";
            case 255: return "ANY";
            default: return "?";
            }
        }

        struct Record
        {
            char const* Section{ "" };
            std::string Name;
            uint16_t Type{ 0 };
            bool TopBit{ false };       // cache-flush in a response record, unicast-response in a question
            uint32_t Ttl{ 0 };
            std::string Data;
            bool IsQuestion{ false };
        };

        struct Message
        {
            uint16_t Id{ 0 };
            uint16_t Flags{ 0 };
            bool IsResponse{ false };
            std::vector<Record> Records;
        };

        inline std::string DescribeRdata(uint8_t const* message, size_t size, size_t start, uint16_t length, uint16_t type)
        {
            if (start + length > size) return "(truncated)";

            size_t pos = start;
            std::string text;

            switch (type)
            {
            case 12:
                if (!ReadName(message, size, pos, text)) return "(bad name)";
                return "-> " + text;

            case 33:
            {
                if (length < 7) return "(bad SRV)";
                auto const port = static_cast<uint16_t>((message[start + 4] << 8) | message[start + 5]);
                pos = start + 6;
                if (!ReadName(message, size, pos, text)) return "(bad SRV target)";
                return "port " + std::to_string(port) + " target " + text;
            }

            case 16:
            {
                if (length == 0) return "(no strings at all)";
                std::string strings;
                size_t cursor = start;
                while (cursor < start + length)
                {
                    auto const part = message[cursor];
                    if (cursor + 1 + part > start + length) return strings + " (bad TXT)";
                    strings += strings.empty() ? "\"" : ", \"";
                    strings.append(reinterpret_cast<char const*>(message + cursor + 1), part);
                    strings += "\"";
                    cursor += 1u + part;
                }
                return strings;
            }

            case 1:
            {
                if (length != 4) return "(bad A)";
                char buffer[32]{};
                snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u", message[start], message[start + 1], message[start + 2], message[start + 3]);
                return buffer;
            }

            case 28:
            {
                if (length != 16) return "(bad AAAA)";
                char buffer[64]{};
                in6_addr address{};
                memcpy(&address, message + start, 16);
                InetNtopA(AF_INET6, &address, buffer, sizeof(buffer));
                return buffer;
            }

            default:
                return "(" + std::to_string(length) + " bytes)";
            }
        }

        inline bool Parse(uint8_t const* message, size_t size, Message& parsed)
        {
            if (size < 12) return false;

            auto const read16 = [&](size_t at) { return static_cast<uint16_t>((message[at] << 8) | message[at + 1]); };

            parsed = Message{};
            parsed.Id = read16(0);
            parsed.Flags = read16(2);
            parsed.IsResponse = (parsed.Flags & 0x8000) != 0;

            uint16_t const counts[4] = { read16(4), read16(6), read16(8), read16(10) };
            static char const* const sections[4] = { "qd", "an", "ns", "ar" };

            size_t pos = 12;

            for (int section = 0; section < 4; section++)
            {
                for (uint16_t i = 0; i < counts[section]; i++)
                {
                    Record record{};
                    record.Section = sections[section];
                    record.IsQuestion = section == 0;

                    if (!ReadName(message, size, pos, record.Name)) return false;

                    if (record.IsQuestion)
                    {
                        if (pos + 4 > size) return false;
                        record.Type = read16(pos);
                        record.TopBit = (read16(pos + 2) & 0x8000) != 0;
                        pos += 4;
                    }
                    else
                    {
                        if (pos + 10 > size) return false;
                        record.Type = read16(pos);
                        record.TopBit = (read16(pos + 2) & 0x8000) != 0;
                        record.Ttl = (static_cast<uint32_t>(read16(pos + 4)) << 16) | read16(pos + 6);
                        auto const length = read16(pos + 8);
                        pos += 10;
                        if (pos + length > size) return false;
                        record.Data = DescribeRdata(message, size, pos, length, record.Type);
                        pos += length;
                    }

                    parsed.Records.push_back(std::move(record));
                }
            }

            return true;
        }

        inline bool ContainsIgnoringCase(std::string const& text, std::string const& part)
        {
            if (part.empty()) return true;

            auto const it = std::search(text.begin(), text.end(), part.begin(), part.end(),
                [](char a, char b) { return tolower(static_cast<unsigned char>(a)) == tolower(static_cast<unsigned char>(b)); });

            return it != text.end();
        }
    }

    // Returns the number of interfaces joined.
    inline int JoinMdnsGroupOnAllInterfaces(SOCKET socket)
    {
        ULONG size = 0;
        GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr, nullptr, &size);

        std::vector<uint8_t> buffer(size);
        auto adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

        if (size == 0 || GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr, adapters, &size) != NO_ERROR)
        {
            return 0;
        }

        int joined = 0;

        for (auto adapter = adapters; adapter != nullptr; adapter = adapter->Next)
        {
            if (adapter->OperStatus != IfOperStatusUp) continue;

            for (auto unicast = adapter->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next)
            {
                if (unicast->Address.lpSockaddr->sa_family != AF_INET) continue;

                ip_mreq request{};
                InetPtonA(AF_INET, "224.0.0.251", &request.imr_multiaddr);
                request.imr_interface = reinterpret_cast<sockaddr_in*>(unicast->Address.lpSockaddr)->sin_addr;

                char text[32]{};
                InetNtopA(AF_INET, &request.imr_interface, text, sizeof(text));

                if (setsockopt(socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<char const*>(&request), sizeof(request)) == 0)
                {
                    joined++;
                    Print("  joined 224.0.0.251 on %s (%s)", text, ToUtf8(adapter->FriendlyName).c_str());
                }
            }
        }

        return joined;
    }

    // Prints every mDNS message that mentions the filter text, for the given number of seconds.
    inline int WatchMdns(std::string const& filter, uint32_t seconds, bool includeQueries)
    {
        auto const socket = WSASocketW(AF_INET, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, 0);
        if (socket == INVALID_SOCKET) { Print("socket failed: %d", WSAGetLastError()); return 1; }

        BOOL reuse = TRUE;
        setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char const*>(&reuse), sizeof(reuse));

        DWORD timeout = 250;
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char const*>(&timeout), sizeof(timeout));

        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_port = htons(5353);
        local.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(socket, reinterpret_cast<sockaddr const*>(&local), sizeof(local)) == SOCKET_ERROR)
        {
            Print("bind to 5353 failed: %d", WSAGetLastError());
            closesocket(socket);
            return 1;
        }

        if (JoinMdnsGroupOnAllInterfaces(socket) == 0)
        {
            Print("could not join the mDNS group on any interface");
            closesocket(socket);
            return 1;
        }

        Print("Watching mDNS for \"%s\" for %u seconds%s.", filter.c_str(), seconds, includeQueries ? ", queries included" : "");

        // per source: PTR records whose owner is a service type, with and without cache-flush
        std::map<std::string, std::pair<uint32_t, uint32_t>> sharedPtrFlush;

        std::vector<uint8_t> buffer(9000);
        auto const end = GetTickCount64() + static_cast<uint64_t>(seconds) * 1000;

        while (!StopRequested() && GetTickCount64() < end)
        {
            sockaddr_in from{};
            int fromLength = sizeof(from);

            auto const received = recvfrom(socket, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0,
                reinterpret_cast<sockaddr*>(&from), &fromLength);

            if (received == SOCKET_ERROR) continue;

            Mdns::Message message{};
            if (!Mdns::Parse(buffer.data(), static_cast<size_t>(received), message)) continue;
            if (!message.IsResponse && !includeQueries) continue;

            bool relevant = false;
            for (auto const& record : message.Records)
            {
                if (Mdns::ContainsIgnoringCase(record.Name, filter) || Mdns::ContainsIgnoringCase(record.Data, filter)) relevant = true;
            }

            if (!relevant) continue;

            char source[32]{};
            InetNtopA(AF_INET, &from.sin_addr, source, sizeof(source));

            SYSTEMTIME now{};
            GetLocalTime(&now);

            Print("%02u:%02u:%02u.%03u  %s from %s:%u  id %u  flags %04X", now.wHour, now.wMinute, now.wSecond, now.wMilliseconds,
                message.IsResponse ? "response" : "query   ", source, ntohs(from.sin_port), message.Id, message.Flags);

            for (auto const& record : message.Records)
            {
                if (record.IsQuestion)
                {
                    Print("    %s  %-58s %-4s %s", record.Section, record.Name.c_str(), Mdns::TypeName(record.Type), record.TopBit ? "QU" : "QM");
                    continue;
                }

                Print("    %s  %-58s %-4s %-5s ttl %-5u %s", record.Section, record.Name.c_str(), Mdns::TypeName(record.Type),
                    record.TopBit ? "FLUSH" : "", record.Ttl, record.Data.c_str());

                // a PTR owned by "_service._proto.local" is the shared DNS-SD enumeration record
                if (message.IsResponse && record.Type == 12 && !record.Name.empty() && record.Name[0] == '_')
                {
                    auto& counts = sharedPtrFlush[source];
                    if (record.TopBit) counts.first++;
                    else counts.second++;
                }
            }
        }

        closesocket(socket);

        Print("Shared service PTR records seen, by source:");
        for (auto const& [source, counts] : sharedPtrFlush)
        {
            Print("  %-16s  %u with cache-flush set, %u without", source.c_str(), counts.first, counts.second);
        }

        return 0;
    }
}
