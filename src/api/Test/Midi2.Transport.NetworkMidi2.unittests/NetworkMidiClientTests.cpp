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

namespace json = winrt::Windows::Data::Json;
namespace enumeration = winrt::Windows::Devices::Enumeration;

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

        // Matches MIDI_NETWORK_DIRECT_CONNECTION_SCAN_INTERVAL_DEFAULT. Restated rather than
        // included, so the tests do not depend on the transport's private headers.
        constexpr uint32_t DefaultDirectConnectionScanInterval{ 20000 };

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


        // Not a skip. If the service is unreachable nothing was verified, and a run of skips
        // reads as green, which is how a broken environment stays invisible.
        bool RequireService()
        {
            if (!g_serviceAvailable)
            {
                Log::Error(L"Windows MIDI Service is not reachable, or the network transport is not enabled.");
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

            auto started = std::chrono::steady_clock::now();

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

            // The endpoint creator both tears down connections and starts new ones, so this is
            // where an unrelated endpoint deletion shows up. Reported, never fatal.
            WarnIfSlowerThan(
                L"Client session establishment",
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started),
                std::chrono::milliseconds(8000));

            return true;
        }


        // ----------------------------------------------------------------------
        // enumerateClients helpers. That response is the only view an app has of what the
        // service believes is configured, so the entry lifetime tests read it directly.
        // ----------------------------------------------------------------------

        std::optional<json::JsonArray> ReadClientsArray()
        {
            auto response = EnumerateClients();

            json::JsonObject parsed{ nullptr };

            if (!json::JsonObject::TryParse(winrt::hstring{ response.ResponseJson }, parsed))
            {
                return std::nullopt;
            }

            if (parsed == nullptr || !parsed.HasKey(L"clients"))
            {
                return std::nullopt;
            }

            auto clients = parsed.GetNamedArray(L"clients", nullptr);

            if (clients == nullptr)
            {
                return std::nullopt;
            }

            return clients;
        }


        std::optional<json::JsonObject> FindClientEntry(_In_ std::wstring const& entryIdentifier)
        {
            auto clients = ReadClientsArray();

            if (!clients.has_value())
            {
                return std::nullopt;
            }

            for (uint32_t i = 0; i < clients->Size(); i++)
            {
                auto entry = clients->GetObjectAt(i);

                if (entry != nullptr &&
                    std::wstring{ entry.GetNamedString(L"entryIdentifier", L"") } == entryIdentifier)
                {
                    return entry;
                }
            }

            return std::nullopt;
        }


        size_t CountClientEntries(_In_ std::wstring const& entryIdentifier)
        {
            auto clients = ReadClientsArray();

            if (!clients.has_value())
            {
                return 0;
            }

            size_t count{ 0 };

            for (uint32_t i = 0; i < clients->Size(); i++)
            {
                auto entry = clients->GetObjectAt(i);

                if (entry != nullptr &&
                    std::wstring{ entry.GetNamedString(L"entryIdentifier", L"") } == entryIdentifier)
                {
                    count++;
                }
            }

            return count;
        }


        bool WaitForClientEntry(
            _In_ std::wstring const& entryIdentifier,
            _In_ std::chrono::milliseconds const timeout)
        {
            auto deadline = std::chrono::steady_clock::now() + timeout;

            while (std::chrono::steady_clock::now() < deadline)
            {
                if (FindClientEntry(entryIdentifier).has_value())
                {
                    return true;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }

            return false;
        }


        std::optional<json::JsonObject> WaitForClientEntryObject(
            _In_ std::wstring const& entryIdentifier,
            _In_ std::chrono::milliseconds const timeout)
        {
            auto deadline = std::chrono::steady_clock::now() + timeout;

            while (std::chrono::steady_clock::now() < deadline)
            {
                auto entry = FindClientEntry(entryIdentifier);

                if (entry.has_value())
                {
                    return entry;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }

            return std::nullopt;
        }


        bool WaitForClientEntryGone(
            _In_ std::wstring const& entryIdentifier,
            _In_ std::chrono::milliseconds const timeout)
        {
            auto deadline = std::chrono::steady_clock::now() + timeout;

            while (std::chrono::steady_clock::now() < deadline)
            {
                if (!FindClientEntry(entryIdentifier).has_value())
                {
                    return true;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }

            return false;
        }


        uint64_t ReadClientLatency(_In_ std::wstring const& entryIdentifier)
        {
            auto entry = FindClientEntry(entryIdentifier);

            if (!entry.has_value())
            {
                return 0;
            }

            return static_cast<uint64_t>(entry->GetNamedNumber(L"currentLatencyTicks", 0));
        }


        uint64_t WaitForNonZeroClientLatency(
            _In_ std::wstring const& entryIdentifier,
            _In_ std::chrono::milliseconds const timeout)
        {
            auto deadline = std::chrono::steady_clock::now() + timeout;

            while (std::chrono::steady_clock::now() < deadline)
            {
                auto latency = ReadClientLatency(entryIdentifier);

                if (latency > 0)
                {
                    return latency;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }

            return 0;
        }


        // Counts live bidirectional UMP endpoints whose name matches. An orphaned endpoint is
        // reachable through no command the transport offers, so the device graph is the only
        // place it can be seen at all. The interface class is restated here rather than included,
        // for the same reason NetworkMidiTestProtocol restates the wire format.
        size_t CountLiveEndpointsNamedWide(_In_ std::wstring const& endpointName)
        {
            constexpr wchar_t UmpBidirectionalInterfaceClass[]{ L"{E7CCE071-3C03-423f-88D3-F1045D02552B}" };

            try
            {
                std::wstring selector{ L"System.Devices.InterfaceClassGuid:=\"" };
                selector += UmpBidirectionalInterfaceClass;
                selector += L"\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True";

                auto devices = enumeration::DeviceInformation::FindAllAsync(
                    winrt::hstring{ selector },
                    {},
                    enumeration::DeviceInformationKind::DeviceInterface).get();

                size_t count{ 0 };

                for (auto const& device : devices)
                {
                    if (std::wstring{ device.Name() }.find(endpointName) != std::wstring::npos)
                    {
                        count++;
                    }
                }

                return count;
            }
            catch (...)
            {
                Log::Warning(L"Could not enumerate MIDI endpoints.");

                return 0;
            }
        }


        size_t CountLiveEndpointsNamed(_In_ std::string const& endpointName)
        {
            return CountLiveEndpointsNamedWide(std::wstring(endpointName.begin(), endpointName.end()));
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
            // Reported per test as a failure. Warning here so the cause appears once, up front.
            Log::Warning(L"The Windows MIDI Service could not be reached. Every client test will fail.");
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
            SetDirectConnectionScanInterval(DefaultDirectConnectionScanInterval);
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


    // A host which reboots says goodbye, or simply stops answering. Either way the client
    // definition has to go back to the endpoint creator, or the connection is dead until the
    // service restarts.
    void ClientTests::ClientReconnectsAfterHostSendsBye()
    {
        if (!RequireService()) return;

        ClientUnderTest client;
        VERIFY_IS_TRUE(EstablishClientSession(client));

        // Same socket and port throughout, which is what a host looks like when it restarts and
        // the client is configured with a fixed address and port.
        client.Host().SetRelatchOnInvitation(true);
        client.Host().ClearHistory();

        VERIFY_IS_TRUE(client.Host().SendBye(ByeReason::PowerDown, "restarting"),
            L"Could not send the Bye which ends the session.");

        auto invitation = client.Host().WaitForCommand(CommandCode::Invitation, InvitationTimeout);

        VERIFY_IS_TRUE(invitation.has_value(),
            L"After the host ended the session, the service must invite it again rather than wait for a service restart.");

        auto data = client.Host().WaitForCommand(CommandCode::UmpData, SessionTimeout);

        if (!data.has_value())
        {
            for (auto const& packet : client.Host().ReceivedPackets())
            {
                Log::Comment(DescribePacket(packet).c_str());
            }
        }

        VERIFY_IS_TRUE(data.has_value(), L"The reconnected session did not establish.");
    }


    void ClientTests::ClientReconnectsAfterHostStopsResponding()
    {
        if (!RequireService()) return;

        // The client only gives up after several unanswered pings, so this one is slow by
        // nature. Logged up front so a slow run does not look like a hang.
        Log::Comment(L"This test waits for the client's ping timeout and can take about a minute. Please let it run.");

        ClientUnderTest client;
        VERIFY_IS_TRUE(EstablishClientSession(client));

        // The host is still bound, but silent. This is the network-outage case rather than the
        // clean shutdown one, so the client has to notice by itself.
        client.Host().SetAutoPingReply(false);
        client.Host().SetRelatchOnInvitation(true);
        client.Host().ClearHistory();

        // Ping interval is 2s and the client tolerates 5 unanswered, so the session ends after
        // roughly 12 seconds. Generous, because the endpoint teardown is on the same worker.
        auto invitation = client.Host().WaitForCommand(CommandCode::Invitation, std::chrono::milliseconds(60000));

        VERIFY_IS_TRUE(invitation.has_value(),
            L"After the session timed out, the service must invite the host again.");

        // Answer normally again, as a returning host would
        client.Host().SetAutoPingReply(true);

        auto data = client.Host().WaitForCommand(CommandCode::UmpData, SessionTimeout);

        VERIFY_IS_TRUE(data.has_value(), L"The session did not re-establish once the host answered again.");
    }


    // The reconnect must not resurrect something the user deliberately took down.
    void ClientTests::ClientDoesNotReconnectAfterUserDisconnect()
    {
        if (!RequireService()) return;

        ClientUnderTest client;
        VERIFY_IS_TRUE(EstablishClientSession(client));

        DisconnectClient(client.EntryIdentifier());

        // Let the Bye exchange finish before counting, or the disconnect's own traffic is
        // mistaken for a reconnect attempt.
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));

        client.Host().ClearHistory();

        // Several scan intervals. The interval is shortened to 1s in ClassSetup.
        std::this_thread::sleep_for(std::chrono::milliseconds(8000));

        auto invitations = client.Host().CountReceived(CommandCode::Invitation);

        Log::Comment(String().Format(L"Invitations seen after a user disconnect: %zu", invitations));

        VERIFY_ARE_EQUAL(static_cast<size_t>(0), invitations,
            L"A client the user disconnected must stay disconnected.");
    }


    // Nothing announces that a direct address has come online, so the service stops inviting it
    // rather than putting invitations on the wire forever for every dead address configured.
    void ClientTests::ClientConnectsToHostWhichComesOnlineLater()
    {
        if (!RequireService()) return;

        Log::Comment(L"This test waits out the full invitation retry budget before the host answers, so it takes a while.");

        ClientUnderTest client;

        // Bound so the test can watch, but silent: the client sees a host which is not there.
        VERIFY_IS_TRUE(client.Start(FakeHostInvitationBehavior::Ignore), L"Could not set up the client.");

        // Let the client exhaust its invitation attempts and cancel with Bye 0x80
        auto give_up = client.Host().WaitForCommand(CommandCode::Bye, InvitationTimeout);

        VERIFY_IS_TRUE(give_up.has_value(), L"The client never cancelled its unanswered invitation.");

        auto byeCommand = give_up->Find(CommandCode::Bye);

        VERIFY_IS_NOT_NULL(byeCommand);

        VERIFY_ARE_EQUAL(ByeReason::InvitationCanceled, byeCommand->GetByeReason(),
            L"Giving up on an unanswered invitation is reported with 0x80.");

        // The host is now switched on, but it has no way to say so.
        client.Host().SetInvitationBehavior(FakeHostInvitationBehavior::Accept);
        client.Host().SetRelatchOnInvitation(true);
        client.Host().ClearHistory();

        // Several scan intervals, shortened to 1s in ClassSetup
        std::this_thread::sleep_for(std::chrono::milliseconds(8000));

        VERIFY_ARE_EQUAL(static_cast<size_t>(0), client.Host().CountReceived(CommandCode::Invitation),
            L"A direct connection which gave up must not keep inviting on its own.");

        // The app telling the service the remote is reachable now
        auto connectResult = ConnectDirectClient(
            client.EntryIdentifier(),
            FakeNetworkHost::Address(),
            client.Host().Port());

        VERIFY_IS_TRUE(connectResult.IsSuccess(),
            L"A connect command for an existing entry must be accepted.");

        auto invitation = client.Host().WaitForCommand(CommandCode::Invitation, InvitationTimeout);

        VERIFY_IS_TRUE(invitation.has_value(),
            L"A connect command must make the service try the direct address again.");

        auto data = client.Host().WaitForCommand(CommandCode::UmpData, SessionTimeout);

        VERIFY_IS_TRUE(data.has_value(), L"The session did not establish after the connect command.");
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

        auto clientPort = client.Host().RemotePort();
        VERIFY_ARE_NOT_EQUAL(static_cast<uint16_t>(0), clientPort, L"The host should know the client's source port.");

        Log::Comment(String().Format(L"Service is using source port %u for this session", clientPort));

        // A third party sending a Bye straight to the service's session port. Its source port is
        // not the host's, so the service must not act on it. If it did, anything on the machine
        // could tear down someone else's session with one datagram.
        SOCKET impostor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        VERIFY_ARE_NOT_EQUAL(INVALID_SOCKET, impostor);

        if (impostor == INVALID_SOCKET) return;

        sockaddr_in target{ };
        target.sin_family = AF_INET;
        target.sin_port = htons(clientPort);
        InetPtonW(AF_INET, L"127.0.0.1", &target.sin_addr);

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

        // the real host should see no Bye Reply, and the session should still work
        client.Host().ClearHistory();
        VERIFY_IS_TRUE(client.Host().SendPing(0x99AABBCC));

        auto packet = client.Host().WaitForCommand(CommandCode::PingReply, ShortTimeout);

        VERIFY_IS_TRUE(packet.has_value(), L"The established session must be undisturbed by a datagram from another peer.");
    }


    // ------------------------------------------------------------------------------
    // Latency measurement
    // ------------------------------------------------------------------------------

    void ClientTests::ClientPingsEvenWhileTheHostIsSendingTraffic()
    {
        if (!RequireService()) return;

        LogSpecRequirement(L"6.14: Ping is how round trip time is measured. Suppressing it whenever anything "
            L"else arrived meant a session carrying MIDI could never be measured.");

        ClientUnderTest client;
        VERIFY_IS_TRUE(EstablishClientSession(client));

        client.Host().ClearHistory();

        // Keep the link busy for longer than the ping interval. Under the old behaviour the
        // watchdog saw recent traffic on every tick and skipped the ping entirely.
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(12000);
        uint16_t sequenceNumber{ 0 };

        while (std::chrono::steady_clock::now() < deadline)
        {
            // a NOOP UMP, which is valid to send and has no side effects on the endpoint
            client.Host().SendUmpData(sequenceNumber++, std::vector<uint32_t>{ 0x00000000u });

            if (client.Host().CountReceived(CommandCode::Ping) > 0)
            {
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }

        VERIFY_IS_GREATER_THAN(
            client.Host().CountReceived(CommandCode::Ping), static_cast<size_t>(0),
            L"A busy session must still be pinged, or its latency can never be measured.");
    }


    void ClientTests::ClientReportsLatencyOnceThePingIsAnswered()
    {
        if (!RequireService()) return;

        ClientUnderTest client;
        VERIFY_IS_TRUE(EstablishClientSession(client));

        // The fake host answers pings automatically, so this only depends on the service pinging.
        auto latency = WaitForNonZeroClientLatency(client.EntryIdentifier(), std::chrono::milliseconds(30000));

        Log::Comment(String().Format(L"currentLatencyTicks reported as %llu", latency));

        VERIFY_IS_GREATER_THAN(latency, 0ull, L"An active session must report a measured round trip.");
    }


    void ClientTests::ReportedLatencySurvivesBeingReadTwice()
    {
        if (!RequireService()) return;

        ClientUnderTest client;
        VERIFY_IS_TRUE(EstablishClientSession(client));

        auto first = WaitForNonZeroClientLatency(client.EntryIdentifier(), std::chrono::milliseconds(30000));
        VERIFY_IS_GREATER_THAN(first, 0ull, L"An active session must report a measured round trip.");

        // An app polls faster than the ping interval, so most reads land in a window with no new
        // sample in it. Reporting zero there reads as "no latency", not "nothing new".
        auto second = ReadClientLatency(client.EntryIdentifier());

        Log::Comment(String().Format(L"first read %llu, immediate second read %llu", first, second));

        VERIFY_IS_GREATER_THAN(second, 0ull, L"Reading the latency twice in a row must not clear it.");
    }


    // ------------------------------------------------------------------------------
    // Entry lifetime
    // ------------------------------------------------------------------------------

    void ClientTests::DisconnectRemovesTheEntryFromEnumerateClients()
    {
        if (!RequireService()) return;

        ClientUnderTest client;
        VERIFY_IS_TRUE(EstablishClientSession(client));

        auto entryIdentifier = client.EntryIdentifier();

        VERIFY_IS_TRUE(FindClientEntry(entryIdentifier).has_value(), L"The connected client is reported.");

        auto result = DisconnectClient(entryIdentifier);
        VERIFY_IS_TRUE(result.IsSuccess(), L"disconnectClient reported success.");

        // enumerateClients walks the configured definitions, not the live clients. Removing only
        // the live client left the entry being reported as unavailable forever, with no way to
        // get rid of it short of restarting the service.
        VERIFY_IS_TRUE(
            WaitForClientEntryGone(entryIdentifier, std::chrono::milliseconds(10000)),
            L"A disconnected client stops being reported at all.");
    }


    void ClientTests::DisconnectingAnEntryWhichNeverConnectedSucceeds()
    {
        if (!RequireService()) return;

        // Port 1 on loopback: nothing is listening, so this entry never becomes a live client.
        auto entryIdentifier = MakeEntryIdentifier();

        auto created = CreateDirectClient(entryIdentifier, L"127.0.0.1", 1);
        VERIFY_IS_TRUE(created.CallSucceeded, L"The entry was accepted.");

        VERIFY_IS_TRUE(
            WaitForClientEntry(entryIdentifier, std::chrono::milliseconds(10000)),
            L"An entry which has not connected is still reported.");

        // The only reason this used to fail is that the lookup went to the live clients. An
        // entry the user can see is an entry the user has to be able to remove.
        auto result = DisconnectClient(entryIdentifier);

        VERIFY_IS_TRUE(result.IsSuccess(), L"Removing an entry which never connected is a success.");

        VERIFY_IS_TRUE(
            WaitForClientEntryGone(entryIdentifier, std::chrono::milliseconds(10000)),
            L"The entry is gone afterwards.");
    }


    void ClientTests::DisconnectingAnUnknownEntryFailsCleanly()
    {
        if (!RequireService()) return;

        // Still has to fail: reporting success would tell a caller something was removed.
        auto result = DisconnectClient(MakeEntryIdentifier());

        VERIFY_IS_TRUE(result.CallSucceeded, L"The transport answered rather than faulting.");
        VERIFY_IS_FALSE(result.ReportedSuccess, L"An entry the service does not have is not a success.");
    }


    void ClientTests::DisconnectDuringClientCreationLeavesNoLiveClient()
    {
        if (!RequireService()) return;

        // Disconnect the instant the host has seen the invitation. The service is then partway
        // through building the client: the session is being agreed and the endpoint is queued.
        FakeNetworkHost host;
        VERIFY_IS_TRUE(host.Start());

        auto entryIdentifier = MakeEntryIdentifier();

        VERIFY_IS_TRUE(CreateDirectClient(entryIdentifier, FakeNetworkHost::Address(), host.Port()).CallSucceeded);

        auto invitation = host.WaitForCommand(CommandCode::Invitation, InvitationTimeout);
        VERIFY_IS_TRUE(invitation.has_value(), L"The service sent an Invitation.");

        auto result = DisconnectClient(entryIdentifier);
        VERIFY_IS_TRUE(result.IsSuccess(), L"Disconnecting mid-creation is accepted.");

        VERIFY_IS_TRUE(
            WaitForClientEntryGone(entryIdentifier, std::chrono::milliseconds(15000)),
            L"The entry is gone from enumerateClients.");

        // The reply to our invitation arrives on the socket receive path and used to build an
        // endpoint regardless of the disconnect, leaving a device node no command could reach.
        // Assert on the device graph, not on the protocol: an orphan is silent, so asking it
        // for a Ping Reply would pass whether or not the endpoint exists.
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(20000);
        size_t liveEndpoints{ 0 };

        while (std::chrono::steady_clock::now() < deadline)
        {
            liveEndpoints = CountLiveEndpointsNamed(host.EndpointName());

            if (liveEndpoints > 0)
            {
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }

        Log::Comment(String().Format(
            L"Live endpoints named '%S' after the disconnect: %zu", host.EndpointName().c_str(), liveEndpoints));

        VERIFY_ARE_EQUAL(
            static_cast<size_t>(0), liveEndpoints,
            L"An entry disconnected while its client was being created leaves no MIDI endpoint behind.");

        host.Stop();
    }


    // ------------------------------------------------------------------------------
    // connectMdns
    // ------------------------------------------------------------------------------
    void ClientTests::CustomEndpointNameIsAppliedWhenTheEndpointIsCreated()
    {
        if (!RequireService()) return;

        FakeNetworkHost host;
        VERIFY_IS_TRUE(host.Start());

        auto entryIdentifier = MakeEntryIdentifier();
        std::string customName{ "Renamed By User " + host.EndpointName().substr(host.EndpointName().size() - 4) };

        VERIFY_IS_TRUE(
            ConnectDirectClient(
                entryIdentifier,
                FakeNetworkHost::Address(),
                host.Port(),
                L"Test Client",
                std::wstring(customName.begin(), customName.end())).IsSuccess());

        // Wait for the session, which is what triggers endpoint creation.
        VERIFY_IS_TRUE(host.WaitForCommand(CommandCode::Invitation, InvitationTimeout).has_value());
        VERIFY_IS_TRUE(host.WaitForCommand(CommandCode::UmpData, SessionTimeout).has_value());

        // The endpoint must appear under the user's name, never under the remote's. Anything
        // matching the remote's own name would mean it was created first and renamed after,
        // which is the churn this avoids.
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(20000);
        size_t named{ 0 };

        while (std::chrono::steady_clock::now() < deadline && named == 0)
        {
            named = CountLiveEndpointsNamed(customName);

            if (named == 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }

        Log::Comment(String().Format(L"Endpoints named '%S': %zu", customName.c_str(), named));

        VERIFY_IS_GREATER_THAN(named, static_cast<size_t>(0),
            L"The endpoint carries the name the user supplied when the connection was created.");

        // The remote's own name appearing as well would mean the endpoint was created under it
        // and renamed after, which is the churn this avoids.
        VERIFY_ARE_EQUAL(static_cast<size_t>(0), CountLiveEndpointsNamed(host.EndpointName()),
            L"No endpoint was created under the remote's name first.");

        DisconnectClient(entryIdentifier);
        host.Stop();
    }


    void ClientTests::CustomEndpointNameSurvivesUnicode()
    {
        if (!RequireService()) return;

        FakeNetworkHost host;
        VERIFY_IS_TRUE(host.Start());

        auto entryIdentifier = MakeEntryIdentifier();

        // Endpoint names are UTF-8 on the wire and the device name is UTF-16, so a name outside
        // ASCII exercises a conversion the default names never reach.
        std::wstring customName{ L"Клавиши \u30B7\u30F3\u30BB " };
        customName += std::wstring(entryIdentifier.begin() + 1, entryIdentifier.begin() + 9);

        VERIFY_IS_TRUE(
            ConnectDirectClient(entryIdentifier, FakeNetworkHost::Address(), host.Port(), L"Test Client", customName).IsSuccess());

        VERIFY_IS_TRUE(host.WaitForCommand(CommandCode::Invitation, InvitationTimeout).has_value());
        VERIFY_IS_TRUE(host.WaitForCommand(CommandCode::UmpData, SessionTimeout).has_value());

        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(20000);
        size_t named{ 0 };

        while (std::chrono::steady_clock::now() < deadline && named == 0)
        {
            named = CountLiveEndpointsNamedWide(customName);

            if (named == 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }

        VERIFY_IS_GREATER_THAN(named, static_cast<size_t>(0),
            L"A non-ASCII endpoint name survives the round trip to the device graph.");

        DisconnectClient(entryIdentifier);
        host.Stop();
    }


    void ClientTests::CustomEndpointNameCanComeFromTheConfigFileEntry()
    {
        if (!RequireService()) return;

        // The same name, arriving as part of a create entry rather than a command. This is the
        // path a configuration file takes at service start.
        FakeNetworkHost host;
        VERIFY_IS_TRUE(host.Start());

        auto entryIdentifier = MakeEntryIdentifier();
        std::string customName{ "Config File Named " + host.EndpointName().substr(host.EndpointName().size() - 4) };

        VERIFY_IS_TRUE(
            CreateDirectClient(
                entryIdentifier,
                FakeNetworkHost::Address(),
                host.Port(),
                false,
                std::wstring(customName.begin(), customName.end())).CallSucceeded);

        VERIFY_IS_TRUE(host.WaitForCommand(CommandCode::Invitation, InvitationTimeout).has_value());
        VERIFY_IS_TRUE(host.WaitForCommand(CommandCode::UmpData, SessionTimeout).has_value());

        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(20000);
        size_t named{ 0 };

        while (std::chrono::steady_clock::now() < deadline && named == 0)
        {
            named = CountLiveEndpointsNamed(customName);

            if (named == 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }

        VERIFY_IS_GREATER_THAN(named, static_cast<size_t>(0),
            L"A custom name in the create entry is applied to the endpoint.");

        DisconnectClient(entryIdentifier);
        host.Stop();
    }


    void ClientTests::ConnectMdnsAcceptsACustomEndpointName()
    {
        if (!RequireService()) return;

        // The device id will never resolve, so nothing connects. What is verified is that the
        // verb accepts the argument and records the entry, which is the half that can be tested
        // without a real advertised host.
        auto entryIdentifier = MakeEntryIdentifier();
        auto matchId = L"\\\\?\\MIDI2#TestOnly_" + entryIdentifier + L"#{aabbccdd-0000-0000-0000-000000000000}";

        VERIFY_IS_TRUE(
            ConnectMdnsClient(entryIdentifier, matchId, L"Mdns Named Test", L"Mdns Custom Name").IsSuccess(),
            L"connectMdns accepts a custom endpoint name.");

        VERIFY_IS_TRUE(WaitForClientEntry(entryIdentifier, std::chrono::milliseconds(10000)));

        DisconnectClient(entryIdentifier);
    }


    void ClientTests::TransportDeclaresEveryCapabilityAnAppReliesOn()
    {
        if (!RequireService()) return;

        auto result = QueryCapabilities();

        VERIFY_IS_TRUE(result.IsSuccess(), L"The transport answers queryCapabilities.");

        json::JsonObject parsed{ nullptr };
        VERIFY_IS_TRUE(json::JsonObject::TryParse(winrt::hstring{ result.ResponseJson }, parsed));

        auto capabilities = parsed.GetNamedObject(L"queryCapabilities", nullptr);
        VERIFY_IS_NOT_NULL(capabilities);

        if (capabilities == nullptr) return;

        // Every verb an app issues, plus the flag that says a name supplied at creation is
        // honoured. An app refuses to run when any of these is missing, so removing one here is
        // a breaking change and this test is the reminder.
        wchar_t const* const required[]
        {
            L"enumerateHosts", L"enumerateClients",
            L"startHost", L"stopHost", L"removeHost",
            L"connectDirect", L"connectMdns", L"disconnectClient",
            L"approveRemoteClient", L"denyRemoteClient", L"disconnectRemoteClient",
            L"getPendingRemoteClients",
            L"customizeEndpoint", L"customEndpointNameOnCreate",
        };

        for (auto const& capability : required)
        {
            VERIFY_IS_TRUE(capabilities.HasKey(capability),
                String().Format(L"Capability '%s' is declared", capability));

            if (capabilities.HasKey(capability))
            {
                VERIFY_IS_TRUE(capabilities.GetNamedBoolean(capability, false),
                    String().Format(L"Capability '%s' is true", capability));
            }
        }

        // Deliberately false: this transport uses its own host and client verbs instead of the
        // generic per-endpoint ones, and MIDI 1.0 port naming is not wired up here.
        for (auto const& notSupported : { L"restartEndpoint", L"disconnectEndpoint", L"reconnectEndpoint", L"customizePorts" })
        {
            if (capabilities.HasKey(notSupported))
            {
                VERIFY_IS_FALSE(capabilities.GetNamedBoolean(notSupported, true),
                    String().Format(L"Capability '%s' is reported unsupported", notSupported));
            }
        }
    }

    void ClientTests::ConnectMdnsCreatesAnEntryMatchedByDeviceId()
    {
        if (!RequireService()) return;

        // A device id which will never resolve, so nothing is connected to. What is being tested
        // is that the entry is created as an mDNS match at all: until this verb existed, the only
        // runtime path created a direct address entry, so a discovered host could not be followed.
        auto entryIdentifier = MakeEntryIdentifier();
        auto matchId = L"\\\\?\\MIDI2#TestOnly_" + entryIdentifier + L"#{aabbccdd-0000-0000-0000-000000000000}";

        auto result = ConnectMdnsClient(entryIdentifier, matchId, L"Mdns Match Test");

        VERIFY_IS_TRUE(result.IsSuccess(), L"connectMdns was accepted.");

        auto entry = WaitForClientEntryObject(entryIdentifier, std::chrono::milliseconds(10000));

        VERIFY_IS_TRUE(entry.has_value(), L"The entry is reported by enumerateClients.");

        if (entry.has_value())
        {
            VERIFY_ARE_EQUAL(
                matchId, std::wstring{ entry->GetNamedString(L"mdnsMatchId", L"") },
                L"The entry carries the device id it was asked to match.");

            VERIFY_IS_FALSE(
                entry->GetNamedBoolean(L"isDirectConnection", true),
                L"An mDNS match is not a direct connection.");
        }

        DisconnectClient(entryIdentifier);
    }


    void ClientTests::ConnectMdnsWithoutAMatchIdFailsCleanly()
    {
        if (!RequireService()) return;

        auto result = ConnectMdnsClient(MakeEntryIdentifier(), L"", L"No Match Id");

        VERIFY_IS_TRUE(result.CallSucceeded, L"The transport answered rather than faulting.");
        VERIFY_IS_FALSE(result.ReportedSuccess, L"An empty device id is rejected.");
    }


    void ClientTests::ConnectMdnsForAnExistingEntryRearmsIt()
    {
        if (!RequireService()) return;

        auto entryIdentifier = MakeEntryIdentifier();
        auto matchId = L"\\\\?\\MIDI2#TestOnly_" + entryIdentifier + L"#{aabbccdd-0000-0000-0000-000000000000}";

        VERIFY_IS_TRUE(ConnectMdnsClient(entryIdentifier, matchId, L"Rearm Test").IsSuccess());
        VERIFY_IS_TRUE(WaitForClientEntry(entryIdentifier, std::chrono::milliseconds(10000)));

        // The same entry arriving again is the app saying "try it now", exactly as it is for a
        // direct connection. It must not create a second entry for the same identifier.
        VERIFY_IS_TRUE(ConnectMdnsClient(entryIdentifier, matchId, L"Rearm Test").IsSuccess());

        VERIFY_ARE_EQUAL(
            static_cast<size_t>(1), CountClientEntries(entryIdentifier),
            L"Re-arming an entry does not duplicate it.");

        DisconnectClient(entryIdentifier);
    }
}
