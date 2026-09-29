// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"
#include "MidiRtpApiTests.h"

using namespace WEX::Logging;
using namespace WEX::Common;

namespace
{
    // spaces rather than periods, because a host name may not contain a period
    constexpr wchar_t TestHostNamePrefix[] = L"MidiApiTest ";

    constexpr uint32_t ServiceWaitMilliseconds{ 5000 };

    // how long an RTP-MIDI remote keeps asking to be let in
    constexpr uint32_t RemoteAskWaitMilliseconds{ 8000 };

    bool TransportIsAvailable()
    {
        try
        {
            return MidiRtpTransportManager::IsTransportAvailable();
        }
        catch (...)
        {
            return false;
        }
    }

    // The one entry in a container, which must be keyed by the id. Keys are compared as GUIDs,
    // because their text form is up to the writer.
    json::JsonObject OnlyEntry(_In_ json::JsonObject const& container, _In_ winrt::guid const& id)
    {
        VERIFY_ARE_EQUAL(container.Size(), 1u, L"exactly one entry");

        auto const first = container.First().Current();
        VERIFY_IS_TRUE(winrt::guid{ std::wstring_view{ first.Key() } } == id, L"keyed by the entry id");

        return first.Value().GetObject();
    }

    json::JsonObject TransportSection(_In_ json::JsonObject const& config)
    {
        return OnlyEntry(config.GetNamedObject(L"endpointTransportPluginSettings"), MidiRtpTransportManager::TransportId());
    }

    std::wstring OnlyKey(_In_ json::JsonObject const& container)
    {
        VERIFY_ARE_EQUAL(container.Size(), 1u, L"exactly one entry");
        return std::wstring{ container.First().Current().Key() };
    }

    MidiRtpConfiguredHost FindHost(_In_ winrt::guid const& hostId)
    {
        for (auto const& host : MidiRtpTransportManager::GetConfiguredHosts())
        {
            if (host.HostId() == hostId) return host;
        }

        return nullptr;
    }

    MidiRtpPendingRemoteClient FindPending(_In_ winrt::guid const& hostId, _In_ std::wstring const& remoteName)
    {
        for (auto const& pending : MidiRtpTransportManager::GetPendingRemoteClients())
        {
            if (pending.HostId() == hostId && std::wstring{ pending.RemoteClientName() } == remoteName) return pending;
        }

        return nullptr;
    }

    MidiRtpConnection FindConnection(_In_ winrt::guid const& hostId, _In_ std::wstring const& remoteName)
    {
        auto const host = FindHost(hostId);
        if (host == nullptr) return nullptr;

        for (auto const& connection : host.Connections())
        {
            if (std::wstring{ connection.RemoteName() } == remoteName) return connection;
        }

        return nullptr;
    }

    bool IsKnown(_In_ winrt::guid const& hostId, _In_ std::wstring const& remoteName, _In_ bool const isAllowed)
    {
        auto const host = FindHost(hostId);
        if (host == nullptr) return false;

        for (auto const& known : host.KnownRemoteClients())
        {
            if (std::wstring{ known.RemoteClientName() } == remoteName && known.IsAllowed() == isAllowed) return true;
        }

        return false;
    }

    std::wstring ThisPcName()
    {
        wchar_t name[256]{};
        DWORD size = ARRAYSIZE(name);

        if (GetComputerNameExW(ComputerNameDnsHostname, name, &size) && size > 0) return std::wstring{ name, size };

        size = ARRAYSIZE(name);
        if (GetComputerNameExW(ComputerNameNetBIOS, name, &size) && size > 0) return std::wstring{ name, size };

        return {};
    }
}

#define SKIP_IF_NO_RTP_TRANSPORT() \
    if (!TransportIsAvailable()) \
    { \
        Log::Result(TestResults::Skipped, L"The RTP-MIDI transport is not installed on this PC."); \
        return; \
    }


bool MidiRtpApiTests::ClassSetup()
{
    WSADATA data{};
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
}

bool MidiRtpApiTests::ClassCleanup()
{
    RemoveCreatedHosts();
    WSACleanup();

    return true;
}

// Each connection leaves a deactivated device node behind for its endpoint. These remove exactly
// the nodes each test caused to be created.
bool MidiRtpApiTests::TestSetup()
{
    m_deviceNodeTracker.Start();
    return true;
}

bool MidiRtpApiTests::TestCleanup()
{
    RemoveCreatedHosts();
    m_deviceNodeTracker.RemoveDeviceNodesCreatedSinceStart();

    return true;
}

_Use_decl_annotations_
winrt::guid MidiRtpApiTests::CreateHost(std::wstring const& name, MidiRtpRemoteClientPolicy const policy)
{
    MidiRtpHostCreationConfig config;

    config.Name(winrt::hstring{ TestHostNamePrefix + name });
    config.RemoteClientPolicy(policy);

    // loopback is enough for these tests, and nothing needs to find them on the network
    config.Advertise(false);

    auto const response = MidiRtpTransportManager::CreateRtpHostAsync(config).get();

    VERIFY_IS_TRUE(response != nullptr);
    VERIFY_IS_TRUE(response.HostId() == config.HostId(), L"the response names the host");

    if (!response.Success())
    {
        Log::Error(String().Format(L"Host creation failed. code=%d message='%s'", static_cast<int32_t>(response.ErrorCode()), response.ErrorMessage().c_str()));
        return winrt::guid{};
    }

    m_createdHosts.push_back(config.HostId());

    return config.HostId();
}

