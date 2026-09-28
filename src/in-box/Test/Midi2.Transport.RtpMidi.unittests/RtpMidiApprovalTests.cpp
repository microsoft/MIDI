// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// Who may connect to a host, and the names hosts and clients get when given none.
// ============================================================================

#include "pch.h"
#include "RtpMidiTransportTests.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using RtpMidiTest::Command;
using RtpMidiTest::ErrorCode;
using RtpMidiTest::FindEntry;
using RtpMidiTest::IsSuccess;
using RtpMidiTest::NewGuidText;
using RtpMidiTest::Peer;
using RtpMidiTest::SameText;
using RtpMidiTest::WaitFor;

namespace json = winrt::Windows::Data::Json;

namespace
{
    std::wstring const RequireApproval{ L"\"remoteClientPolicy\":\"requireApproval\"" };

    // runs however the test ends, so a failure does not leave a host running for the next test
    template <typename Action>
    class Finally
    {
    public:
        explicit Finally(Action action) : m_action(std::move(action)) {}
        ~Finally() { try { m_action(); } catch (...) {} }

        Finally(Finally const&) = delete;
        Finally& operator=(Finally const&) = delete;

    private:
        Action m_action;
    };

    bool Lists(json::JsonObject const& host, std::wstring const& key, std::wstring const& remoteName, bool const untilRestart)
    {
        if (host == nullptr || !host.HasKey(key)) return false;

        auto const list = host.GetNamedArray(key);

        for (uint32_t i = 0; i < list.Size(); i++)
        {
            auto const entry = list.GetObjectAt(i);

            if (SameText(std::wstring{ entry.GetNamedString(L"remoteName", L"") }, remoteName) &&
                entry.GetNamedBoolean(L"untilRestart", !untilRestart) == untilRestart)
            {
                return true;
            }
        }

        return false;
    }

    bool WasRefused(Peer& remote)
    {
        auto const ended = remote.Ended();
        return !ended.empty() && ended.back() == RtpMidi::EndReason::Rejected;
    }

    std::wstring ThisPcName()
    {
        wchar_t buffer[256]{};
        DWORD size{ ARRAYSIZE(buffer) };

        VERIFY_WIN32_BOOL_SUCCEEDED(GetComputerNameExW(ComputerNameDnsHostname, buffer, &size));
        return std::wstring{ buffer, size };
    }
}


std::wstring RtpMidiTransportTests::CreateHost(std::wstring const& fields, uint16_t& port)
{
    port = 0;

    auto const hostId = NewGuidText();
    auto const section = L"{\"create\":{\"hosts\":{\"" + hostId + L"\":{\"port\":\"auto\",\"advertise\":false" +
        (fields.empty() ? std::wstring{} : L"," + fields) + L"}}}}";

    VERIFY_IS_TRUE(IsSuccess(Send(section)), L"the host is accepted");

    VERIFY_IS_TRUE(WaitFor([&]()
    {
        auto const host = FindHost(hostId);
        if (host == nullptr || !host.GetNamedBoolean(L"hasStarted", false)) return false;

        port = static_cast<uint16_t>(host.GetNamedNumber(L"actualPort", 0));
        return port != 0;
    }, 5000), L"the host starts");

    return hostId;
}

void RtpMidiTransportTests::RemoveHost(std::wstring const& hostId)
{
    Send(Command(L"removeHost", { { L"entryIdentifier", hostId } }));
}

json::JsonObject RtpMidiTransportTests::Decide(std::wstring const& verb, std::wstring const& hostId, std::wstring const& remoteName, std::wstring const& scope)
{
    std::map<std::wstring, std::wstring> arguments{ { L"entryIdentifier", hostId }, { L"remoteName", remoteName } };
    if (!scope.empty()) arguments.emplace(L"scope", scope);

    return Send(Command(verb, arguments));
}

