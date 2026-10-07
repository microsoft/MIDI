// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Sockets, DNS-SD advertising and text conversion for the rtpMIDI transport.
//
// Adapted from the prototype spike, which proved each piece against macOS. Dual-stack IPv6
// sockets, because macOS invites over IPv6 link-local as readily as over IPv4.
// ============================================================================

#pragma once

#include <array>
#include <compare>
#include <shared_mutex>

#include "RtpMidiReplySources.h"

namespace RtpMidiText
{
    inline std::wstring Utf8ToWide(_In_ std::string const& text)
    {
        if (text.empty()) return {};

        auto const size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (size <= 0) return {};

        std::wstring result(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
        return result;
    }

    inline std::string WideToUtf8(_In_ std::wstring const& text)
    {
        if (text.empty()) return {};

        auto const size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};

        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

    inline size_t Utf8ByteCount(_In_ std::wstring const& text)
    {
        return WideToUtf8(text).size();
    }

    // FNV-1a over UTF-8. Stable across builds, processes and reboots, which std::hash is not.
    inline std::wstring StableHash(_In_ std::wstring const& text)
    {
        uint64_t hash = 0xCBF29CE484222325ull;

        for (auto const byte : WideToUtf8(text))
        {
            hash ^= static_cast<uint8_t>(byte);
            hash *= 0x100000001B3ull;
        }

        wchar_t buffer[17]{};
        swprintf_s(buffer, L"%016llX", hash);
        return buffer;
    }

    inline std::wstring GuidToString(_In_ GUID const& guid)
    {
        wchar_t buffer[40]{};
        if (StringFromGUID2(guid, buffer, ARRAYSIZE(buffer)) == 0) return {};
        return buffer;
    }
}

namespace RtpMidiNet
{
    inline RtpMidi::PeerAddress FromSockaddr(_In_ sockaddr_storage const& storage)
    {
        RtpMidi::PeerAddress address{};

        if (storage.ss_family == AF_INET)
        {
            auto const& v4 = reinterpret_cast<sockaddr_in const&>(storage);
            address.Family = 4;
            memcpy(address.Bytes.data(), &v4.sin_addr, 4);
            address.Port = ntohs(v4.sin_port);
        }
        else if (storage.ss_family == AF_INET6)
        {
            auto const& v6 = reinterpret_cast<sockaddr_in6 const&>(storage);
            address.Port = ntohs(v6.sin6_port);

            if (IN6_IS_ADDR_V4MAPPED(&v6.sin6_addr))
            {
                address.Family = 4;
                memcpy(address.Bytes.data(), &v6.sin6_addr.u.Byte[12], 4);
            }
            else
            {
                address.Family = 6;
                memcpy(address.Bytes.data(), &v6.sin6_addr, 16);
                address.ScopeId = v6.sin6_scope_id;
            }
        }

        return address;
    }

    // Always an IPv6 socket address, because the sockets are dual-stack
    inline sockaddr_in6 ToSockaddr(_In_ RtpMidi::PeerAddress const& address)
    {
        sockaddr_in6 v6{};
        v6.sin6_family = AF_INET6;
        v6.sin6_port = htons(address.Port);

        if (address.Family == 4)
        {
            v6.sin6_addr.u.Byte[10] = 0xFF;
            v6.sin6_addr.u.Byte[11] = 0xFF;
            memcpy(&v6.sin6_addr.u.Byte[12], address.Bytes.data(), 4);
        }
        else
        {
            memcpy(&v6.sin6_addr, address.Bytes.data(), 16);
            v6.sin6_scope_id = address.ScopeId;
        }

        return v6;
    }

