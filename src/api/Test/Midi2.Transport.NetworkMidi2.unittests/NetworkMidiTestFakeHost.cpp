// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include <WexTestClass.h>

using namespace WEX::Common;
using namespace WEX::Logging;

namespace NetworkMidiTest
{
    namespace
    {
        // Unique enough for concurrently created endpoints in one run, and stable in shape so
        // the generated device instance ids stay readable in Device Manager.
        std::string MakeUniqueSuffix()
        {
            static std::atomic<uint32_t> counter{ 0 };

            auto value = counter.fetch_add(1);

            char buffer[64]{ };
            sprintf_s(buffer, "%08X%02X", static_cast<uint32_t>(GetCurrentProcessId()), value & 0xFF);

            return std::string{ buffer };
        }
    }


    FakeNetworkHost::~FakeNetworkHost()
    {
        Stop();
    }


    bool FakeNetworkHost::Start()
    {
        if (m_socket != INVALID_SOCKET)
        {
            return true;
        }

        if (m_endpointName.empty())
        {
            m_endpointName = "FakeHost " + MakeUniqueSuffix();
        }

        if (m_productInstanceId.empty())
        {
            m_productInstanceId = "fh" + MakeUniqueSuffix();
        }

        m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        if (m_socket == INVALID_SOCKET)
        {
            Log::Error(String().Format(L"FakeNetworkHost: socket() failed with %d", WSAGetLastError()));
            return false;
        }

        sockaddr_in local{ };
        local.sin_family = AF_INET;
        local.sin_port = 0;                                 // ephemeral
        InetPtonW(AF_INET, L"127.0.0.1", &local.sin_addr);

        if (bind(m_socket, reinterpret_cast<sockaddr*>(&local), sizeof(local)) == SOCKET_ERROR)
        {
            Log::Error(String().Format(L"FakeNetworkHost: bind() failed with %d", WSAGetLastError()));
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            return false;
        }

        sockaddr_in bound{ };
        int boundLength = sizeof(bound);

        if (getsockname(m_socket, reinterpret_cast<sockaddr*>(&bound), &boundLength) == SOCKET_ERROR)
        {
            Log::Error(String().Format(L"FakeNetworkHost: getsockname() failed with %d", WSAGetLastError()));
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            return false;
        }

        m_port = ntohs(bound.sin_port);

        // so the receive loop can notice the stop request
        DWORD receiveTimeout{ 250 };
        setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char const*>(&receiveTimeout), sizeof(receiveTimeout));

        m_receiverThread = std::jthread([this](std::stop_token stopToken) { ReceiverLoop(stopToken); });

        Log::Comment(String().Format(
            L"FakeNetworkHost listening on 127.0.0.1:%u as '%S' / '%S'",
            m_port,
            m_endpointName.c_str(),
            m_productInstanceId.c_str()));

