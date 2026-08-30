// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================

#include "pch.h"

namespace NetworkMidiTest
{
    namespace
    {
        constexpr uint16_t MdnsPort = 5353;
        constexpr char MdnsMulticastAddress[] = "224.0.0.251";

        constexpr uint16_t DnsTypeA = 1;
        constexpr uint16_t DnsTypePtr = 12;
        constexpr uint16_t DnsTypeTxt = 16;
        constexpr uint16_t DnsTypeSrv = 33;

        // A response can legally chain compression pointers. Cap the follow count so a
        // malicious or malformed packet cannot spin us.
        constexpr int MaxCompressionJumps = 64;

        struct DnsRecord
        {
            std::string Name;
            uint16_t Type{ 0 };
            size_t RDataOffset{ 0 };
            size_t RDataLength{ 0 };
        };


        std::wstring Widen(_In_ std::string const& value)
        {
            if (value.empty()) return std::wstring{ };

            auto required = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);

            if (required <= 0) return std::wstring{ };

            std::wstring result(static_cast<size_t>(required), L'\0');

            MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), required);

            return result;
        }


        // Reads a DNS name at the supplied offset, following compression pointers.
        // Returns false if the packet is malformed. consumedEnd receives the offset just past
        // the name as encoded at 'offset' (not past any pointer target).
        bool ReadDnsName(
            std::vector<uint8_t> const& packet,
            size_t offset,
            std::string& name,
            size_t& consumedEnd)
        {
            name.clear();

            bool jumped{ false };
            int jumps{ 0 };
            size_t cursor = offset;

            consumedEnd = offset;

            while (true)
            {
                if (cursor >= packet.size()) return false;

                uint8_t length = packet[cursor];

                if (length == 0)
                {
                    ++cursor;

                    if (!jumped) consumedEnd = cursor;

                    return true;
                }

                if ((length & 0xC0) == 0xC0)
                {
                    if (cursor + 1 >= packet.size()) return false;

                    size_t pointer = (static_cast<size_t>(length & 0x3F) << 8) | packet[cursor + 1];

                    if (!jumped)
                    {
                        consumedEnd = cursor + 2;
                        jumped = true;
                    }

                    if (++jumps > MaxCompressionJumps) return false;
                    if (pointer >= packet.size()) return false;

                    cursor = pointer;

                    continue;
                }

                if ((length & 0xC0) != 0) return false;      // reserved label type

                ++cursor;

                if (cursor + length > packet.size()) return false;

                if (!name.empty()) name.push_back('.');

                name.append(reinterpret_cast<char const*>(packet.data() + cursor), length);

                cursor += length;
            }
        }


        bool ParseResponse(std::vector<uint8_t> const& packet, std::vector<DnsRecord>& records)
        {
            if (packet.size() < 12) return false;

            auto readUInt16 = [&packet](size_t offset) -> uint16_t
                {
                    return static_cast<uint16_t>((packet[offset] << 8) | packet[offset + 1]);
                };

            uint16_t questionCount = readUInt16(4);
            uint16_t answerCount = readUInt16(6);
            uint16_t authorityCount = readUInt16(8);
            uint16_t additionalCount = readUInt16(10);

            size_t cursor = 12;

            // skip questions
            for (uint16_t i = 0; i < questionCount; i++)
            {
                std::string ignored;
                size_t nameEnd{ 0 };

                if (!ReadDnsName(packet, cursor, ignored, nameEnd)) return false;

                cursor = nameEnd + 4;                        // QTYPE + QCLASS

                if (cursor > packet.size()) return false;
            }

            size_t totalRecords =
                static_cast<size_t>(answerCount) +
                static_cast<size_t>(authorityCount) +
                static_cast<size_t>(additionalCount);

            for (size_t i = 0; i < totalRecords; i++)
            {
                std::string name;
                size_t nameEnd{ 0 };

                if (!ReadDnsName(packet, cursor, name, nameEnd)) return false;

                cursor = nameEnd;

                if (cursor + 10 > packet.size()) return false;

                DnsRecord record{ };
                record.Name = name;
                record.Type = readUInt16(cursor);

                uint16_t rdataLength = readUInt16(cursor + 8);

                cursor += 10;

                if (cursor + rdataLength > packet.size()) return false;

                record.RDataOffset = cursor;
                record.RDataLength = rdataLength;

                records.push_back(record);

                cursor += rdataLength;
            }

            return true;
        }


        std::vector<uint8_t> BuildPtrQuery(std::string const& serviceType)
        {
            std::vector<uint8_t> packet;

            auto appendUInt16 = [&packet](uint16_t value)
                {
                    packet.push_back(static_cast<uint8_t>(value >> 8));
                    packet.push_back(static_cast<uint8_t>(value & 0xFF));
                };

            appendUInt16(0);            // transaction id, 0 for mDNS
            appendUInt16(0);            // flags: standard query
            appendUInt16(1);            // QDCOUNT
            appendUInt16(0);            // ANCOUNT
            appendUInt16(0);            // NSCOUNT
            appendUInt16(0);            // ARCOUNT

            // QNAME, as length-prefixed labels
            std::string remaining = serviceType;

            while (!remaining.empty())
            {
                auto dot = remaining.find('.');
                auto label = dot == std::string::npos ? remaining : remaining.substr(0, dot);

                if (label.size() > 63) return std::vector<uint8_t>{ };

                packet.push_back(static_cast<uint8_t>(label.size()));
                packet.insert(packet.end(), label.begin(), label.end());

                remaining = dot == std::string::npos ? std::string{ } : remaining.substr(dot + 1);
            }

            packet.push_back(0);        // root label

            appendUInt16(DnsTypePtr);
            appendUInt16(0x8001);       // QU bit set, so responders answer our source port directly

            return packet;
        }


        std::vector<uint32_t> GetLocalIPv4Addresses()
        {
            std::vector<uint32_t> addresses;

            ULONG size = 16 * 1024;
            std::vector<uint8_t> buffer(size);

            auto table = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

            auto result = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr, table, &size);

            if (result == ERROR_BUFFER_OVERFLOW)
            {
                buffer.resize(size);
                table = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
                result = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr, table, &size);
            }

            if (result != NO_ERROR) return addresses;

            for (auto adapter = table; adapter != nullptr; adapter = adapter->Next)
            {
                if (adapter->OperStatus != IfOperStatusUp) continue;
                if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;

                for (auto unicast = adapter->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next)
                {
                    if (unicast->Address.lpSockaddr->sa_family != AF_INET) continue;

                    auto in4 = reinterpret_cast<sockaddr_in*>(unicast->Address.lpSockaddr);

                    addresses.push_back(in4->sin_addr.s_addr);
                }
            }

            return addresses;
        }
    }


    _Use_decl_annotations_
    std::vector<MdnsDiscoveredHost> DiscoverMdnsServices(
        std::string const& serviceType,
        std::chrono::milliseconds const timeout)
    {
        std::vector<MdnsDiscoveredHost> discovered;

        auto query = BuildPtrQuery(serviceType);

        if (query.empty()) return discovered;

        auto localAddresses = GetLocalIPv4Addresses();

        if (localAddresses.empty()) return discovered;

        // One socket per interface. Multicast egress follows the interface the socket is
        // bound to, and a machine with several NICs would otherwise only query one of them.
        std::vector<SOCKET> sockets;

        auto cleanup = wil::scope_exit([&sockets]()
            {
                for (auto s : sockets)
                {
                    if (s != INVALID_SOCKET) closesocket(s);
                }
            });

        sockaddr_in destination{ };
        destination.sin_family = AF_INET;
        destination.sin_port = htons(MdnsPort);
        inet_pton(AF_INET, MdnsMulticastAddress, &destination.sin_addr);

        for (auto localAddress : localAddresses)
        {
            SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

            if (s == INVALID_SOCKET) continue;

            sockaddr_in local{ };
            local.sin_family = AF_INET;
            local.sin_addr.s_addr = localAddress;
            local.sin_port = 0;

            if (bind(s, reinterpret_cast<sockaddr*>(&local), sizeof(local)) == SOCKET_ERROR)
            {
                closesocket(s);

                continue;
            }

            DWORD ttl = 255;
            setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<char const*>(&ttl), sizeof(ttl));
            setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<char const*>(&localAddress), sizeof(localAddress));

            if (sendto(s, reinterpret_cast<char const*>(query.data()), static_cast<int>(query.size()), 0,
                    reinterpret_cast<sockaddr const*>(&destination), sizeof(destination)) == SOCKET_ERROR)
            {
                closesocket(s);

                continue;
            }

            sockets.push_back(s);
        }

        if (sockets.empty()) return discovered;

        // Accumulate records across every response before correlating. SRV, TXT and A records
        // frequently arrive in packets separate from the PTR that names the instance.
        std::vector<std::string> instanceNames;
        std::map<std::string, uint16_t> portsByInstance;
        std::map<std::string, std::string> targetsByInstance;
        std::map<std::string, std::map<std::wstring, std::wstring>> textByInstance;
        std::map<std::string, std::vector<std::wstring>> addressesByTarget;

        auto deadline = std::chrono::steady_clock::now() + timeout;
        auto nextQuery = std::chrono::steady_clock::now() + std::chrono::milliseconds(750);

        while (std::chrono::steady_clock::now() < deadline)
        {
            // Responders suppress answers to a query they have very recently answered, so a
            // single burst regularly misses hosts. Retransmitting is what a real mDNS client
            // does and is what makes discovery repeatable back-to-back.
            if (std::chrono::steady_clock::now() >= nextQuery)
            {
                for (auto s : sockets)
                {
                    sendto(s, reinterpret_cast<char const*>(query.data()), static_cast<int>(query.size()), 0,
                        reinterpret_cast<sockaddr const*>(&destination), sizeof(destination));
                }

                nextQuery = std::chrono::steady_clock::now() + std::chrono::milliseconds(750);
            }

            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());

            if (remaining.count() <= 0) break;

            // Bounded so the retransmit above stays on schedule.
            auto slice = std::min<std::chrono::milliseconds::rep>(remaining.count(), 250);

            fd_set readSet;
            FD_ZERO(&readSet);

            SOCKET highest{ 0 };

            for (auto s : sockets)
            {
                FD_SET(s, &readSet);

                if (s > highest) highest = s;
            }

            timeval tv{ };
            tv.tv_sec = static_cast<long>(slice / 1000);
            tv.tv_usec = static_cast<long>((slice % 1000) * 1000);

            int ready = select(static_cast<int>(highest + 1), &readSet, nullptr, nullptr, &tv);

            if (ready < 0) break;
            if (ready == 0) continue;       // nothing this slice; keep querying until the deadline

            for (auto s : sockets)
            {
                if (!FD_ISSET(s, &readSet)) continue;

                std::vector<uint8_t> buffer(9000);

                sockaddr_in from{ };
                int fromLength = sizeof(from);

                int received = recvfrom(s, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0,
                                    reinterpret_cast<sockaddr*>(&from), &fromLength);

                if (received <= 0) continue;

                buffer.resize(static_cast<size_t>(received));

                std::vector<DnsRecord> records;

                if (!ParseResponse(buffer, records)) continue;

                for (auto const& record : records)
                {
                    if (record.Type == DnsTypePtr && record.Name == serviceType)
                    {
                        std::string instance;
                        size_t end{ 0 };

                        if (ReadDnsName(buffer, record.RDataOffset, instance, end) && !instance.empty())
                        {
                            if (std::find(instanceNames.begin(), instanceNames.end(), instance) == instanceNames.end())
                            {
                                instanceNames.push_back(instance);
                            }
                        }
                    }
                    else if (record.Type == DnsTypeSrv && record.RDataLength >= 6)
                    {
                        auto offset = record.RDataOffset;

                        uint16_t port = static_cast<uint16_t>((buffer[offset + 4] << 8) | buffer[offset + 5]);

                        std::string target;
                        size_t end{ 0 };

                        if (ReadDnsName(buffer, offset + 6, target, end))
                        {
                            portsByInstance[record.Name] = port;
                            targetsByInstance[record.Name] = target;
                        }
                    }
                    else if (record.Type == DnsTypeTxt)
                    {
                        auto offset = record.RDataOffset;
                        auto end = record.RDataOffset + record.RDataLength;

                        auto& attributes = textByInstance[record.Name];

                        while (offset < end)
                        {
                            uint8_t length = buffer[offset];

                            ++offset;

                            if (offset + length > end) break;

                            std::string entry(reinterpret_cast<char const*>(buffer.data() + offset), length);

                            offset += length;

                            auto equals = entry.find('=');

                            if (equals != std::string::npos)
                            {
                                attributes[Widen(entry.substr(0, equals))] = Widen(entry.substr(equals + 1));
                            }
                        }
                    }
                    else if (record.Type == DnsTypeA && record.RDataLength == 4)
                    {
                        char text[INET_ADDRSTRLEN]{ };

                        in_addr address{ };
                        memcpy(&address, buffer.data() + record.RDataOffset, 4);

                        if (inet_ntop(AF_INET, &address, text, sizeof(text)) != nullptr)
                        {
                            auto& list = addressesByTarget[record.Name];
                            auto wide = Widen(std::string{ text });

                            if (std::find(list.begin(), list.end(), wide) == list.end())
                            {
                                list.push_back(wide);
                            }
                        }
                    }
                }
            }
        }

        for (auto const& instance : instanceNames)
        {
            MdnsDiscoveredHost host{ };

            host.FullName = Widen(instance);

            auto dot = instance.find('.');
            host.InstanceName = Widen(dot == std::string::npos ? instance : instance.substr(0, dot));

            auto portIt = portsByInstance.find(instance);
            if (portIt != portsByInstance.end()) host.Port = portIt->second;

            auto targetIt = targetsByInstance.find(instance);

            if (targetIt != targetsByInstance.end())
            {
                host.TargetHostName = Widen(targetIt->second);

                auto addressIt = addressesByTarget.find(targetIt->second);

                if (addressIt != addressesByTarget.end()) host.IPv4Addresses = addressIt->second;
            }

            auto textIt = textByInstance.find(instance);
            if (textIt != textByInstance.end()) host.TextAttributes = textIt->second;

            discovered.push_back(host);
        }

        return discovered;
    }
}
