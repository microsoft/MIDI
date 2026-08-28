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
using namespace NetworkMidiTest;

namespace json = winrt::Windows::Data::Json;

namespace
{
    // The host these tests create. One per class: the allow and deny lists live on the host, so
    // each test uses a distinct client identity rather than a fresh host.
    std::wstring g_hostEntryIdentifier{ };
    uint16_t g_hostPort{ 0 };
    bool g_hostReady{ false };

    // Shared with the collision tests, which deliberately try to claim it a second time.
    // Generated per run: a host keeps its service instance name reserved even after stopHost,
    // and there is no verb which removes one, so a fixed name would leave this class runnable
    // only once per service lifetime.
    std::wstring g_hostServiceInstanceName{ };

    std::unique_ptr<WinsockScope> g_winsock;

    constexpr auto PendingPollInterval = std::chrono::milliseconds(250);
    constexpr auto PendingPollTimeout = std::chrono::milliseconds(20000);
    constexpr auto ReplyTimeout = std::chrono::milliseconds(8000);


    // The name becomes a DNS-SD instance, so this keeps to lowercase alphanumerics and dashes.
    std::wstring MakeServiceInstanceName()
    {
        std::wstring suffix;

        for (auto const ch : MakeEntryIdentifier())
        {
            if (iswalnum(ch))
            {
                suffix += static_cast<wchar_t>(towlower(ch));
            }

            if (suffix.length() >= 12)
            {
                break;
            }
        }

        return L"midi2-approval-test-" + suffix;
    }


    std::wstring Widen(_In_ std::string const& value)
    {
        return std::wstring(value.begin(), value.end());
    }


    HostEndpointAddress LocalHostAddress()
    {
        HostEndpointAddress address{ };

        address.HostNameOrAddress = L"127.0.0.1";
        address.Port = g_hostPort;
        address.DiscoveredVia = L"created by the approval tests";

        return address;
    }


    std::optional<json::JsonObject> ParseResponse(_In_ ServiceConfigResult const& result)
    {
        json::JsonObject parsed{ nullptr };

        if (!json::JsonObject::TryParse(winrt::hstring{ result.ResponseJson }, parsed))
        {
            return std::nullopt;
        }

        return parsed;
    }


    // The host entry this class created, from an enumerateHosts response.
    std::optional<json::JsonObject> FindOurHost(_In_ json::JsonObject const& response)
    {
        if (!response.HasKey(L"hosts"))
        {
            return std::nullopt;
        }

        auto hosts = response.GetNamedArray(L"hosts", nullptr);

        if (hosts == nullptr)
        {
            return std::nullopt;
        }

        for (uint32_t i = 0; i < hosts.Size(); i++)
        {
            auto host = hosts.GetObjectAt(i);

            if (host != nullptr &&
                std::wstring{ host.GetNamedString(L"entryIdentifier", L"") } == g_hostEntryIdentifier)
            {
                return host;
            }
        }

        return std::nullopt;
    }


    // A newly created host lives only as a pending definition until the endpoint creation worker
    // instantiates it, and stopHost only knows about instantiated hosts.
    bool WaitForHostPresent(
        _In_ std::wstring const& entryIdentifier,
        _In_ std::chrono::milliseconds const timeout = PendingPollTimeout)
    {
        auto deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline)
        {
            auto response = ParseResponse(EnumerateHosts());

            if (response.has_value() && response->HasKey(L"hosts"))
            {
                auto hosts = response->GetNamedArray(L"hosts", nullptr);

                if (hosts != nullptr)
                {
                    for (uint32_t i = 0; i < hosts.Size(); i++)
                    {
                        auto host = hosts.GetObjectAt(i);

                        if (host != nullptr &&
                            std::wstring{ host.GetNamedString(L"entryIdentifier", L"") } == entryIdentifier)
                        {
                            return true;
                        }
                    }
                }
            }

            std::this_thread::sleep_for(PendingPollInterval);
        }

