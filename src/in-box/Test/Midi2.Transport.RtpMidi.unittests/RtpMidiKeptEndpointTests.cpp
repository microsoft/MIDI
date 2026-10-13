// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// A client entry which keeps its endpoint while its remote is away. Asked for by a customer whose
// DAWs drop a control surface's setup when they start while its port is missing.
// ============================================================================

#include "pch.h"
#include "RtpMidiTransportTests.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using RtpMidiTest::Command;
using RtpMidiTest::EntryState;
using RtpMidiTest::ErrorCode;
using RtpMidiTest::IsSuccess;
using RtpMidiTest::NewGuidText;
using RtpMidiTest::Peer;
using RtpMidiTest::SameText;
using RtpMidiTest::WaitFor;

namespace
{
    // removes what a test made however it ends
    template <typename Action>
    class AtExit
    {
    public:
        explicit AtExit(_In_ Action action) : m_action(std::move(action)) {}
        ~AtExit() { try { m_action(); } catch (...) {} }

        AtExit(AtExit const&) = delete;
        AtExit& operator=(AtExit const&) = delete;

    private:
        Action m_action;
    };

    // A port on ::1 which takes invitations and never answers them, like a device switched off
    // whose address is still in use. Lives as long as the test class.
    std::unique_ptr<RtpMidiTest::UdpSocket> g_silentRemote;

    bool Contains(_In_ std::vector<uint32_t> const& words, _In_ uint32_t const word)
    {
        return std::find(words.begin(), words.end(), word) != words.end();
    }

    bool ContainsSequence(_In_ std::vector<uint8_t> const& haystack, _In_ std::vector<uint8_t> const& needle)
    {
        return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end()) != haystack.end();
    }

    std::wstring ClientSection(_In_ std::wstring const& clientId, _In_ std::wstring const& fields)
    {
        return L"{\"create\":{\"clients\":{\"" + clientId + L"\":{" + fields + L"}}}}";
    }

    std::wstring DirectFields(_In_ std::wstring const& name, _In_ uint16_t const port, _In_ std::wstring const& endpointName, _In_ std::wstring const& more)
    {
        return L"\"name\":\"" + name + L"\",\"remoteAddress\":\"::1\",\"remotePort\":" + std::to_wstring(port) +
            L",\"customEndpointName\":\"" + endpointName + L"\"" + (more.empty() ? L"" : L"," + more);
    }

    // the endpoints with this name the transport has created and not removed
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

    uint32_t ClientConnectionId(_In_ winrt::Windows::Data::Json::JsonObject const& client)
    {
        if (client == nullptr || !client.HasKey(L"connections")) return 0;

        auto const connections = client.GetNamedArray(L"connections");
        if (connections.Size() == 0) return 0;

        return static_cast<uint32_t>(connections.GetObjectAt(0).GetNamedNumber(L"connectionId", 0));
    }
}


void RtpMidiTransportTests::DefineKeptClientBeforeStart()
{
    // before the transport starts Winsock for this process
    WSADATA data{};
    VERIFY_ARE_EQUAL(WSAStartup(MAKEWORD(2, 2), &data), 0);

    g_silentRemote = std::make_unique<RtpMidiTest::UdpSocket>();
    VERIFY_IS_TRUE(g_silentRemote->Bind(0, false), L"a port that never answers is bound");

    m_keptClientId = NewGuidText();

    VERIFY_IS_TRUE(IsSuccess(Send(ClientSection(m_keptClientId,
        DirectFields(L"Harness Kept Client", g_silentRemote->Port(), L"Harness Kept Port", L"\"offlineRetention\":\"always\"")))),
        L"a client keeping its endpoint is defined before the endpoint manager starts");
}

void RtpMidiTransportTests::NoteKeptEndpointAfterStart()
{
    auto const live = Live(*m_deviceManager, L"Harness Kept Port");
    if (live.size() == 1) m_keptEndpointAtStart = live.front().InterfaceId;
}