    // IPv4, or IPv6 with an optional %scope suffix
    inline bool TryParseAddress(_In_ std::wstring const& text, _In_ uint16_t const port, _Out_ RtpMidi::PeerAddress& address)
    {
        address = RtpMidi::PeerAddress{};

        in_addr v4{};
        if (InetPtonW(AF_INET, text.c_str(), &v4) == 1)
        {
            address.Family = 4;
            memcpy(address.Bytes.data(), &v4, 4);
            address.Port = port;
            return true;
        }

        std::wstring literal = text;
        uint32_t scope = 0;

        auto const percent = literal.find(L'%');
        if (percent != std::wstring::npos)
        {
            scope = static_cast<uint32_t>(wcstoul(literal.c_str() + percent + 1, nullptr, 10));
            literal.resize(percent);
        }

        in6_addr v6{};
        if (InetPtonW(AF_INET6, literal.c_str(), &v6) == 1)
        {
            address.Family = 6;
            memcpy(address.Bytes.data(), &v6, 16);
            address.ScopeId = scope;
            address.Port = port;
            return true;
        }

        return false;
    }

    inline std::wstring AddressToString(_In_ RtpMidi::PeerAddress const& address)
    {
        if (address.Family == 4)
        {
            wchar_t buffer[INET_ADDRSTRLEN]{};
            InetNtopW(AF_INET, address.Bytes.data(), buffer, ARRAYSIZE(buffer));
            return buffer;
        }

        if (address.Family == 6)
        {
            wchar_t buffer[INET6_ADDRSTRLEN]{};
            InetNtopW(AF_INET6, address.Bytes.data(), buffer, ARRAYSIZE(buffer));

            std::wstring text{ buffer };
            if (address.ScopeId != 0) text += L"%" + std::to_wstring(address.ScopeId);
            return text;
        }

        return {};
    }

    class UdpSocket
    {
    public:
        using ReceiveHandler = std::function<void(RtpMidi::PeerAddress const& from, uint8_t const* data, size_t size)>;

        UdpSocket() = default;
        ~UdpSocket() { Close(); }

        UdpSocket(_In_ UdpSocket const&) = delete;
        UdpSocket& operator=(_In_ UdpSocket const&) = delete;

        bool Bind(_In_ uint16_t const port, _Out_ int& error)
        {
            error = 0;

            auto const socket = WSASocketW(AF_INET6, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, WSA_FLAG_OVERLAPPED);
            if (socket == INVALID_SOCKET) { error = WSAGetLastError(); return false; }

            m_socket = socket;

            DWORD off = 0;
            setsockopt(socket, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<char const*>(&off), sizeof(off));

            DWORD on = 1;
            setsockopt(socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<char const*>(&on), sizeof(on));

            // An ICMP port unreachable from one send would otherwise fail the next receive, and one
            // peer going away must not break the socket for every other connection on it.
            BOOL reportReset = FALSE;
            DWORD returned = 0;
            WSAIoctl(socket, SIO_UDP_CONNRESET, &reportReset, sizeof(reportReset), nullptr, 0, &returned, nullptr, nullptr);

            // Every connection on a host shares one socket, so a burst from several remotes at once
            // has to fit in its buffer while the receive thread catches up. The default is far
            // smaller than a few peers sending SysEx at the same moment.
            int receiveBufferBytes = 1024 * 1024;
            setsockopt(socket, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char const*>(&receiveBufferBytes), sizeof(receiveBufferBytes));

            // Which address and adapter each datagram arrived on. Both levels, because this is a
            // dual-stack socket and IPv4 arrivals report at the IPv4 level. Measured: IPv4 and
            // link-local IPv6 both report the adapter, and a reply sent with the address set
            // goes out from it.
            DWORD packetInfo = 1;
            setsockopt(socket, IPPROTO_IP, IP_PKTINFO, reinterpret_cast<char const*>(&packetInfo), sizeof(packetInfo));
            setsockopt(socket, IPPROTO_IPV6, IPV6_PKTINFO, reinterpret_cast<char const*>(&packetInfo), sizeof(packetInfo));

            GUID receiveMessageId = WSAID_WSARECVMSG;
            DWORD functionBytes = 0;

            if (WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &receiveMessageId, sizeof(receiveMessageId),
                &m_receiveMessage, sizeof(m_receiveMessage), &functionBytes, nullptr, nullptr) == SOCKET_ERROR)
            {
                m_receiveMessage = nullptr;
            }