json::JsonObject RtpMidiTransportTests::FindPending(std::wstring const& hostId, std::wstring const& remoteName)
{
    auto const response = Send(Command(L"getPendingRemoteClients"));
    if (!IsSuccess(response) || !response.HasKey(L"pendingRemoteClients")) return nullptr;

    auto const list = response.GetNamedArray(L"pendingRemoteClients");

    for (uint32_t i = 0; i < list.Size(); i++)
    {
        auto const entry = list.GetObjectAt(i);

        if (SameText(std::wstring{ entry.GetNamedString(L"entryIdentifier", L"") }, hostId) &&
            std::wstring{ entry.GetNamedString(L"remoteName", L"") } == remoteName)
        {
            return entry;
        }
    }

    return nullptr;
}


void RtpMidiTransportTests::TestInvitationIsHeldUntilApprovedOnce()
{
    uint16_t port{ 0 };
    auto const hostId = CreateHost(L"\"name\":\"Approval Host\"," + RequireApproval, port);
    Finally cleanup{ [&]() { RemoveHost(hostId); } };

    auto const baseline = m_deviceManager->Endpoints().size();

    Peer remote("Waiting Peer", false);
    VERIFY_IS_TRUE(remote.Start());
    remote.Invite(port);

    json::JsonObject pending{ nullptr };
    VERIFY_IS_TRUE(WaitFor([&]() { pending = FindPending(hostId, L"Waiting Peer"); return pending != nullptr; }, 3000), L"the remote is listed as waiting");

    VERIFY_IS_TRUE(std::wstring{ pending.GetNamedString(L"hostName", L"") } == L"Approval Host", L"the entry names the host being asked");
    VERIFY_IS_FALSE(pending.GetNamedString(L"remoteAddress", L"").empty(), L"and where the ask came from");
    VERIFY_IS_FALSE(pending.GetNamedString(L"requestTime", L"").empty(), L"and when");
    VERIFY_IS_FALSE(pending.GetNamedBoolean(L"approved", true), L"and that nobody has decided yet");

    // two more asks go by
    Sleep(2500);

    VERIFY_IS_TRUE(m_deviceManager->Endpoints().size() == baseline, L"no endpoint while it waits");
    VERIFY_IS_TRUE(remote.ConnectedCount() == 0, L"the remote is not connected");
    VERIFY_IS_TRUE(remote.Ended().empty(), L"and was not refused, so it keeps asking");

    VERIFY_IS_TRUE(IsSuccess(Decide(L"approveRemoteClient", hostId, L"Waiting Peer", L"once")), L"approve it for this connection");

    VERIFY_IS_TRUE(WaitFor([&]() { return remote.ConnectedCount() == 1; }, 3000), L"its next ask gets in");
    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints().size() == baseline + 1; }, 3000), L"and it gets an endpoint");
    VERIFY_IS_TRUE(m_deviceManager->Endpoints()[baseline].EndpointName == L"Waiting Peer");
    VERIFY_IS_TRUE(FindPending(hostId, L"Waiting Peer") == nullptr, L"it is no longer listed as waiting");

    remote.Stop();
    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints()[baseline].Removed; }, 3000), L"its endpoint goes when it leaves");

    Peer again("Waiting Peer", false);
    VERIFY_IS_TRUE(again.Start());
    again.Invite(port);

    VERIFY_IS_TRUE(WaitFor([&]() { return FindPending(hostId, L"Waiting Peer") != nullptr; }, 3000), L"an approval for one connection is not remembered");
}