void MidiRtpApiTests::RemoveCreatedHosts()
{
    for (auto const& hostId : m_createdHosts)
    {
        try
        {
            MidiRtpTransportManager::RemoveRtpHostAsync(MidiRtpHostRemovalConfig(hostId)).get();
        }
        catch (...)
        {
            // best effort
        }
    }

    m_createdHosts.clear();
}


void MidiRtpApiTests::TestConstantsMatchRtpMidi()
{
    VERIFY_IS_TRUE(MidiRtpTransportManager::TransportId() == winrt::guid{ L"{54C9B2F6-C235-4000-A675-9F6958A1A4FA}" });

    VERIFY_IS_TRUE(std::wstring{ MidiRtpTransportManager::MidiRtpDnsServiceType() } == L"_apple-midi._udp");
    VERIFY_IS_TRUE(std::wstring{ MidiRtpTransportManager::MidiRtpDnsDomain() } == L"local");
    VERIFY_IS_TRUE(std::wstring{ MidiRtpTransportManager::MidiRtpDnsSdQueryName() } == L"_apple-midi._udp.local");

    VERIFY_IS_TRUE(MidiRtpTransportManager::DefaultHostPort() == 5004);
}

void MidiRtpApiTests::TestHostCreationConfigDefaults()
{
    MidiRtpHostCreationConfig first;
    MidiRtpHostCreationConfig second;

    VERIFY_IS_TRUE(first.HostId() != winrt::guid{}, L"a new config has an id");
    VERIFY_IS_TRUE(first.HostId() != second.HostId(), L"and each one is different");
    VERIFY_IS_TRUE(first.TransportId() == MidiRtpTransportManager::TransportId());

    VERIFY_IS_TRUE(first.Name().empty(), L"this PC's name unless set");
    VERIFY_IS_TRUE(first.ServiceInstanceName().empty());
    VERIFY_IS_TRUE(first.UseAutomaticPortAllocation());
    VERIFY_IS_TRUE(first.ManuallyAssignedPort() == 5004);
    VERIFY_IS_TRUE(first.AllowPortFallback());
    VERIFY_IS_TRUE(first.Advertise());
    VERIFY_IS_TRUE(first.RemoteClientPolicy() == MidiRtpRemoteClientPolicy::AllowAny, L"the same default as Network MIDI 2.0");
    VERIFY_IS_TRUE(first.SendRecoveryJournal());

    MidiRtpClientConnectConfig client;

    VERIFY_IS_TRUE(client.ClientId() != winrt::guid{});
    VERIFY_IS_TRUE(client.MatchCriteria() == nullptr);
    VERIFY_IS_TRUE(client.AutoReconnect());
    VERIFY_IS_TRUE(client.SendRecoveryJournal());

    MidiRtpClientMatchCriteria match;
    VERIFY_IS_TRUE(match.DirectPort() == 5004, L"the port RTP-MIDI devices use unless told otherwise");
}

void MidiRtpApiTests::TestHostCreationConfigJson()
{
    MidiRtpHostCreationConfig config;

    auto host = OnlyEntry(TransportSection(config.ConfigJson()).GetNamedObject(L"create").GetNamedObject(L"hosts"), config.HostId());

    VERIFY_IS_FALSE(host.HasKey(L"name"), L"an empty name is left out, so the service uses this PC's name");
    VERIFY_IS_FALSE(host.HasKey(L"serviceInstanceName"));
    VERIFY_IS_TRUE(std::wstring{ host.GetNamedString(L"port") } == L"auto");
    VERIFY_IS_TRUE(host.GetNamedBoolean(L"allowPortFallback"));
    VERIFY_IS_TRUE(host.GetNamedBoolean(L"advertise"));
    VERIFY_IS_TRUE(host.GetNamedBoolean(L"enabled"));
    VERIFY_IS_TRUE(host.GetNamedBoolean(L"sendRecoveryJournal"));
    VERIFY_IS_TRUE(std::wstring{ host.GetNamedString(L"remoteClientPolicy") } == L"allowAny");

    config.Name(L"  Studio PC  ");
    config.ServiceInstanceName(L"Studio");
    config.UseAutomaticPortAllocation(false);
    config.ManuallyAssignedPort(5010);
    config.Advertise(false);
    config.RemoteClientPolicy(MidiRtpRemoteClientPolicy::RequireApproval);

    host = OnlyEntry(TransportSection(config.ConfigJson()).GetNamedObject(L"create").GetNamedObject(L"hosts"), config.HostId());

    VERIFY_IS_TRUE(std::wstring{ host.GetNamedString(L"name") } == L"Studio PC", L"trimmed");
    VERIFY_IS_TRUE(std::wstring{ host.GetNamedString(L"serviceInstanceName") } == L"Studio");
    VERIFY_IS_TRUE(std::wstring{ host.GetNamedString(L"port") } == L"5010");
    VERIFY_IS_FALSE(host.GetNamedBoolean(L"advertise"));
    VERIFY_IS_TRUE(std::wstring{ host.GetNamedString(L"remoteClientPolicy") } == L"requireApproval");
}