            sockaddr_in6 local{};
            local.sin6_family = AF_INET6;
            local.sin6_addr = in6addr_any;
            local.sin6_port = htons(port);

            if (bind(socket, reinterpret_cast<sockaddr const*>(&local), sizeof(local)) == SOCKET_ERROR)
            {
                error = WSAGetLastError();
                Close();
                return false;
            }

            sockaddr_in6 bound{};
            int length = sizeof(bound);
            getsockname(socket, reinterpret_cast<sockaddr*>(&bound), &length);
            m_port = ntohs(bound.sin6_port);

            return true;
        }

        uint16_t Port() const noexcept { return m_port; }

        // Before StartReceiving. A datagram arriving on any other interface is dropped, as though
        // nothing were listening. Empty means every interface.
        void LimitToInterfaces(_In_ std::vector<uint32_t> interfaces) { m_interfaces = std::move(interfaces); }

        HRESULT StartReceiving(_In_ ReceiveHandler handler, _In_ PCWSTR const threadName) noexcept
        {
            try
            {
                m_handler = std::move(handler);
                m_thread = std::thread([this]() { ReceiveLoop(); });

                SetThreadDescription(m_thread.native_handle(), threadName);
                SetThreadPriority(m_thread.native_handle(), THREAD_PRIORITY_HIGHEST);

                return S_OK;
            }
            CATCH_RETURN();
        }

        bool Send(_In_ RtpMidi::PeerAddress const& to, _In_ std::vector<uint8_t> const& datagram) noexcept
        {
            auto const socket = m_socket.load();
            if (socket == INVALID_SOCKET || datagram.empty()) return false;

            auto const address = ToSockaddr(to);

            // From the address the remote reached this PC on. The socket is bound to every
            // address, so otherwise Windows picks one, and a remote which only accepts its peer's
            // address drops the reply. RFC 1122 4.1.3.5 asks for this.
            LocalAddress local{};

            if (m_replySources.TryGet(to, local))
            {
                if (SendFrom(socket, address, datagram, local)) return true;
            }

            auto const sent = sendto(socket, reinterpret_cast<char const*>(datagram.data()), static_cast<int>(datagram.size()), 0,
                reinterpret_cast<sockaddr const*>(&address), sizeof(address));

            return sent == static_cast<int>(datagram.size());
        }

        void Close() noexcept
        {
            auto const socket = m_socket.exchange(INVALID_SOCKET);

            // closing the socket is what wakes the blocked receive
            if (socket != INVALID_SOCKET) closesocket(socket);

            if (m_thread.joinable())
            {
                // a joinable std::thread left behind terminates the process when it is destroyed
                if (m_thread.get_id() != std::this_thread::get_id()) m_thread.join();
                else m_thread.detach();
            }
        }

    private:
        // Every datagram can name a new remote, so the table is bounded
        static constexpr size_t MaxReplySources = 1024;