void RtpMidiTransportTests::TestApproveAlwaysIsRemembered()
{
    uint16_t port{ 0 };
    auto const hostId = CreateHost(RequireApproval, port);
    Finally cleanup{ [&]() { RemoveHost(hostId); } };

    auto const baseline = m_deviceManager->Endpoints().size();

    Peer first("Always Peer", false);
    VERIFY_IS_TRUE(first.Start());
    first.Invite(port);

    VERIFY_IS_TRUE(WaitFor([&]() { return FindPending(hostId, L"Always Peer") != nullptr; }, 3000), L"the remote waits");
    VERIFY_IS_TRUE(IsSuccess(Decide(L"approveRemoteClient", hostId, L"Always Peer", L"always")), L"approve it always");

    VERIFY_IS_TRUE(WaitFor([&]() { return first.ConnectedCount() == 1; }, 3000), L"it connects");
    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints().size() == baseline + 1; }, 3000), L"with an endpoint");
    VERIFY_IS_TRUE(FindPending(hostId, L"Always Peer") == nullptr, L"and is not listed as waiting");
    VERIFY_IS_TRUE(Lists(FindHost(hostId), L"allowedClients", L"Always Peer", false), L"the host lists it as allowed, kept after a restart");

    first.Stop();
    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints()[baseline].Removed; }, 3000));

    // the name matches without regard to case, as the approval is recorded
    Peer second("ALWAYS PEER", false);
    VERIFY_IS_TRUE(second.Start());
    second.Invite(port);

    VERIFY_IS_TRUE(WaitFor([&]() { return second.ConnectedCount() == 1; }, 3000), L"next time it connects without asking");
    VERIFY_IS_TRUE(FindPending(hostId, L"ALWAYS PEER") == nullptr, L"nobody was asked");
}

void RtpMidiTransportTests::TestApproveUntilRestartBeforeTheRemoteAsks()
{
    uint16_t port{ 0 };
    auto const hostId = CreateHost(RequireApproval, port);
    Finally cleanup{ [&]() { RemoveHost(hostId); } };

    VERIFY_IS_TRUE(IsSuccess(Decide(L"approveRemoteClient", hostId, L"Early Peer", L"untilRestart")), L"approve a remote which has not asked yet");
    VERIFY_IS_TRUE(Lists(FindHost(hostId), L"allowedClients", L"Early Peer", true), L"the host lists it as allowed until the service restarts");

    Peer remote("Early Peer", false);
    VERIFY_IS_TRUE(remote.Start());
    remote.Invite(port);

    VERIFY_IS_TRUE(WaitFor([&]() { return remote.ConnectedCount() == 1; }, 3000), L"it connects without waiting");
}

void RtpMidiTransportTests::TestDenyOnceRefusesTheWaitingRemote()
{
    uint16_t port{ 0 };
    auto const hostId = CreateHost(RequireApproval, port);
    Finally cleanup{ [&]() { RemoveHost(hostId); } };

    auto const baseline = m_deviceManager->Endpoints().size();

    Peer remote("Refused Peer", false);
    VERIFY_IS_TRUE(remote.Start());
    remote.Invite(port);

    VERIFY_IS_TRUE(WaitFor([&]() { return FindPending(hostId, L"Refused Peer") != nullptr; }, 3000), L"the remote waits");
    VERIFY_IS_TRUE(IsSuccess(Decide(L"denyRemoteClient", hostId, L"Refused Peer", L"once")), L"refuse this request");

    VERIFY_IS_TRUE(WaitFor([&]() { return WasRefused(remote); }, 3000), L"its next ask is refused, so it stops asking");
    VERIFY_IS_TRUE(FindPending(hostId, L"Refused Peer") == nullptr, L"it is no longer listed");
    VERIFY_IS_TRUE(m_deviceManager->Endpoints().size() == baseline, L"no endpoint was made");

    Peer again("Refused Peer", false);
    VERIFY_IS_TRUE(again.Start());
    again.Invite(port);

    VERIFY_IS_TRUE(WaitFor([&]() { return FindPending(hostId, L"Refused Peer") != nullptr; }, 3000), L"a refusal of one request is not remembered");
}

