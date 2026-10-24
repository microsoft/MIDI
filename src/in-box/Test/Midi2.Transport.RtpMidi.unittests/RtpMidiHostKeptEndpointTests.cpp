// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// A host which keeps a remote's endpoint while the remote is away. Asked for by a customer who
// wants Windows to be the host for their device, so the device connects to this PC.
// ============================================================================

#include "pch.h"
#include "RtpMidiTransportTests.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using RtpMidiTest::Command;
using RtpMidiTest::FindConnection;
using RtpMidiTest::IsSuccess;
using RtpMidiTest::Peer;
using RtpMidiTest::SameText;
using RtpMidiTest::WaitFor;

namespace json = winrt::Windows::Data::Json;

namespace
{
    // removes what a test made however it ends
    template <typename Action>
    class Cleanup
    {
    public:
        explicit Cleanup(_In_ Action action) : m_action(std::move(action)) {}
        ~Cleanup() { try { m_action(); } catch (...) {} }

        Cleanup(Cleanup const&) = delete;
        Cleanup& operator=(Cleanup const&) = delete;

    private:
        Action m_action;
    };

    std::wstring const AlwaysRemoteName{ L"Harness Always Remote" };

    bool Contains(_In_ std::vector<uint32_t> const& words, _In_ uint32_t const word)
    {
        return std::find(words.begin(), words.end(), word) != words.end();
    }

    bool ContainsSequence(_In_ std::vector<uint8_t> const& haystack, _In_ std::vector<uint8_t> const& needle)
    {
        return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end()) != haystack.end();
    }

    // one host's remotes with their own settings, as the create section a configuration change sends
    std::wstring SettingsSection(_In_ std::wstring const& hostId, _In_ std::wstring const& remoteClients)
    {
        return L"{\"create\":{\"remoteClientSettings\":{\"" + hostId + L"\":{\"remoteClients\":[" + remoteClients + L"]}}}}";
    }

    std::wstring Kept(_In_ std::wstring const& remoteName, _In_ std::wstring const& offlineRetention)
    {
        return L"{\"remoteName\":\"" + remoteName + L"\",\"offlineRetention\":" + offlineRetention + L"}";
    }

    std::vector<RtpMidiTest::ActivatedEndpoint> Live(_In_ RtpMidiTest::MockDeviceManager& deviceManager, _In_ std::wstring const& endpointName)
    {
        std::vector<RtpMidiTest::ActivatedEndpoint> live;

        for (auto const& endpoint : deviceManager.Endpoints())
        {
            if (!endpoint.Removed && endpoint.EndpointName == endpointName) live.push_back(endpoint);
        }

        return live;
    }

    size_t Created(_In_ RtpMidiTest::MockDeviceManager& deviceManager, _In_ std::wstring const& endpointName)
    {
        auto const endpoints = deviceManager.Endpoints();
        return static_cast<size_t>(std::count_if(endpoints.begin(), endpoints.end(),
            [&](RtpMidiTest::ActivatedEndpoint const& endpoint) { return endpoint.EndpointName == endpointName; }));
    }

    // the remote's own settings as the host's status lists them
    json::JsonObject SettingsStatus(_In_ json::JsonObject const& host, _In_ std::wstring const& remoteName)
    {
        if (host == nullptr || !host.HasKey(L"remoteClientSettings")) return nullptr;

        auto const list = host.GetNamedArray(L"remoteClientSettings");

        for (uint32_t i = 0; i < list.Size(); i++)
        {
            auto const entry = list.GetObjectAt(i);
            if (SameText(std::wstring{ entry.GetNamedString(L"remoteName", L"") }, remoteName)) return entry;
        }

        return nullptr;
    }

    std::wstring KeptEndpointId(_In_ json::JsonObject const& host, _In_ std::wstring const& remoteName)
    {
        auto const status = SettingsStatus(host, remoteName);
        return status == nullptr ? std::wstring{} : std::wstring{ status.GetNamedString(L"endpointDeviceId", L"") };
    }
}