void RtpMidiTransportTests::TestKeptEndpointIsThereBeforeTheRemote()
{
    AtExit cleanup([&]() { Send(Command(L"removeClient", { { L"entryIdentifier", m_keptClientId } })); });

    VERIFY_IS_FALSE(m_keptEndpointAtStart.empty(), L"the endpoint was there when the endpoint manager finished starting");

    // the worker has run many times since, and the remote has never answered
    auto live = Live(*m_deviceManager, L"Harness Kept Port");
    VERIFY_IS_TRUE(live.size() == 1, L"it is still there, once");
    VERIFY_IS_TRUE(SameText(live.front().InterfaceId, m_keptEndpointAtStart), L"and it is the same endpoint");
    VERIFY_IS_TRUE(live.front().NativeFormat == MidiDataFormats_ByteStream, L"it is a byte stream endpoint like any other");
    VERIFY_IS_TRUE(live.front().PropertyCount > 0, L"with its MIDI 1.0 port properties");

    auto client = FindClient(m_keptClientId);
    VERIFY_IS_TRUE(client != nullptr, L"the client is listed");
    VERIFY_IS_TRUE(std::wstring{ client.GetNamedString(L"offlineRetention", L"") } == L"always", L"status: offline retention");
    VERIFY_IS_TRUE(SameText(std::wstring{ client.GetNamedString(L"endpointDeviceId", L"") }, m_keptEndpointAtStart), L"status: the endpoint, with no connection");
    VERIFY_IS_TRUE(EntryState(client) != L"live", L"status: not connected");

    RtpMidiTest::OpenedEndpoint opened(m_transport, m_keptEndpointAtStart, 31);
    VERIFY_IS_TRUE(opened.IsOpen(), L"an app opens it while the remote is away");

    uint32_t const whileAway{ 0x20904010 };
    VERIFY_ARE_EQUAL(opened.Send(&whileAway, sizeof(whileAway)), S_OK, L"what it sends while the remote is away is accepted and dropped");

    // Only the port changes. The address is part of the endpoint's identity, the port is not.
    Peer remoteHost("Harness Kept Remote", true);
    VERIFY_IS_TRUE(remoteHost.Start(), L"the remote binds a loopback port pair");

    VERIFY_IS_TRUE(IsSuccess(Send(ClientSection(m_keptClientId,
        DirectFields(L"Harness Kept Client", remoteHost.ControlPort(), L"Harness Kept Port", L"\"offlineRetention\":\"always\"")))),
        L"the client is pointed at the remote");

    VERIFY_IS_TRUE(WaitFor([&]() { return remoteHost.ConnectedCount() == 1; }, 8000), L"the client connects");
    VERIFY_IS_TRUE(WaitFor([&]() { return EntryState(FindClient(m_keptClientId)) == L"live"; }, 3000), L"the entry is live");

    VERIFY_IS_TRUE(Created(*m_deviceManager, L"Harness Kept Port") == 1, L"no second endpoint was made for the connection");

    remoteHost.Send({ 0x92, 0x30, 0x40 });
    VERIFY_IS_TRUE(WaitFor([&]() { return Contains(opened.Received().Words(), 0x20923040); }, 3000),
        L"the app which opened it before the connection hears the remote");
    VERIFY_IS_FALSE(opened.Received().WrongContext(), L"with the context it was opened with");
    VERIFY_IS_FALSE(ContainsSequence(remoteHost.Received(), { 0x90, 0x40, 0x10 }), L"what was sent while the remote was away never reaches it later");

    uint32_t const noteOn{ 0x20914050 };
    VERIFY_SUCCEEDED(opened.Send(&noteOn, sizeof(noteOn)), L"the app sends");
    VERIFY_IS_TRUE(WaitFor([&]() { return ContainsSequence(remoteHost.Received(), { 0x91, 0x40, 0x50 }); }, 3000), L"the remote receives it");

    client = FindClient(m_keptClientId);
    VERIFY_IS_TRUE(SameText(std::wstring{ client.GetNamedString(L"endpointDeviceId", L"") }, m_keptEndpointAtStart), L"status: the same endpoint while connected");

    VERIFY_IS_TRUE(IsSuccess(Send(Command(L"removeClient", { { L"entryIdentifier", m_keptClientId } }))), L"remove the client");
    VERIFY_IS_TRUE(WaitFor([&]() { return Live(*m_deviceManager, L"Harness Kept Port").empty(); }, 3000), L"its endpoint goes with it");
    VERIFY_IS_TRUE(FindClient(m_keptClientId) == nullptr, L"the client is no longer listed");
}