        static bool SendFrom(
            _In_ SOCKET const socket,
            _In_ sockaddr_in6 const& to,
            _In_ std::vector<uint8_t> const& datagram,
            _In_ LocalAddress const& local) noexcept
        {
            char control[WSA_CMSG_SPACE(sizeof(IN6_PKTINFO))]{};

            WSABUF buffer{ static_cast<ULONG>(datagram.size()), reinterpret_cast<char*>(const_cast<uint8_t*>(datagram.data())) };

            WSAMSG message{};
            message.name = reinterpret_cast<sockaddr*>(const_cast<sockaddr_in6*>(&to));
            message.namelen = sizeof(to);
            message.lpBuffers = &buffer;
            message.dwBufferCount = 1;
            message.Control.buf = control;

            auto const header = reinterpret_cast<WSACMSGHDR*>(control);

            // an IPv4 remote on this dual-stack socket takes the IPv4 form
            if (local.Family == AF_INET)
            {
                IN_PKTINFO info{};
                info.ipi_addr = local.IPv4;
                info.ipi_ifindex = local.InterfaceIndex;

                header->cmsg_level = IPPROTO_IP;
                header->cmsg_type = IP_PKTINFO;
                header->cmsg_len = WSA_CMSG_LEN(sizeof(info));
                memcpy(WSA_CMSG_DATA(header), &info, sizeof(info));

                message.Control.len = WSA_CMSG_SPACE(sizeof(info));
            }
            else
            {
                IN6_PKTINFO info{};
                info.ipi6_addr = local.IPv6;
                info.ipi6_ifindex = local.InterfaceIndex;

                header->cmsg_level = IPPROTO_IPV6;
                header->cmsg_type = IPV6_PKTINFO;
                header->cmsg_len = WSA_CMSG_LEN(sizeof(info));
                memcpy(WSA_CMSG_DATA(header), &info, sizeof(info));

                message.Control.len = WSA_CMSG_SPACE(sizeof(info));
            }

            DWORD sent{ 0 };

            return WSASendMsg(socket, &message, 0, &sent, nullptr, nullptr) == 0 && sent == datagram.size();
        }

        static bool TryReadLocalAddress(_In_ WSAMSG& message, _Out_ LocalAddress& local) noexcept
        {
            local = LocalAddress{};

            for (auto header = WSA_CMSG_FIRSTHDR(&message); header != nullptr; header = WSA_CMSG_NXTHDR(&message, header))
            {
                if (header->cmsg_level == IPPROTO_IP && header->cmsg_type == IP_PKTINFO)
                {
                    IN_PKTINFO info{};
                    memcpy(&info, WSA_CMSG_DATA(header), sizeof(info));

                    local.Family = AF_INET;
                    local.IPv4 = info.ipi_addr;
                    local.InterfaceIndex = info.ipi_ifindex;

                    return true;
                }

                if (header->cmsg_level == IPPROTO_IPV6 && header->cmsg_type == IPV6_PKTINFO)
                {
                    IN6_PKTINFO info{};
                    memcpy(&info, WSA_CMSG_DATA(header), sizeof(info));

                    local.Family = AF_INET6;
                    local.IPv6 = info.ipi6_addr;
                    local.InterfaceIndex = info.ipi6_ifindex;

                    return true;
                }
            }

            return false;
        }

        void ReceiveLoop() noexcept
        {
            try
            {
                std::vector<uint8_t> buffer(65536);

                for (;;)
                {
                    auto const socket = m_socket.load();
                    if (socket == INVALID_SOCKET) break;

                    sockaddr_storage from{};
                    int fromLength = sizeof(from);
                    int received{ SOCKET_ERROR };

                    LocalAddress local{};
                    bool knowLocal{ false };

                    if (m_receiveMessage != nullptr)
                    {
                        char control[256]{};

                        WSABUF data{ static_cast<ULONG>(buffer.size()), reinterpret_cast<char*>(buffer.data()) };

                        WSAMSG message{};
                        message.name = reinterpret_cast<sockaddr*>(&from);
                        message.namelen = fromLength;
                        message.lpBuffers = &data;
                        message.dwBufferCount = 1;
                        message.Control.buf = control;
                        message.Control.len = sizeof(control);

                        DWORD bytes{ 0 };

                        if (m_receiveMessage(socket, &message, &bytes, nullptr, nullptr) == 0)
                        {
                            received = static_cast<int>(bytes);
                            knowLocal = TryReadLocalAddress(message, local);
                        }
                    }
                    else
                    {
                        received = recvfrom(socket, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0,
                            reinterpret_cast<sockaddr*>(&from), &fromLength);
                    }

                    if (received == SOCKET_ERROR)
                    {
                        auto const error = WSAGetLastError();
                        if (error == WSAECONNRESET || error == WSAEMSGSIZE) continue;
                        break;
                    }

                    // A host limited to one adapter does not hear what arrives on the others. One
                    // which cannot tell where a datagram arrived does not answer it either.
                    if (!m_interfaces.empty() &&
                        (!knowLocal || std::find(m_interfaces.begin(), m_interfaces.end(), local.InterfaceIndex) == m_interfaces.end()))
                    {
                        continue;
                    }

                    if (received > 0 && m_handler)
                    {
                        auto const peer = FromSockaddr(from);

                        if (knowLocal) m_replySources.Remember(peer, local, GetTickCount64());

                        // one datagram that fails must not stop the socket for every connection on it
                        try
                        {
                            m_handler(peer, buffer.data(), static_cast<size_t>(received));
                        }
                        CATCH_LOG();
                    }
                }
            }
            CATCH_LOG();
        }