void RtpMidiTransportTests::TestDenyAlwaysEndsTheConnectionAndRefusesFromThenOn()
{
    uint16_t port{ 0 };
    auto const hostId = CreateHost(RequireApproval, port);
    Finally cleanup{ [&]() { RemoveHost(hostId); } };

    auto const baseline = m_deviceManager->Endpoints().size();

    VERIFY_IS_TRUE(IsSuccess(Decide(L"approveRemoteClient", hostId, L"Changed Mind", L"always")));

    Peer remote("Changed Mind", false);
    VERIFY_IS_TRUE(remote.Start());
    remote.Invite(port);

    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints().size() == baseline + 1; }, 3000), L"the allowed remote connects");

    VERIFY_IS_TRUE(IsSuccess(Decide(L"denyRemoteClient", hostId, L"Changed Mind", L"always")), L"then refuse it always");

    VERIFY_IS_TRUE(WaitFor([&]() { return !remote.Ended().empty(); }, 3000), L"its connection is ended");
    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints()[baseline].Removed; }, 3000), L"and its endpoint removed");

    auto const host = FindHost(hostId);
    VERIFY_IS_TRUE(Lists(host, L"deniedClients", L"Changed Mind", false), L"the host lists it as refused");
    VERIFY_IS_FALSE(Lists(host, L"allowedClients", L"Changed Mind", false), L"and no longer as allowed");

    Peer again("Changed Mind", false);
    VERIFY_IS_TRUE(again.Start());
    again.Invite(port);

    VERIFY_IS_TRUE(WaitFor([&]() { return WasRefused(again); }, 3000), L"its next connection is refused at once");
    VERIFY_IS_TRUE(FindPending(hostId, L"Changed Mind") == nullptr, L"without asking anyone");
    VERIFY_IS_TRUE(m_deviceManager->Endpoints().size() == baseline + 1, L"and without an endpoint");
}

void RtpMidiTransportTests::TestForgettingADecisionAsksAgain()
{
    uint16_t port{ 0 };
    auto const hostId = CreateHost(RequireApproval, port);
    Finally cleanup{ [&]() { RemoveHost(hostId); } };

    VERIFY_IS_TRUE(IsSuccess(Decide(L"denyRemoteClient", hostId, L"Forgotten Peer", L"always")));
    VERIFY_IS_TRUE(IsSuccess(Decide(L"forgetRemoteClient", hostId, L"Forgotten Peer", L"")), L"forget the refusal");
    VERIFY_IS_FALSE(Lists(FindHost(hostId), L"deniedClients", L"Forgotten Peer", false), L"the host no longer lists it");

    VERIFY_IS_TRUE(IsSuccess(Decide(L"forgetRemoteClient", hostId, L"Never Decided", L"")), L"forgetting nothing is not an error");

    Peer remote("Forgotten Peer", false);
    VERIFY_IS_TRUE(remote.Start());
    remote.Invite(port);

    VERIFY_IS_TRUE(WaitFor([&]() { return FindPending(hostId, L"Forgotten Peer") != nullptr; }, 3000), L"it is asked about again");
    VERIFY_IS_TRUE(remote.Ended().empty(), L"rather than refused");
}

void RtpMidiTransportTests::TestRememberedDecisionsFromTheConfigurationFile()
{
    auto const hostId = NewGuidText();
    Finally cleanup{ [&]() { RemoveHost(hostId); } };

    // the shape an app saves: the decisions beside the host, not inside it
    auto const section =
        L"{\"create\":{\"hosts\":{\"" + hostId + L"\":{\"port\":\"auto\",\"advertise\":false," + RequireApproval + L"}}," +
        L"\"remoteClientDecisions\":{\"" + hostId + L"\":{" +
        L"\"allowedClients\":[{\"remoteName\":\"Known Peer\"},{\"no name\":1},\"junk\"]," +
        L"\"deniedClients\":[{\"remoteName\":\"Blocked Peer\"}]}}}}";

    VERIFY_IS_TRUE(IsSuccess(Send(section)), L"the host and its decisions are accepted");

    uint16_t port{ 0 };
    VERIFY_IS_TRUE(WaitFor([&]()
    {
        auto const host = FindHost(hostId);
        if (host == nullptr || !host.GetNamedBoolean(L"hasStarted", false)) return false;

        port = static_cast<uint16_t>(host.GetNamedNumber(L"actualPort", 0));
        return port != 0;
    }, 5000), L"the host starts");

    auto const host = FindHost(hostId);
    VERIFY_IS_TRUE(Lists(host, L"allowedClients", L"Known Peer", false), L"the allowed remote is listed");
    VERIFY_IS_TRUE(Lists(host, L"deniedClients", L"Blocked Peer", false), L"the refused remote is listed");
    VERIFY_ARE_EQUAL(host.GetNamedArray(L"allowedClients").Size(), 1u, L"entries without a name are skipped");

    Peer known("Known Peer", false);
    Peer blocked("Blocked Peer", false);
    VERIFY_IS_TRUE(known.Start() && blocked.Start());

    known.Invite(port);
    blocked.Invite(port);

    VERIFY_IS_TRUE(WaitFor([&]() { return known.ConnectedCount() == 1; }, 3000), L"the remembered approval lets it in");
    VERIFY_IS_TRUE(WaitFor([&]() { return WasRefused(blocked); }, 3000), L"the remembered refusal keeps it out");
}