void RtpMidiTransportTests::TestKeptEndpointStaysWhileTheRemoteIsAway()
{
    std::wstring const endpointName{ L"Harness Kept B" };

    Peer remoteHost("Harness Kept Remote B", true);
    VERIFY_IS_TRUE(remoteHost.Start(), L"the remote binds a loopback port pair");

    auto const clientId = NewGuidText();
    AtExit cleanup([&]() { Send(Command(L"removeClient", { { L"entryIdentifier", clientId } })); });

    VERIFY_IS_TRUE(IsSuccess(Send(ClientSection(clientId,
        DirectFields(L"Harness Kept Client B", remoteHost.ControlPort(), endpointName, L"\"offlineRetention\":\"always\"")))),
        L"a client keeping its endpoint is accepted");

    VERIFY_IS_TRUE(WaitFor([&]() { return remoteHost.ConnectedCount() == 1 && Live(*m_deviceManager, endpointName).size() == 1; }, 8000),
        L"it connects and has one endpoint");

    auto const endpoint = Live(*m_deviceManager, endpointName).front();
    VERIFY_IS_TRUE(endpoint.EndpointName == endpointName, L"named after the entry, not the remote");

    RtpMidiTest::OpenedEndpoint opened(m_transport, endpoint.InterfaceId, 32);
    VERIFY_IS_TRUE(opened.IsOpen(), L"an app opens it");

    // a note the remote leaves sounding when it goes
    remoteHost.Send({ 0x93, 0x31, 0x50 });
    VERIFY_IS_TRUE(WaitFor([&]() { return Contains(opened.Received().Words(), 0x20933150); }, 3000), L"the app hears the remote");

    remoteHost.EndAll();

    VERIFY_IS_TRUE(WaitFor([&]() { return Contains(opened.Received().Words(), 0x20833140); }, 3000),
        L"the app is sent a Note Off for the note left sounding");
    VERIFY_IS_TRUE(WaitFor([&]() { return EntryState(FindClient(clientId)) == L"failed"; }, 3000), L"the entry waits to try again");

    Sleep(1500);

    VERIFY_IS_TRUE(Live(*m_deviceManager, endpointName).size() == 1, L"the endpoint stays while the remote is away");
    VERIFY_IS_TRUE(opened.IsOpen(), L"and the app keeps it open");

    uint32_t const whileAway{ 0x20904011 };
    VERIFY_ARE_EQUAL(opened.Send(&whileAway, sizeof(whileAway)), S_OK, L"what it sends meanwhile is accepted and dropped");

    VERIFY_IS_TRUE(SameText(std::wstring{ FindClient(clientId).GetNamedString(L"endpointDeviceId", L"") }, endpoint.InterfaceId),
        L"status: the entry still names its endpoint");

    VERIFY_IS_TRUE(IsSuccess(Send(Command(L"reconnectClient", { { L"entryIdentifier", clientId } }))), L"reconnect now");
    VERIFY_IS_TRUE(WaitFor([&]() { return remoteHost.ConnectedCount() == 1; }, 8000), L"the remote is back");

    VERIFY_IS_TRUE(Created(*m_deviceManager, endpointName) == 1, L"the connection is handed to the endpoint already there");

    remoteHost.Send({ 0x94, 0x32, 0x60 });
    VERIFY_IS_TRUE(WaitFor([&]() { return Contains(opened.Received().Words(), 0x20943260); }, 3000), L"the app hears the remote again without opening anything");

    uint32_t const afterReturn{ 0x20954070 };
    VERIFY_SUCCEEDED(opened.Send(&afterReturn, sizeof(afterReturn)), L"the app sends");
    VERIFY_IS_TRUE(WaitFor([&]() { return ContainsSequence(remoteHost.Received(), { 0x95, 0x40, 0x70 }); }, 3000), L"the remote receives it");
    VERIFY_IS_FALSE(ContainsSequence(remoteHost.Received(), { 0x90, 0x40, 0x11 }), L"what was sent while it was away never arrives");

    // the customer disconnecting it here is different: the endpoint goes, and stays gone
    auto const connectionId = ClientConnectionId(FindClient(clientId));
    VERIFY_IS_TRUE(connectionId != 0, L"the connection is listed");

    VERIFY_IS_TRUE(IsSuccess(Send(Command(L"disconnectRemoteClient",
        { { L"entryIdentifier", clientId }, { L"connectionId", std::to_wstring(connectionId) } }))), L"disconnect the client here");

    VERIFY_IS_TRUE(WaitFor([&]() { return Live(*m_deviceManager, endpointName).empty(); }, 3000), L"the endpoint goes when the customer disconnects");
    VERIFY_IS_TRUE(EntryState(FindClient(clientId)) == L"unavailable", L"and the entry waits to be reconnected");
    VERIFY_IS_TRUE(FindClient(clientId).GetNamedString(L"endpointDeviceId", L"x").empty(), L"status: no endpoint");

    VERIFY_IS_TRUE(IsSuccess(Send(Command(L"reconnectClient", { { L"entryIdentifier", clientId } }))), L"reconnect it");
    VERIFY_IS_TRUE(WaitFor([&]() { return Live(*m_deviceManager, endpointName).size() == 1 && remoteHost.ConnectedCount() == 1; }, 8000),
        L"the endpoint and the connection come back");
    VERIFY_IS_TRUE(SameText(Live(*m_deviceManager, endpointName).front().InstanceId, endpoint.InstanceId), L"under the same instance id");

    VERIFY_IS_TRUE(IsSuccess(Send(Command(L"removeClient", { { L"entryIdentifier", clientId } }))), L"remove the client");
    VERIFY_IS_TRUE(WaitFor([&]() { return Live(*m_deviceManager, endpointName).empty(); }, 3000), L"the endpoint goes with it");
}


