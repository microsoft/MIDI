// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// A Network MIDI 2.0 host implemented in the test, so that "midisrv as a client" can be driven
// end to end on one machine with no external device. It binds loopback, so it also works in a
// CI container.
//
// Like NetworkMidiTestProtocol, this shares no code with the transport under test. It is a
// second, independent reading of M2-124-UM v1.0, which is the point: if the transport's client
// and the transport's host both got a field wrong in the same way, a test built from the
// transport's own writer would still pass.

#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "NetworkMidiTestProtocol.h"

namespace NetworkMidiTest
{
    // How the fake host answers an incoming Invitation. Each value corresponds to a branch the
    // client is required to handle.
    enum class FakeHostInvitationBehavior
    {
        // Spec 6.7: reply Invitation Reply: Accepted straight away
        Accept,

        // Spec 6.6 then 6.7: reply Pending, then Accepted once PendingDelay has elapsed
        PendingThenAccept,

        // Spec 6.6: reply Pending and never resolve it. The client is expected to wait, not to
        // keep re-inviting.
        PendingForever,

        // Say nothing at all. Spec 6.4: the client repeats, then gives up with Bye 0x80.
        Ignore,

        // Reply with a Bye using ByeReason
        RejectWithBye,

        // Spec 6.9: Invitation Reply: Authentication Required
        RequireAuthentication,

        // Spec 6.10: Invitation Reply: User Authentication Required
        RequireUserAuthentication,
    };


    class FakeNetworkHost
    {
    public:
        FakeNetworkHost() = default;
        ~FakeNetworkHost();

        FakeNetworkHost(FakeNetworkHost const&) = delete;
        FakeNetworkHost& operator=(FakeNetworkHost const&) = delete;

        // Binds an ephemeral port on loopback. Returns false if the socket could not be created.
        bool Start();
        void Stop();

        bool IsRunning() const { return m_socket != INVALID_SOCKET; }

        uint16_t Port() const { return m_port; }
        static std::wstring Address() { return L"127.0.0.1"; }

        // Identity this host reports in its Invitation Reply. Defaults are unique per instance
        // so concurrently created endpoints do not collide.
        void SetEndpointName(_In_ std::string const& value) { m_endpointName = value; }
        void SetProductInstanceId(_In_ std::string const& value) { m_productInstanceId = value; }
        std::string EndpointName() const { return m_endpointName; }
        std::string ProductInstanceId() const { return m_productInstanceId; }

        void SetInvitationBehavior(_In_ FakeHostInvitationBehavior const behavior) { m_invitationBehavior = behavior; }
        void SetPendingDelay(_In_ std::chrono::milliseconds const delay) { m_pendingDelayMilliseconds = static_cast<uint32_t>(delay.count()); }
        void SetByeReason(_In_ ByeReason const reason) { m_byeReason = reason; }

        // A real host answers pings. Turning this off is how a test drives the client's
        // liveness timeout.
        void SetAutoPingReply(_In_ bool const enabled) { m_autoPingReply = enabled; }

        // A host must answer a Bye with a Bye Reply (spec 6.16). Turning this off is how a test
        // drives the client's Bye retry.
        void SetAutoByeReply(_In_ bool const enabled) { m_autoByeReply = enabled; }

        // Answers Session Reset with Session Reset Reply, per spec 6.13.
        void SetAutoSessionResetReply(_In_ bool const enabled) { m_autoSessionResetReply = enabled; }

        // Observation ------------------------------------------------------------------

        // Waits for a datagram containing the given command. Everything seen along the way is
        // retained for diagnostics.
        std::optional<ParsedPacket> WaitForCommand(
            _In_ CommandCode const code,
            _In_ std::chrono::milliseconds const timeout);

        // Waits for the Nth occurrence, counting from the start of the run.
        std::optional<ParsedPacket> WaitForCommandCount(
            _In_ CommandCode const code,
            _In_ size_t const count,
            _In_ std::chrono::milliseconds const timeout);

        // Total number of commands of this code received since Start or ClearHistory.
        size_t CountReceived(_In_ CommandCode const code);

        std::vector<ParsedPacket> ReceivedPackets();
        void ClearHistory();

        // True once a datagram has arrived, meaning the remote address is known and Send works.
        bool HasRemote() const { return m_remoteKnown; }

        // The source port the service is using for this session. Zero until first contact.
        uint16_t RemotePort() const { return m_remotePort; }

        // Datagrams received from anything other than the latched remote. A real host keys a
        // session on address and port, so these are counted and otherwise ignored.
        size_t IgnoredFromOtherSourceCount() const { return m_ignoredFromOtherSource; }

        // Sending ----------------------------------------------------------------------

        bool Send(_In_ std::vector<uint8_t> const& bytes);
        bool Send(_In_ PacketBuilder const& builder) { return Send(builder.Bytes()); }

        bool SendPing(_In_ uint32_t const pingId);
        bool SendBye(_In_ ByeReason const reason, _In_ std::string const& text = "");
        bool SendSessionReset();
        bool SendUmpData(_In_ uint16_t const sequenceNumber, _In_ std::vector<uint32_t> const& words);
        bool SendInvitationReplyAccepted();

    private:
        void ReceiverLoop(_In_ std::stop_token const stopToken);
        void HandlePacket(_In_ ParsedPacket const& packet);
        void HandleInvitation();

        bool SendToRemote(_In_ std::vector<uint8_t> const& bytes);

        SOCKET m_socket{ INVALID_SOCKET };
        uint16_t m_port{ 0 };

        sockaddr_storage m_remoteAddress{ };
        int m_remoteAddressLength{ 0 };
        std::atomic<bool> m_remoteKnown{ false };
        std::atomic<uint16_t> m_remotePort{ 0 };
        std::atomic<size_t> m_ignoredFromOtherSource{ 0 };

        std::string m_endpointName{ };
        std::string m_productInstanceId{ };

        std::atomic<FakeHostInvitationBehavior> m_invitationBehavior{ FakeHostInvitationBehavior::Accept };
        std::atomic<uint32_t> m_pendingDelayMilliseconds{ 0 };
        std::atomic<ByeReason> m_byeReason{ ByeReason::UserDidNotAccept };
        std::atomic<bool> m_autoPingReply{ true };
        std::atomic<bool> m_autoByeReply{ true };
        std::atomic<bool> m_autoSessionResetReply{ true };

        // Set once we have answered a Pending invitation, so a repeat invitation does not
        // restart the timer.
        std::atomic<bool> m_pendingReplySent{ false };
        std::atomic<bool> m_sessionAccepted{ false };

        std::mutex m_historyLock;
        std::condition_variable m_historySignal;
        std::vector<ParsedPacket> m_received{ };

        // Declared last so it is joined before anything it touches is destroyed.
        std::jthread m_receiverThread;
    };
}