void RtpMidiTransportTests::TestDeniedRemoteIsRefusedByAHostWhichAllowsAnyone()
{
    uint16_t port{ 0 };
    auto const hostId = CreateHost(L"", port);
    Finally cleanup{ [&]() { RemoveHost(hostId); } };

    VERIFY_IS_TRUE(std::wstring{ FindHost(hostId).GetNamedString(L"remoteClientPolicy", L"") } == L"allowAny", L"a host without a policy lets anyone in");
    VERIFY_IS_TRUE(IsSuccess(Decide(L"denyRemoteClient", hostId, L"Unwelcome Peer", L"always")));

    Peer unwelcome("Unwelcome Peer", false);
    Peer welcome("Welcome Peer", false);
    VERIFY_IS_TRUE(unwelcome.Start() && welcome.Start());

    unwelcome.Invite(port);
    welcome.Invite(port);

    VERIFY_IS_TRUE(WaitFor([&]() { return WasRefused(unwelcome); }, 3000), L"except a remote which was refused");
    VERIFY_IS_TRUE(WaitFor([&]() { return welcome.ConnectedCount() == 1; }, 3000), L"everyone else gets in");
}

void RtpMidiTransportTests::TestUnrecognizedPolicyRequiresApproval()
{
    uint16_t port{ 0 };
    auto const hostId = CreateHost(L"\"remoteClientPolicy\":\"sometimes\"", port);
    Finally cleanup{ [&]() { RemoveHost(hostId); } };

    VERIFY_IS_TRUE(std::wstring{ FindHost(hostId).GetNamedString(L"remoteClientPolicy", L"") } == L"requireApproval",
        L"a policy the transport does not know is read as requiring approval");

    Peer remote("Cautious Peer", false);
    VERIFY_IS_TRUE(remote.Start());
    remote.Invite(port);

    VERIFY_IS_TRUE(WaitFor([&]() { return FindPending(hostId, L"Cautious Peer") != nullptr; }, 3000), L"so a remote waits for a decision");
}