        return true;
    }


    void FakeNetworkHost::Stop()
    {
        if (m_receiverThread.joinable())
        {
            m_receiverThread.request_stop();
            m_receiverThread.join();
        }

        if (m_socket != INVALID_SOCKET)
        {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }

        m_remoteKnown = false;
        m_remotePort = 0;
        m_port = 0;
    }


    void FakeNetworkHost::ReceiverLoop(std::stop_token const stopToken)
    {
        std::vector<uint8_t> buffer(2048);

        while (!stopToken.stop_requested())
        {
            sockaddr_storage from{ };
            int fromLength = sizeof(from);

            auto received = recvfrom(
                m_socket,
                reinterpret_cast<char*>(buffer.data()),
                static_cast<int>(buffer.size()),
                0,
                reinterpret_cast<sockaddr*>(&from),
                &fromLength);

            if (received == SOCKET_ERROR)
            {
                auto error = WSAGetLastError();

                // the receive timeout is how the loop notices the stop request
                if (error == WSAETIMEDOUT || error == WSAEINTR)
                {
                    continue;
                }

                break;
            }

            if (received <= 0)
            {
                continue;
            }

            // A Session is keyed on the remote address and port, so the first sender owns this
            // host for the rest of the run. Without latching, anything else on the machine
            // could redirect our replies just by sending us a datagram.
            bool fromLatchedRemote{ true };

            if (!m_remoteKnown)
            {
                m_remoteAddress = from;
                m_remoteAddressLength = fromLength;
                m_remoteKnown = true;

                if (from.ss_family == AF_INET)
                {
                    m_remotePort = ntohs(reinterpret_cast<sockaddr_in const*>(&from)->sin_port);
                }
            }
            else if (from.ss_family == AF_INET && m_remoteAddress.ss_family == AF_INET)
            {
                auto const* incoming = reinterpret_cast<sockaddr_in const*>(&from);
                auto const* latched = reinterpret_cast<sockaddr_in const*>(&m_remoteAddress);

                fromLatchedRemote =
                    incoming->sin_port == latched->sin_port &&
                    incoming->sin_addr.S_un.S_addr == latched->sin_addr.S_un.S_addr;
            }

            auto packet = ParsePacket(buffer.data(), static_cast<size_t>(received));

            {
                std::lock_guard<std::mutex> lock(m_historyLock);
                m_received.push_back(packet);
            }

            m_historySignal.notify_all();

            if (!fromLatchedRemote)
            {
                m_ignoredFromOtherSource++;
                continue;
            }

            if (packet.SignatureValid)
            {
                HandlePacket(packet);
            }
        }
    }


    void FakeNetworkHost::HandlePacket(ParsedPacket const& packet)
    {
        for (auto const& command : packet.Commands)
        {
            switch (command.Code)
            {
            case CommandCode::Invitation:
            case CommandCode::InvitationWithAuthentication:
            case CommandCode::InvitationWithUserAuthentication:
                HandleInvitation();
                break;

            case CommandCode::Ping:
                if (m_autoPingReply)
                {
                    PacketBuilder builder;
                    builder.StartPacket().AddPingReply(command.GetPayloadUInt32(0));
                    SendToRemote(builder.Bytes());
                }
                break;

            case CommandCode::Bye:
                if (m_autoByeReply)
                {
                    PacketBuilder builder;
                    builder.StartPacket().AddByeReply();
                    SendToRemote(builder.Bytes());
                }

                m_sessionAccepted = false;
                m_pendingReplySent = false;
                break;

            case CommandCode::SessionReset:
                if (m_autoSessionResetReply)
                {
                    PacketBuilder builder;
                    builder.StartPacket().AddSessionResetReply();
                    SendToRemote(builder.Bytes());
                }
                break;

            default:
                break;
            }
        }
    }


    void FakeNetworkHost::HandleInvitation()
    {
        switch (m_invitationBehavior.load())
        {
        case FakeHostInvitationBehavior::Accept:
            SendInvitationReplyAccepted();
            break;

        case FakeHostInvitationBehavior::PendingThenAccept:
        {
            // Only the first invitation starts the clock. A client which keeps re-inviting must
            // not be able to postpone acceptance.
            if (!m_pendingReplySent.exchange(true))
            {
                PacketBuilder builder;
                builder.StartPacket().AddCommandHeader(
                    CommandCode::InvitationReplyPending,
                    PacketBuilder::PaddedWordCount(m_endpointName.size()),
                    static_cast<uint8_t>(PacketBuilder::PaddedWordCount(m_endpointName.size())),
                    static_cast<uint8_t>(0));
                builder.AddPaddedString(m_endpointName);

                SendToRemote(builder.Bytes());

                auto delay = m_pendingDelayMilliseconds.load();

                std::thread([this, delay]()
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(delay));

                        if (m_socket != INVALID_SOCKET)
                        {
                            SendInvitationReplyAccepted();
                        }
                    }).detach();
            }
            break;
        }

        case FakeHostInvitationBehavior::PendingForever:
        {
            if (!m_pendingReplySent.exchange(true))
            {
                PacketBuilder builder;
                builder.StartPacket().AddCommandHeader(
                    CommandCode::InvitationReplyPending,
                    PacketBuilder::PaddedWordCount(m_endpointName.size()),
                    static_cast<uint8_t>(PacketBuilder::PaddedWordCount(m_endpointName.size())),
                    static_cast<uint8_t>(0));
                builder.AddPaddedString(m_endpointName);

                SendToRemote(builder.Bytes());
            }
            break;
        }

        case FakeHostInvitationBehavior::Ignore:
            break;

        case FakeHostInvitationBehavior::RejectWithBye:
        {
            PacketBuilder builder;
            builder.StartPacket().AddBye(m_byeReason.load(), "Rejected by fake host");
            SendToRemote(builder.Bytes());
            break;
        }

        case FakeHostInvitationBehavior::RequireAuthentication:
        {
            // Spec 6.9: reply carries a 16 byte crypto nonce, then the host's name
            PacketBuilder builder;
            builder.StartPacket().AddCommandHeader(
                CommandCode::InvitationReplyAuthenticationRequired,
                static_cast<uint8_t>(4 + PacketBuilder::PaddedWordCount(m_endpointName.size())),
                static_cast<uint8_t>(PacketBuilder::PaddedWordCount(m_endpointName.size())),
                static_cast<uint8_t>(0));

            for (int i = 0; i < 4; i++)
            {
                builder.AddUInt32(0xA5A5A5A5);
            }

            builder.AddPaddedString(m_endpointName);

            SendToRemote(builder.Bytes());
            break;
        }

        case FakeHostInvitationBehavior::RequireUserAuthentication:
        {
            PacketBuilder builder;
            builder.StartPacket().AddCommandHeader(
                CommandCode::InvitationReplyUserAuthenticationRequired,
                static_cast<uint8_t>(4 + PacketBuilder::PaddedWordCount(m_endpointName.size())),
                static_cast<uint8_t>(PacketBuilder::PaddedWordCount(m_endpointName.size())),
                static_cast<uint8_t>(0));

            for (int i = 0; i < 4; i++)
            {
                builder.AddUInt32(0x5A5A5A5A);
            }

            builder.AddPaddedString(m_endpointName);

            SendToRemote(builder.Bytes());
            break;
        }
        }
    }


    bool FakeNetworkHost::SendInvitationReplyAccepted()
    {
        // Spec 6.7: Command Specific Data 1 is the UMP Endpoint Name length in words, and the
        // payload is the name followed by the product instance id.
        auto nameWords = PacketBuilder::PaddedWordCount(m_endpointName.size());
        auto idWords = PacketBuilder::PaddedWordCount(m_productInstanceId.size());

        PacketBuilder builder;
        builder.StartPacket().AddCommandHeader(
            CommandCode::InvitationReplyAccepted,
            static_cast<uint8_t>(nameWords + idWords),
            static_cast<uint8_t>(nameWords),
            static_cast<uint8_t>(0));

        builder.AddPaddedString(m_endpointName);
        builder.AddPaddedString(m_productInstanceId);

        m_sessionAccepted = true;

        return SendToRemote(builder.Bytes());
    }


    bool FakeNetworkHost::SendPing(uint32_t const pingId)
    {
        PacketBuilder builder;
        builder.StartPacket().AddPing(pingId);

        return SendToRemote(builder.Bytes());
    }


    bool FakeNetworkHost::SendBye(ByeReason const reason, std::string const& text)
    {
        PacketBuilder builder;
        builder.StartPacket().AddBye(reason, text);

        return SendToRemote(builder.Bytes());
    }


    bool FakeNetworkHost::SendSessionReset()
    {
        PacketBuilder builder;
        builder.StartPacket().AddSessionReset();

        return SendToRemote(builder.Bytes());
    }


    bool FakeNetworkHost::SendUmpData(uint16_t const sequenceNumber, std::vector<uint32_t> const& words)
    {
        PacketBuilder builder;
        builder.StartPacket().AddUmpData(sequenceNumber, words);

        return SendToRemote(builder.Bytes());
    }


    bool FakeNetworkHost::Send(std::vector<uint8_t> const& bytes)
    {
        return SendToRemote(bytes);
    }


    bool FakeNetworkHost::SendToRemote(std::vector<uint8_t> const& bytes)
    {
        if (m_socket == INVALID_SOCKET || !m_remoteKnown)
        {
            return false;
        }

        auto sent = sendto(
            m_socket,
            reinterpret_cast<char const*>(bytes.data()),
            static_cast<int>(bytes.size()),
            0,
            reinterpret_cast<sockaddr const*>(&m_remoteAddress),
            m_remoteAddressLength);

        return sent == static_cast<int>(bytes.size());
    }


    std::optional<ParsedPacket> FakeNetworkHost::WaitForCommand(
        CommandCode const code,
        std::chrono::milliseconds const timeout)
    {
        return WaitForCommandCount(code, 1, timeout);
    }


    std::optional<ParsedPacket> FakeNetworkHost::WaitForCommandCount(
        CommandCode const code,
        size_t const count,
        std::chrono::milliseconds const timeout)
    {
        std::unique_lock<std::mutex> lock(m_historyLock);

        size_t searchFrom{ 0 };
        size_t found{ 0 };

        auto deadline = std::chrono::steady_clock::now() + timeout;

        for (;;)
        {
            while (searchFrom < m_received.size())
            {
                auto const& packet = m_received[searchFrom];
                searchFrom++;

                if (!packet.SignatureValid) continue;

                found += packet.Count(code);

                if (found >= count)
                {
                    return packet;
                }
            }

            if (m_historySignal.wait_until(lock, deadline) == std::cv_status::timeout)
            {
                // one last look, in case a packet landed as the deadline passed
                while (searchFrom < m_received.size())
                {
                    auto const& packet = m_received[searchFrom];
                    searchFrom++;

                    if (!packet.SignatureValid) continue;

                    found += packet.Count(code);

                    if (found >= count)
                    {
                        return packet;
                    }
                }

                return std::nullopt;
            }
        }
    }


    size_t FakeNetworkHost::CountReceived(CommandCode const code)
    {
        std::lock_guard<std::mutex> lock(m_historyLock);

        size_t total{ 0 };

        for (auto const& packet : m_received)
        {
            if (packet.SignatureValid)
            {
                total += packet.Count(code);
            }
        }

        return total;
    }


    std::vector<ParsedPacket> FakeNetworkHost::ReceivedPackets()
    {
        std::lock_guard<std::mutex> lock(m_historyLock);

        return m_received;
    }


    void FakeNetworkHost::ClearHistory()
    {
        std::lock_guard<std::mutex> lock(m_historyLock);

        m_received.clear();
    }
}