void MidiRtpApiTests::TestClientConnectConfigJson()
{
    MidiRtpClientMatchCriteria advertised;
    advertised.ServiceInstanceName(L"Studio Mac");

    MidiRtpClientConnectConfig config;
    config.MatchCriteria(advertised);
    config.Comment(L"written by the API tests");
    config.CustomEndpointName(L"Mac");

    auto client = OnlyEntry(TransportSection(config.ConfigJson()).GetNamedObject(L"create").GetNamedObject(L"clients"), config.ClientId());

    VERIFY_IS_TRUE(std::wstring{ client.GetNamedString(L"serviceInstanceName") } == L"Studio Mac");
    VERIFY_IS_FALSE(client.HasKey(L"remoteAddress"));
    VERIFY_IS_FALSE(client.HasKey(L"name"), L"an empty name is left out");
    VERIFY_IS_TRUE(std::wstring{ client.GetNamedString(L"_comment") } == L"written by the API tests");
    VERIFY_IS_TRUE(std::wstring{ client.GetNamedString(L"customEndpointName") } == L"Mac");
    VERIFY_IS_TRUE(client.GetNamedBoolean(L"autoReconnect"));
    VERIFY_IS_TRUE(client.GetNamedBoolean(L"enabled"));

    MidiRtpClientMatchCriteria direct;
    direct.DirectHostNameOrIPAddress(L" 192.168.1.20 ");
    direct.DirectPort(5008);

    config.MatchCriteria(direct);
    config.AutoReconnect(false);

    client = OnlyEntry(TransportSection(config.ConfigJson()).GetNamedObject(L"create").GetNamedObject(L"clients"), config.ClientId());

    VERIFY_IS_FALSE(client.HasKey(L"serviceInstanceName"));
    VERIFY_IS_TRUE(std::wstring{ client.GetNamedString(L"remoteAddress") } == L"192.168.1.20", L"trimmed");
    VERIFY_IS_TRUE(client.GetNamedNumber(L"remotePort") == 5008.0);
    VERIFY_IS_FALSE(client.GetNamedBoolean(L"autoReconnect"));
}

void MidiRtpApiTests::TestRemovalConfigJson()
{
    auto const hostId = foundation::GuidHelper::CreateNewGuid();
    auto const clientId = foundation::GuidHelper::CreateNewGuid();

    auto const hostSection = TransportSection(MidiRtpHostRemovalConfig(hostId).ConfigJson());

    VERIFY_IS_FALSE(hostSection.HasKey(L"create"), L"a removal stores nothing");

    auto const hostRemove = hostSection.GetNamedObject(L"remove");
    VERIFY_ARE_EQUAL(OnlyEntry(hostRemove.GetNamedObject(L"hosts"), hostId).Size(), 0u);
    VERIFY_ARE_EQUAL(OnlyEntry(hostRemove.GetNamedObject(L"remoteClientDecisions"), hostId).Size(), 0u, L"the host's saved decisions go with it");

    auto const clientRemove = TransportSection(MidiRtpClientDisconnectConfig(clientId).ConfigJson()).GetNamedObject(L"remove");
    VERIFY_ARE_EQUAL(OnlyEntry(clientRemove.GetNamedObject(L"clients"), clientId).Size(), 0u);
}

void MidiRtpApiTests::TestKnownClientsConfigWritesBothLists()
{
    MidiRtpHostCreationConfig creation;
    MidiRtpHostKnownClientsConfig known(creation.HostId());

    auto entry = OnlyEntry(TransportSection(known.ConfigJson()).GetNamedObject(L"create").GetNamedObject(L"remoteClientDecisions"), creation.HostId());

    // the file merge replaces lists, so an empty list has to be written to clear a saved one
    VERIFY_ARE_EQUAL(entry.GetNamedArray(L"allowedClients").Size(), 0u);
    VERIFY_ARE_EQUAL(entry.GetNamedArray(L"deniedClients").Size(), 0u);

    known.KnownClients().Append(MidiRtpKnownRemoteClient(L"Pete's iPad", true));
    known.KnownClients().Append(MidiRtpKnownRemoteClient(L"Unknown Laptop", false));
    known.KnownClients().Append(MidiRtpKnownRemoteClient(L"", true));

    auto const decisions = TransportSection(known.ConfigJson()).GetNamedObject(L"create").GetNamedObject(L"remoteClientDecisions");
    entry = OnlyEntry(decisions, creation.HostId());

    auto const allowed = entry.GetNamedArray(L"allowedClients");
    auto const denied = entry.GetNamedArray(L"deniedClients");

    VERIFY_ARE_EQUAL(allowed.Size(), 1u, L"a remote with no name is left out");
    VERIFY_ARE_EQUAL(denied.Size(), 1u);
    VERIFY_IS_TRUE(std::wstring{ allowed.GetObjectAt(0).GetNamedString(L"remoteName") } == L"Pete's iPad");
    VERIFY_IS_TRUE(std::wstring{ denied.GetObjectAt(0).GetNamedString(L"remoteName") } == L"Unknown Laptop");

    // The file merge matches these keys as text, so the decisions only land beside their host
    // if both are written the same way. This is the one place the text form matters.
    auto const hostKey = OnlyKey(TransportSection(creation.ConfigJson()).GetNamedObject(L"create").GetNamedObject(L"hosts"));
    VERIFY_IS_TRUE(OnlyKey(decisions) == hostKey, L"decisions and host share the same key text");
}