void RtpMidiTransportTests::DefineKeptHostRemoteBeforeStart()
{
    // the shared host, which is already defined, keeps this remote's endpoint always
    VERIFY_IS_TRUE(IsSuccess(Send(SettingsSection(m_hostId, Kept(AlwaysRemoteName, L"\"always\"")))),
        L"a remote whose endpoint the host always keeps is defined before the endpoint manager starts");
}

void RtpMidiTransportTests::NoteKeptHostEndpointAfterStart()
{
    auto const live = Live(*m_deviceManager, AlwaysRemoteName);
    if (live.size() == 1) m_keptHostEndpointAtStart = live.front().InterfaceId;
}


void RtpMidiTransportTests::TestHostKeptEndpointIsThereBeforeTheRemote()
{
    Cleanup cleanup([&]() { Send(SettingsSection(m_hostId, L"")); });

    VERIFY_IS_FALSE(m_keptHostEndpointAtStart.empty(), L"the endpoint was there when the endpoint manager finished starting");

    auto live = Live(*m_deviceManager, AlwaysRemoteName);
    VERIFY_IS_TRUE(live.size() == 1, L"it is still there, once");
    VERIFY_IS_TRUE(SameText(live.front().InterfaceId, m_keptHostEndpointAtStart), L"and it is the same endpoint");
    VERIFY_IS_TRUE(live.front().PropertyCount > 0, L"with its MIDI 1.0 port properties");

    auto const status = SettingsStatus(FindHost(m_hostId), AlwaysRemoteName);
    VERIFY_IS_TRUE(status != nullptr, L"the remote's settings are listed");
    VERIFY_IS_TRUE(std::wstring{ status.GetNamedString(L"offlineRetention", L"") } == L"always", L"status: offline retention");
    VERIFY_IS_TRUE(SameText(std::wstring{ status.GetNamedString(L"endpointDeviceId", L"") }, m_keptHostEndpointAtStart), L"status: the kept endpoint");

    RtpMidiTest::OpenedEndpoint opened(m_transport, m_keptHostEndpointAtStart, 81);
    VERIFY_IS_TRUE(opened.IsOpen(), L"an app opens it before the remote has ever connected");

    uint32_t const whileAway{ 0x20904012 };
    VERIFY_ARE_EQUAL(opened.Send(&whileAway, sizeof(whileAway)), S_OK, L"what it sends while the remote is away is accepted and dropped");

    {
        Peer remote("Harness Always Remote", false);
        VERIFY_IS_TRUE(remote.Start(), L"the remote binds a loopback port pair");
        remote.Invite(m_hostPort);

        VERIFY_IS_TRUE(WaitFor([&]() { return remote.ConnectedCount() == 1; }, 5000), L"the remote connects to the host");
        VERIFY_IS_TRUE(WaitFor([&]() { return KeptEndpointId(FindHost(m_hostId), AlwaysRemoteName).empty(); }, 3000),
            L"status: no endpoint is kept while it is connected");

        VERIFY_IS_TRUE(Created(*m_deviceManager, AlwaysRemoteName) == 1, L"no second endpoint was made for the connection");

        remote.Send({ 0x92, 0x33, 0x40 });
        VERIFY_IS_TRUE(WaitFor([&]() { return Contains(opened.Received().Words(), 0x20923340); }, 3000),
            L"the app which opened it before the connection hears the remote");

        uint32_t const noteOn{ 0x20914051 };
        VERIFY_SUCCEEDED(opened.Send(&noteOn, sizeof(noteOn)), L"the app sends");
        VERIFY_IS_TRUE(WaitFor([&]() { return ContainsSequence(remote.Received(), { 0x91, 0x40, 0x51 }); }, 3000), L"the remote receives it");
        VERIFY_IS_FALSE(ContainsSequence(remote.Received(), { 0x90, 0x40, 0x12 }), L"what was sent while it was away never arrives");

        remote.EndAll();
    }

    VERIFY_IS_TRUE(WaitFor([&]() { return SameText(KeptEndpointId(FindHost(m_hostId), AlwaysRemoteName), m_keptHostEndpointAtStart); }, 3000),
        L"status: the endpoint is kept again once the remote leaves");
    VERIFY_IS_TRUE(Live(*m_deviceManager, AlwaysRemoteName).size() == 1, L"it is still there");

    uint32_t const afterLeaving{ 0x20904013 };
    VERIFY_ARE_EQUAL(opened.Send(&afterLeaving, sizeof(afterLeaving)), S_OK, L"what the app sends now is dropped too");

    VERIFY_IS_TRUE(IsSuccess(Send(SettingsSection(m_hostId, L""))), L"the remote goes back to the host's settings");
    VERIFY_IS_TRUE(WaitFor([&]() { return Live(*m_deviceManager, AlwaysRemoteName).empty(); }, 3000), L"and its kept endpoint goes");
}