        std::atomic<SOCKET> m_socket{ INVALID_SOCKET };
        uint16_t m_port{ 0 };
        ReceiveHandler m_handler;
        std::thread m_thread;

        LPFN_WSARECVMSG m_receiveMessage{ nullptr };
        std::vector<uint32_t> m_interfaces;

        ReplySourceTable m_replySources{ MaxReplySources };
    };

    // AppleMIDI puts the data port at the control port plus one
    class PortPair
    {
    public:
        // Tries the preferred port first, then each range in turn, stepping by two
        bool Bind(
            _In_ uint16_t const preferredControlPort,
            _In_ std::vector<std::pair<uint16_t, uint16_t>> const& fallbackRanges,
            _Out_ bool& usedFallback)
        {
            usedFallback = false;

            if (preferredControlPort != 0 && TryBind(preferredControlPort)) return true;

            for (auto const& range : fallbackRanges)
            {
                for (uint32_t port = range.first; port + 1 <= range.second; port += 2)
                {
                    if (port == preferredControlPort) continue;

                    if (TryBind(static_cast<uint16_t>(port)))
                    {
                        usedFallback = preferredControlPort != 0;
                        return true;
                    }
                }
            }

            return false;
        }

        UdpSocket& Control() noexcept { return m_control; }
        UdpSocket& Data() noexcept { return m_data; }

        // Before either starts receiving. Empty is every interface.
        void LimitToInterfaces(_In_ std::vector<uint32_t> const& interfaces)
        {
            m_control.LimitToInterfaces(interfaces);
            m_data.LimitToInterfaces(interfaces);
        }

        void Close() noexcept
        {
            m_control.Close();
            m_data.Close();
        }

    private:
        bool TryBind(_In_ uint16_t const port)
        {
            int error = 0;

            if (!m_control.Bind(port, error)) return false;

            if (!m_data.Bind(static_cast<uint16_t>(port + 1), error))
            {
                m_control.Close();
                return false;
            }

            return true;
        }

        UdpSocket m_control;
        UdpSocket m_data;
    };

    // DNS-SD registration through the Windows DNS client. Registered once for the life of a host:
    // every new registration announces the shared PTR record with the cache-flush bit set, which
    // briefly removes other devices' AppleMIDI sessions from caches on the network. The endpoint
    // manager repeats the announcement correctly afterward. See midi_dnssd_announcer.h.
    class DnssdAdvertiser
    {
    public:
        DnssdAdvertiser() = default;
        ~DnssdAdvertiser() { Unregister(); }

        DnssdAdvertiser(_In_ DnssdAdvertiser const&) = delete;
        DnssdAdvertiser& operator=(_In_ DnssdAdvertiser const&) = delete;