void MidiRtpApiTests::TestCommandConfigsHaveNothingToSave()
{
    auto const hostId = foundation::GuidHelper::CreateNewGuid();

    MidiRtpRemoteClientApprovalConfig approval(hostId, L"Someone", true, true);
    VERIFY_IS_TRUE(approval.HostId() == hostId);
    VERIFY_IS_TRUE(std::wstring{ approval.RemoteClientName() } == L"Someone");
    VERIFY_IS_TRUE(approval.Approve());
    VERIFY_IS_TRUE(approval.ScopeIsThisRequestOnly());
    VERIFY_ARE_EQUAL(approval.ConfigJson().Size(), 0u);

    MidiRtpRemoteClientDisconnectConfig disconnect(hostId, 7);
    VERIFY_ARE_EQUAL(disconnect.ConnectionId(), 7u);
    VERIFY_ARE_EQUAL(disconnect.ConfigJson().Size(), 0u);

    MidiRtpRemoteClientForgetConfig forget(hostId, L"Someone");
    VERIFY_IS_TRUE(std::wstring{ forget.RemoteClientName() } == L"Someone");
    VERIFY_ARE_EQUAL(forget.ConfigJson().Size(), 0u);
}

void MidiRtpApiTests::TestNullConfigsAreRejected()
{
    VERIFY_IS_TRUE(MidiRtpTransportManager::CreateRtpHostAsync(nullptr).get().ErrorCode() == MidiRtpHostCreationErrorCode::InvalidArgument);
    VERIFY_IS_TRUE(MidiRtpTransportManager::RemoveRtpHostAsync(nullptr).get().ErrorCode() == MidiRtpHostRemovalErrorCode::InvalidArgument);
    VERIFY_IS_TRUE(MidiRtpTransportManager::ConnectRtpClientAsync(nullptr).get().ErrorCode() == MidiRtpClientConnectErrorCode::InvalidArgument);
    VERIFY_IS_TRUE(MidiRtpTransportManager::DisconnectRtpClientAsync(nullptr).get().ErrorCode() == MidiRtpClientDisconnectErrorCode::InvalidArgument);
    VERIFY_IS_TRUE(MidiRtpTransportManager::ApproveOrDenyRemoteClientConnectRequestAsync(nullptr).get().ErrorCode() == MidiRtpRemoteClientApprovalErrorCode::InvalidArgument);
    VERIFY_IS_TRUE(MidiRtpTransportManager::DisconnectRemoteClientAsync(nullptr).get().ErrorCode() == MidiRtpRemoteClientDisconnectErrorCode::InvalidArgument);
    VERIFY_IS_TRUE(MidiRtpTransportManager::ForgetRemoteClientAsync(nullptr).get().ErrorCode() == MidiRtpRemoteClientForgetErrorCode::InvalidArgument);
}


void MidiRtpApiTests::TestCreateStopStartRemoveHost()
{
    SKIP_IF_NO_RTP_TRANSPORT();

    auto const hostId = CreateHost(L"Lifecycle", MidiRtpRemoteClientPolicy::AllowAny);
    VERIFY_IS_TRUE(hostId != winrt::guid{});

    // creation only returns once the host is up
    auto host = FindHost(hostId);
    VERIFY_IS_TRUE(host != nullptr);
    VERIFY_IS_TRUE(host.HasStarted());
    VERIFY_IS_TRUE(host.IsEnabled());
    VERIFY_IS_TRUE(std::wstring{ host.Name() } == std::wstring{ TestHostNamePrefix } + L"Lifecycle");
    VERIFY_IS_TRUE(std::wstring{ host.ConfiguredPort() } == L"auto");
    VERIFY_IS_TRUE(host.ActualPort() != 0, L"a running host reports the port it has");
    VERIFY_IS_FALSE(host.Advertise());
    VERIFY_IS_TRUE(host.RemoteClientPolicy() == MidiRtpRemoteClientPolicy::AllowAny);
    VERIFY_ARE_EQUAL(host.Connections().Size(), 0u);
    VERIFY_ARE_EQUAL(host.LastErrorCode(), 0);

    auto const stopped = MidiRtpTransportManager::StopRtpHostAsync(hostId).get();
    VERIFY_IS_TRUE(stopped.Success(), stopped.ErrorMessage().c_str());
    VERIFY_IS_TRUE(stopped.HostId() == hostId);
    VERIFY_IS_TRUE(RtpMidiTest::WaitFor([&]() { auto h = FindHost(hostId); return h != nullptr && !h.HasStarted(); }, ServiceWaitMilliseconds), L"a stopped host keeps its entry");

    auto const started = MidiRtpTransportManager::StartRtpHostAsync(hostId).get();
    VERIFY_IS_TRUE(started.Success(), started.ErrorMessage().c_str());
    VERIFY_IS_TRUE(RtpMidiTest::WaitFor([&]() { auto h = FindHost(hostId); return h != nullptr && h.HasStarted(); }, ServiceWaitMilliseconds), L"and starts again");

    auto const removed = MidiRtpTransportManager::RemoveRtpHostAsync(MidiRtpHostRemovalConfig(hostId)).get();
    VERIFY_IS_TRUE(removed.Success(), removed.ErrorMessage().c_str());
    VERIFY_IS_TRUE(removed.HostId() == hostId);
    VERIFY_IS_TRUE(RtpMidiTest::WaitFor([&]() { return FindHost(hostId) == nullptr; }, ServiceWaitMilliseconds), L"a removed host is gone");

    m_createdHosts.clear();
}