void RtpMidiTransportTests::TestHostKeepsAnEndpointChosenWhileConnected()
{
    std::wstring const remoteName{ L"Harness Timed Remote" };

    uint16_t port{ 0 };
    auto const hostId = CreateHost(L"\"name\":\"Timed Keeping Host\"", port);
    Cleanup removeHost([&]() { RemoveHost(hostId); });

    RtpMidiTest::ActivatedEndpoint endpoint{};

    {
        Peer remote("Harness Timed Remote", false);
        VERIFY_IS_TRUE(remote.Start(), L"the remote binds a loopback port pair");
        remote.Invite(port);

        VERIFY_IS_TRUE(WaitFor([&]() { return remote.ConnectedCount() == 1 && Live(*m_deviceManager, remoteName).size() == 1; }, 5000),
            L"the remote connects, with nothing set for it, and gets an endpoint");

        endpoint = Live(*m_deviceManager, remoteName).front();

        // decided after the remote connected: read when it leaves
        VERIFY_IS_TRUE(IsSuccess(Send(SettingsSection(hostId, Kept(remoteName, L"2")))), L"the remote is given two seconds of offline retention");

        auto const status = SettingsStatus(FindHost(hostId), remoteName);
        VERIFY_IS_TRUE(status != nullptr && std::wstring{ status.GetNamedString(L"offlineRetention", L"") } == L"2", L"status: two seconds");

        remote.EndAll();
    }

    VERIFY_IS_TRUE(WaitFor([&]() { return SameText(KeptEndpointId(FindHost(hostId), remoteName), endpoint.InterfaceId); }, 3000),
        L"the endpoint is kept when the remote leaves");

    RtpMidiTest::OpenedEndpoint opened(m_transport, endpoint.InterfaceId, 82);
    VERIFY_IS_TRUE(opened.IsOpen(), L"an app opens it while the remote is away");

    uint32_t const whileAway{ 0x20904014 };
    VERIFY_ARE_EQUAL(opened.Send(&whileAway, sizeof(whileAway)), S_OK, L"what it sends is dropped");

    {
        Peer back("Harness Timed Remote", false);
        VERIFY_IS_TRUE(back.Start(), L"the remote comes back on new ports");
        back.Invite(port);

        VERIFY_IS_TRUE(WaitFor([&]() { return back.ConnectedCount() == 1; }, 5000), L"it connects again");
        VERIFY_IS_TRUE(WaitFor([&]() { return KeptEndpointId(FindHost(hostId), remoteName).empty(); }, 3000), L"status: nothing is kept while it is connected");
        VERIFY_IS_TRUE(Created(*m_deviceManager, remoteName) == 1, L"it is handed the same endpoint");

        back.Send({ 0x94, 0x34, 0x60 });
        VERIFY_IS_TRUE(WaitFor([&]() { return Contains(opened.Received().Words(), 0x20943460); }, 3000), L"the app hears it without opening anything");
        VERIFY_IS_FALSE(ContainsSequence(back.Received(), { 0x90, 0x40, 0x14 }), L"what was sent while it was away never arrives");

        back.EndAll();
    }

    VERIFY_IS_TRUE(WaitFor([&]() { return !KeptEndpointId(FindHost(hostId), remoteName).empty(); }, 3000), L"kept again once it leaves");

    auto const left = std::chrono::steady_clock::now();

    VERIFY_IS_TRUE(WaitFor([&]() { return Live(*m_deviceManager, remoteName).empty(); }, 8000), L"and removed once its two seconds are up");

    auto const keptFor = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - left).count();
    Log::Comment(String().Format(L"The endpoint went %lld ms after the remote left.", keptFor));
    VERIFY_IS_GREATER_THAN_OR_EQUAL(keptFor, 1500ll, L"not before its time");
}


