// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// A deliberately dumb UDP peer. Winsock rather than DatagramSocket, because these tests need to
// emit bytes that no well-behaved API would let them emit, and need precise receive timeouts.

#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "NetworkMidiTestProtocol.h"

namespace NetworkMidiTest
{
    struct HostEndpointAddress
    {
        std::wstring HostNameOrAddress{ };
        uint16_t Port{ 0 };

        // for logging only
        std::wstring DiscoveredVia{ };
        std::wstring AdvertisedEndpointName{ };
        std::wstring AdvertisedProductInstanceId{ };

        bool IsValid() const { return !HostNameOrAddress.empty() && Port != 0; }
    };


    // One-shot Winsock lifetime for the test DLL.
    class WinsockScope
    {
    public:
        WinsockScope();
        ~WinsockScope();

        bool IsInitialized() const { return m_initialized; }

    private:
        bool m_initialized{ false };
    };


    class UdpTestClient
    {
    public:
        UdpTestClient() = default;
        ~UdpTestClient();

        UdpTestClient(UdpTestClient const&) = delete;
        UdpTestClient& operator=(UdpTestClient const&) = delete;

        // Binds an ephemeral local port and remembers the remote. Spec 3.3: a client uses its
        // own dedicated source port, and may use a new one for every session.
        bool Open(_In_ HostEndpointAddress const& remote);
        void Close();

        bool IsOpen() const { return m_socket != INVALID_SOCKET; }
        uint16_t LocalPort() const { return m_localPort; }

        bool Send(_In_ std::vector<uint8_t> const& bytes);
        bool Send(_In_ PacketBuilder const& builder) { return Send(builder.Bytes()); }

        // Returns the next datagram, or nothing if the timeout elapses first.
        std::optional<std::vector<uint8_t>> Receive(_In_ std::chrono::milliseconds const timeout);

        // Receives and parses, skipping anything that isn't a MIDI packet.
        std::optional<ParsedPacket> ReceivePacket(_In_ std::chrono::milliseconds const timeout);

        // Keeps receiving until a packet containing the wanted command arrives or the overall
        // deadline passes. Every packet seen along the way is recorded for diagnostics.
        std::optional<ParsedPacket> WaitForCommand(
            _In_ CommandCode const code,
            _In_ std::chrono::milliseconds const timeout);

        // Drains anything already queued, so a test can start from a known state.
        void DrainPending(_In_ std::chrono::milliseconds const settleTime = std::chrono::milliseconds(250));

        // A real client answers the host's Pings. Without that the host reaps the session as
        // dead and silently drops everything sent afterwards. Disable only to test that.
        void SetAutoPingReply(_In_ bool const enabled) { m_autoPingReply = enabled; }

        std::vector<ParsedPacket> const& ReceivedPackets() const { return m_receivedPackets; }
        void ClearHistory() { m_receivedPackets.clear(); }

    private:
        struct ReceivedDatagram
        {
            std::vector<uint8_t> Bytes;
            ParsedPacket Parsed;
        };

        void ReceiverLoop(_In_ std::stop_token const stopToken);
        std::optional<ReceivedDatagram> PopReceived(_In_ std::chrono::milliseconds const timeout);

        SOCKET m_socket{ INVALID_SOCKET };
        sockaddr_storage m_remoteAddress{ };
        int m_remoteAddressLength{ 0 };
        uint16_t m_localPort{ 0 };

        std::atomic<bool> m_autoPingReply{ true };

        std::mutex m_queueLock;
        std::condition_variable m_queueSignal;
        std::deque<ReceivedDatagram> m_queue;

        std::vector<ParsedPacket> m_receivedPackets{ };

        // Declared last so it is joined before the members it uses are destroyed.
        std::jthread m_receiverThread;
    };
}