void MidiRtpApiTests::TestHostWithNoNameUsesThisPcName()
{
    SKIP_IF_NO_RTP_TRANSPORT();

    MidiRtpHostCreationConfig config;
    config.Advertise(false);

    auto const response = MidiRtpTransportManager::CreateRtpHostAsync(config).get();
    if (response.Success()) m_createdHosts.push_back(config.HostId());

    VERIFY_IS_TRUE(response.Success(), response.ErrorMessage().c_str());

    auto const host = FindHost(config.HostId());
    VERIFY_IS_TRUE(host != nullptr);

    auto const expected = ThisPcName();
    VERIFY_IS_FALSE(expected.empty());
    VERIFY_IS_TRUE(std::wstring{ host.Name() } == expected, String().Format(L"host name '%s', PC name '%s'", host.Name().c_str(), expected.c_str()));
}

void MidiRtpApiTests::TestEntriesWhichDoNotExistAreReported()
{
    SKIP_IF_NO_RTP_TRANSPORT();

    auto const unknown = foundation::GuidHelper::CreateNewGuid();

    auto const removed = MidiRtpTransportManager::RemoveRtpHostAsync(MidiRtpHostRemovalConfig(unknown)).get();
    VERIFY_IS_FALSE(removed.Success());
    VERIFY_IS_TRUE(removed.ErrorCode() == MidiRtpHostRemovalErrorCode::HostNotFound, removed.ErrorMessage().c_str());
    VERIFY_IS_FALSE(removed.ErrorMessage().empty(), L"with a message to show");

    VERIFY_IS_TRUE(MidiRtpTransportManager::StopRtpHostAsync(unknown).get().ErrorCode() == MidiRtpHostUpdateErrorCode::HostNotFound);
    VERIFY_IS_TRUE(MidiRtpTransportManager::ReconnectRtpClientAsync(unknown).get().ErrorCode() == MidiRtpClientConnectErrorCode::ClientNotFound);
    VERIFY_IS_TRUE(MidiRtpTransportManager::DisconnectRtpClientAsync(MidiRtpClientDisconnectConfig(unknown)).get().ErrorCode() == MidiRtpClientDisconnectErrorCode::ClientNotFound);

    VERIFY_IS_TRUE(MidiRtpTransportManager::ApproveOrDenyRemoteClientConnectRequestAsync(
        MidiRtpRemoteClientApprovalConfig(unknown, L"Someone", true, false)).get().ErrorCode() == MidiRtpRemoteClientApprovalErrorCode::HostNotFound);

    VERIFY_IS_TRUE(MidiRtpTransportManager::ForgetRemoteClientAsync(
        MidiRtpRemoteClientForgetConfig(unknown, L"Someone")).get().ErrorCode() == MidiRtpRemoteClientForgetErrorCode::HostNotFound);

    VERIFY_IS_TRUE(MidiRtpTransportManager::DisconnectRemoteClientAsync(
        MidiRtpRemoteClientDisconnectConfig(unknown, 1)).get().ErrorCode() == MidiRtpRemoteClientDisconnectErrorCode::HostNotFound);

    // a host which exists, and a connection which does not
    auto const hostId = CreateHost(L"No Connections", MidiRtpRemoteClientPolicy::AllowAny);
    VERIFY_IS_TRUE(hostId != winrt::guid{});

    auto const disconnected = MidiRtpTransportManager::DisconnectRemoteClientAsync(MidiRtpRemoteClientDisconnectConfig(hostId, 999999)).get();
    VERIFY_IS_TRUE(disconnected.ErrorCode() == MidiRtpRemoteClientDisconnectErrorCode::ConnectionNotFound, disconnected.ErrorMessage().c_str());

    // approving once needs a remote which is waiting
    auto const approved = MidiRtpTransportManager::ApproveOrDenyRemoteClientConnectRequestAsync(MidiRtpRemoteClientApprovalConfig(hostId, L"Nobody Waiting", true, true)).get();
    VERIFY_IS_TRUE(approved.ErrorCode() == MidiRtpRemoteClientApprovalErrorCode::PendingRemoteClientNotFound, approved.ErrorMessage().c_str());
}

void MidiRtpApiTests::TestClientWithNoRemoteIsRejected()
{
    SKIP_IF_NO_RTP_TRANSPORT();

    MidiRtpClientConnectConfig config;

    auto const response = MidiRtpTransportManager::ConnectRtpClientAsync(config).get();
    VERIFY_IS_FALSE(response.Success());
    VERIFY_IS_TRUE(response.ClientId() == config.ClientId());
    VERIFY_IS_TRUE(response.ErrorCode() == MidiRtpClientConnectErrorCode::InvalidOrMissingMatchCriteria, response.ErrorMessage().c_str());

    bool listed{ false };
    for (auto const& client : MidiRtpTransportManager::GetConfiguredClients())
    {
        if (client.ClientId() == config.ClientId()) listed = true;
    }

    VERIFY_IS_FALSE(listed, L"a refused client is not kept");
}