void RtpMidiTransportTests::TestHostKeptEndpointGoesWithItsSettingsAndItsHost()
{
    std::wstring const remoteName{ L"Harness Removal Remote" };

    uint16_t port{ 0 };
    auto const hostId = CreateHost(L"\"name\":\"Removal Keeping Host\"", port);
    Cleanup removeHost([&]() { RemoveHost(hostId); });

    VERIFY_IS_TRUE(IsSuccess(Send(SettingsSection(hostId, Kept(remoteName, L"\"always\"")))), L"the remote is kept always");
    VERIFY_IS_TRUE(WaitFor([&]() { return Live(*m_deviceManager, remoteName).size() == 1; }, 3000), L"its endpoint is made without a connection");

    // the worker passes many times meanwhile, and must leave it alone
    Sleep(1500);
    VERIFY_IS_TRUE(Created(*m_deviceManager, remoteName) == 1, L"it stays, rather than being removed and made again");

    VERIFY_IS_TRUE(IsSuccess(Send(SettingsSection(hostId, Kept(remoteName, L"\"immediate\"")))), L"changed to remove at once while it is away");
    VERIFY_IS_TRUE(WaitFor([&]() { return Live(*m_deviceManager, remoteName).empty(); }, 3000), L"the endpoint goes straight away");

    VERIFY_IS_TRUE(IsSuccess(Send(SettingsSection(hostId, Kept(remoteName, L"\"always\"")))), L"kept always again");
    VERIFY_IS_TRUE(WaitFor([&]() { return Live(*m_deviceManager, remoteName).size() == 1; }, 3000), L"the endpoint is back");

    VERIFY_IS_TRUE(IsSuccess(Send(L"{\"create\":{\"hosts\":{\"" + hostId + L"\":{\"name\":\"Removal Keeping Host\",\"port\":\"auto\",\"advertise\":false,\"enabled\":false}}}}")),
        L"the host is switched off");
    VERIFY_IS_TRUE(WaitFor([&]() { return Live(*m_deviceManager, remoteName).empty(); }, 3000), L"a switched off host keeps nothing");

    VERIFY_IS_TRUE(IsSuccess(Send(L"{\"create\":{\"hosts\":{\"" + hostId + L"\":{\"name\":\"Removal Keeping Host\",\"port\":\"auto\",\"advertise\":false,\"enabled\":true}}}}")),
        L"the host is switched on");
    VERIFY_IS_TRUE(WaitFor([&]() { return Live(*m_deviceManager, remoteName).size() == 1; }, 5000), L"the remote's settings came through, and so does its endpoint");

    RemoveHost(hostId);
    VERIFY_IS_TRUE(WaitFor([&]() { return Live(*m_deviceManager, remoteName).empty(); }, 3000), L"removing the host removes the endpoint");
}