void RtpMidiTransportTests::TestKeptEndpointRunsOutAfterItsRetention()
{
    std::wstring const endpointName{ L"Harness Kept C" };

    Peer remoteHost("Harness Kept Remote C", true);
    VERIFY_IS_TRUE(remoteHost.Start(), L"the remote binds a loopback port pair");

    auto const clientId = NewGuidText();
    AtExit cleanup([&]() { Send(Command(L"removeClient", { { L"entryIdentifier", clientId } })); });

    VERIFY_IS_TRUE(IsSuccess(Send(ClientSection(clientId,
        DirectFields(L"Harness Kept Client C", remoteHost.ControlPort(), endpointName, L"\"offlineRetention\":\"3\"")))),
        L"a client keeping its endpoint for three seconds is accepted");

    VERIFY_IS_TRUE(WaitFor([&]() { return remoteHost.ConnectedCount() == 1 && Live(*m_deviceManager, endpointName).size() == 1; }, 8000),
        L"it connects and has one endpoint");
    VERIFY_IS_TRUE(std::wstring{ FindClient(clientId).GetNamedString(L"offlineRetention", L"") } == L"3", L"status: three seconds");

    auto const endpoint = Live(*m_deviceManager, endpointName).front();

    remoteHost.EndAll();
    auto const ended = std::chrono::steady_clock::now();

    VERIFY_IS_TRUE(WaitFor([&]() { return EntryState(FindClient(clientId)) == L"failed"; }, 3000), L"the remote is away");
    VERIFY_IS_TRUE(Live(*m_deviceManager, endpointName).size() == 1, L"the endpoint stays at first");

    VERIFY_IS_TRUE(WaitFor([&]() { return Live(*m_deviceManager, endpointName).empty(); }, 8000), L"and goes once the three seconds are up");

    auto const after = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - ended).count();
    Log::Comment(String().Format(L"Removed %lld ms after the remote left", static_cast<long long>(after)));
    VERIFY_IS_TRUE(after >= 2500, L"not before");

    VERIFY_IS_TRUE(IsSuccess(Send(Command(L"reconnectClient", { { L"entryIdentifier", clientId } }))), L"reconnect");
    VERIFY_IS_TRUE(WaitFor([&]() { return remoteHost.ConnectedCount() == 1 && Live(*m_deviceManager, endpointName).size() == 1; }, 8000),
        L"the endpoint is made again when the remote is back");
    VERIFY_IS_TRUE(SameText(Live(*m_deviceManager, endpointName).front().InstanceId, endpoint.InstanceId), L"under the same instance id");
}