void MidiRtpApiTests::TestListsAreReadable()
{
    SKIP_IF_NO_RTP_TRANSPORT();

    // a settings app polls these, so they must always answer with a list
    for (int i = 0; i < 20; i++)
    {
        VERIFY_IS_TRUE(MidiRtpTransportManager::GetConfiguredHosts() != nullptr);
        VERIFY_IS_TRUE(MidiRtpTransportManager::GetConfiguredClients() != nullptr);
        VERIFY_IS_TRUE(MidiRtpTransportManager::GetPendingRemoteClients() != nullptr);
    }

    auto const advertised = MidiRtpTransportManager::GetAdvertisedHosts();
    VERIFY_IS_TRUE(advertised != nullptr);

    for (auto const& host : advertised)
    {
        Log::Comment(String().Format(L"advertised: '%s' on %s:%u%s", host.ServiceInstanceName().c_str(), host.HostName().c_str(),
            static_cast<uint32_t>(host.Port()), host.IsThisPc() ? L" (this PC)" : L""));

        VERIFY_ARE_EQUAL(host.IPAddresses().Size(), host.IPv4Addresses().Size() + host.IPv6Addresses().Size());
    }
}

void MidiRtpApiTests::TestApproveOnceThenDisconnectTheRemote()
{
    SKIP_IF_NO_RTP_TRANSPORT();

    auto const hostId = CreateHost(L"Approval", MidiRtpRemoteClientPolicy::RequireApproval);
    VERIFY_IS_TRUE(hostId != winrt::guid{});

    auto const port = FindHost(hostId).ActualPort();
    std::wstring const remoteName{ L"SDK Waiting Peer" };

    RtpMidiTest::Peer remote("SDK Waiting Peer", false);
    VERIFY_IS_TRUE(remote.Start(), L"the test remote binds");
    remote.Invite(port);

    MidiRtpPendingRemoteClient pending{ nullptr };
    VERIFY_IS_TRUE(RtpMidiTest::WaitFor([&]() { pending = FindPending(hostId, remoteName); return pending != nullptr; }, RemoteAskWaitMilliseconds), L"the remote is listed as waiting");

    VERIFY_IS_FALSE(pending.RemoteAddress().empty(), L"with where it asked from");
    VERIFY_IS_FALSE(pending.HostServiceInstanceName().empty(), L"and the host it asked for");
    VERIFY_IS_FALSE(pending.IsApproved());

    auto const now = winrt::clock::now();
    VERIFY_IS_TRUE(pending.RequestTime() <= now && now - pending.RequestTime() < std::chrono::minutes(1), L"and when it asked");

    VERIFY_IS_TRUE(remote.ConnectedCount() == 0, L"a waiting remote is not let in");

    auto const approval = MidiRtpTransportManager::ApproveOrDenyRemoteClientConnectRequestAsync(MidiRtpRemoteClientApprovalConfig(hostId, remoteName, true, true)).get();
    VERIFY_IS_TRUE(approval.Success(), approval.ErrorMessage().c_str());
    VERIFY_IS_TRUE(approval.HostId() == hostId);
    VERIFY_IS_TRUE(std::wstring{ approval.RemoteClientName() } == remoteName);

    VERIFY_IS_TRUE(RtpMidiTest::WaitFor([&]() { return remote.ConnectedCount() == 1; }, RemoteAskWaitMilliseconds), L"its next ask gets in");

    MidiRtpConnection connection{ nullptr };
    VERIFY_IS_TRUE(RtpMidiTest::WaitFor([&]() { connection = FindConnection(hostId, remoteName); return connection != nullptr && connection.IsConnected() && !connection.EndpointDeviceId().empty(); }, ServiceWaitMilliseconds), L"the host lists the connection and its endpoint");

    VERIFY_IS_TRUE(connection.ConnectionId() != 0);
    VERIFY_IS_FALSE(connection.ThisPcInvited(), L"the remote asked, this PC did not");
    VERIFY_IS_FALSE(connection.EndpointDeviceId().empty(), L"and it has an endpoint");
    VERIFY_IS_TRUE(connection.LocalPort() == port);

    VERIFY_IS_TRUE(FindPending(hostId, remoteName) == nullptr, L"it is no longer waiting");
    VERIFY_IS_FALSE(IsKnown(hostId, remoteName, true), L"an approval for one connection is not kept");

    auto const disconnected = MidiRtpTransportManager::DisconnectRemoteClientAsync(MidiRtpRemoteClientDisconnectConfig(hostId, connection.ConnectionId())).get();
    VERIFY_IS_TRUE(disconnected.Success(), disconnected.ErrorMessage().c_str());

    VERIFY_IS_TRUE(RtpMidiTest::WaitFor([&]() { return !remote.Ended().empty(); }, ServiceWaitMilliseconds), L"the remote is told the connection ended");
    VERIFY_IS_TRUE(RtpMidiTest::WaitFor([&]() { return FindConnection(hostId, remoteName) == nullptr; }, ServiceWaitMilliseconds), L"and the host no longer lists it");

    auto const again = MidiRtpTransportManager::DisconnectRemoteClientAsync(MidiRtpRemoteClientDisconnectConfig(hostId, connection.ConnectionId())).get();
    VERIFY_IS_TRUE(again.ErrorCode() == MidiRtpRemoteClientDisconnectErrorCode::ConnectionNotFound, again.ErrorMessage().c_str());

    remote.Stop();
}