void RtpMidiTransportTests::TestOnlyOneRemoteOfANameIsKept()
{
    std::wstring const remoteName{ L"Harness Twin" };

    uint16_t port{ 0 };
    auto const hostId = CreateHost(L"\"name\":\"Twin Keeping Host\"", port);
    Cleanup removeHost([&]() { RemoveHost(hostId); });

    VERIFY_IS_TRUE(IsSuccess(Send(SettingsSection(hostId, Kept(remoteName, L"\"always\"")))), L"the name is kept always");
    VERIFY_IS_TRUE(WaitFor([&]() { return Live(*m_deviceManager, remoteName).size() == 1; }, 3000), L"its endpoint is made");

    auto const kept = Live(*m_deviceManager, remoteName).front();

    Peer first("Harness Twin", false);
    VERIFY_IS_TRUE(first.Start(), L"the first remote binds a loopback port pair");
    first.Invite(port);
    VERIFY_IS_TRUE(WaitFor([&]() { return first.ConnectedCount() == 1 && KeptEndpointId(FindHost(hostId), remoteName).empty(); }, 5000),
        L"the first remote takes the kept endpoint");

    {
        // the engine treats one name from one address as the same device, so this one is on IPv4
        Peer second("Harness Twin", false, true);
        VERIFY_IS_TRUE(second.Start(), L"a second remote with the same name binds a loopback port pair");
        second.Invite(port);

        VERIFY_IS_TRUE(WaitFor([&]() { return second.ConnectedCount() == 1 && Live(*m_deviceManager, remoteName).size() == 2; }, 5000),
            L"the second gets an endpoint of its own");

        second.EndAll();
    }

    VERIFY_IS_TRUE(WaitFor([&]() { return Live(*m_deviceManager, remoteName).size() == 1; }, 3000), L"the second remote's endpoint is not kept");
    VERIFY_IS_TRUE(SameText(Live(*m_deviceManager, remoteName).front().InterfaceId, kept.InterfaceId), L"the one left is the kept one");

    first.EndAll();

    VERIFY_IS_TRUE(WaitFor([&]() { return SameText(KeptEndpointId(FindHost(hostId), remoteName), kept.InterfaceId); }, 3000),
        L"the first remote's endpoint is kept when it leaves");

    Sleep(1000);
    VERIFY_IS_TRUE(Created(*m_deviceManager, remoteName) == 2, L"only the kept endpoint and the second remote's were ever made");
}


void RtpMidiTransportTests::TestDisconnectingARemoteHereEndsItsTimedEndpoint()
{
    std::wstring const remoteName{ L"Harness Disconnected Remote" };

    uint16_t port{ 0 };
    auto const hostId = CreateHost(L"\"name\":\"Disconnect Keeping Host\"", port);
    Cleanup removeHost([&]() { RemoveHost(hostId); });

    VERIFY_IS_TRUE(IsSuccess(Send(SettingsSection(hostId, Kept(remoteName, L"300")))), L"the remote is kept for five minutes");

    Peer remote("Harness Disconnected Remote", false);
    VERIFY_IS_TRUE(remote.Start(), L"the remote binds a loopback port pair");
    remote.Invite(port);

    VERIFY_IS_TRUE(WaitFor([&]() { return remote.ConnectedCount() == 1 && Live(*m_deviceManager, remoteName).size() == 1; }, 5000),
        L"it connects and gets an endpoint");

    auto const connection = FindConnection(FindHost(hostId), remoteName);
    VERIFY_IS_TRUE(connection != nullptr, L"the connection is listed");

    auto const connectionId = static_cast<uint32_t>(connection.GetNamedNumber(L"connectionId", 0));

    VERIFY_IS_TRUE(IsSuccess(Send(Command(L"disconnectRemoteClient",
        { { L"entryIdentifier", hostId }, { L"connectionId", std::to_wstring(connectionId) } }))), L"the customer disconnects it here");

    VERIFY_IS_TRUE(WaitFor([&]() { return Live(*m_deviceManager, remoteName).empty(); }, 3000), L"its endpoint goes with the connection");
    VERIFY_IS_TRUE(KeptEndpointId(FindHost(hostId), remoteName).empty(), L"status: nothing is kept");
}


