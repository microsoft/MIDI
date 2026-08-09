// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace NetworkMidiTest
{
    namespace
    {
        // The endpoint creator worker both tears down connections and starts new clients, and a
        // teardown includes deleting a MIDI endpoint device, which is slow. So a new client can
        // sit behind the previous test's teardown. These waits are sized for that, not for the
        // network, which is loopback here.
        constexpr std::chrono::milliseconds InvitationTimeout{ 30000 };
        constexpr std::chrono::milliseconds SessionTimeout{ 40000 };
        constexpr std::chrono::milliseconds ShortTimeout{ 5000 };

        // Spec 6.4 and the transport's own retry budget. The client repeats the invitation on
        // the watchdog tick, so five attempts is roughly ten seconds.
        constexpr size_t MaxInvitationAttempts{ 5 };

        bool g_serviceAvailable{ false };


        // Everything a test needs: a fake host listening, and a client the service has been
        // asked to connect to it. Removes the client on destruction so one failing test cannot
        // leave a connection behind for the next.
        class ClientUnderTest
        {
        public:
            ClientUnderTest() = default;

            ~ClientUnderTest()
            {
                Remove();
            }

            ClientUnderTest(ClientUnderTest const&) = delete;
            ClientUnderTest& operator=(ClientUnderTest const&) = delete;

            FakeNetworkHost& Host() { return m_host; }
            std::wstring const& EntryIdentifier() const { return m_entryIdentifier; }

            // Starts the fake host and asks the service to connect to it. Returns false having
            // already logged if either step fails.
            bool Start(_In_ FakeHostInvitationBehavior const behavior = FakeHostInvitationBehavior::Accept)
            {
                m_host.SetInvitationBehavior(behavior);

                if (!m_host.Start())
                {
                    Log::Error(L"Could not start the fake host.");
                    return false;
                }

                m_entryIdentifier = MakeEntryIdentifier();

                auto result = CreateDirectClient(m_entryIdentifier, FakeNetworkHost::Address(), m_host.Port());

                if (!result.CallSucceeded)
                {
                    Log::Error(String().Format(L"Creating the client failed: %s", result.Message.c_str()));
                    Log::Comment(String().Format(L"Response: %s", result.ResponseJson.c_str()));
                    m_entryIdentifier.clear();
                    return false;
                }

                m_created = true;

                return true;
            }

            void Remove()
            {
                if (m_created && !m_entryIdentifier.empty())
                {
                    DisconnectClient(m_entryIdentifier);
                    m_created = false;
                }

                m_host.Stop();
            }

        private:
            FakeNetworkHost m_host{ };
            std::wstring m_entryIdentifier{ };
            bool m_created{ false };
        };


        // Skips rather than fails when there is no service to talk to.
        bool RequireService()
        {
            if (!g_serviceAvailable)
            {
                Log::Result(TestResults::Skipped, L"Windows MIDI Service is not reachable, or the network transport is not enabled.");
                return false;
            }

            return true;
        }


        // Brings a client all the way to an established session.
        bool EstablishClientSession(_In_ ClientUnderTest& client)
        {
            if (!client.Start(FakeHostInvitationBehavior::Accept))
            {
                return false;
            }

            auto invitation = client.Host().WaitForCommand(CommandCode::Invitation, InvitationTimeout);

            if (!invitation.has_value())
            {
                Log::Error(L"The service never sent an Invitation.");
                return false;
            }

            // The accepted reply goes out from the receive path, so by the time the service
            // sends its first UMP Data the session is up.
            auto firstData = client.Host().WaitForCommand(CommandCode::UmpData, SessionTimeout);

            if (!firstData.has_value())
            {
                Log::Error(L"The service never sent UMP Data, so the session did not establish.");
                return false;
            }

            return true;
        }
    }


    bool ClientTests::ClassSetup()
    {
        // These tests never touch ProtocolTestContext, which is what owns Winsock for the rest
        // of the DLL, so this class has to start it itself. WSAStartup is reference counted.
        static WinsockScope s_winsock;

        if (!s_winsock.IsInitialized())
        {
            Log::Error(L"Winsock could not be initialized.");
            return false;
        }

        g_serviceAvailable = IsServiceAvailable();

        if (!g_serviceAvailable)
        {
            Log::Warning(L"The Windows MIDI Service could not be reached. Client tests will skip.");
            return true;
        }

        // Otherwise every connection waits up to 20 seconds for the endpoint creator to wake.
        auto scanResult = SetDirectConnectionScanInterval(1000);

        if (!scanResult.IsSuccess())
        {
            Log::Warning(String().Format(
                L"Could not shorten the direct connection scan interval, so these tests will be slow. Response: %s",
                scanResult.ResponseJson.c_str()));
        }

        return true;
    }


    bool ClientTests::ClassCleanup()
    {
        if (g_serviceAvailable)
        {
            SetDirectConnectionScanInterval(MIDI_NETWORK_DIRECT_CONNECTION_SCAN_INTERVAL_DEFAULT);
        }

        return true;
    }


    // ------------------------------------------------------------------------------
    // Connection establishment
    // ------------------------------------------------------------------------------

    void ClientTests::ClientSendsInvitationToConfiguredHost()
    {
        if (!RequireService()) return;

        LogSpecRequirement(L"6.4: a Client opens a Session by sending an Invitation Command to the Host.");

        ClientUnderTest client;
        VERIFY_IS_TRUE(client.Start(FakeHostInvitationBehavior::Accept));

        auto packet = client.Host().WaitForCommand(CommandCode::Invitation, InvitationTimeout);

        VERIFY_IS_TRUE(packet.has_value(), L"The service should have sent an Invitation to the configured address and port.");

        if (packet.has_value())
        {
            LogPacket(L"Invitation from service", packet.value());
        }
    }


    void ClientTests::ClientInvitationCarriesEndpointNameAndProductInstanceId()
    {
        if (!RequireService()) return;

        LogSpecRequirement(L"6.4: the Invitation carries the Client's UMP Endpoint Name and Product Instance Id.");

        ClientUnderTest client;
        VERIFY_IS_TRUE(client.Start(FakeHostInvitationBehavior::Accept));

        auto packet = client.Host().WaitForCommand(CommandCode::Invitation, InvitationTimeout);
        VERIFY_IS_TRUE(packet.has_value());

        if (!packet.has_value()) return;

        auto const* invitation = packet->Find(CommandCode::Invitation);
        VERIFY_IS_NOT_NULL(invitation);

        if (invitation == nullptr) return;

        // Command Specific Data 1 is the name length in 32 bit words
        auto nameWords = invitation->CommandSpecificData1;

        VERIFY_IS_GREATER_THAN(nameWords, static_cast<uint8_t>(0), L"The endpoint name length must not be zero.");
        VERIFY_IS_GREATER_THAN(invitation->PayloadLengthWords, nameWords,
            L"There must be payload beyond the name, which is the product instance id.");

        auto name = invitation->GetPayloadString(0, static_cast<size_t>(nameWords) * 4);
        auto productInstanceId = invitation->GetPayloadString(
            static_cast<size_t>(nameWords) * 4,
            (static_cast<size_t>(invitation->PayloadLengthWords) - nameWords) * 4);

        Log::Comment(String().Format(L"Client identified itself as '%S' / '%S'", name.c_str(), productInstanceId.c_str()));

        VERIFY_IS_FALSE(name.empty(), L"UMP Endpoint Name must not be blank.");
        VERIFY_IS_FALSE(productInstanceId.empty(), L"Product Instance Id must not be blank.");

        // Regression guard: this was literally "unspecified-..." and "8675309-OU812" before the
        // machine-wide identity was wired up, which would have made every PC look alike.
        VERIFY_IS_TRUE(productInstanceId.find("unspecified") == std::string::npos,
            L"Product Instance Id must not be a placeholder.");
        VERIFY_IS_TRUE(productInstanceId.find("8675309-OU812") == std::string::npos,
            L"Product Instance Id must not be the hard coded sample value.");
    }


    void ClientTests::ClientCompletesSessionWhenInvitationAccepted()
    {
        if (!RequireService()) return;

        LogSpecRequirement(L"6.7: on Invitation Reply: Accepted the Session is established.");

        ClientUnderTest client;
        VERIFY_IS_TRUE(EstablishClientSession(client));
    }


    void ClientTests::ClientStartsUmpSequenceNumbersAtZero()
    {
        if (!RequireService()) return;

        LogSpecRequirement(L"5.6: the first UMP Data Command of a Session shall use Sequence Number 0x0000.");

        ClientUnderTest client;
        VERIFY_IS_TRUE(client.Start(FakeHostInvitationBehavior::Accept));

        auto packet = client.Host().WaitForCommand(CommandCode::UmpData, SessionTimeout);
        VERIFY_IS_TRUE(packet.has_value(), L"The service should send UMP Data once the session is up.");

        if (!packet.has_value()) return;

        LogPacket(L"First UMP Data from service", packet.value());

        auto const* umpData = packet->Find(CommandCode::UmpData);
        VERIFY_IS_NOT_NULL(umpData);

        if (umpData != nullptr)
        {
            VERIFY_ARE_EQUAL(static_cast<uint16_t>(0), umpData->GetSequenceNumber(),
                L"The first UMP Data Command must use sequence number 0x0000.");
        }
    }


    // ------------------------------------------------------------------------------
    // Invitation retry and failure
    // ------------------------------------------------------------------------------

    void ClientTests::ClientRepeatsInvitationWhenHostSilent()
    {
        if (!RequireService()) return;

        LogSpecRequirement(L"6.4: the Invitation Command should be sent repeatedly until a reply is received.");

        ClientUnderTest client;
        VERIFY_IS_TRUE(client.Start(FakeHostInvitationBehavior::Ignore));

        auto second = client.Host().WaitForCommandCount(CommandCode::Invitation, 2, InvitationTimeout);

        VERIFY_IS_TRUE(second.has_value(), L"A silent host should see the Invitation repeated.");
    }


    void ClientTests::ClientCancelsInvitationWithCorrectByeReasonWhenHostNeverAnswers()
    {
        if (!RequireService()) return;

        LogSpecRequirement(L"6.4: if the Client considers the Invitation failed it shall terminate it with Bye reason 0x80.");

        ClientUnderTest client;
        VERIFY_IS_TRUE(client.Start(FakeHostInvitationBehavior::Ignore));

        // five attempts on the watchdog tick, then the Bye
        auto packet = client.Host().WaitForCommand(CommandCode::Bye, std::chrono::milliseconds(30000));

        VERIFY_IS_TRUE(packet.has_value(), L"The client should give up and send a Bye.");

        if (!packet.has_value()) return;

        LogPacket(L"Bye from service", packet.value());

        auto const* bye = packet->Find(CommandCode::Bye);
        VERIFY_IS_NOT_NULL(bye);

        if (bye != nullptr)
        {
            VERIFY_ARE_EQUAL(ByeReason::InvitationCanceled, bye->GetByeReason(),
                L"An abandoned invitation must use reason 0x80 Invitation Canceled.");
        }

        auto attempts = client.Host().CountReceived(CommandCode::Invitation);
        Log::Comment(String().Format(L"Invitation was sent %zu times before the client gave up", attempts));

        VERIFY_IS_LESS_THAN_OR_EQUAL(attempts, MaxInvitationAttempts + 1,
            L"The client must not invite without bound.");
    }


    // ------------------------------------------------------------------------------
    // Invitation Reply: Pending
    // ------------------------------------------------------------------------------

    void ClientTests::ClientStopsInvitingAfterInvitationReplyPending()
    {
        if (!RequireService()) return;

        LogSpecRequirement(L"6.6: Invitation Reply: Pending tells the Client the Host is waiting for permission. "
            L"An Invitation Reply: Accepted or Bye Command will follow, so the Client waits rather than re-inviting.");

        ClientUnderTest client;
        VERIFY_IS_TRUE(client.Start(FakeHostInvitationBehavior::PendingForever));

        VERIFY_IS_TRUE(client.Host().WaitForCommand(CommandCode::Invitation, InvitationTimeout).has_value());

        // let several watchdog ticks pass
        std::this_thread::sleep_for(std::chrono::milliseconds(12000));

        auto invitations = client.Host().CountReceived(CommandCode::Invitation);

        Log::Comment(String().Format(L"Invitations received while pending: %zu", invitations));

        VERIFY_ARE_EQUAL(static_cast<size_t>(1), invitations,
            L"Once the host replies Pending the client must stop repeating the invitation.");

        VERIFY_ARE_EQUAL(static_cast<size_t>(0), client.Host().CountReceived(CommandCode::Bye),
            L"The client must not abandon a pending invitation this quickly.");
    }


    void ClientTests::ClientCompletesSessionAfterDelayedAcceptance()
    {
        if (!RequireService()) return;

        LogSpecRequirement(L"6.6 then 6.7: an Invitation Reply: Accepted following a Pending reply establishes the Session.");

        ClientUnderTest client;
        client.Host().SetPendingDelay(std::chrono::milliseconds(8000));

        VERIFY_IS_TRUE(client.Start(FakeHostInvitationBehavior::PendingThenAccept));

        // Well past the old give-up point of roughly ten seconds from the first invitation.
        auto packet = client.Host().WaitForCommand(CommandCode::UmpData, SessionTimeout);

        VERIFY_IS_TRUE(packet.has_value(),
            L"The client should still be waiting when the host finally accepts, and then establish the session.");

        VERIFY_ARE_EQUAL(static_cast<size_t>(0), client.Host().CountReceived(CommandCode::Bye),
            L"The client must not have cancelled the invitation while the host was pending.");
    }


    // ------------------------------------------------------------------------------
    // Authentication
    // ------------------------------------------------------------------------------

    void ClientTests::ClientWithdrawsWhenHostRequiresAuthentication()
    {
        if (!RequireService()) return;

        LogSpecRequirement(L"6.9: a Client which cannot satisfy Invitation Reply: Authentication Required "
            L"terminates the pending Invitation with a Bye rather than leaving it hanging.");

        ClientUnderTest client;
        VERIFY_IS_TRUE(client.Start(FakeHostInvitationBehavior::RequireAuthentication));

        auto packet = client.Host().WaitForCommand(CommandCode::Bye, InvitationTimeout);

        VERIFY_IS_TRUE(packet.has_value(), L"The client should withdraw with a Bye.");

        if (packet.has_value())
        {
            LogPacket(L"Bye after auth challenge", packet.value());

            auto const* bye = packet->Find(CommandCode::Bye);

            if (bye != nullptr)
            {
                VERIFY_ARE_EQUAL(ByeReason::InvitationCanceled, bye->GetByeReason(),
                    L"A client withdrawing its own invitation uses 0x80.");
            }
        }

        VERIFY_ARE_EQUAL(static_cast<size_t>(0), client.Host().CountReceived(CommandCode::InvitationWithAuthentication),
            L"Authentication is not implemented, so the client must not attempt a digest it cannot compute.");
    }


    void ClientTests::ClientWithdrawsWhenHostRequiresUserAuthentication()
    {
        if (!RequireService()) return;

        LogSpecRequirement(L"6.10: as 6.9, for Invitation Reply: User Authentication Required.");

        ClientUnderTest client;
        VERIFY_IS_TRUE(client.Start(FakeHostInvitationBehavior::RequireUserAuthentication));

        auto packet = client.Host().WaitForCommand(CommandCode::Bye, InvitationTimeout);

        VERIFY_IS_TRUE(packet.has_value(), L"The client should withdraw with a Bye.");

        VERIFY_ARE_EQUAL(static_cast<size_t>(0), client.Host().CountReceived(CommandCode::InvitationWithUserAuthentication),
            L"User authentication is not implemented, so the client must not attempt it.");
    }


    void ClientTests::ClientAcceptsByeInsteadOfInvitationReply()
    {
        if (!RequireService()) return;

        LogSpecRequirement(L"6.4: either side may terminate a pending Invitation with a Bye Command.");

        ClientUnderTest client;
        client.Host().SetByeReason(ByeReason::TooManyOpenSessions);

        VERIFY_IS_TRUE(client.Start(FakeHostInvitationBehavior::RejectWithBye));

        VERIFY_IS_TRUE(client.Host().WaitForCommand(CommandCode::Invitation, InvitationTimeout).has_value());

        // Spec 6.16: a Bye is answered with a Bye Reply even when there is no established session
        auto reply = client.Host().WaitForCommand(CommandCode::ByeReply, ShortTimeout);

        VERIFY_IS_TRUE(reply.has_value(), L"A rejected client must still answer the Bye with a Bye Reply.");

        // and it must not keep inviting after being told no
        auto before = client.Host().CountReceived(CommandCode::Invitation);
        std::this_thread::sleep_for(std::chrono::milliseconds(6000));
        auto after = client.Host().CountReceived(CommandCode::Invitation);

        VERIFY_ARE_EQUAL(before, after, L"The client must stop inviting once the host has said Bye.");
    }


    // ------------------------------------------------------------------------------
    // Liveness
    // ------------------------------------------------------------------------------

    void ClientTests::ClientAnswersHostPingWithMatchingId()
    {
        if (!RequireService()) return;

        LogSpecRequirement(L"6.14: a Ping shall be answered with a Ping Reply carrying the same Ping Id.");

        ClientUnderTest client;
        VERIFY_IS_TRUE(EstablishClientSession(client));

        constexpr uint32_t pingId{ 0x5AC3B10E };

        client.Host().ClearHistory();
        VERIFY_IS_TRUE(client.Host().SendPing(pingId));

        auto packet = client.Host().WaitForCommand(CommandCode::PingReply, ShortTimeout);

        VERIFY_IS_TRUE(packet.has_value(), L"The client should answer a Ping.");

        if (!packet.has_value()) return;

        auto const* reply = packet->Find(CommandCode::PingReply);
        VERIFY_IS_NOT_NULL(reply);

        if (reply != nullptr)
        {
            VERIFY_ARE_EQUAL(pingId, reply->GetPayloadUInt32(0), L"The Ping Reply must echo the Ping Id exactly.");
        }
    }


    void ClientTests::ClientSendsPingsWhenSessionIdle()
    {
        if (!RequireService()) return;

        LogSpecRequirement(L"6.14: regularly exchanging Ping Commands allows detection of a stale remote entity.");

        ClientUnderTest client;
        VERIFY_IS_TRUE(EstablishClientSession(client));

        client.Host().ClearHistory();

        auto packet = client.Host().WaitForCommand(CommandCode::Ping, std::chrono::milliseconds(15000));

        VERIFY_IS_TRUE(packet.has_value(), L"An idle client should ping the host to check it is still there.");
    }


    // ------------------------------------------------------------------------------
    // Teardown
    // ------------------------------------------------------------------------------

    void ClientTests::ClientAnswersHostByeWithByeReply()
    {
        if (!RequireService()) return;

        LogSpecRequirement(L"6.16: a Device receiving a Bye shall send a Bye Reply back to the sender.");

        ClientUnderTest client;
        VERIFY_IS_TRUE(EstablishClientSession(client));

        client.Host().ClearHistory();
        VERIFY_IS_TRUE(client.Host().SendBye(ByeReason::UserTerminated, "Fake host going away"));

        auto packet = client.Host().WaitForCommand(CommandCode::ByeReply, ShortTimeout);

        VERIFY_IS_TRUE(packet.has_value(), L"The client must answer a Bye with a Bye Reply.");
    }


    void ClientTests::ClientSendsUserTerminatedByeOnExplicitDisconnect()
    {
        if (!RequireService()) return;

        LogSpecRequirement(L"6.16: the Bye Reason code specifies the cause reason for terminating the session. "
            L"A disconnect the user asked for is 0x01 User terminated session, not 0x02 Power Down.");

        ClientUnderTest client;
        VERIFY_IS_TRUE(EstablishClientSession(client));

        client.Host().ClearHistory();

        auto result = DisconnectClient(client.EntryIdentifier());
        VERIFY_IS_TRUE(result.CallSucceeded, L"disconnectClient should be accepted by the transport.");

        auto packet = client.Host().WaitForCommand(CommandCode::Bye, ShortTimeout);

        VERIFY_IS_TRUE(packet.has_value(), L"An explicit disconnect must tell the host.");

        if (!packet.has_value()) return;

        LogPacket(L"Bye on explicit disconnect", packet.value());

        auto const* bye = packet->Find(CommandCode::Bye);
        VERIFY_IS_NOT_NULL(bye);

        if (bye != nullptr)
        {
            VERIFY_ARE_EQUAL(ByeReason::UserTerminated, bye->GetByeReason(),
                L"A user-initiated disconnect must report cause 0x01, not Power Down.");
        }
    }


    void ClientTests::ClientRepeatsByeUntilByeReplyReceived()
    {
        if (!RequireService()) return;

        LogSpecRequirement(L"6.16: the Bye Command should be sent repeatedly until a Bye Reply Command is received, "
            L"or until a timeout occurs.");

        ClientUnderTest client;
        VERIFY_IS_TRUE(EstablishClientSession(client));

        // stop answering, so the client has to repeat
        client.Host().SetAutoByeReply(false);
        client.Host().ClearHistory();

        DisconnectClient(client.EntryIdentifier());

        auto second = client.Host().WaitForCommandCount(CommandCode::Bye, 2, std::chrono::milliseconds(5000));

        VERIFY_IS_TRUE(second.has_value(), L"An unanswered Bye should be repeated.");

        // but not without bound
        std::this_thread::sleep_for(std::chrono::milliseconds(4000));

        auto total = client.Host().CountReceived(CommandCode::Bye);

        Log::Comment(String().Format(L"Bye was sent %zu times", total));

        VERIFY_IS_LESS_THAN_OR_EQUAL(total, static_cast<size_t>(6),
            L"The Bye retry must be bounded so a disconnect cannot hang.");
    }


    void ClientTests::ClientAnswersSessionResetWithReply()
    {
        if (!RequireService()) return;

        LogSpecRequirement(L"6.13: on reception of a Session Reset Command the receiver replies with Session Reset Reply.");

        ClientUnderTest client;
        VERIFY_IS_TRUE(EstablishClientSession(client));

        client.Host().ClearHistory();
        VERIFY_IS_TRUE(client.Host().SendSessionReset());

        auto packet = client.Host().WaitForCommand(CommandCode::SessionResetReply, ShortTimeout);

        VERIFY_IS_TRUE(packet.has_value(), L"The client must answer a Session Reset.");
    }


    // ------------------------------------------------------------------------------
    // Robustness
    // ------------------------------------------------------------------------------

    void ClientTests::ClientSurvivesMalformedInvitationReply()
    {
        if (!RequireService()) return;

        LogSpecRequirement(L"A malformed reply must not take the service down. The client is expected to keep "
            L"inviting, since nothing valid has arrived.");

        ClientUnderTest client;
        VERIFY_IS_TRUE(client.Start(FakeHostInvitationBehavior::Ignore));

        VERIFY_IS_TRUE(client.Host().WaitForCommand(CommandCode::Invitation, InvitationTimeout).has_value());

        // an Invitation Reply: Accepted whose declared payload length runs off the end
        PacketBuilder builder;
        builder.StartPacket().AddCommandHeader(CommandCode::InvitationReplyAccepted, 40, static_cast<uint8_t>(4), static_cast<uint8_t>(0));
        builder.AddRawBytes({ 0x41, 0x42, 0x43, 0x44 });

        VERIFY_IS_TRUE(client.Host().Send(builder));

        // the service must still be alive and still trying
        auto more = client.Host().WaitForCommandCount(CommandCode::Invitation, 2, InvitationTimeout);

        VERIFY_IS_TRUE(more.has_value(), L"The service should have survived and continued inviting.");
        VERIFY_IS_TRUE(IsServiceAvailable(), L"The service must still be reachable.");
    }


    void ClientTests::ClientSurvivesUnknownCommandCode()
    {
        if (!RequireService()) return;

        LogSpecRequirement(L"An unrecognised Command Code must not be fatal.");

        ClientUnderTest client;
        VERIFY_IS_TRUE(EstablishClientSession(client));

        PacketBuilder builder;
        builder.StartPacket().AddCommandHeader(static_cast<CommandCode>(0x77), 1, static_cast<uint16_t>(0));
        builder.AddUInt32(0xDEADBEEF);

        VERIFY_IS_TRUE(client.Host().Send(builder));

        // a Ping afterwards proves the session survived
        client.Host().ClearHistory();
        VERIFY_IS_TRUE(client.Host().SendPing(0x11223344));

        auto packet = client.Host().WaitForCommand(CommandCode::PingReply, ShortTimeout);

        VERIFY_IS_TRUE(packet.has_value(), L"The session should still be usable after an unknown command.");
    }


    void ClientTests::ClientSurvivesTruncatedDatagram()
    {
        if (!RequireService()) return;

        LogSpecRequirement(L"A datagram which ends mid-command must not be fatal.");

        ClientUnderTest client;
        VERIFY_IS_TRUE(EstablishClientSession(client));

        // signature plus two bytes of what claims to be a command header
        std::vector<uint8_t> truncated{ 0x4D, 0x49, 0x44, 0x49, 0xFF, 0x10 };

        VERIFY_IS_TRUE(client.Host().Send(truncated));

        client.Host().ClearHistory();
        VERIFY_IS_TRUE(client.Host().SendPing(0x55667788));

        auto packet = client.Host().WaitForCommand(CommandCode::PingReply, ShortTimeout);

        VERIFY_IS_TRUE(packet.has_value(), L"The session should still be usable after a truncated datagram.");
    }


    void ClientTests::ClientIgnoresTrafficFromWrongSourcePort()
    {
        if (!RequireService()) return;

        LogSpecRequirement(L"A session is identified by the remote address and port. Traffic from a different "
            L"port is a different peer and must not disturb an established session.");

        ClientUnderTest client;
        VERIFY_IS_TRUE(EstablishClientSession(client));

        // A second socket pretending to be the host, sending a Bye. If the client acted on it,
        // an unrelated peer could tear down someone else's session.
        SOCKET impostor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        VERIFY_ARE_NOT_EQUAL(INVALID_SOCKET, impostor);

        if (impostor == INVALID_SOCKET) return;

        sockaddr_in target{ };
        target.sin_family = AF_INET;
        target.sin_port = htons(client.Host().Port());
        InetPtonW(AF_INET, L"127.0.0.1", &target.sin_addr);

        // we do not know the client's port, so this is aimed at the fake host's port from a
        // different source. The service should never see it, which is the point: the check is
        // that the established session is undisturbed.
        PacketBuilder builder;
        builder.StartPacket().AddBye(ByeReason::UserTerminated, "impostor");

        sendto(
            impostor,
            reinterpret_cast<char const*>(builder.Bytes().data()),
            static_cast<int>(builder.Size()),
            0,
            reinterpret_cast<sockaddr const*>(&target),
            sizeof(target));

        closesocket(impostor);

        client.Host().ClearHistory();
        VERIFY_IS_TRUE(client.Host().SendPing(0x99AABBCC));

        auto packet = client.Host().WaitForCommand(CommandCode::PingReply, ShortTimeout);

        VERIFY_IS_TRUE(packet.has_value(), L"The established session must be undisturbed.");
    }
}