void MidiRtpApiTests::TestDenyAlwaysIsAKnownClientUntilForgotten()
{
    SKIP_IF_NO_RTP_TRANSPORT();

    auto const hostId = CreateHost(L"Denial", MidiRtpRemoteClientPolicy::RequireApproval);
    VERIFY_IS_TRUE(hostId != winrt::guid{});

    std::wstring const remoteName{ L"SDK Refused Peer" };

    RtpMidiTest::Peer remote("SDK Refused Peer", false);
    VERIFY_IS_TRUE(remote.Start(), L"the test remote binds");
    remote.Invite(FindHost(hostId).ActualPort());

    VERIFY_IS_TRUE(RtpMidiTest::WaitFor([&]() { return FindPending(hostId, remoteName) != nullptr; }, RemoteAskWaitMilliseconds), L"the remote waits");

    auto const denial = MidiRtpTransportManager::ApproveOrDenyRemoteClientConnectRequestAsync(MidiRtpRemoteClientApprovalConfig(hostId, remoteName, false, false)).get();
    VERIFY_IS_TRUE(denial.Success(), denial.ErrorMessage().c_str());

    VERIFY_IS_TRUE(FindPending(hostId, remoteName) == nullptr, L"a decided remote is no longer waiting");
    VERIFY_IS_TRUE(IsKnown(hostId, remoteName, false), L"the host lists the refusal, so an app can save it");
    VERIFY_IS_TRUE(RtpMidiTest::WaitFor([&]() { return !remote.Ended().empty(); }, RemoteAskWaitMilliseconds), L"its next ask is refused");
    VERIFY_IS_TRUE(remote.ConnectedCount() == 0);

    auto const forgotten = MidiRtpTransportManager::ForgetRemoteClientAsync(MidiRtpRemoteClientForgetConfig(hostId, remoteName)).get();
    VERIFY_IS_TRUE(forgotten.Success(), forgotten.ErrorMessage().c_str());
    VERIFY_IS_TRUE(std::wstring{ forgotten.RemoteClientName() } == remoteName);
    VERIFY_IS_FALSE(IsKnown(hostId, remoteName, false), L"a forgotten remote is not listed");

    remote.Stop();
}


// ------------------------------------------------------------------------------------
// What is saved in the configuration file. Saved only, never sent, so the running service
// does not see any of these entries.
// ------------------------------------------------------------------------------------

namespace
{
    namespace svc = winrt::Windows::Devices::Midi2::ServiceConfig;

    bool ConfigFileRegisteredOrSkip()
    {
        if (svc::MidiServiceTransportPluginConfigManager::ConfigFilePath().empty())
        {
            Log::Result(TestResults::Skipped, L"No configuration file is registered on this PC, so nothing can be saved.");
            return false;
        }

        return true;
    }

    MidiRtpSavedHost FindSavedHost(_In_ winrt::guid const& hostId)
    {
        for (auto const& host : MidiRtpTransportManager::GetSavedHosts())
        {
            if (host != nullptr && host.HostId() == hostId) return host;
        }

        return nullptr;
    }

    MidiRtpSavedClient FindSavedClient(_In_ winrt::guid const& clientId)
    {
        for (auto const& client : MidiRtpTransportManager::GetSavedClients())
        {
            if (client != nullptr && client.ClientId() == clientId) return client;
        }

        return nullptr;
    }

    void VerifySaved(_In_ svc::MidiServiceConfigSaveResponse const& response, _In_ PCWSTR description)
    {
        VERIFY_IS_TRUE(response != nullptr);

        if (!response.Success())
        {
            Log::Comment(String().Format(L"Save failed: result=%d '%s'", static_cast<int>(response.Result()), response.ErrorMessage().c_str()));
        }

        VERIFY_IS_TRUE(response.Success(), description);
    }

    // for cleanup, so these never throw
    void RemoveSavedHost(_In_ winrt::guid const& hostId) noexcept
    {
        try
        {
            svc::MidiServiceTransportPluginConfigManager::SaveUpdate(MidiRtpHostRemovalConfig(hostId));
        }
        catch (...)
        {
        }
    }

    void RemoveSavedClient(_In_ winrt::guid const& clientId) noexcept
    {
        try
        {
            svc::MidiServiceTransportPluginConfigManager::SaveUpdate(MidiRtpClientDisconnectConfig(clientId));
        }
        catch (...)
        {
        }
    }
}