        return false;
    }


    struct ConnectionState
    {
        bool Present{ false };
        bool PendingApproval{ false };
        bool SessionActive{ false };
    };


    // What the polling feed currently says about one remote identity. This is exactly the call
    // the settings app will make on its timer.
    ConnectionState PollConnectionState(
        _In_ std::string const& umpEndpointName,
        _In_ std::string const& productInstanceId)
    {
        ConnectionState state{ };

        auto response = ParseResponse(EnumerateHosts());

        if (!response.has_value())
        {
            return state;
        }

        auto host = FindOurHost(response.value());

        if (!host.has_value() || !host->HasKey(L"connections"))
        {
            return state;
        }

        auto connections = host->GetNamedArray(L"connections", nullptr);

        if (connections == nullptr)
        {
            return state;
        }

        auto wantName = Widen(umpEndpointName);
        auto wantProductInstanceId = Widen(productInstanceId);

        for (uint32_t i = 0; i < connections.Size(); i++)
        {
            auto connection = connections.GetObjectAt(i);

            if (connection == nullptr)
            {
                continue;
            }

            if (std::wstring{ connection.GetNamedString(L"umpEndpointName", L"") } == wantName &&
                std::wstring{ connection.GetNamedString(L"productInstanceId", L"") } == wantProductInstanceId)
            {
                state.Present = true;
                state.PendingApproval = connection.GetNamedBoolean(L"pendingApproval", false);
                state.SessionActive = connection.GetNamedBoolean(L"sessionActive", false);

                return state;
            }
        }

        return state;
    }


    bool WaitForPending(
        _In_ std::string const& umpEndpointName,
        _In_ std::string const& productInstanceId)
    {
        auto deadline = std::chrono::steady_clock::now() + PendingPollTimeout;

        while (std::chrono::steady_clock::now() < deadline)
        {
            auto state = PollConnectionState(umpEndpointName, productInstanceId);

            if (state.Present && state.PendingApproval)
            {
                return true;
            }

            std::this_thread::sleep_for(PendingPollInterval);
        }

        return false;
    }


    // Same, with a caller-chosen budget, for the retry loop which re-invites between rounds.
    bool WaitForPendingWithin(
        _In_ std::string const& umpEndpointName,
        _In_ std::string const& productInstanceId,
        _In_ std::chrono::milliseconds const timeout)
    {
        auto deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline)
        {
            auto state = PollConnectionState(umpEndpointName, productInstanceId);

            if (state.Present && state.PendingApproval)
            {
                return true;
            }

            std::this_thread::sleep_for(PendingPollInterval);
        }

        return false;
    }


    bool WaitForSessionActive(
        _In_ std::string const& umpEndpointName,
        _In_ std::string const& productInstanceId)
    {
        auto deadline = std::chrono::steady_clock::now() + PendingPollTimeout;

        while (std::chrono::steady_clock::now() < deadline)
        {
            auto state = PollConnectionState(umpEndpointName, productInstanceId);

            if (state.Present && state.SessionActive)
            {
                return true;
            }

            std::this_thread::sleep_for(PendingPollInterval);
        }

        return false;
    }


    bool WaitForConnectionReleased(
        _In_ std::string const& umpEndpointName,
        _In_ std::string const& productInstanceId)
    {
        auto deadline = std::chrono::steady_clock::now() + PendingPollTimeout;

        while (std::chrono::steady_clock::now() < deadline)
        {
            if (!PollConnectionState(umpEndpointName, productInstanceId).Present)
            {
                return true;
            }

            std::this_thread::sleep_for(PendingPollInterval);
        }

        return false;
    }


    // Sends one invitation and returns the first reply. Auto ping reply stays on so the host
    // does not reap the connection while a test is deciding.
    std::optional<ParsedPacket> Invite(
        _In_ UdpTestClient& client,
        _In_ std::string const& umpEndpointName,
        _In_ std::string const& productInstanceId)
    {
        PacketBuilder builder;
        builder.StartPacket().AddInvitation(umpEndpointName, productInstanceId);

        if (!client.Send(builder))
        {
            return std::nullopt;
        }

        return client.ReceivePacket(ReplyTimeout);
    }


    // A reply may legitimately take more than one datagram to arrive, because the host answers
    // Pending first and the decision follows later. Everything drained is logged: a Bye from a
    // failed endpoint creation and no reply at all are both "not Accepted", and the difference
    // matters when this fails.
    bool WaitForCommand(
        _In_ UdpTestClient& client,
        _In_ CommandCode const wanted,
        _In_ std::chrono::milliseconds const timeout)
    {
        auto deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline)
        {
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());

            auto packet = client.ReceivePacket(remaining);

            if (!packet.has_value())
            {
                Log::Comment(String().Format(
                    L"Nothing further received while waiting for %s", CommandCodeToString(wanted).c_str()));

                return false;
            }

            Log::Comment(String().Format(
                L"While waiting for %s, received: %s",
                CommandCodeToString(wanted).c_str(),
                DescribePacket(packet.value()).c_str()));

            if (packet->Contains(wanted))
            {
                return true;
            }
        }

        Log::Comment(String().Format(
            L"Timed out waiting for %s", CommandCodeToString(wanted).c_str()));

        return false;
    }


    // The pending entry for one identity, from a getPendingRemoteClients response.
    std::optional<json::JsonObject> FindPendingClient(
        _In_ std::string const& umpEndpointName,
        _In_ std::string const& productInstanceId)
    {
        auto response = ParseResponse(GetPendingRemoteClients());

        if (!response.has_value() || !response->HasKey(L"pendingRemoteClients"))
        {
            return std::nullopt;
        }

        auto clients = response->GetNamedArray(L"pendingRemoteClients", nullptr);

        if (clients == nullptr)
        {
            return std::nullopt;
        }

        auto wantName = Widen(umpEndpointName);
        auto wantProductInstanceId = Widen(productInstanceId);

        for (uint32_t i = 0; i < clients.Size(); i++)
        {
            auto client = clients.GetObjectAt(i);

            if (client == nullptr)
            {
                continue;
            }

            if (std::wstring{ client.GetNamedString(L"umpEndpointName", L"") } == wantName &&
                std::wstring{ client.GetNamedString(L"productInstanceId", L"") } == wantProductInstanceId)
            {
                return client;
            }
        }

        return std::nullopt;
    }


    // Distinct per test so one test's allow or deny entry cannot decide another's outcome.
    std::string UniqueName(_In_ std::string const& suffix)
    {
        return "ApprovalTest-" + suffix;
    }

    std::string UniqueProductInstanceId(_In_ std::string const& suffix)
    {
        return "APPROVALTEST-" + suffix;
    }


    // A distinct service instance name for every host the port tests create, so a rejection can
    // only ever be about the port.
    std::wstring MakeUniqueServiceInstanceName(_In_ std::wstring const& tag)
    {
        static std::atomic<uint32_t> counter{ 0 };

        return L"midi2-approval-test-" + tag + L"-" +
            std::to_wstring(GetTickCount64()) + L"-" +
            std::to_wstring(counter.fetch_add(1));
    }


    // Asks the system for an ephemeral port and gives it straight back. Nothing else on the
    // machine is likely to take it in the meantime, and no other network MIDI host holds it,
    // which is what these tests actually need.
    std::optional<uint16_t> PickLikelyFreePort()
    {
        SOCKET probe = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        if (probe == INVALID_SOCKET)
        {
            return std::nullopt;
        }

        auto closeProbe = wil::scope_exit([&]() { closesocket(probe); });

        sockaddr_in address{ };
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;

        if (bind(probe, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
        {
            return std::nullopt;
        }

        sockaddr_in bound{ };
        int boundLength = sizeof(bound);

        if (getsockname(probe, reinterpret_cast<sockaddr*>(&bound), &boundLength) == SOCKET_ERROR)
        {
            return std::nullopt;
        }

        return ntohs(bound.sin_port);
    }


    uint32_t ReportedErrorCode(_In_ ServiceConfigResult const& result)
    {
        auto response = ParseResponse(result);

        if (!response.has_value() || !response->HasKey(L"errorCode"))
        {
            return 0;
        }

        return static_cast<uint32_t>(response->GetNamedNumber(L"errorCode", 0));
    }
}


bool NetworkMidiApprovalTests::ClassSetup()
{
    g_hostReady = false;

    if (!IsServiceAvailable())
    {
        Log::Error(L"Windows MIDI Services is not reachable, so the approval flow cannot be tested.");
        return false;
    }

    g_winsock = std::make_unique<WinsockScope>();

    g_hostEntryIdentifier = MakeEntryIdentifier();
    g_hostServiceInstanceName = MakeServiceInstanceName();

    auto created = CreateHost(
        g_hostEntryIdentifier,
        L"Approval Test Host",
        L"APPROVALTESTHOST",
        g_hostServiceInstanceName,
        true);

    if (!created.IsSuccess())
    {
        Log::Error(String().Format(
            L"Could not create the approval test host: %s", created.ResponseJson.c_str()));
        return false;
    }

    // Creation may or may not start it depending on when the endpoint creator wakes, so this
    // asks explicitly and tolerates "already started".
    StartHost(g_hostEntryIdentifier);

    auto deadline = std::chrono::steady_clock::now() + PendingPollTimeout;

    while (std::chrono::steady_clock::now() < deadline)
    {
        auto response = ParseResponse(EnumerateHosts());

        if (response.has_value())
        {
            auto host = FindOurHost(response.value());

            if (host.has_value() && host->GetNamedBoolean(L"hasStarted", false))
            {
                auto portText = std::wstring{ host->GetNamedString(L"actualPort", L"") };

                if (!portText.empty())
                {
                    g_hostPort = static_cast<uint16_t>(std::stoul(portText));
                }

                break;
            }
        }

        std::this_thread::sleep_for(PendingPollInterval);
    }

    if (g_hostPort == 0)
    {
        Log::Error(L"The approval test host never reported a started state with a port.");
        return false;
    }

    Log::Comment(String().Format(L"Approval test host listening on 127.0.0.1:%u", g_hostPort));

    g_hostReady = true;

    return true;
}


bool NetworkMidiApprovalTests::ClassCleanup()
{
    if (!g_hostEntryIdentifier.empty())
    {
        // Not StopHost: a stopped host keeps its entry, its socket and its advertisement, and
        // keeps its service instance name reserved.
        RemoveHost(g_hostEntryIdentifier);
    }

    g_winsock.reset();

    return true;
}


void NetworkMidiApprovalTests::InvitationIsHeldPendingAndAppearsInEnumerateHosts()
{
    VERIFY_IS_TRUE(g_hostReady, L"Approval test host is available");

    LogSpecRequirement(L"6.6 - A Host may reply Invitation Reply Pending while permission is sought, and follow with Accepted or Bye.");

    auto name = UniqueName("Pending");
    auto productInstanceId = UniqueProductInstanceId("Pending");

    UdpTestClient client;
    VERIFY_IS_TRUE(client.Open(LocalHostAddress()));

    auto reply = Invite(client, name, productInstanceId);

    VERIFY_IS_TRUE(reply.has_value(), L"Host replied to the invitation");
    VERIFY_IS_TRUE(
        reply->Contains(CommandCode::InvitationReplyPending),
        L"Host replied Pending rather than accepting a client which needs approval");
    VERIFY_IS_FALSE(
        reply->Contains(CommandCode::InvitationReplyAccepted),
        L"Host did not accept a client which needs approval");

    // This is the poll the settings app performs on its timer.
    VERIFY_IS_TRUE(
        WaitForPending(name, productInstanceId),
        L"Pending client is visible in the enumerateHosts feed");

    auto state = PollConnectionState(name, productInstanceId);
    VERIFY_IS_FALSE(state.SessionActive, L"A pending client has no active session");

    // leave nothing behind for the next test
    DenyRemoteClient(g_hostEntryIdentifier, Widen(name), Widen(productInstanceId), L"untilRestart");

    client.Close();
}


void NetworkMidiApprovalTests::PendingRemoteClientsFeedCarriesEnoughToDecide()
{
    VERIFY_IS_TRUE(g_hostReady, L"Approval test host is available");

    auto name = UniqueName("PendingFeed");
    auto productInstanceId = UniqueProductInstanceId("PendingFeed");

    UdpTestClient client;
    VERIFY_IS_TRUE(client.Open(LocalHostAddress()));

    auto reply = Invite(client, name, productInstanceId);
    VERIFY_IS_TRUE(reply.has_value() && reply->Contains(CommandCode::InvitationReplyPending));

    VERIFY_IS_TRUE(WaitForPending(name, productInstanceId), L"Client is pending");

    auto entry = FindPendingClient(name, productInstanceId);
    VERIFY_IS_TRUE(entry.has_value(), L"Pending client appears in getPendingRemoteClients");

    // The three values which have to go straight back into the decision command.
    VERIFY_ARE_EQUAL(
        g_hostEntryIdentifier,
        std::wstring{ entry->GetNamedString(L"entryIdentifier", L"") },
        L"Entry carries the host identifier the decision command needs");

    VERIFY_ARE_EQUAL(
        Widen(name),
        std::wstring{ entry->GetNamedString(L"umpEndpointName", L"") });

    VERIFY_ARE_EQUAL(
        Widen(productInstanceId),
        std::wstring{ entry->GetNamedString(L"productInstanceId", L"") });

    // Which of this PC's hosts is being asked.
    VERIFY_ARE_EQUAL(
        g_hostServiceInstanceName,
        std::wstring{ entry->GetNamedString(L"hostServiceInstanceName", L"") });

    VERIFY_IS_FALSE(
        std::wstring{ entry->GetNamedString(L"hostUmpEndpointName", L"") }.empty(),
        L"Entry names the host being asked");

    // Address detail, for a user who needs to tell two similar devices apart.
    VERIFY_ARE_EQUAL(
        std::wstring{ L"127.0.0.1" },
        std::wstring{ entry->GetNamedString(L"remoteAddress", L"") });

    // ISO 8601 UTC. Checked for shape rather than value, since the clock is the machine's.
    auto requestTime = std::wstring{ entry->GetNamedString(L"requestTime", L"") };

    Log::Comment(String().Format(L"requestTime: %s", requestTime.c_str()));

    VERIFY_IS_FALSE(requestTime.empty(), L"Entry records when the client asked");

    // yyyy-mm-ddThh:mm:ss.fffffffZ
    VERIFY_ARE_EQUAL((size_t)10, requestTime.find(L'T'), L"requestTime is not ISO 8601");
    VERIFY_IS_TRUE(requestTime.back() == L'Z', L"requestTime is not UTC");

    // Once decided it is no longer pending, so it must leave this feed even though the
    // connection itself lingers.
    DenyRemoteClient(g_hostEntryIdentifier, Widen(name), Widen(productInstanceId), L"untilRestart");

    auto deadline = std::chrono::steady_clock::now() + PendingPollTimeout;
    bool departed{ false };

    while (std::chrono::steady_clock::now() < deadline)
    {
        if (!FindPendingClient(name, productInstanceId).has_value())
        {
            departed = true;
            break;
        }

        std::this_thread::sleep_for(PendingPollInterval);
    }

    VERIFY_IS_TRUE(departed, L"A decided client no longer appears in the pending feed");

    client.Close();
}


void NetworkMidiApprovalTests::ApproveOnceAcceptsTheWaitingClient(){
    VERIFY_IS_TRUE(g_hostReady, L"Approval test host is available");

    auto name = UniqueName("ApproveOnce");
    auto productInstanceId = UniqueProductInstanceId("ApproveOnce");

    UdpTestClient client;
    VERIFY_IS_TRUE(client.Open(LocalHostAddress()));

    auto reply = Invite(client, name, productInstanceId);
    VERIFY_IS_TRUE(reply.has_value() && reply->Contains(CommandCode::InvitationReplyPending));

    VERIFY_IS_TRUE(WaitForPending(name, productInstanceId), L"Client is pending before approval");

    auto approved = ApproveRemoteClient(
        g_hostEntryIdentifier, Widen(name), Widen(productInstanceId), L"once");

    VERIFY_IS_TRUE(approved.IsSuccess(), L"approveRemoteClient reported success");

    VERIFY_IS_TRUE(
        WaitForCommand(client, CommandCode::InvitationReplyAccepted, PendingPollTimeout),
        L"Host accepted the invitation after the user approved it");

    VERIFY_IS_TRUE(
        WaitForSessionActive(name, productInstanceId),
        L"Approved client shows an active session in the feed");

    client.Close();
}


void NetworkMidiApprovalTests::ApproveAlwaysIsRememberedForTheNextConnection()
{
    VERIFY_IS_TRUE(g_hostReady, L"Approval test host is available");

    auto name = UniqueName("ApproveAlways");
    auto productInstanceId = UniqueProductInstanceId("ApproveAlways");

    {
        UdpTestClient client;
        VERIFY_IS_TRUE(client.Open(LocalHostAddress()));

        auto reply = Invite(client, name, productInstanceId);
        VERIFY_IS_TRUE(reply.has_value() && reply->Contains(CommandCode::InvitationReplyPending));

        VERIFY_IS_TRUE(WaitForPending(name, productInstanceId), L"Client is pending before approval");

        auto approved = ApproveRemoteClient(
            g_hostEntryIdentifier, Widen(name), Widen(productInstanceId), L"always");

        VERIFY_IS_TRUE(approved.IsSuccess(), L"approveRemoteClient reported success");

        VERIFY_IS_TRUE(
            WaitForCommand(client, CommandCode::InvitationReplyAccepted, PendingPollTimeout),
            L"Host accepted the invitation after the user approved it");

        // The endpoint identity is a hash of product instance id and endpoint name, deliberately
        // with no port in it, so the same device cannot hold two sessions at once. Closing the
        // socket does not tell the host anything, so the session is ended properly and the
        // endpoint waited out before the same identity comes back.
        EndSession(client);

        client.Close();
    }

    VERIFY_IS_TRUE(
        WaitForConnectionReleased(name, productInstanceId),
        L"The first session was released before the client returns");

    // A new socket means a new source port, which is a different connection to the host. The
    // identity is what was remembered, so this one must not be held for a second decision.
    UdpTestClient returning;
    VERIFY_IS_TRUE(returning.Open(LocalHostAddress()));

    auto reply = Invite(returning, name, productInstanceId);

    VERIFY_IS_TRUE(reply.has_value(), L"Host replied to the returning client");

    // Reaching Accepted is itself the proof, and the only reliable one. InvitationReplyPending
    // means two different things on the wire - awaiting a user, and awaiting endpoint creation -
    // so its presence says nothing. What matters is that no approval command is issued anywhere
    // in this block: a client still needing approval would sit pending until the timeout.
    if (!reply->Contains(CommandCode::InvitationReplyAccepted))
    {
        Log::Comment(String().Format(
            L"First reply to the returning client: %s", DescribePacket(reply.value()).c_str()));

        VERIFY_IS_TRUE(
            WaitForCommand(returning, CommandCode::InvitationReplyAccepted, PendingPollTimeout),
            L"A client on the allow list is accepted without any further approval");
    }

    // The approval-specific state, which the feed reports separately from the wire command.
    auto state = PollConnectionState(name, productInstanceId);
    VERIFY_IS_FALSE(state.PendingApproval, L"A client on the allow list is never awaiting approval");

    returning.Close();
}


void NetworkMidiApprovalTests::DenyUntilRestartRefusesTheWaitingClient()
{
    VERIFY_IS_TRUE(g_hostReady, L"Approval test host is available");

    auto name = UniqueName("DenyUntilRestart");
    auto productInstanceId = UniqueProductInstanceId("DenyUntilRestart");

    UdpTestClient client;
    VERIFY_IS_TRUE(client.Open(LocalHostAddress()));

    auto reply = Invite(client, name, productInstanceId);
    VERIFY_IS_TRUE(reply.has_value() && reply->Contains(CommandCode::InvitationReplyPending));

    VERIFY_IS_TRUE(WaitForPending(name, productInstanceId), L"Client is pending before denial");

    auto denied = DenyRemoteClient(
        g_hostEntryIdentifier, Widen(name), Widen(productInstanceId), L"untilRestart");

    VERIFY_IS_TRUE(denied.IsSuccess(), L"denyRemoteClient reported success");

    VERIFY_IS_TRUE(
        WaitForCommand(client, CommandCode::Bye, PendingPollTimeout),
        L"Host sent Bye after the user denied the invitation");

    client.Close();

    // Held in memory until the service restarts, so a second attempt is refused too.
    UdpTestClient returning;
    VERIFY_IS_TRUE(returning.Open(LocalHostAddress()));

    auto secondReply = Invite(returning, name, productInstanceId);

    VERIFY_IS_TRUE(secondReply.has_value(), L"Host replied to the returning denied client");
    VERIFY_IS_FALSE(
        secondReply->Contains(CommandCode::InvitationReplyAccepted),
        L"A client denied until restart is not accepted on a later attempt");

    returning.Close();
}


void NetworkMidiApprovalTests::DenyAlwaysIsRememberedForTheNextConnection()
{
    VERIFY_IS_TRUE(g_hostReady, L"Approval test host is available");

    auto name = UniqueName("DenyAlways");
    auto productInstanceId = UniqueProductInstanceId("DenyAlways");

    {
        UdpTestClient client;
        VERIFY_IS_TRUE(client.Open(LocalHostAddress()));

        auto reply = Invite(client, name, productInstanceId);
        VERIFY_IS_TRUE(reply.has_value() && reply->Contains(CommandCode::InvitationReplyPending));

        VERIFY_IS_TRUE(WaitForPending(name, productInstanceId), L"Client is pending before denial");

        auto denied = DenyRemoteClient(
            g_hostEntryIdentifier, Widen(name), Widen(productInstanceId), L"always");

        VERIFY_IS_TRUE(denied.IsSuccess(), L"denyRemoteClient reported success");

        VERIFY_IS_TRUE(
            WaitForCommand(client, CommandCode::Bye, PendingPollTimeout),
            L"Host sent Bye after the user denied the invitation");

        client.Close();
    }

    // On the deny list, so this must be refused outright rather than parked for another decision.
    UdpTestClient returning;
    VERIFY_IS_TRUE(returning.Open(LocalHostAddress()));

    auto reply = Invite(returning, name, productInstanceId);

    VERIFY_IS_TRUE(reply.has_value(), L"Host replied to the returning denied client");
    VERIFY_IS_TRUE(
        reply->Contains(CommandCode::Bye),
        L"A client on the deny list is refused with Bye");
    VERIFY_IS_FALSE(
        reply->Contains(CommandCode::InvitationReplyPending),
        L"A client on the deny list is not put into the pending state again");
    VERIFY_IS_FALSE(
        reply->Contains(CommandCode::InvitationReplyAccepted),
        L"A client on the deny list is never accepted");

    returning.Close();
}


void NetworkMidiApprovalTests::DecisionForAnUnknownIdentityLeavesThePendingClientWaiting()
{
    VERIFY_IS_TRUE(g_hostReady, L"Approval test host is available");

    auto name = UniqueName("Untouched");
    auto productInstanceId = UniqueProductInstanceId("Untouched");

    UdpTestClient client;
    VERIFY_IS_TRUE(client.Open(LocalHostAddress()));

    auto reply = Invite(client, name, productInstanceId);
    VERIFY_IS_TRUE(reply.has_value() && reply->Contains(CommandCode::InvitationReplyPending));

    VERIFY_IS_TRUE(WaitForPending(name, productInstanceId), L"Client is pending");

    // An approval aimed at somebody else entirely.
    ApproveRemoteClient(
        g_hostEntryIdentifier,
        Widen(UniqueName("NobodyIsWaitingUnderThisName")),
        Widen(UniqueProductInstanceId("NOBODY")),
        L"once");

    // The waiting client must be exactly where it was.
    auto state = PollConnectionState(name, productInstanceId);

    VERIFY_IS_TRUE(state.Present, L"The pending client is still tracked");
    VERIFY_IS_TRUE(state.PendingApproval, L"The pending client is still awaiting its own decision");
    VERIFY_IS_FALSE(state.SessionActive, L"The pending client was not accepted by somebody else's approval");

    DenyRemoteClient(g_hostEntryIdentifier, Widen(name), Widen(productInstanceId), L"untilRestart");

    client.Close();
}


// ------------------------------------------------------------------------------
// disconnectRemoteClient
//
// Before this verb existed there was no way to end one remote client's session with a host on
// this PC. A settings app had to deny the client instead, which also refuses it in future - a
// very different thing from "stop this connection now".
// ------------------------------------------------------------------------------

namespace
{
    // Brings one identity all the way to an active session on the class host. Returns false
    // having already logged, so the callers stay readable.
    bool EstablishApprovedSession(
        _In_ UdpTestClient& client,
        _In_ std::string const& name,
        _In_ std::string const& productInstanceId)
    {
        if (!client.Open(LocalHostAddress()))
        {
            Log::Error(L"Could not open the test client socket.");
            return false;
        }

        // Spec 6.4: a client repeats its invitation until it is answered. UdpTestClient does not
        // do that on its own, and a single datagram is not enough here: these tests run straight
        // after one which tears an endpoint down, and the host's receive path can stall while
        // that teardown runs. One lost invitation would otherwise wait out the whole timeout for
        // a pending state which nothing was left to ask for.
        bool pending{ false };

        for (uint16_t attempt = 0; attempt < 5 && !pending; attempt++)
        {
            if (!Invite(client, name, productInstanceId).has_value())
            {
                Log::Comment(String().Format(L"No reply to invitation attempt %u", attempt + 1));
            }

            pending = WaitForPendingWithin(name, productInstanceId, std::chrono::milliseconds(6000));
        }

        if (!pending)
        {
            Log::Error(L"The client never reached the pending state.");
            return false;
        }

        auto approved = ApproveRemoteClient(
            g_hostEntryIdentifier, Widen(name), Widen(productInstanceId), L"once");

        if (!approved.IsSuccess())
        {
            Log::Error(String().Format(L"approveRemoteClient failed: %s", approved.ResponseJson.c_str()));
            return false;
        }

        if (!WaitForCommand(client, CommandCode::InvitationReplyAccepted, PendingPollTimeout))
        {
            Log::Error(L"The host never accepted the approved invitation.");
            return false;
        }

        if (!WaitForSessionActive(name, productInstanceId))
        {
            Log::Error(L"The approved client never showed an active session in the feed.");
            return false;
        }

        return true;
    }
}


void NetworkMidiApprovalTests::DisconnectRemoteClientEndsAnEstablishedSession()
{
    VERIFY_IS_TRUE(g_hostReady, L"Approval test host is available");

    auto name = UniqueName("DisconnectBye");
    auto productInstanceId = UniqueProductInstanceId("DisconnectBye");

    UdpTestClient client;
    VERIFY_IS_TRUE(EstablishApprovedSession(client, name, productInstanceId));

    client.ClearHistory();

    auto result = DisconnectRemoteClient(g_hostEntryIdentifier, Widen(name), Widen(productInstanceId));

    VERIFY_IS_TRUE(result.IsSuccess(), L"disconnectRemoteClient reported success for a connected client");

    // Spec 6.16: ending a session tells the remote, with a reason. A disconnect the user asked
    // for is 0x01 User Terminated, the same reason the client role sends.
    VERIFY_IS_TRUE(
        WaitForCommand(client, CommandCode::Bye, PendingPollTimeout),
        L"The disconnected remote client is told the session has ended");

    client.Close();
}


void NetworkMidiApprovalTests::DisconnectRemoteClientReleasesTheConnectionFromTheFeed()
{
    VERIFY_IS_TRUE(g_hostReady, L"Approval test host is available");

    auto name = UniqueName("DisconnectFeed");
    auto productInstanceId = UniqueProductInstanceId("DisconnectFeed");

    UdpTestClient client;
    VERIFY_IS_TRUE(EstablishApprovedSession(client, name, productInstanceId));

    auto result = DisconnectRemoteClient(g_hostEntryIdentifier, Widen(name), Widen(productInstanceId));
    VERIFY_IS_TRUE(result.IsSuccess());

    // Absence from the connections array is the only way an app learns a remote went away, so a
    // disconnected client which lingers there is indistinguishable from one still connected.
    VERIFY_IS_TRUE(
        WaitForConnectionReleased(name, productInstanceId),
        L"A disconnected remote client stops being reported as a connection");

    client.Close();
}


void NetworkMidiApprovalTests::DisconnectedRemoteClientIsNotRememberedAsDenied()
{
    VERIFY_IS_TRUE(g_hostReady, L"Approval test host is available");

    auto name = UniqueName("DisconnectThenReturn");
    auto productInstanceId = UniqueProductInstanceId("DisconnectThenReturn");

    {
        UdpTestClient client;
        VERIFY_IS_TRUE(EstablishApprovedSession(client, name, productInstanceId));

        auto result = DisconnectRemoteClient(g_hostEntryIdentifier, Widen(name), Widen(productInstanceId));
        VERIFY_IS_TRUE(result.IsSuccess());

        client.Close();
    }

    VERIFY_IS_TRUE(
        WaitForConnectionReleased(name, productInstanceId),
        L"The first session was released before the client returns");

    // The whole point of the verb: nothing was written down, so this identity is treated as new
    // again rather than being refused. A denial would send a Bye instead of holding it pending.
    UdpTestClient returning;
    VERIFY_IS_TRUE(returning.Open(LocalHostAddress()));

    auto reply = Invite(returning, name, productInstanceId);
    VERIFY_IS_TRUE(reply.has_value(), L"The host replied to the returning client");

    VERIFY_IS_TRUE(
        WaitForPending(name, productInstanceId),
        L"A disconnected client is asked about again rather than refused outright");

    DenyRemoteClient(g_hostEntryIdentifier, Widen(name), Widen(productInstanceId), L"untilRestart");

    returning.Close();
}


void NetworkMidiApprovalTests::DisconnectRemoteClientForAnUnknownIdentityFailsCleanly()
{
    VERIFY_IS_TRUE(g_hostReady, L"Approval test host is available");

    // Reporting success here would tell a caller a disconnect happened when nothing did.
    auto result = DisconnectRemoteClient(
        g_hostEntryIdentifier,
        Widen(UniqueName("NobodyIsConnectedUnderThisName")),
        Widen(UniqueProductInstanceId("NOBODY")));

    VERIFY_IS_TRUE(result.CallSucceeded, L"The transport answered rather than faulting");
    VERIFY_IS_FALSE(result.ReportedSuccess, L"Disconnecting a client which is not connected is not a success");
}


void NetworkMidiApprovalTests::DisconnectRemoteClientForAnUnknownHostFailsCleanly()
{
    VERIFY_IS_TRUE(g_hostReady, L"Approval test host is available");

    auto result = DisconnectRemoteClient(
        MakeEntryIdentifier(),
        Widen(UniqueName("AnyName")),
        Widen(UniqueProductInstanceId("ANY")));

    VERIFY_IS_TRUE(result.CallSucceeded, L"The transport answered rather than faulting");
    VERIFY_IS_FALSE(result.ReportedSuccess, L"A host the service does not have is not a success");
}


void NetworkMidiApprovalTests::DisconnectRemoteClientWithoutAnIdentityFailsCleanly()
{
    VERIFY_IS_TRUE(g_hostReady, L"Approval test host is available");

    // A remote client is named by the pair. Half of it could match the wrong device.
    auto result = DisconnectRemoteClient(g_hostEntryIdentifier, L"", L"");

    VERIFY_IS_TRUE(result.CallSucceeded, L"The transport answered rather than faulting");
    VERIFY_IS_FALSE(result.ReportedSuccess, L"An empty identity is rejected");
}


void NetworkMidiApprovalTests::RemoteWithAnIncompleteIdentityIsNotListedAsConnected()
{
    VERIFY_IS_TRUE(g_hostReady, L"Approval test host is available");

    auto name = UniqueName("HalfIdentity");

    // Counted first, because other tests in this class may legitimately have connections up.
    auto connectionCountNow = []() -> uint32_t
        {
            auto response = ParseResponse(EnumerateHosts());

            if (!response.has_value())
            {
                return 0;
            }

            auto host = FindOurHost(response.value());

            if (!host.has_value() || !host->HasKey(L"connections"))
            {
                return 0;
            }

            auto connections = host->GetNamedArray(L"connections", nullptr);

            return connections == nullptr ? 0 : connections.Size();
        };

    auto const before = connectionCountNow();

    // An invitation with no product instance id. The host refuses it, but the connection object
    // it allocated to read the datagram lives on until the idle reaper takes it. Reporting that
    // as a connected device gave the user a row whose Disconnect and Block buttons could never
    // work, because both address a remote by the identity pair this remote never supplied.
    UdpTestClient client;
    VERIFY_IS_TRUE(client.Open(LocalHostAddress()));

    PacketBuilder builder;
    builder.StartPacket().AddInvitation(name, "");

    VERIFY_IS_TRUE(client.Send(builder));

    // Long enough for the host to have processed it and for a poll to have seen it.
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    auto response = ParseResponse(EnumerateHosts());
    VERIFY_IS_TRUE(response.has_value());

    auto host = FindOurHost(response.value());
    VERIFY_IS_TRUE(host.has_value());

    if (host.has_value() && host->HasKey(L"connections"))
    {
        auto connections = host->GetNamedArray(L"connections", nullptr);

        for (uint32_t i = 0; connections != nullptr && i < connections.Size(); i++)
        {
            auto connection = connections.GetObjectAt(i);

            if (connection == nullptr)
            {
                continue;
            }

            auto listedName = std::wstring{ connection.GetNamedString(L"umpEndpointName", L"") };
            auto listedProductInstanceId = std::wstring{ connection.GetNamedString(L"productInstanceId", L"") };

            VERIFY_ARE_NOT_EQUAL(Widen(name), listedName, L"A remote with no product instance id is not listed");

            // Nothing listed may be missing either half, whoever put it there.
            VERIFY_IS_FALSE(listedName.empty(), L"A listed connection always has a UMP endpoint name");
            VERIFY_IS_FALSE(listedProductInstanceId.empty(), L"A listed connection always has a product instance id");
        }

        VERIFY_ARE_EQUAL(before, connections == nullptr ? 0u : connections.Size(),
            L"A refused remote does not add a connected device");
    }

    client.Close();
}


void NetworkMidiApprovalTests::HostConnectionReportsStatisticsForAnActiveSession(){
    VERIFY_IS_TRUE(g_hostReady, L"Approval test host is available");

    auto name = UniqueName("HostStats");
    auto productInstanceId = UniqueProductInstanceId("HostStats");

    UdpTestClient client;
    VERIFY_IS_TRUE(EstablishApprovedSession(client, name, productInstanceId));

    // The host only measures a round trip when it pings and the remote answers. The test client
    // answers pings automatically, so this is purely a question of whether the host pings at all
    // while a session is up. It used to ping only when the link had gone silent, which meant a
    // live session never produced a latency sample.
    auto deadline = std::chrono::steady_clock::now() + PendingPollTimeout;

    uint64_t latencyTicks{ 0 };
    uint64_t packetsSent{ 0 };
    uint64_t packetsReceived{ 0 };

    while (std::chrono::steady_clock::now() < deadline && latencyTicks == 0)
    {
        auto response = ParseResponse(EnumerateHosts());

        if (response.has_value())
        {
            auto host = FindOurHost(response.value());

            if (host.has_value() && host->HasKey(L"connections"))
            {
                auto connections = host->GetNamedArray(L"connections", nullptr);

                for (uint32_t i = 0; connections != nullptr && i < connections.Size(); i++)
                {
                    auto connection = connections.GetObjectAt(i);

                    if (connection == nullptr ||
                        std::wstring{ connection.GetNamedString(L"umpEndpointName", L"") } != Widen(name))
                    {
                        continue;
                    }

                    latencyTicks = static_cast<uint64_t>(connection.GetNamedNumber(L"currentLatencyTicks", 0));
                    packetsSent = static_cast<uint64_t>(connection.GetNamedNumber(L"totalNetworkPacketsSent", 0));
                    packetsReceived = static_cast<uint64_t>(connection.GetNamedNumber(L"totalNetworkPacketsReceived", 0));
                }
            }
        }

        if (latencyTicks == 0)
        {
            std::this_thread::sleep_for(PendingPollInterval);
        }
    }

    Log::Comment(String().Format(
        L"latency ticks %llu, packets sent %llu, packets received %llu",
        latencyTicks, packetsSent, packetsReceived));

    VERIFY_IS_GREATER_THAN(packetsSent, 0ull, L"The host counts what it has sent to this remote");
    VERIFY_IS_GREATER_THAN(packetsReceived, 0ull, L"The host counts what it has received from this remote");
    VERIFY_IS_GREATER_THAN(latencyTicks, 0ull, L"An active session measures a round trip from the ping reply");

    // The average is retained rather than being consumed by the first reader, so an app polling
    // faster than the ping interval does not see it flicker back to zero.
    auto response = ParseResponse(EnumerateHosts());
    uint64_t secondReadLatency{ 0 };

    if (response.has_value())
    {
        auto host = FindOurHost(response.value());

        if (host.has_value() && host->HasKey(L"connections"))
        {
            auto connections = host->GetNamedArray(L"connections", nullptr);

            for (uint32_t i = 0; connections != nullptr && i < connections.Size(); i++)
            {
                auto connection = connections.GetObjectAt(i);

                if (connection != nullptr &&
                    std::wstring{ connection.GetNamedString(L"umpEndpointName", L"") } == Widen(name))
                {
                    secondReadLatency = static_cast<uint64_t>(connection.GetNamedNumber(L"currentLatencyTicks", 0));
                }
            }
        }
    }

    VERIFY_IS_GREATER_THAN(secondReadLatency, 0ull, L"Reading the latency twice in a row does not clear it");

    DisconnectRemoteClient(g_hostEntryIdentifier, Widen(name), Widen(productInstanceId));

    client.Close();
}


void NetworkMidiApprovalTests::SecondHostWithTheSameServiceInstanceNameIsRejected()
{
    VERIFY_IS_TRUE(g_hostReady, L"Approval test host is available");

    // The class host already holds this name.
    auto duplicateEntryIdentifier = MakeEntryIdentifier();

    auto result = CreateHost(
        duplicateEntryIdentifier,
        L"Duplicate Service Instance Host",
        L"DUPLICATESERVICEINSTANCE",
        g_hostServiceInstanceName,
        false);

    // Stopping it regardless: if the check ever regresses and the host is created, the next run
    // must not inherit it.
    auto cleanup = wil::scope_exit([&]() { RemoveHost(duplicateEntryIdentifier); });

    VERIFY_IS_TRUE(result.CallSucceeded, L"The configuration call itself completed");

    VERIFY_IS_FALSE(
        result.ReportedSuccess,
        L"A host claiming a service instance name already in use is rejected");

    auto response = ParseResponse(result);
    VERIFY_IS_TRUE(response.has_value(), L"The rejection came back as parseable JSON");

    // The caller needs something it can act on, not just a false.
    auto message = std::wstring{ response->GetNamedString(L"message", L"") };

    VERIFY_IS_FALSE(message.empty(), L"The rejection carries a description");
    Log::Comment(String().Format(L"Reported message: %s", message.c_str()));

    VERIFY_IS_TRUE(response->HasKey(L"errorCode"), L"The rejection carries an error code");

    auto errorCode = static_cast<uint32_t>(response->GetNamedNumber(L"errorCode", 0));

    Log::Comment(String().Format(L"Reported errorCode: 0x%08X", errorCode));

    VERIFY_ARE_EQUAL(
        static_cast<uint32_t>(NETWORK_ERROR_CODE_SERVICE_INSTANCE_NAME_IN_USE),
        errorCode,
        L"The error code identifies a duplicate name rather than a generic failure");
}


void NetworkMidiApprovalTests::HostWithAnUnusedServiceInstanceNameIsAccepted()
{
    VERIFY_IS_TRUE(g_hostReady, L"Approval test host is available");

    // Without this, a check which rejected every host would pass the collision test.
    auto entryIdentifier = MakeEntryIdentifier();
    auto serviceInstanceName = std::wstring{ L"midi2-approval-test-unused-" } + std::to_wstring(GetTickCount64());

    auto result = CreateHost(
        entryIdentifier,
        L"Unused Service Instance Host",
        L"UNUSEDSERVICEINSTANCE",
        serviceInstanceName,
        false);

    auto cleanup = wil::scope_exit([&]() { RemoveHost(entryIdentifier); });

    VERIFY_IS_TRUE(
        result.IsSuccess(),
        L"A host with a service instance name nobody else holds is accepted");
}


// removeHost exists because stopHost keeps the entry, and an entry keeps its service instance
// name. Without this a host could never be recreated under the same name, or cleaned up at all,
// short of restarting the service.
void NetworkMidiApprovalTests::RemovedHostReleasesItsServiceInstanceName()
{
    VERIFY_IS_TRUE(g_hostReady, L"Approval test host is available");

    auto serviceInstanceName = std::wstring{ L"midi2-approval-test-reuse-" } + std::to_wstring(GetTickCount64());

    auto firstEntryIdentifier = MakeEntryIdentifier();

    VERIFY_IS_TRUE(
        CreateHost(firstEntryIdentifier, L"Reuse Host One", L"REUSEONE", serviceInstanceName, false).IsSuccess(),
        L"First host created");

    VERIFY_IS_TRUE(WaitForHostPresent(firstEntryIdentifier), L"First host was instantiated");

    // Stopping is not enough: the entry, and so the name, survives.
    VERIFY_IS_TRUE(StopHost(firstEntryIdentifier).IsSuccess(), L"First host stopped");

    auto blocked = CreateHost(MakeEntryIdentifier(), L"Reuse Host Blocked", L"REUSEBLOCKED", serviceInstanceName, false);

    VERIFY_IS_FALSE(
        blocked.ReportedSuccess,
        L"A merely stopped host still holds its service instance name");

    VERIFY_IS_TRUE(RemoveHost(firstEntryIdentifier).IsSuccess(), L"First host removed");

    auto secondEntryIdentifier = MakeEntryIdentifier();

    auto reused = CreateHost(secondEntryIdentifier, L"Reuse Host Two", L"REUSETWO", serviceInstanceName, false);

    auto cleanup = wil::scope_exit([&]() { RemoveHost(secondEntryIdentifier); });

    VERIFY_IS_TRUE(
        reused.IsSuccess(),
        L"The service instance name is free once the host holding it is removed");
}


void NetworkMidiApprovalTests::RemovingAnUnknownHostReportsFailure()
{
    auto result = RemoveHost(MakeEntryIdentifier());

    VERIFY_IS_TRUE(result.CallSucceeded, L"The configuration call itself completed");

    VERIFY_IS_FALSE(
        result.ReportedSuccess,
        L"Removing a host which does not exist is reported rather than silently succeeding");
}


namespace
{
    // Asks the network, not the platform's discovery cache, whether an instance is being
    // advertised. Retries because a single query can be lost: mDNS is unacknowledged multicast.
    bool WaitForAdvertisementState(
        _In_ std::wstring const& serviceInstanceName,
        _In_ bool const wantPresent,
        _In_ std::chrono::milliseconds const timeout)
    {
        auto const deadline = std::chrono::steady_clock::now() + timeout;

        do
        {
            bool present{ false };

            auto const discovered = DiscoverMdnsServices("_midi2._udp.local", std::chrono::milliseconds(1500));

            std::wstringstream seen;
            seen << L"mDNS round saw " << discovered.size() << L" instance(s): ";

            for (auto const& host : discovered)
            {
                seen << L"'" << host.InstanceName << L"' ";

                if (_wcsicmp(host.InstanceName.c_str(), serviceInstanceName.c_str()) == 0)
                {
                    present = true;
                }
            }

            Log::Comment(String().Format(L"%s (looking for '%s', want present=%d)",
                seen.str().c_str(), serviceInstanceName.c_str(), wantPresent ? 1 : 0));

            if (present == wantPresent)
            {
                return true;
            }

        } while (std::chrono::steady_clock::now() < deadline);

        return false;
    }
}


// https://github.com/microsoft/MIDI/issues/1149. A removed host must stop answering mDNS
// queries. If it keeps answering, every watcher on the network goes on offering a host which
// cannot be connected to, and no remove event is ever raised.
void NetworkMidiApprovalTests::RemovedHostIsNoLongerAdvertisedOnTheNetwork()
{
    auto entryIdentifier = MakeEntryIdentifier();
    auto serviceInstanceName = std::wstring{ L"midi2-advert-removed-" } + std::to_wstring(GetTickCount64());

    VERIFY_IS_TRUE(
        CreateHost(entryIdentifier, L"Advertisement Removal Host", L"ADVERTREMOVED", serviceInstanceName, false, L"auto", true).IsSuccess(),
        L"Host created");

    auto cleanup = wil::scope_exit([&]() { RemoveHost(entryIdentifier); });

    VERIFY_IS_TRUE(WaitForHostPresent(entryIdentifier), L"Host was instantiated");

    // The removal below proves nothing unless the host was demonstrably on the wire first.
    VERIFY_IS_TRUE(
        WaitForAdvertisementState(serviceInstanceName, true, std::chrono::milliseconds(15000)),
        L"The new host answers mDNS queries");

    VERIFY_IS_TRUE(RemoveHost(entryIdentifier).IsSuccess(), L"Host removed");

    cleanup.release();

    VERIFY_IS_TRUE(
        WaitForAdvertisementState(serviceInstanceName, false, std::chrono::milliseconds(20000)),
        L"A removed host stops answering mDNS queries");
}


// stopHost keeps the entry, but a stopped host has no socket bound and cannot be connected to,
// so it must not go on advertising either.
void NetworkMidiApprovalTests::StoppedHostIsNoLongerAdvertisedOnTheNetwork()
{
    auto entryIdentifier = MakeEntryIdentifier();
    auto serviceInstanceName = std::wstring{ L"midi2-advert-stopped-" } + std::to_wstring(GetTickCount64());

    VERIFY_IS_TRUE(
        CreateHost(entryIdentifier, L"Advertisement Stop Host", L"ADVERTSTOPPED", serviceInstanceName, false, L"auto", true).IsSuccess(),
        L"Host created");

    auto cleanup = wil::scope_exit([&]() { RemoveHost(entryIdentifier); });

    VERIFY_IS_TRUE(WaitForHostPresent(entryIdentifier), L"Host was instantiated");

    VERIFY_IS_TRUE(
        WaitForAdvertisementState(serviceInstanceName, true, std::chrono::milliseconds(15000)),
        L"The new host answers mDNS queries");

    VERIFY_IS_TRUE(StopHost(entryIdentifier).IsSuccess(), L"Host stopped");

    VERIFY_IS_TRUE(
        WaitForAdvertisementState(serviceInstanceName, false, std::chrono::milliseconds(20000)),
        L"A stopped host stops answering mDNS queries");
}


// Two hosts sharing a port would mean two sockets fighting over the same inbound datagrams. The
// second bind is the one that fails, at start time, long after the user pressed the button, so
// the collision is caught up front instead.
namespace
{
    // Holds a real UDP port for the lifetime of the object, so the service genuinely cannot bind
    // it. Nothing about this is specific to MIDI: it stands in for whatever else on the machine
    // got there first.
    struct PortHolder
    {
        SOCKET Handle{ INVALID_SOCKET };
        uint16_t Port{ 0 };

        bool Hold(_In_ uint16_t const port)
        {
            Handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

            if (Handle == INVALID_SOCKET) return false;

            BOOL exclusive{ TRUE };
            setsockopt(Handle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                reinterpret_cast<char const*>(&exclusive), sizeof(exclusive));

            sockaddr_in address{ };
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = INADDR_ANY;
            address.sin_port = htons(port);

            if (bind(Handle, reinterpret_cast<sockaddr const*>(&address), sizeof(address)) == SOCKET_ERROR)
            {
                closesocket(Handle);
                Handle = INVALID_SOCKET;

                return false;
            }

            Port = port;

            return true;
        }

        ~PortHolder()
        {
            if (Handle != INVALID_SOCKET) closesocket(Handle);
        }
    };

    std::optional<uint16_t> ReadActualPort(_In_ std::wstring const& entryIdentifier)
    {
        auto result = EnumerateHosts();

        if (!result.IsSuccess()) return std::nullopt;

        auto response = ParseResponse(result);

        if (!response.has_value()) return std::nullopt;

        auto hosts = response->GetNamedArray(L"hosts", nullptr);

        if (hosts == nullptr) return std::nullopt;

        for (uint32_t i = 0; i < hosts.Size(); i++)
        {
            auto host = hosts.GetObjectAt(i);

            if (_wcsicmp(std::wstring{ host.GetNamedString(L"entryIdentifier", L"") }.c_str(), entryIdentifier.c_str()) != 0)
            {
                continue;
            }

            auto const portText = std::wstring{ host.GetNamedString(L"actualPort", L"") };

            if (portText.empty()) return std::nullopt;

            return static_cast<uint16_t>(std::stoul(portText));
        }

        return std::nullopt;
    }

    bool ReadPortFallbackUsed(_In_ std::wstring const& entryIdentifier, _Out_ bool& found)
    {
        found = false;

        auto result = EnumerateHosts();

        if (!result.IsSuccess()) return false;

        auto response = ParseResponse(result);

        if (!response.has_value()) return false;

        auto hosts = response->GetNamedArray(L"hosts", nullptr);

        if (hosts == nullptr) return false;

        for (uint32_t i = 0; i < hosts.Size(); i++)
        {
            auto host = hosts.GetObjectAt(i);

            if (_wcsicmp(std::wstring{ host.GetNamedString(L"entryIdentifier", L"") }.c_str(), entryIdentifier.c_str()) != 0)
            {
                continue;
            }

            found = true;

            return host.GetNamedBoolean(L"portFallbackUsed", false);
        }

        return false;
    }

    // A host which could not bind is still listed, with hasStarted false, so absence is the
    // wrong thing to look for.
    bool WaitForHostStarted(
        _In_ std::wstring const& entryIdentifier,
        _In_ std::chrono::milliseconds const timeout)
    {
        auto const deadline = std::chrono::steady_clock::now() + timeout;

        do
        {
            auto response = ParseResponse(EnumerateHosts());

            if (response.has_value())
            {
                auto hosts = response->GetNamedArray(L"hosts", nullptr);

                if (hosts != nullptr)
                {
                    for (uint32_t i = 0; i < hosts.Size(); i++)
                    {
                        auto host = hosts.GetObjectAt(i);

                        if (_wcsicmp(std::wstring{ host.GetNamedString(L"entryIdentifier", L"") }.c_str(), entryIdentifier.c_str()) == 0 &&
                            host.GetNamedBoolean(L"hasStarted", false))
                        {
                            return true;
                        }
                    }
                }
            }

            std::this_thread::sleep_for(PendingPollInterval);

        } while (std::chrono::steady_clock::now() < deadline);

        return false;
    }
}


void NetworkMidiApprovalTests::HostFallsBackWhenTheConfiguredPortIsTakenByAnotherProcess()
{
    auto const blockedPort = PickLikelyFreePort();

    VERIFY_IS_TRUE(blockedPort.has_value());

    PortHolder holder;

    VERIFY_IS_TRUE(holder.Hold(blockedPort.value()), L"Test took the port before the service could");

    Log::Comment(String().Format(L"Holding port %d against the service", holder.Port));

    auto entryIdentifier = MakeEntryIdentifier();

    auto result = CreateHost(
        entryIdentifier,
        L"Port Fallback Host",
        L"PORTFALLBACK",
        MakeServiceInstanceName(),
        false,
        std::to_wstring(holder.Port),
        false,
        true);

    auto cleanup = wil::scope_exit([&]() { RemoveHost(entryIdentifier); });

    VERIFY_IS_TRUE(result.IsSuccess(), L"The host was accepted");

    VERIFY_IS_TRUE(WaitForHostPresent(entryIdentifier), L"The host started despite its port being taken");

    auto const actualPort = ReadActualPort(entryIdentifier);

    VERIFY_IS_TRUE(actualPort.has_value(), L"The host reports a bound port");

    Log::Comment(String().Format(L"Host bound to port %d instead of %d", actualPort.value(), holder.Port));

    VERIFY_ARE_NOT_EQUAL(holder.Port, actualPort.value(), L"The host did not get the blocked port");

    bool found{ false };

    VERIFY_IS_TRUE(
        ReadPortFallbackUsed(entryIdentifier, found),
        L"The host reports that it fell back, so an app can tell the user");

    VERIFY_IS_TRUE(found);
}


void NetworkMidiApprovalTests::HostWithoutFallbackDoesNotStartWhenItsPortIsTaken()
{
    auto const blockedPort = PickLikelyFreePort();

    VERIFY_IS_TRUE(blockedPort.has_value());

    PortHolder holder;

    VERIFY_IS_TRUE(holder.Hold(blockedPort.value()));

    auto entryIdentifier = MakeEntryIdentifier();

    auto result = CreateHost(
        entryIdentifier,
        L"No Port Fallback Host",
        L"NOPORTFALLBACK",
        MakeServiceInstanceName(),
        false,
        std::to_wstring(holder.Port),
        false,
        false);

    auto cleanup = wil::scope_exit([&]() { RemoveHost(entryIdentifier); });

    // The entry is accepted; it is starting the host which fails, and that is deliberate. The
    // user asked for one specific port and did not agree to any other.
    if (result.IsSuccess())
    {
        VERIFY_IS_FALSE(
            WaitForHostStarted(entryIdentifier, std::chrono::milliseconds(6000)),
            L"A host refused its port and told not to move does not start");
    }
    else
    {
        Log::Comment(L"Creation itself was refused, which is also an acceptable outcome");
    }
}


void NetworkMidiApprovalTests::SecondHostWithTheSameManualPortIsRejected()
{
    VERIFY_IS_TRUE(g_hostReady, L"Approval test host is available");

    auto port = PickLikelyFreePort();

    VERIFY_IS_TRUE(port.has_value(), L"A free port could be found to test with");

    auto const portText = std::to_wstring(port.value());

    Log::Comment(String().Format(L"Both hosts will ask for port %s", portText.c_str()));

    auto firstEntryIdentifier = MakeEntryIdentifier();

    auto first = CreateHost(
        firstEntryIdentifier,
        L"Manual Port Host One",
        L"MANUALPORTONE",
        MakeUniqueServiceInstanceName(L"port-first"),
        false,
        portText);

    auto cleanupFirst = wil::scope_exit([&]() { RemoveHost(firstEntryIdentifier); });

    VERIFY_IS_TRUE(first.IsSuccess(), L"The first host claimed the port");

    auto secondEntryIdentifier = MakeEntryIdentifier();

    auto second = CreateHost(
        secondEntryIdentifier,
        L"Manual Port Host Two",
        L"MANUALPORTTWO",
        MakeUniqueServiceInstanceName(L"port-second"),
        false,
        portText);

    // If the check ever regresses, the host must not survive into the next run.
    auto cleanupSecond = wil::scope_exit([&]() { RemoveHost(secondEntryIdentifier); });

    VERIFY_IS_TRUE(second.CallSucceeded, L"The configuration call itself completed");

    VERIFY_IS_FALSE(
        second.ReportedSuccess,
        L"A second host claiming a port already spoken for is rejected");

    VERIFY_ARE_EQUAL(
        static_cast<uint32_t>(NETWORK_ERROR_CODE_HOST_PORT_IN_USE),
        ReportedErrorCode(second),
        L"The error code identifies a port collision rather than a generic failure");
}


// The automatic case is the one that is easy to miss: the entry says "auto", so comparing
// configured ports would find nothing, yet the socket is bound to a real number a manual entry
// can collide with.
void NetworkMidiApprovalTests::ManualPortMatchingAnAutomaticallyAssignedPortIsRejected()
{
    VERIFY_IS_TRUE(g_hostReady, L"Approval test host is available");

    // g_hostPort is what the system handed the class host, which asked for automatic allocation.
    auto const portText = std::to_wstring(g_hostPort);

    Log::Comment(String().Format(
        L"The class host was automatically assigned port %s; claiming it manually", portText.c_str()));

    auto entryIdentifier = MakeEntryIdentifier();

    auto result = CreateHost(
        entryIdentifier,
        L"Automatic Port Collision Host",
        L"AUTOPORTCOLLISION",
        MakeUniqueServiceInstanceName(L"port-auto"),
        false,
        portText);

    auto cleanup = wil::scope_exit([&]() { RemoveHost(entryIdentifier); });

    VERIFY_IS_TRUE(result.CallSucceeded, L"The configuration call itself completed");

    VERIFY_IS_FALSE(
        result.ReportedSuccess,
        L"A manual port which duplicates an automatically assigned one is rejected");

    VERIFY_ARE_EQUAL(
        static_cast<uint32_t>(NETWORK_ERROR_CODE_HOST_PORT_IN_USE),
        ReportedErrorCode(result),
        L"The error code identifies a port collision");
}


// Without this, a check which rejected every manual port would pass the collision tests.
void NetworkMidiApprovalTests::HostWithAnUnusedManualPortIsAccepted()
{
    VERIFY_IS_TRUE(g_hostReady, L"Approval test host is available");

    auto port = PickLikelyFreePort();

    VERIFY_IS_TRUE(port.has_value(), L"A free port could be found to test with");

    auto entryIdentifier = MakeEntryIdentifier();

    auto result = CreateHost(
        entryIdentifier,
        L"Free Manual Port Host",
        L"FREEMANUALPORT",
        MakeUniqueServiceInstanceName(L"port-free"),
        false,
        std::to_wstring(port.value()));

    auto cleanup = wil::scope_exit([&]() { RemoveHost(entryIdentifier); });

    VERIFY_IS_TRUE(
        result.IsSuccess(),
        L"A host asking for a port nobody else holds is accepted");
}


void NetworkMidiApprovalTests::HostWithAnOutOfRangePortIsRejected()
{
    // 0 means "pick one for me" at the socket layer, which is not what a manual entry asked for,
    // and 65536 does not exist.
    for (auto const& portText : { std::wstring{ L"0" }, std::wstring{ L"65536" }, std::wstring{ L"70000" } })
    {
        auto entryIdentifier = MakeEntryIdentifier();

        auto result = CreateHost(
            entryIdentifier,
            L"Out Of Range Port Host",
            L"OUTOFRANGEPORT",
            MakeUniqueServiceInstanceName(L"port-range"),
            false,
            portText);

        auto cleanup = wil::scope_exit([&]() { RemoveHost(entryIdentifier); });

        VERIFY_IS_TRUE(result.CallSucceeded, L"The configuration call itself completed");

        if (portText == L"0")
        {
            // "0" is a documented way of saying automatic, so it is accepted, not rejected.
            VERIFY_IS_TRUE(
                result.IsSuccess(),
                L"A port of zero is treated as a request for automatic allocation");

            continue;
        }

        VERIFY_IS_FALSE(
            result.ReportedSuccess,
            String().Format(L"A port of %s is rejected", portText.c_str()));

        VERIFY_ARE_EQUAL(
            static_cast<uint32_t>(NETWORK_ERROR_CODE_INVALID_HOST_PORT),
            ReportedErrorCode(result),
            L"The error code identifies an invalid port");
    }
}


void NetworkMidiApprovalTests::HostWithANonNumericPortIsRejected()
{
    auto entryIdentifier = MakeEntryIdentifier();

    auto result = CreateHost(
        entryIdentifier,
        L"Non Numeric Port Host",
        L"NONNUMERICPORT",
        MakeUniqueServiceInstanceName(L"port-text"),
        false,
        L"five thousand");

    auto cleanup = wil::scope_exit([&]() { RemoveHost(entryIdentifier); });

    VERIFY_IS_TRUE(result.CallSucceeded, L"The configuration call itself completed");

    VERIFY_IS_FALSE(
        result.ReportedSuccess,
        L"A port which is not a number is rejected rather than silently becoming zero");

    VERIFY_ARE_EQUAL(
        static_cast<uint32_t>(NETWORK_ERROR_CODE_INVALID_HOST_PORT),
        ReportedErrorCode(result),
        L"The error code identifies an invalid port");
}


// Spec 6.4 sizes the UMP Endpoint Name at 98 bytes and the Product Instance Id at 42. Neither is
// a multiple of four, and the writer used to round its cap down to a whole 32-bit word, so a full
// length Product Instance Id went onto the wire two bytes short. The mDNS advertisement carried
// the untruncated value, so a remote device saw the advertised identity and the in-session
// identity as two different devices and could not join the one it could see.
void NetworkMidiApprovalTests::MaximumLengthIdentityStringsSurviveTheWire()
{
    VERIFY_IS_TRUE(g_hostReady, L"Approval test host is available");

    // Unique, but still exactly at the maximum, so the boundary is what is under test.
    auto const suffix = Widen(std::string{ "-" }) + std::to_wstring(GetTickCount64());

    std::wstring endpointName(MaxEndpointNameBytes - suffix.length(), L'N');
    endpointName += suffix;

    std::wstring productInstanceId(MaxProductInstanceIdBytes - suffix.length(), L'P');
    productInstanceId += suffix;

    VERIFY_ARE_EQUAL(MaxEndpointNameBytes, endpointName.length());
    VERIFY_ARE_EQUAL(MaxProductInstanceIdBytes, productInstanceId.length());

    auto entryIdentifier = MakeEntryIdentifier();

    auto cleanup = wil::scope_exit([&]() { RemoveHost(entryIdentifier); });

    VERIFY_IS_TRUE(
        CreateHost(
            entryIdentifier,
            endpointName,
            productInstanceId,
            MakeUniqueServiceInstanceName(L"maxid"),
            false).IsSuccess(),
        L"Host created with maximum length identity strings");

    StartHost(entryIdentifier);

    // find the port it was given
    uint16_t port{ 0 };

    auto deadline = std::chrono::steady_clock::now() + PendingPollTimeout;

    while (std::chrono::steady_clock::now() < deadline && port == 0)
    {
        auto response = ParseResponse(EnumerateHosts());

        if (response.has_value() && response->HasKey(L"hosts"))
        {
            auto hosts = response->GetNamedArray(L"hosts", nullptr);

            for (uint32_t i = 0; hosts != nullptr && i < hosts.Size(); i++)
            {
                auto host = hosts.GetObjectAt(i);

                if (host != nullptr &&
                    std::wstring{ host.GetNamedString(L"entryIdentifier", L"") } == entryIdentifier &&
                    host.GetNamedBoolean(L"hasStarted", false))
                {
                    auto const portText = std::wstring{ host.GetNamedString(L"actualPort", L"") };

                    if (!portText.empty())
                    {
                        port = static_cast<uint16_t>(std::stoul(portText));
                    }

                    break;
                }
            }
        }

        if (port == 0)
        {
            std::this_thread::sleep_for(PendingPollInterval);
        }
    }

    VERIFY_ARE_NOT_EQUAL(static_cast<uint16_t>(0), port, L"The host started and reported a port");

    HostEndpointAddress address{};
    address.HostNameOrAddress = L"127.0.0.1";
    address.Port = port;

    UdpTestClient client;
    VERIFY_IS_TRUE(client.Open(address));

    auto& context = ProtocolTestContext::Current();

    PacketBuilder builder;
    builder.StartPacket().AddInvitation(
        context.MakeUniqueEndpointName("MaxId"),
        context.MakeUniqueProductInstanceId("M"));

    VERIFY_IS_TRUE(client.Send(builder));

    auto reply = client.WaitForCommand(CommandCode::InvitationReplyAccepted, ReplyTimeout);

    VERIFY_IS_TRUE(reply.has_value(), L"The host accepted the invitation");

    auto accepted = reply->Find(CommandCode::InvitationReplyAccepted);
    VERIFY_IS_NOT_NULL(accepted);

    size_t const nameBytes = static_cast<size_t>(accepted->CommandSpecificData1) * sizeof(uint32_t);
    size_t const payloadBytes = static_cast<size_t>(accepted->PayloadLengthWords) * sizeof(uint32_t);

    auto const reportedName = accepted->GetPayloadString(0, nameBytes);
    auto const reportedProductInstanceId = accepted->GetPayloadString(nameBytes, payloadBytes - nameBytes);

    Log::Comment(String().Format(
        L"Host reported name of %zu bytes and product instance id of %zu bytes",
        reportedName.length(),
        reportedProductInstanceId.length()));

    VERIFY_ARE_EQUAL(
        MaxProductInstanceIdBytes, reportedProductInstanceId.length(),
        L"A maximum length Product Instance Id reaches the wire whole");

    VERIFY_ARE_EQUAL(
        MaxEndpointNameBytes, reportedName.length(),
        L"A maximum length UMP Endpoint Name reaches the wire whole");

    VERIFY_ARE_EQUAL(
        winrt::to_string(productInstanceId), reportedProductInstanceId,
        L"The Product Instance Id on the wire is the one the host was configured with");

    VERIFY_ARE_EQUAL(
        winrt::to_string(endpointName), reportedName,
        L"The UMP Endpoint Name on the wire is the one the host was configured with");

    EndSession(client);
}


void NetworkMidiApprovalTests::CreateThenImmediatelyRemoveLeavesNoHostBehind()
{
    Log::Comment(
        L"This test creates and removes hosts repeatedly and waits for the service to settle, "
        L"so it takes appreciably longer than the others here. It has not hung.");

    VERIFY_IS_TRUE(g_hostReady, L"Approval test host is available");

    auto const before = CountConfiguredHosts();

    VERIFY_IS_TRUE(before.has_value(), L"The host count could be read before the run");

    // No wait between create and remove, so the removal lands while the endpoint creator
    // thread may still be building the host. Repeated, because the window is small.
    const int iterations = 15;

    std::vector<std::wstring> createdIdentifiers;

    for (int i = 0; i < iterations; i++)
    {
        auto entryIdentifier = MakeEntryIdentifier();
        auto serviceInstanceName = std::wstring{ L"midi2-approval-test-norace-" }
            + std::to_wstring(GetTickCount64())
            + L"-"
            + std::to_wstring(i);

        auto created = CreateHost(
            entryIdentifier,
            L"No Race Host",
            L"NORACE",
            serviceInstanceName,
            false);

        VERIFY_IS_TRUE(created.IsSuccess(), L"Host created");

        createdIdentifiers.push_back(entryIdentifier);

        auto removed = RemoveHost(entryIdentifier);

        // Removing an entry which was accepted but has not been built yet is still a removal,
        // so this must not come back as "host not found".
        VERIFY_IS_TRUE(
            removed.IsSuccess(),
            L"Removing a host which may still be starting is reported as a success");
    }

    // let anything still in flight inside the service finish before counting
    Sleep(3000);

    auto const after = CountConfiguredHosts();

    VERIFY_IS_TRUE(after.has_value(), L"The host count could be read after the run");

    Log::Comment(String().Format(
        L"%d create/remove cycles. Hosts before %zu, after %zu.",
        iterations,
        before.value(),
        after.value()));

    VERIFY_ARE_EQUAL(
        before.value(),
        after.value(),
        L"Creating and immediately removing hosts leaves none behind");

    // and none of the specific entries survived
    auto enumerated = EnumerateHosts();

    VERIFY_IS_TRUE(enumerated.IsSuccess(), L"Hosts could be enumerated");

    for (auto const& identifier : createdIdentifiers)
    {
        VERIFY_IS_TRUE(
            enumerated.ResponseJson.find(identifier) == std::wstring::npos,
            L"A removed host does not appear in the enumeration");
    }
}
