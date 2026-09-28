// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Sockets, DNS-SD advertising and text conversion for the rtpMIDI transport.
//
// Adapted from the prototype spike, which proved each piece against macOS. Dual-stack IPv6
// sockets, because macOS invites over IPv6 link-local as readily as over IPv4.
// ============================================================================

#pragma once

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

    // Prefers routable IPv4. A link-local IPv6 address from the shared browser carries no
    // interface index, so it cannot be reached and is never chosen.
    inline bool ChooseServiceAddress(
        _In_ WindowsMidiServicesInternal::MidiDnssdService const& service,
        _Out_ RtpMidi::PeerAddress& chosen)
    {
        chosen = RtpMidi::PeerAddress{};

        RtpMidi::PeerAddress linkLocalV4{};
        bool haveLinkLocalV4 = false;

        for (auto const& text : service.IPv4Addresses)
        {
            RtpMidi::PeerAddress address{};
            if (!TryParseAddress(text, service.Port, address)) continue;

            if (address.Bytes[0] == 169 && address.Bytes[1] == 254)
            {
                if (!haveLinkLocalV4) { linkLocalV4 = address; haveLinkLocalV4 = true; }
                continue;
            }

            chosen = address;
            return true;
        }

        if (haveLinkLocalV4)
        {
            chosen = linkLocalV4;
            return true;
        }

        for (auto const& text : service.IPv6Addresses)
        {
            RtpMidi::PeerAddress address{};
            if (!TryParseAddress(text, service.Port, address)) continue;
            if (address.Bytes[0] == 0xFE && (address.Bytes[1] & 0xC0) == 0x80) continue;

            chosen = address;
            return true;
        }

        return false;
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

                    auto const received = recvfrom(socket, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0,
                        reinterpret_cast<sockaddr*>(&from), &fromLength);

                    if (received == SOCKET_ERROR)
                    {
                        auto const error = WSAGetLastError();
                        if (error == WSAECONNRESET || error == WSAEMSGSIZE) continue;
                        break;
                    }

                    if (received > 0 && m_handler)
                    {
                        // one datagram that fails must not stop the socket for every connection on it
                        try
                        {
                            m_handler(FromSockaddr(from), buffer.data(), static_cast<size_t>(received));
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
        ~DnssdAdvertiser() { Unregister(); }

        HRESULT Register(
            _In_ std::wstring const& instanceLabel,
            _In_ uint16_t const port,
            _In_ DWORD const timeoutMilliseconds) noexcept
        {
            try
            {
                m_requestedFullName = instanceLabel + L"." + MIDI_RTP_DNSSD_SERVICE_TYPE;

                wchar_t computerName[256]{};
                DWORD size = ARRAYSIZE(computerName) - 8;
                RETURN_IF_WIN32_BOOL_FALSE(GetComputerNameExW(ComputerNameDnsHostname, computerName, &size));
                m_hostName = std::wstring{ computerName } + L".local";

                // Without a pair the TXT record goes out empty, which RFC 6763 does not allow.
                // AppleMIDI peers ignore TXT content.
                PCWSTR keys[] = { L"txtvers" };
                PCWSTR values[] = { L"1" };

                m_instance = DnsServiceConstructInstance(m_requestedFullName.c_str(), m_hostName.c_str(), nullptr, nullptr, port, 0, 0, 1, keys, values);
                RETURN_LAST_ERROR_IF_NULL(m_instance);

                m_request = DNS_SERVICE_REGISTER_REQUEST{};
                m_request.Version = DNS_QUERY_REQUEST_VERSION1;
                m_request.InterfaceIndex = 0;
                m_request.pServiceInstance = m_instance;
                m_request.pRegisterCompletionCallback = &DnssdAdvertiser::Completed;
                m_request.pQueryContext = this;
                m_request.unicastEnabled = FALSE;

                m_completed = false;

                auto const status = DnsServiceRegister(&m_request, &m_cancel);

                if (status != DNS_REQUEST_PENDING)
                {
                    DnsServiceFreeInstance(m_instance);
                    m_instance = nullptr;
                    RETURN_WIN32(status);
                }

                m_registered = true;

                auto lock = std::unique_lock{ m_lock };
                RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_TIMEOUT),
                    !m_changed.wait_for(lock, std::chrono::milliseconds(timeoutMilliseconds), [this]() { return m_completed; }));

                RETURN_IF_WIN32_ERROR(m_status);

                return S_OK;
            }
            CATCH_RETURN();
        }

        void Unregister() noexcept
        {
            if (!m_registered) return;
            m_registered = false;

            if (!m_completed) DnsServiceRegisterCancel(&m_cancel);

            {
                auto lock = std::scoped_lock{ m_lock };
                m_completed = false;
            }

            // the goodbye for the records is sent by the DNS client service, not by this process
            if (DnsServiceDeRegister(&m_request, nullptr) == DNS_REQUEST_PENDING)
            {
                auto lock = std::unique_lock{ m_lock };
                m_changed.wait_for(lock, std::chrono::milliseconds(2000), [this]() { return m_completed; });
            }

            if (m_instance != nullptr)
            {
                DnsServiceFreeInstance(m_instance);
                m_instance = nullptr;
            }
        }

        // The label actually on the network. The responder renames a colliding label rather than refusing it.
        std::wstring RegisteredLabel() const
        {
            auto lock = std::scoped_lock{ m_lock };

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
            auto lock = std::scoped_lock{ m_lock };
            return !m_registeredFullName.empty() && _wcsicmp(m_registeredFullName.c_str(), m_requestedFullName.c_str()) != 0;
        }

    private:
        static VOID WINAPI Completed(_In_ DWORD status, _In_ PVOID context, _In_opt_ PDNS_SERVICE_INSTANCE instance)
        {
            auto self = static_cast<DnssdAdvertiser*>(context);

            {
                auto lock = std::scoped_lock{ self->m_lock };

                self->m_status = status;

                if (instance != nullptr && instance->pszInstanceName != nullptr)
                {
                    self->m_registeredFullName = instance->pszInstanceName;
                }

                self->m_completed = true;
            }

            if (instance != nullptr) DnsServiceFreeInstance(instance);

            self->m_changed.notify_all();
        }

        std::wstring m_requestedFullName;
        std::wstring m_registeredFullName;
        std::wstring m_hostName;

        PDNS_SERVICE_INSTANCE m_instance{ nullptr };
        DNS_SERVICE_REGISTER_REQUEST m_request{};
        DNS_SERVICE_CANCEL m_cancel{};

        mutable std::mutex m_lock;
        std::condition_variable m_changed;
        bool m_completed{ false };
        bool m_registered{ false };
        DWORD m_status{ ERROR_SUCCESS };
    };
}