void RtpMidiTransportTests::TestHostOfflineRetentionValuesAreChecked()
{
    uint16_t port{ 0 };
    auto const hostId = CreateHost(L"\"name\":\"Retention Values Host\"", port);
    Cleanup removeHost([&]() { RemoveHost(hostId); });

    auto const response = Send(SettingsSection(hostId,
        Kept(L"Good Remote", L"\"90\"") + L"," + Kept(L"Bad Remote", L"\"forever\"") + L"," + Kept(L"Too Long Remote", L"86401")));

    VERIFY_IS_FALSE(IsSuccess(response), L"a value the transport cannot read is reported");

    auto const host = FindHost(hostId);

    auto const good = SettingsStatus(host, L"Good Remote");
    VERIFY_IS_TRUE(good != nullptr, L"the remote with a good value is kept");
    VERIFY_IS_TRUE(std::wstring{ good.GetNamedString(L"offlineRetention", L"") } == L"90", L"with its value");

    VERIFY_IS_TRUE(SettingsStatus(host, L"Bad Remote") == nullptr, L"a word it does not know is skipped, not guessed at");
    VERIFY_IS_TRUE(SettingsStatus(host, L"Too Long Remote") == nullptr, L"more than a day is skipped");

    VERIFY_IS_TRUE(IsSuccess(Send(SettingsSection(hostId, L"{\"remoteName\":\"Plain Remote\",\"sendSpeedLimit\":2}"))), L"settings without a retention");

    auto const plain = SettingsStatus(FindHost(hostId), L"Plain Remote");
    VERIFY_IS_TRUE(plain != nullptr && std::wstring{ plain.GetNamedString(L"offlineRetention", L"") } == L"immediate",
        L"remove the endpoint at once, as before there was a choice");
}


void RtpMidiTransportTests::TestRestartedRemoteKeepsItsHostEndpoint()
{
    std::wstring const remoteName{ L"Harness Restarting Kept Remote" };

    uint16_t port{ 0 };
    auto const hostId = CreateHost(L"\"name\":\"Restart Keeping Host\"", port);
    Cleanup removeHost([&]() { RemoveHost(hostId); });

    VERIFY_IS_TRUE(IsSuccess(Send(SettingsSection(hostId, Kept(remoteName, L"300")))), L"the remote is kept for five minutes");

    // a device which restarts invites again from a new port with a new SSRC, and never said goodbye
    auto firstLife = std::make_unique<Peer>("Harness Restarting Kept Remote", false);
    VERIFY_IS_TRUE(firstLife->Start(), L"the remote binds");
    firstLife->Invite(port);
    VERIFY_IS_TRUE(WaitFor([&]() { return firstLife->ConnectedCount() == 1 && Live(*m_deviceManager, remoteName).size() == 1; }, 5000),
        L"it connects and gets an endpoint");

    auto const endpoint = Live(*m_deviceManager, remoteName).front();

    RtpMidiTest::OpenedEndpoint opened(m_transport, endpoint.InterfaceId, 83);
    VERIFY_IS_TRUE(opened.IsOpen(), L"an app opens it");

    auto secondLife = std::make_unique<Peer>("Harness Restarting Kept Remote", false);
    VERIFY_IS_TRUE(secondLife->Start(), L"its second life binds");
    secondLife->Invite(port);
    VERIFY_IS_TRUE(WaitFor([&]() { return secondLife->ConnectedCount() == 1; }, 5000), L"its second life connects");

    secondLife->Send({ 0x95, 0x35, 0x70 });
    VERIFY_IS_TRUE(WaitFor([&]() { return Contains(opened.Received().Words(), 0x20953570); }, 3000),
        L"the app hears the second life on the endpoint it already had open");
    VERIFY_IS_TRUE(Created(*m_deviceManager, remoteName) == 1, L"no endpoint was made or removed for the restart");

    // the first life's goodbye arrives late, and must not take the endpoint from the second life
    firstLife.reset();
    Sleep(500);

    VERIFY_IS_TRUE(Live(*m_deviceManager, remoteName).size() == 1, L"the endpoint is still there");
    VERIFY_IS_TRUE(KeptEndpointId(FindHost(hostId), remoteName).empty(), L"status: the second life has it, nothing is kept");

    uint32_t const toSecondLife{ 0x20964071 };
    VERIFY_SUCCEEDED(opened.Send(&toSecondLife, sizeof(toSecondLife)), L"the app sends");
    VERIFY_IS_TRUE(WaitFor([&]() { return ContainsSequence(secondLife->Received(), { 0x96, 0x40, 0x71 }); }, 3000), L"the second life receives it");
}


