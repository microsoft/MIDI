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
    bool WaitForHostPresent(_In_ std::wstring const& entryIdentifier)
    {
        auto deadline = std::chrono::steady_clock::now() + PendingPollTimeout;

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