        // Waits for the DNS client to finish probing the name, and gives up early when stopToken
        // is signaled. A non-zero interface index advertises on that adapter only.
        HRESULT Register(
            _In_ std::wstring const& instanceLabel,
            _In_ uint16_t const port,
            _In_ DWORD const timeoutMilliseconds,
            _In_ std::stop_token const& stopToken,
            _In_ uint32_t const interfaceIndex = 0) noexcept
        {
            try
            {
                RETURN_HR_IF(E_ILLEGAL_STATE_CHANGE, m_registration != nullptr);

                auto const requestedFullName = instanceLabel + L"." + MIDI_RTP_DNSSD_SERVICE_TYPE;

                wchar_t computerName[256]{};
                DWORD size = ARRAYSIZE(computerName) - 8;
                RETURN_IF_WIN32_BOOL_FALSE(GetComputerNameExW(ComputerNameDnsHostname, computerName, &size));
                auto const hostName = std::wstring{ computerName } + L".local";

                // Without a pair the TXT record goes out empty, which RFC 6763 does not allow.
                // AppleMIDI peers ignore TXT content.
                PCWSTR keys[] = { L"txtvers" };
                PCWSTR values[] = { L"1" };

                auto const registration = std::make_shared<Registration>();

                registration->Instance = DnsServiceConstructInstance(requestedFullName.c_str(), hostName.c_str(), nullptr, nullptr, port, 0, 0, 1, keys, values);
                RETURN_LAST_ERROR_IF_NULL(registration->Instance);

                registration->Request.Version = DNS_QUERY_REQUEST_VERSION1;
                registration->Request.InterfaceIndex = interfaceIndex;
                registration->Request.pServiceInstance = registration->Instance;
                registration->Request.pRegisterCompletionCallback = &DnssdAdvertiser::Completed;
                registration->Request.pQueryContext = registration.get();
                registration->Request.unicastEnabled = FALSE;

                // owed before the call, because the callback can come before the call returns
                registration->OweCallback(registration);

                auto const status = DnsServiceRegister(&registration->Request, &registration->Cancel);

                if (status != DNS_REQUEST_PENDING)
                {
                    // nothing started, so no callback is coming
                    registration->ForgiveCallback();
                    RETURN_WIN32(status);
                }

                {
                    auto namesLock = std::scoped_lock{ m_namesLock };
                    m_requestedFullName = requestedFullName;
                    m_registeredFullName.clear();
                }

                m_registration = registration;

                auto lock = std::unique_lock{ registration->Lock };

                if (!registration->Changed.wait_for(lock, stopToken, std::chrono::milliseconds(timeoutMilliseconds),
                    [&]() { return registration->Completions > 0; }))
                {
                    return stopToken.stop_requested() ? HRESULT_FROM_WIN32(ERROR_CANCELLED) : HRESULT_FROM_WIN32(ERROR_TIMEOUT);
                }

                RETURN_IF_WIN32_ERROR(registration->Status);

                auto registeredFullName = registration->RegisteredFullName;
                lock.unlock();

                {
                    auto namesLock = std::scoped_lock{ m_namesLock };
                    m_registeredFullName = std::move(registeredFullName);
                }

                return S_OK;
            }
            CATCH_RETURN();
        }

        // Sends the goodbye and waits a short while for the DNS client to confirm it. A callback
        // still to come after that finds its registration alive, because the registration keeps
        // itself alive until every callback it is owed has come.
        void Unregister() noexcept
        {
            auto const registration = std::exchange(m_registration, nullptr);
            if (registration == nullptr) return;

            try
            {
                uint32_t completions{ 0 };

                {
                    auto lock = std::scoped_lock{ registration->Lock };
                    completions = registration->Completions;
                }

                // A registration still probing is canceled. Whether its callback comes anyway is
                // not documented, so it stays owed.
                if (completions == 0) DnsServiceRegisterCancel(&registration->Cancel);

                registration->OweCallback(registration);

                // the goodbye for the records is sent by the DNS client service, not by this process
                if (DnsServiceDeRegister(&registration->Request, nullptr) == DNS_REQUEST_PENDING)
                {
                    auto lock = std::unique_lock{ registration->Lock };
                    registration->Changed.wait_for(lock, std::chrono::milliseconds(2000),
                        [&]() { return registration->Completions > completions; });
                }
                else
                {
                    registration->ForgiveCallback();
                }
            }
            catch (...)
            {
                // leaving now is safe for the same reason
            }
        }