// While its remote is away, a kept endpoint goes back only to an address that remote connected
// from. A device sending the name from anywhere else is asked about first, even on a host which
// lets anyone in, because an app may still have the endpoint open.
void RtpMidiTransportTests::TestKeptHostEndpointGoesBackOnlyToItsAddress()
{
    std::wstring const remoteName{ L"Harness Address Kept Remote" };

    uint16_t port{ 0 };
    auto const hostId = CreateHost(L"\"name\":\"Address Keeping Host\"", port);
    Cleanup removeHost([&]() { RemoveHost(hostId); });

    VERIFY_IS_TRUE(IsSuccess(Send(SettingsSection(hostId, Kept(remoteName, L"\"always\"")))), L"the remote is kept always");
    VERIFY_IS_TRUE(WaitFor([&]() { return Live(*m_deviceManager, remoteName).size() == 1; }, 3000), L"its endpoint is made before it connects");

    auto const kept = Live(*m_deviceManager, remoteName).front();

    auto const pendingAddress = [&]()
        {
            auto const pending = FindPending(hostId, remoteName);
            return pending == nullptr ? std::wstring{} : std::wstring{ pending.GetNamedString(L"remoteAddress", L"") };
        };

    {
        Peer first("Harness Address Kept Remote", false);
        VERIFY_IS_TRUE(first.Start(), L"the remote binds an IPv6 loopback port pair");
        first.Invite(port);

        VERIFY_IS_TRUE(WaitFor([&]() { return first.ConnectedCount() == 1 && KeptEndpointId(FindHost(hostId), remoteName).empty(); }, 5000),
            L"the first remote to use the name takes the endpoint, without being asked about");

        first.EndAll();
    }

    VERIFY_IS_TRUE(WaitFor([&]() { return SameText(KeptEndpointId(FindHost(hostId), remoteName), kept.InterfaceId); }, 3000),
        L"the endpoint is kept once it leaves");

    {
        Peer impostor("Harness Address Kept Remote", false, true);
        VERIFY_IS_TRUE(impostor.Start(), L"another device with the name binds an IPv4 loopback port pair");
        impostor.Invite(port);

        VERIFY_IS_TRUE(WaitFor([&]() { return pendingAddress() == L"127.0.0.1"; }, 3000), L"it is asked about, showing its address");

        Sleep(1500);
        VERIFY_IS_TRUE(impostor.ConnectedCount() == 0, L"it waits");
        VERIFY_IS_TRUE(SameText(KeptEndpointId(FindHost(hostId), remoteName), kept.InterfaceId), L"the endpoint is still kept for its remote");
        VERIFY_IS_TRUE(Created(*m_deviceManager, remoteName) == 1, L"and nothing else was made");

        VERIFY_IS_TRUE(IsSuccess(Decide(L"approveRemoteClient", hostId, remoteName, L"once")), L"approve it");

        VERIFY_IS_TRUE(WaitFor([&]() { return impostor.ConnectedCount() == 1 && KeptEndpointId(FindHost(hostId), remoteName).empty(); }, 3000),
            L"an approved address gets the kept endpoint");
        VERIFY_IS_TRUE(Created(*m_deviceManager, remoteName) == 1, L"the same one");

        impostor.EndAll();
    }

    VERIFY_IS_TRUE(WaitFor([&]() { return SameText(KeptEndpointId(FindHost(hostId), remoteName), kept.InterfaceId); }, 3000),
        L"kept again once it leaves");

    Peer back("Harness Address Kept Remote", false);
    VERIFY_IS_TRUE(back.Start(), L"the first remote comes back from its address");
    back.Invite(port);

    VERIFY_IS_TRUE(WaitFor([&]() { return back.ConnectedCount() == 1 && KeptEndpointId(FindHost(hostId), remoteName).empty(); }, 5000),
        L"it gets its endpoint back without being asked about");
    VERIFY_IS_TRUE(FindPending(hostId, remoteName) == nullptr, L"nobody was asked");
    VERIFY_IS_TRUE(Created(*m_deviceManager, remoteName) == 1, L"still the one endpoint");
}