void MidiRtpApiTests::TestSavedHostFollowsSavedChanges()
{
    if (!ConfigFileRegisteredOrSkip()) return;

    MidiRtpHostCreationConfig config;
    config.Name(winrt::hstring{ std::wstring{ TestHostNamePrefix } + L"Saved" });
    config.ServiceInstanceName(L"MidiApiTest Saved Host");
    config.UseAutomaticPortAllocation(false);
    config.ManuallyAssignedPort(5510);
    config.AllowPortFallback(false);
    config.Advertise(false);
    config.RemoteClientPolicy(MidiRtpRemoteClientPolicy::RequireApproval);
    config.SendRecoveryJournal(false);

    auto const hostId = config.HostId();

    VERIFY_IS_TRUE(FindSavedHost(hostId) == nullptr, L"not saved to begin with");

    auto removeEntry = wil::scope_exit([&] { RemoveSavedHost(hostId); });

    VerifySaved(svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config), L"saving the host works");

    auto saved = FindSavedHost(hostId);

    VERIFY_IS_TRUE(saved != nullptr, L"it is listed once saved");
    VERIFY_IS_TRUE(saved.Name() == config.Name(), L"with its name");
    VERIFY_IS_TRUE(saved.ServiceInstanceName() == config.ServiceInstanceName(), L"its service instance name");
    VERIFY_IS_TRUE(saved.IsEnabled());
    VERIFY_IS_FALSE(saved.UseAutomaticPortAllocation());
    VERIFY_ARE_EQUAL(saved.ManuallyAssignedPort(), (uint16_t)5510);
    VERIFY_IS_FALSE(saved.AllowPortFallback());
    VERIFY_IS_FALSE(saved.Advertise());
    VERIFY_IS_TRUE(saved.RemoteClientPolicy() == MidiRtpRemoteClientPolicy::RequireApproval, L"its policy");
    VERIFY_IS_FALSE(saved.SendRecoveryJournal());
    VERIFY_ARE_EQUAL(saved.KnownRemoteClients().Size(), 0u);

    MidiRtpHostKnownClientsConfig knownClients(hostId);
    knownClients.KnownClients().Append(MidiRtpKnownRemoteClient(L"MidiApiTest Allowed", true));
    knownClients.KnownClients().Append(MidiRtpKnownRemoteClient(L"MidiApiTest Denied", false));

    VerifySaved(svc::MidiServiceTransportPluginConfigManager::SaveUpdate(knownClients), L"saving the host's decisions works");

    saved = FindSavedHost(hostId);

    VERIFY_IS_TRUE(saved != nullptr);
    VERIFY_ARE_EQUAL(saved.KnownRemoteClients().Size(), 2u);

    for (auto const& client : saved.KnownRemoteClients())
    {
        VERIFY_IS_TRUE(client.IsAllowed() == (std::wstring{ client.RemoteClientName() } == L"MidiApiTest Allowed"), L"each decision is saved as made");
    }

    VerifySaved(svc::MidiServiceTransportPluginConfigManager::SaveUpdate(MidiRtpHostRemovalConfig(hostId)), L"removing it works");

    VERIFY_IS_TRUE(FindSavedHost(hostId) == nullptr, L"a removed host is no longer listed");
}


// The decisions only mean something while their host is saved
void MidiRtpApiTests::TestSavingKnownClientsForUnsavedHostIsRefused()
{
    if (!ConfigFileRegisteredOrSkip()) return;

    auto const hostId = foundation::GuidHelper::CreateNewGuid();

    auto removeEntry = wil::scope_exit([&] { RemoveSavedHost(hostId); });

    MidiRtpHostKnownClientsConfig knownClients(hostId);
    knownClients.KnownClients().Append(MidiRtpKnownRemoteClient(L"MidiApiTest Allowed", true));

    auto const response = svc::MidiServiceTransportPluginConfigManager::SaveUpdate(knownClients);

    VERIFY_IS_TRUE(response != nullptr);
    VERIFY_IS_FALSE(response.Success(), L"decisions for a host which is not saved are not saved");
    VERIFY_IS_TRUE(response.Result() == svc::MidiServiceConfigSaveResult::ErrorEntryNotSaved, L"and it says why");
}


void MidiRtpApiTests::TestSavedClientFollowsSavedChanges()
{
    if (!ConfigFileRegisteredOrSkip()) return;

    MidiRtpClientMatchCriteria match;
    match.DirectHostNameOrIPAddress(L"192.0.2.20");
    match.DirectPort(5520);

    MidiRtpClientConnectConfig connect;
    connect.Comment(L"MidiApiTest saved client");
    connect.Name(L"MidiApiTest Local");
    connect.CustomEndpointName(L"MidiApiTest Custom");
    connect.MatchCriteria(match);
    connect.AutoReconnect(false);
    connect.SendRecoveryJournal(false);

    auto const clientId = connect.ClientId();

    VERIFY_IS_TRUE(FindSavedClient(clientId) == nullptr, L"not saved to begin with");

    auto removeEntry = wil::scope_exit([&] { RemoveSavedClient(clientId); });

    VerifySaved(svc::MidiServiceTransportPluginConfigManager::SaveUpdate(connect), L"saving the client works");

    auto const saved = FindSavedClient(clientId);

    VERIFY_IS_TRUE(saved != nullptr, L"it is listed once saved");
    VERIFY_IS_TRUE(saved.Comment() == connect.Comment(), L"with its comment");
    VERIFY_IS_TRUE(saved.Name() == connect.Name(), L"the name this PC gives");
    VERIFY_IS_TRUE(saved.CustomEndpointName() == connect.CustomEndpointName(), L"its custom endpoint name");
    VERIFY_IS_TRUE(saved.MatchCriteria().DirectHostNameOrIPAddress() == match.DirectHostNameOrIPAddress(), L"its address");
    VERIFY_ARE_EQUAL(saved.MatchCriteria().DirectPort(), (uint16_t)5520);
    VERIFY_IS_TRUE(saved.MatchCriteria().ServiceInstanceName().empty());
    VERIFY_IS_FALSE(saved.AutoReconnect());
    VERIFY_IS_FALSE(saved.SendRecoveryJournal());
    VERIFY_IS_TRUE(saved.IsEnabled());

    VerifySaved(svc::MidiServiceTransportPluginConfigManager::SaveUpdate(MidiRtpClientDisconnectConfig(clientId)), L"saving a disconnect works");

    VERIFY_IS_TRUE(FindSavedClient(clientId) == nullptr, L"and forgets the saved client");
}