void RtpMidiTransportTests::TestApprovalCommandsCheckTheirArguments()
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    uint16_t port{ 0 };
    auto const hostId = CreateHost(RequireApproval, port);
    Finally cleanup{ [&]() { RemoveHost(hostId); } };

    auto const unknownHost = NewGuidText();

    struct Case { std::wstring Json; uint32_t ExpectedError; wchar_t const* What; };

    std::vector<Case> const cases =
    {
        { Command(L"approveRemoteClient", { { L"remoteName", L"x" }, { L"scope", L"once" } }), 3, L"no host" },
        { Command(L"approveRemoteClient", { { L"entryIdentifier", L"zzz" }, { L"remoteName", L"x" }, { L"scope", L"once" } }), 4, L"a host id which is not a GUID" },
        { Command(L"approveRemoteClient", { { L"entryIdentifier", unknownHost }, { L"remoteName", L"x" }, { L"scope", L"always" } }), 5, L"a host which does not exist" },
        { Command(L"approveRemoteClient", { { L"entryIdentifier", hostId }, { L"scope", L"once" } }), 13, L"no remote name" },
        { Command(L"approveRemoteClient", { { L"entryIdentifier", hostId }, { L"remoteName", L"" }, { L"scope", L"once" } }), 13, L"an empty remote name" },
        { Command(L"approveRemoteClient", { { L"entryIdentifier", hostId }, { L"remoteName", std::wstring(256, L'n') }, { L"scope", L"always" } }), 9, L"a remote name too long to list" },
        { Command(L"approveRemoteClient", { { L"entryIdentifier", hostId }, { L"remoteName", L"x" } }), 15, L"no scope" },
        { Command(L"approveRemoteClient", { { L"entryIdentifier", hostId }, { L"remoteName", L"x" }, { L"scope", L"forever" } }), 15, L"a scope which does not exist" },
        { Command(L"approveRemoteClient", { { L"entryIdentifier", hostId }, { L"remoteName", L"Nobody Waiting" }, { L"scope", L"once" } }), 14, L"approving a request nobody made" },
        { Command(L"denyRemoteClient", { { L"entryIdentifier", unknownHost }, { L"remoteName", L"x" }, { L"scope", L"always" } }), 5, L"refusing for a host which does not exist" },
        { Command(L"forgetRemoteClient", { { L"entryIdentifier", unknownHost }, { L"remoteName", L"x" } }), 5, L"forgetting for a host which does not exist" },
        { Command(L"forgetRemoteClient", { { L"entryIdentifier", hostId } }), 13, L"forgetting without a remote name" },
    };

    for (auto const& testCase : cases)
    {
        auto const response = Send(testCase.Json);

        VERIFY_IS_TRUE(!IsSuccess(response) && ErrorCode(response) == testCase.ExpectedError,
            String().Format(L"%s: success %d, error %u (wanted %u)", testCase.What, IsSuccess(response) ? 1 : 0, ErrorCode(response), testCase.ExpectedError));
    }

    VERIFY_IS_TRUE(IsSuccess(Decide(L"denyRemoteClient", hostId, L"Nobody Waiting", L"once")), L"refusing a request nobody made does no harm");
    VERIFY_IS_TRUE(IsSuccess(Send(Command(L"getPendingRemoteClients"))), L"the waiting list can always be read");
}

void RtpMidiTransportTests::TestNamesDefaultToThisPcName()
{
    auto const pcName = ThisPcName();
    Log::Comment(String().Format(L"This PC is %s", pcName.c_str()));

    uint16_t port{ 0 };
    auto const hostId = CreateHost(L"\"name\":\"\"", port);
    Finally removeHost{ [&]() { RemoveHost(hostId); } };

    auto const host = FindHost(hostId);
    VERIFY_IS_TRUE(std::wstring{ host.GetNamedString(L"name", L"") } == pcName, L"a host given an empty name takes the PC's name");
    VERIFY_IS_TRUE(std::wstring{ host.GetNamedString(L"serviceInstanceName", L"") } == pcName, L"and advertises under it");

    Peer remoteHost("Name Check Host", true);
    VERIFY_IS_TRUE(remoteHost.Start());

    auto const clientId = NewGuidText();
    Finally removeClient{ [&]() { Send(Command(L"removeClient", { { L"entryIdentifier", clientId } })); } };

    VERIFY_IS_TRUE(IsSuccess(Send(L"{\"create\":{\"clients\":{\"" + clientId + L"\":{\"remoteAddress\":\"::1\",\"remotePort\":" +
        std::to_wstring(remoteHost.ControlPort()) + L"}}}}")), L"a client without a name is accepted");

    auto const client = FindClient(clientId);
    VERIFY_IS_TRUE(client != nullptr && std::wstring{ client.GetNamedString(L"name", L"") } == pcName, L"and takes the PC's name");

    std::string pcNameUtf8(pcName.size() * 3, '\0');
    pcNameUtf8.resize(static_cast<size_t>(WideCharToMultiByte(CP_UTF8, 0, pcName.c_str(), static_cast<int>(pcName.size()), pcNameUtf8.data(), static_cast<int>(pcNameUtf8.size()), nullptr, nullptr)));

    VERIFY_IS_TRUE(WaitFor([&]() { return remoteHost.LastRemoteName() == pcNameUtf8; }, 8000), L"which is the name the remote sees");
}