        // The label actually on the network. The responder renames a colliding label rather than refusing it.
        std::wstring RegisteredLabel() const
        {
            auto lock = std::scoped_lock{ m_namesLock };

            std::wstring const suffix = std::wstring{ L"." } + MIDI_RTP_DNSSD_SERVICE_TYPE;
            auto name = m_registeredFullName.empty() ? m_requestedFullName : m_registeredFullName;

            if (name.size() > suffix.size() && _wcsicmp(name.c_str() + name.size() - suffix.size(), suffix.c_str()) == 0)
            {
                name.resize(name.size() - suffix.size());
            }

            return name;
        }

        bool WasRenamed() const
        {
            auto lock = std::scoped_lock{ m_namesLock };
            return !m_registeredFullName.empty() && _wcsicmp(m_registeredFullName.c_str(), m_requestedFullName.c_str()) != 0;
        }

    private:
        // Everything a DNS client callback touches. A canceled request may or may not still get
        // its callback, and nothing can wait for that, so the registration holds a reference to
        // itself while any callback is owed to it.
        struct Registration
        {
            Registration() = default;
            ~Registration() { if (Instance != nullptr) DnsServiceFreeInstance(Instance); }

            Registration(_In_ Registration const&) = delete;
            Registration& operator=(_In_ Registration const&) = delete;

            void OweCallback(_In_ std::shared_ptr<Registration> const& self)
            {
                auto lock = std::scoped_lock{ Lock };

                CallbacksOwed++;
                KeepAlive = self;
            }

            // for a request which did not start, so its callback is not coming
            void ForgiveCallback()
            {
                std::shared_ptr<Registration> release{ nullptr };

                auto lock = std::scoped_lock{ Lock };
                if (CallbacksOwed > 0 && --CallbacksOwed == 0) release = std::move(KeepAlive);
            }

            PDNS_SERVICE_INSTANCE Instance{ nullptr };
            DNS_SERVICE_REGISTER_REQUEST Request{};
            DNS_SERVICE_CANCEL Cancel{};

            std::mutex Lock;
            std::condition_variable_any Changed;
            uint32_t Completions{ 0 };
            uint32_t CallbacksOwed{ 0 };
            DWORD Status{ ERROR_SUCCESS };
            std::wstring RegisteredFullName;
            std::shared_ptr<Registration> KeepAlive{ nullptr };
        };

        // Runs on a DNS client thread, where an exception would end the process
        static VOID WINAPI Completed(_In_ DWORD status, _In_ PVOID context, _In_opt_ PDNS_SERVICE_INSTANCE instance) noexcept
        {
            auto const registration = static_cast<Registration*>(context);

            if (registration != nullptr)
            {
                try
                {
                    // the last callback owed lets the registration go, once this one is done with it
                    std::shared_ptr<Registration> release{ nullptr };

                    {
                        auto lock = std::scoped_lock{ registration->Lock };

                        registration->Status = status;
                        registration->Completions++;

                        if (registration->CallbacksOwed > 0 && --registration->CallbacksOwed == 0) release = std::move(registration->KeepAlive);

                        if (instance != nullptr && instance->pszInstanceName != nullptr)
                        {
                            try
                            {
                                registration->RegisteredFullName = instance->pszInstanceName;
                            }
                            catch (...)
                            {
                                // the label is only reported, so the requested one stands in
                            }
                        }
                    }

                    registration->Changed.notify_all();
                }
                catch (...)
                {
                }
            }

            if (instance != nullptr) DnsServiceFreeInstance(instance);
        }

        std::shared_ptr<Registration> m_registration{ nullptr };

        mutable std::mutex m_namesLock;
        std::wstring m_requestedFullName;
        std::wstring m_registeredFullName;
    };
}