void RtpMidiTransportTests::TestKeepingCanBeTurnedOnAndOffWhileConnected()
{
    std::wstring const endpointName{ L"Harness Kept D" };

    Peer remoteHost("Harness Kept Remote D", true);
    VERIFY_IS_TRUE(remoteHost.Start(), L"the remote binds a loopback port pair");

    auto const clientId = NewGuidText();
    AtExit cleanup([&]() { Send(Command(L"removeClient", { { L"entryIdentifier", clientId } })); });

    auto const fields = [&](std::wstring const& retention)
        {
            return DirectFields(L"Harness Kept Client D", remoteHost.ControlPort(), endpointName,
                retention.empty() ? L"" : L"\"offlineRetention\":\"" + retention + L"\"");
        };

    VERIFY_IS_TRUE(IsSuccess(Send(ClientSection(clientId, fields(L"")))), L"a client which does not keep its endpoint");
    VERIFY_IS_TRUE(WaitFor([&]() { return remoteHost.ConnectedCount() == 1 && Live(*m_deviceManager, endpointName).size() == 1; }, 8000),
        L"it connects and has one endpoint");
    VERIFY_IS_TRUE(std::wstring{ FindClient(clientId).GetNamedString(L"offlineRetention", L"") } == L"immediate", L"status: immediate by default");

    auto const endpoint = Live(*m_deviceManager, endpointName).front();

    VERIFY_IS_TRUE(IsSuccess(Send(ClientSection(clientId, fields(L"always")))), L"keeping is turned on while connected");

    Sleep(1500);

    VERIFY_IS_TRUE(Created(*m_deviceManager, endpointName) == 1, L"the endpoint already there is kept, not made again");
    VERIFY_IS_TRUE(remoteHost.ConnectedCount() == 1, L"and the connection is not dropped");

    remoteHost.EndAll();
    VERIFY_IS_TRUE(WaitFor([&]() { return EntryState(FindClient(clientId)) == L"failed"; }, 3000), L"the remote is away");

    Sleep(1500);
    VERIFY_IS_TRUE(Live(*m_deviceManager, endpointName).size() == 1, L"the endpoint stays");

    VERIFY_IS_TRUE(IsSuccess(Send(ClientSection(clientId, fields(L"immediate")))), L"keeping is turned off while the remote is away");
    VERIFY_IS_TRUE(WaitFor([&]() { return Live(*m_deviceManager, endpointName).empty(); }, 3000), L"the endpoint goes");

    VERIFY_IS_TRUE(IsSuccess(Send(Command(L"reconnectClient", { { L"entryIdentifier", clientId } }))), L"reconnect");
    VERIFY_IS_TRUE(WaitFor([&]() { return remoteHost.ConnectedCount() == 1 && Live(*m_deviceManager, endpointName).size() == 1; }, 8000),
        L"the remote gets an endpoint as before");
    VERIFY_IS_TRUE(SameText(Live(*m_deviceManager, endpointName).front().InstanceId, endpoint.InstanceId), L"under the same instance id");

    remoteHost.EndAll();
    VERIFY_IS_TRUE(WaitFor([&]() { return Live(*m_deviceManager, endpointName).empty(); }, 3000), L"which goes with the connection, as it always has");
}


void RtpMidiTransportTests::TestOfflineRetentionValuesAreChecked()
{
    struct Case { std::wstring Value; bool Accepted; std::wstring Reported; };

    std::vector<Case> const cases =
    {
        { L"\"always\"", true, L"always" },
        { L"\"Always\"", true, L"always" },
        { L"\"immediate\"", true, L"immediate" },
        { L"\"0\"", true, L"immediate" },
        { L"\"86400\"", true, L"86400" },
        { L"30", true, L"30" },
        { L"\"forever\"", false, L"" },
        { L"\"-1\"", false, L"" },
        { L"\"30s\"", false, L"" },
        { L"\"86401\"", false, L"" },
        { L"86401", false, L"" },
        { L"-1", false, L"" },
        { L"1.5", false, L"" },
        { L"\"\"", false, L"" },
        { L"true", false, L"" },
        { L"[]", false, L"" },
    };

    for (auto const& testCase : cases)
    {
        auto const clientId = NewGuidText();
        AtExit cleanup([&]() { Send(Command(L"removeClient", { { L"entryIdentifier", clientId } })); });

        // nothing answers there, so nothing connects while the case is checked
        auto const response = Send(ClientSection(clientId,
            DirectFields(L"Harness Retention Values", g_silentRemote->Port(), L"Harness Retention Values", L"\"autoReconnect\":false,\"offlineRetention\":" + testCase.Value)));

        if (!testCase.Accepted)
        {
            VERIFY_IS_TRUE(!IsSuccess(response) && ErrorCode(response) == 6u,
                String().Format(L"%s is refused as an invalid entry (error %u)", testCase.Value.c_str(), ErrorCode(response)));
            VERIFY_IS_TRUE(FindClient(clientId) == nullptr, L"and not kept");
            continue;
        }

        VERIFY_IS_TRUE(IsSuccess(response), String().Format(L"%s is accepted", testCase.Value.c_str()));

        auto const client = FindClient(clientId);
        VERIFY_IS_TRUE(client != nullptr, L"the client is listed");
        VERIFY_IS_TRUE(std::wstring{ client.GetNamedString(L"offlineRetention", L"") } == testCase.Reported,
            String().Format(L"%s is reported as %s", testCase.Value.c_str(), testCase.Reported.c_str()));

        // without reconnecting, nothing would ever come back to a kept endpoint
        if (testCase.Value == L"\"always\"")
        {
            Sleep(1500);
            VERIFY_IS_TRUE(Live(*m_deviceManager, L"Harness Retention Values").empty(), L"a client which does not reconnect keeps no endpoint");
        }
    }
}
