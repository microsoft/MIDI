// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "RtpMidiTransportTests.h"

#include "WindowsMidiServices_i.c"

extern "C" IMAGE_DOS_HEADER __ImageBase;

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using RtpMidiTest::Command;
using RtpMidiTest::EntryState;
using RtpMidiTest::ErrorCode;
using RtpMidiTest::FindConnection;
using RtpMidiTest::FindEntry;
using RtpMidiTest::IsSuccess;
using RtpMidiTest::NewGuidText;
using RtpMidiTest::Peer;
using RtpMidiTest::SameText;
using RtpMidiTest::WaitFor;

namespace json = winrt::Windows::Data::Json;

namespace
{
    // {54c9b2f6-c235-4000-a675-9f6958a1a4fa}
    constexpr GUID TransportClsid{ 0x54c9b2f6, 0xc235, 0x4000, { 0xa6, 0x75, 0x9f, 0x69, 0x58, 0xa1, 0xa4, 0xfa } };

    // data bytes carried by the SysEx7 packets in a UMP word stream
    std::vector<uint8_t> SysEx7Payload(std::vector<uint32_t> const& words)
    {
        std::vector<uint8_t> payload;

        for (size_t i = 0; i + 1 < words.size(); )
        {
            auto const type = words[i] >> 28;
            size_t const length = type <= 2 ? 1 : type == 3 || type == 4 ? 2 : type == 5 ? 4 : 1;

            if (type == 3)
            {
                auto const count = (words[i] >> 16) & 0x0F;
                uint8_t const bytes[6] =
                {
                    static_cast<uint8_t>(words[i] >> 8), static_cast<uint8_t>(words[i]),
                    static_cast<uint8_t>(words[i + 1] >> 24), static_cast<uint8_t>(words[i + 1] >> 16),
                    static_cast<uint8_t>(words[i + 1] >> 8), static_cast<uint8_t>(words[i + 1])
                };

                for (uint32_t b = 0; b < count && b < 6; b++) payload.push_back(bytes[b]);
            }

            i += length;
        }

        return payload;
    }

    bool Contains(std::vector<uint32_t> const& words, uint32_t const word)
    {
        return std::find(words.begin(), words.end(), word) != words.end();
    }

    bool ContainsSequence(std::vector<uint8_t> const& haystack, std::vector<uint8_t> const& needle)
    {
        return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end()) != haystack.end();
    }

    // MIDI 1.0 Note On and Note Off UMPs, not counting the NOOPs and anything else
    uint32_t CountNotes(std::vector<uint32_t> const& words)
    {
        return static_cast<uint32_t>(std::count_if(words.begin(), words.end(), [](uint32_t const word)
        {
            return (word >> 28) == 2 && ((word >> 20) & 0xF) != 0 && (((word >> 16) & 0xF0) == 0x90 || ((word >> 16) & 0xF0) == 0x80);
        }));
    }
}

json::JsonObject RtpMidiTransportTests::Send(std::wstring const& text, HRESULT* result)
{
    LPWSTR response{ nullptr };
    auto const hr = m_configuration->UpdateConfiguration(text.c_str(), &response);
    if (result != nullptr) *result = hr;

    json::JsonObject parsed{ nullptr };

    if (response != nullptr)
    {
        json::JsonObject::TryParse(response, parsed);
        CoTaskMemFree(response);
    }

    return parsed;
}

json::JsonObject RtpMidiTransportTests::FindHost(std::wstring const& hostId)
{
    return FindEntry(Send(Command(L"enumerateHosts")), L"hosts", hostId);
}

json::JsonObject RtpMidiTransportTests::FindClient(std::wstring const& clientId)
{
    return FindEntry(Send(Command(L"enumerateClients")), L"clients", clientId);
}

bool RtpMidiTransportTests::ClassSetup()
{
    // the transport sits beside this DLL in the build output, unless /p:TransportDll=<path> names one
    std::wstring dllPath;
    String parameter;

    if (SUCCEEDED(RuntimeParameters::TryGetValue(L"TransportDll", parameter)) && !parameter.IsEmpty())
    {
        dllPath = static_cast<wchar_t const*>(parameter);
    }
    else
    {
        std::wstring modulePath(MAX_PATH, L'\0');
        modulePath.resize(GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), modulePath.data(), static_cast<DWORD>(modulePath.size())));
        dllPath = modulePath.substr(0, modulePath.find_last_of(L'\\') + 1) + L"Midi2.RtpMidiTransport.dll";
    }

    Log::Comment(String().Format(L"Transport: %s", dllPath.c_str()));

    auto const module = LoadLibraryExW(dllPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    VERIFY_IS_NOT_NULL(module, L"the transport DLL loads");

    using GetClassObject = HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, LPVOID*);
    auto const getClassObject = reinterpret_cast<GetClassObject>(GetProcAddress(module, "DllGetClassObject"));
    VERIFY_IS_NOT_NULL(getClassObject, L"the DLL exports DllGetClassObject");

    IClassFactory* factory{ nullptr };
    VERIFY_SUCCEEDED(getClassObject(TransportClsid, __uuidof(IClassFactory), reinterpret_cast<void**>(&factory)));

    auto const created = factory->CreateInstance(nullptr, __uuidof(IMidiTransport), reinterpret_cast<void**>(&m_transport));
    factory->Release();
    VERIFY_SUCCEEDED(created, L"the transport is created");

    m_deviceManager = new RtpMidiTest::MockDeviceManager();
    m_protocolManager = new RtpMidiTest::MockProtocolManager();

    VERIFY_SUCCEEDED(m_transport->Activate(__uuidof(IMidiTransportConfigurationManager), reinterpret_cast<void**>(&m_configuration)));
    VERIFY_SUCCEEDED(m_configuration->Initialize(TransportClsid, m_deviceManager, nullptr));

    // defined before the endpoint manager starts, as a host in the configuration file is
    m_hostId = NewGuidText();
    VERIFY_IS_TRUE(IsSuccess(Send(L"{\"create\":{\"hosts\":{\"" + m_hostId + L"\":{\"name\":\"Harness Host\",\"port\":\"auto\",\"advertise\":false}}}}")),
        L"a create section for one host is accepted");

    VERIFY_SUCCEEDED(m_transport->Activate(__uuidof(IMidiEndpointManager), reinterpret_cast<void**>(&m_endpointManager)));
    VERIFY_SUCCEEDED(m_endpointManager->Initialize(m_deviceManager, m_protocolManager));

    VERIFY_IS_TRUE(WaitFor([&]()
    {
        auto const host = FindHost(m_hostId);
        if (host == nullptr || !host.GetNamedBoolean(L"hasStarted", false)) return false;

        m_hostPort = static_cast<uint16_t>(host.GetNamedNumber(L"actualPort", 0));
        return m_hostPort != 0;
    }, 5000), L"the host starts on a port");

    Log::Comment(String().Format(L"The shared host is on port %u", m_hostPort));

    return true;
}

bool RtpMidiTransportTests::ClassCleanup()
{
    if (m_configuration != nullptr && !m_hostId.empty())
    {
        Send(Command(L"removeHost", { { L"entryIdentifier", m_hostId } }));
    }

    if (m_endpointManager != nullptr)
    {
        VERIFY_SUCCEEDED(m_endpointManager->Shutdown());
        m_endpointManager->Release();
    }

    if (m_configuration != nullptr)
    {
        VERIFY_SUCCEEDED(m_configuration->Shutdown());
        m_configuration->Release();
    }

    if (m_deviceManager != nullptr)
    {
        VERIFY_ARE_EQUAL(m_deviceManager->UnknownRemovals(), 0u, L"every removal named an endpoint the transport created");
    }

    if (m_transport != nullptr) m_transport->Release();

    return true;
}

void RtpMidiTransportTests::TestHostDefinedBeforeTheEndpointManagerStarts()
{
    VERIFY_IS_TRUE(m_deviceManager->HadParent(), L"the parent device is created when the endpoint manager starts");

    auto const host = FindHost(m_hostId);
    VERIFY_IS_TRUE(host != nullptr, L"the host is listed");

    VERIFY_IS_TRUE(host.GetNamedBoolean(L"hasStarted", false), L"the host has started");
    VERIFY_IS_TRUE(std::wstring{ host.GetNamedString(L"configuredPort", L"") } == L"auto", L"the host was configured with an automatic port");
    VERIFY_IS_FALSE(host.GetNamedBoolean(L"advertise", true), L"the host does not advertise");
    VERIFY_ARE_EQUAL(static_cast<uint32_t>(host.GetNamedNumber(L"actualPort", 0)), static_cast<uint32_t>(m_hostPort));
}

void RtpMidiTransportTests::TestRemoteInvitesHost()
{
    auto const baseline = m_deviceManager->Endpoints().size();

    Peer remote("Harness Peer", false);
    remote.SetSyncDelay(3);
    VERIFY_IS_TRUE(remote.Start(), L"the remote binds a loopback port pair");
    remote.Invite(m_hostPort);

    VERIFY_IS_TRUE(WaitFor([&]() { return remote.ConnectedCount() == 1; }, 5000), L"the remote reaches connected");
    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints().size() == baseline + 1; }, 5000), L"one endpoint is created for the connection");

    auto const endpoint = m_deviceManager->Endpoints()[baseline];
    Log::Comment(String().Format(L"Endpoint %s", endpoint.InstanceId.c_str()));

    VERIFY_IS_TRUE(endpoint.EndpointName == L"Harness Peer", L"the endpoint is named after the remote");
    VERIFY_IS_TRUE(endpoint.TransportCode == L"RTPMIDI", L"the transport code is RTPMIDI");
    VERIFY_IS_TRUE(endpoint.TransportId == TransportClsid, L"the endpoint carries the transport id");
    VERIFY_IS_TRUE(endpoint.NativeFormat == MidiDataFormats_ByteStream, L"the native format is a byte stream");
    VERIFY_IS_TRUE(endpoint.InstanceId.rfind(L"MIDIU_RTPMIDI_", 0) == 0, L"the instance id has the transport prefix");
    VERIFY_IS_TRUE(endpoint.PropertyCount > 0, L"the MIDI 1.0 port properties are supplied");
    VERIFY_IS_FALSE(m_deviceManager->WrongParent(), L"the endpoint is created under the transport's parent device");
    VERIFY_ARE_EQUAL(m_protocolManager->m_calls.load(), 0u, L"no MIDI 2.0 discovery is started for a byte stream endpoint");
    VERIFY_IS_TRUE(remote.LastRemoteName() == "Harness Host", L"the remote sees the host's name");

    RtpMidiTest::OpenedEndpoint opened(m_transport, endpoint.InterfaceId, 42);
    VERIFY_IS_TRUE(opened.IsOpen(), L"the service opens the endpoint");

    // remote to service
    std::vector<uint8_t> sysex{ 0xF0, 0x7D };
    for (uint8_t i = 0; i < 18; i++) sysex.push_back(static_cast<uint8_t>(0x10 + i));
    sysex.push_back(0xF7);

    remote.Send({ 0x90, 0x3C, 0x64 });
    remote.Send({ 0x80, 0x3C, 0x40 });
    remote.Send({ 0xB3, 0x07, 0x55 });
    remote.Send(sysex);

    std::vector<uint8_t> const expectedSysExPayload(sysex.begin() + 1, sysex.end() - 1);

    VERIFY_IS_TRUE(WaitFor([&]()
    {
        auto const words = opened.Received().Words();
        return Contains(words, 0x20903C64) && Contains(words, 0x20803C40) && Contains(words, 0x20B30755) &&
            SysEx7Payload(words) == expectedSysExPayload;
    }, 3000), L"remote messages arrive as UMP, SysEx intact");

    VERIFY_IS_FALSE(opened.Received().WrongContext(), L"every callback carries the context the service gave");
    VERIFY_IS_FALSE(opened.Received().BadPosition(), L"timestamps are set and never go backward");

    // service to remote: MIDI 1.0 and MIDI 2.0 channel voice, and a SysEx7 packet
    uint32_t const outgoing[] =
    {
        0x20904540,                 // MIDI 1.0 Note On, note 0x45, velocity 0x40
        0x40904800, 0x80000000,     // MIDI 2.0 Note On, note 0x48, velocity 0x8000
        0x30037D01, 0x02000000,     // SysEx7 complete in one packet: 7D 01 02
    };

    VERIFY_SUCCEEDED(opened.Send(outgoing, sizeof(outgoing)), L"the service sends to the endpoint");

    VERIFY_IS_TRUE(WaitFor([&]()
    {
        auto const received = remote.Received();
        return ContainsSequence(received, { 0x90, 0x45, 0x40 }) &&
            ContainsSequence(received, { 0x90, 0x48, 0x40 }) &&
            ContainsSequence(received, { 0xF0, 0x7D, 0x01, 0x02, 0xF7 });
    }, 3000), L"the remote receives MIDI 1.0 bytes for all three");

    VERIFY_IS_TRUE(WaitFor([&]() { return remote.AnyClockSync(); }, 5000), L"clock sync completes");

    json::JsonObject connection{ nullptr };
    VERIFY_IS_TRUE(WaitFor([&]()
    {
        connection = FindConnection(FindHost(m_hostId), L"Harness Peer");
        return connection != nullptr && connection.GetNamedNumber(L"currentLatencyTicks", 0) > 0;
    }, 5000), L"the host reports its connection with a latency figure");

    VERIFY_IS_TRUE(connection.GetNamedBoolean(L"connected", false), L"status: connected");
    VERIFY_IS_FALSE(connection.GetNamedBoolean(L"thisPcInvited", true), L"status: the remote invited");
    VERIFY_IS_TRUE(SameText(std::wstring{ connection.GetNamedString(L"endpointDeviceId", L"") }, endpoint.InterfaceId), L"status: endpoint id");
    VERIFY_IS_TRUE(connection.HasKey(L"remoteHostName") && connection.GetNamedString(L"remoteHostName", L"x").empty(),
        L"status: no host name for a remote that nothing advertises");
    VERIFY_IS_TRUE(connection.GetNamedNumber(L"totalNetworkPacketsReceived", 0) >= 4, L"status: packets received");
    VERIFY_ARE_EQUAL(connection.GetNamedNumber(L"totalMessagesSent", 0), 3.0, L"status: three messages sent");

    Log::Comment(String().Format(L"Latency %.0f ticks, best %.0f ticks",
        connection.GetNamedNumber(L"currentLatencyTicks", 0), connection.GetNamedNumber(L"bestLatencyTicks", 0)));

    uint64_t latencyTicks{ 0 };
    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->LatencyWritesFor(endpoint.InterfaceId, latencyTicks) > 0; }, 8000),
        L"the one-way latency is written to the endpoint for the scheduler");

    // clock sync is done by now, so the remote's RTP timestamps are mapped to local time
    remote.SendAhead({ 0x90, 0x3D, 0x21 }, 500);
    VERIFY_IS_TRUE(WaitFor([&]() { return Contains(opened.Received().Words(), 0x20903D21); }, 3000), L"a message the remote stamped 50 ms ahead arrives");
    VERIFY_IS_FALSE(opened.Received().StampedAfterArrival(), L"no message is stamped later than it reached the service");

    auto const connectionId = static_cast<uint32_t>(connection.GetNamedNumber(L"connectionId", 0));

    VERIFY_IS_TRUE(IsSuccess(Send(Command(L"disconnectRemoteClient",
        { { L"entryIdentifier", m_hostId }, { L"connectionId", std::to_wstring(connectionId) } }))), L"disconnect the remote from here");

    VERIFY_IS_TRUE(WaitFor([&]() { return !remote.Ended().empty(); }, 3000), L"the remote is told the connection ended");
    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints()[baseline].Removed; }, 3000), L"the endpoint is removed");
}

void RtpMidiTransportTests::TestClientConnectsToRemoteHost()
{
    auto const baseline = m_deviceManager->Endpoints().size();

    Peer remoteHost("Harness Remote Host", true);
    VERIFY_IS_TRUE(remoteHost.Start(), L"the remote host binds a loopback port pair");

    auto const clientId = NewGuidText();
    auto const clientSection =
        L"{\"create\":{\"clients\":{\"" + clientId + L"\":{\"name\":\"Harness Client\",\"remoteAddress\":\"::1\",\"remotePort\":" +
        std::to_wstring(remoteHost.ControlPort()) + L"}}}}";

    VERIFY_IS_TRUE(IsSuccess(Send(clientSection)), L"a create section for one client is accepted");
    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints().size() == baseline + 1; }, 8000), L"an endpoint is created for the client connection");

    auto const clientEndpoint = m_deviceManager->Endpoints()[baseline];
    VERIFY_IS_TRUE(clientEndpoint.EndpointName == L"Harness Remote Host", L"the endpoint is named after the remote host");
    VERIFY_IS_TRUE(remoteHost.LastRemoteName() == "Harness Client", L"the remote host sees this PC's client name");
    VERIFY_IS_TRUE(EntryState(FindClient(clientId)) == L"live", L"the client entry is live");

    {
        RtpMidiTest::OpenedEndpoint opened(m_transport, clientEndpoint.InterfaceId, 7);
        VERIFY_IS_TRUE(opened.IsOpen(), L"the service opens the client endpoint");

        remoteHost.Send({ 0x91, 0x40, 0x7F });
        VERIFY_IS_TRUE(WaitFor([&]() { return Contains(opened.Received().Words(), 0x2091407F); }, 3000), L"a message from the remote host arrives");

        // the remote ends it: a retry is due later, and the endpoint goes now
        remoteHost.EndAll();

        VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints()[baseline].Removed; }, 3000), L"the endpoint goes when the remote ends the connection");
    }

    json::JsonObject client{ nullptr };
    VERIFY_IS_TRUE(WaitFor([&]()
    {
        client = FindClient(clientId);
        return EntryState(client) == L"failed";
    }, 3000), L"the client entry reports failed, waiting to retry");

    VERIFY_ARE_EQUAL(static_cast<uint32_t>(client.GetNamedNumber(L"lastError", 0)), static_cast<uint32_t>(HRESULT_FROM_WIN32(ERROR_GRACEFUL_DISCONNECT)),
        L"the last error says the remote ended it");

    VERIFY_IS_TRUE(IsSuccess(Send(Command(L"reconnectClient", { { L"entryIdentifier", clientId } }))), L"reconnect the client now");
    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints().size() == baseline + 2; }, 8000), L"the client connects again");

    auto const reconnected = m_deviceManager->Endpoints()[baseline + 1];
    VERIFY_IS_TRUE(SameText(reconnected.InstanceId, clientEndpoint.InstanceId), L"the endpoint comes back with the same instance id");
    VERIFY_IS_TRUE(reconnected.UniqueIdentifier == clientEndpoint.UniqueIdentifier, L"and the same unique identifier");

    auto const endsBefore = remoteHost.Ended().size();

    VERIFY_IS_TRUE(IsSuccess(Send(Command(L"removeClient", { { L"entryIdentifier", clientId } }))), L"remove the client");
    VERIFY_IS_TRUE(WaitFor([&]() { return remoteHost.Ended().size() > endsBefore; }, 3000), L"the remote host is told");
    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints()[baseline + 1].Removed; }, 3000), L"the endpoint is removed");
    VERIFY_IS_TRUE(FindClient(clientId) == nullptr, L"the client is no longer listed");
}

void RtpMidiTransportTests::TestSameNameFromTwoRemotesGetsTwoEndpoints()
{
    auto const baseline = m_deviceManager->Endpoints().size();

    // the engine treats one name from one address as the same device, so the second is on IPv4
    Peer first("Same Name", false);
    Peer second("Same Name", false, true);
    VERIFY_IS_TRUE(first.Start() && second.Start(), L"two remotes bind");

    first.Invite(m_hostPort);
    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints().size() == baseline + 1; }, 5000), L"the first gets an endpoint");

    second.Invite(m_hostPort);
    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints().size() == baseline + 2; }, 5000), L"the second gets its own endpoint");

    auto const endpoints = m_deviceManager->Endpoints();
    VERIFY_IS_FALSE(SameText(endpoints[baseline].InstanceId, endpoints[baseline + 1].InstanceId), L"the two instance ids differ");
    VERIFY_IS_FALSE(endpoints[baseline].Removed, L"the first endpoint is untouched by the second");

    first.Stop();
    second.Stop();

    VERIFY_IS_TRUE(WaitFor([&]()
    {
        auto const e = m_deviceManager->Endpoints();
        return e[baseline].Removed && e[baseline + 1].Removed;
    }, 3000), L"both endpoints go when the remotes leave");
}

void RtpMidiTransportTests::TestRestartedRemoteReplacesItsOldConnection()
{
    auto const baseline = m_deviceManager->Endpoints().size();

    // a device which restarts invites again from a new port with a new SSRC, and never said goodbye
    auto firstLife = std::make_unique<Peer>("Restarting Peer", false);
    VERIFY_IS_TRUE(firstLife->Start(), L"the restarting remote binds");
    firstLife->Invite(m_hostPort);
    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints().size() == baseline + 1; }, 5000), L"the restarting remote gets an endpoint");

    auto secondLife = std::make_unique<Peer>("Restarting Peer", false);
    VERIFY_IS_TRUE(secondLife->Start(), L"its second life binds");
    secondLife->Invite(m_hostPort);
    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints().size() == baseline + 2; }, 5000), L"it gets an endpoint again");

    auto endpoints = m_deviceManager->Endpoints();
    VERIFY_IS_TRUE(endpoints[baseline].Removed, L"the old endpoint was removed first");
    VERIFY_IS_TRUE(SameText(endpoints[baseline].InstanceId, endpoints[baseline + 1].InstanceId), L"the new endpoint has the same instance id");

    // the first life's goodbye arrives late, and must not end the second life's connection
    firstLife.reset();
    Sleep(500);

    endpoints = m_deviceManager->Endpoints();
    VERIFY_IS_FALSE(endpoints[baseline + 1].Removed, L"a late goodbye from the old life leaves the new endpoint alone");
    VERIFY_IS_TRUE(secondLife->ConnectedCount() == 1, L"and the new connection stays up");

    secondLife->Stop();

    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints()[baseline + 1].Removed; }, 3000), L"the endpoint goes when it leaves");
}

void RtpMidiTransportTests::TestHostileConfigurationIsRejected()
{
    struct Case { std::wstring Json; uint32_t ExpectedError; wchar_t const* What; };

    auto const badId = NewGuidText();
    std::wstring const longName(64, L'n');

    std::vector<Case> const cases =
    {
        { L"this is not json", 1, L"not JSON" },
        { L"{\"create\":[]}", 0, L"create is an array" },
        { L"{\"create\":{\"hosts\":[]}}", 0, L"hosts is an array" },
        { L"{\"create\":{\"hosts\":{\"not-a-guid\":{\"name\":\"x\"}}}}", 4, L"entry key is not a GUID" },
        { L"{\"create\":{\"hosts\":{\"" + badId + L"\":\"text\"}}}", 6, L"entry is not an object" },
        { L"{\"create\":{\"hosts\":{\"" + badId + L"\":{\"name\":123}}}}", 12, L"name is a number" },
        { L"{\"create\":{\"hosts\":{\"" + badId + L"\":{\"name\":\"" + longName + L"\"}}}}", 9, L"name longer than 63 bytes" },
        { L"{\"create\":{\"hosts\":{\"" + badId + L"\":{\"name\":\"a.b\"}}}}", 12, L"advertised name with a period" },
        { L"{\"create\":{\"hosts\":{\"" + badId + L"\":{\"name\":\"x\",\"port\":\"99999\"}}}}", 10, L"port out of range" },
        { L"{\"create\":{\"hosts\":{\"" + badId + L"\":{\"name\":\"x\",\"port\":true}}}}", 10, L"port is a boolean" },
        { L"{\"create\":{\"clients\":{\"" + badId + L"\":{\"name\":\"x\"}}}}", 8, L"client with no remote" },
        { L"{\"create\":{\"clients\":{\"" + badId + L"\":{\"name\":\"x\",\"remoteAddress\":\"::1\",\"serviceInstanceName\":\"y\"}}}}", 8, L"client with two remotes" },
        { L"{\"transportCommand\":{\"commandName\":123}}", 2, L"command name is a number" },
        { L"{\"transportCommand\":{\"commandName\":\"startHost\",\"commandArguments\":{\"entryIdentifier\":\"zzz\"}}}", 4, L"command with a bad entry id" },
        { L"{\"transportCommand\":{\"commandName\":\"startHost\"}}", 3, L"command with no entry id" },
        { L"{\"transportCommand\":{\"commandName\":\"disconnectRemoteClient\",\"commandArguments\":{\"entryIdentifier\":\"" + m_hostId + L"\",\"connectionId\":\"-1\"}}}", 11, L"negative connection id" },
        { L"{\"update\":[1,\"x\",{\"match\":7}]}", 0, L"customization array of junk" },
    };

    for (auto const& testCase : cases)
    {
        HRESULT callResult{ S_OK };
        auto const response = Send(testCase.Json, &callResult);

        bool ok = SUCCEEDED(callResult) && response != nullptr;
        if (ok && testCase.ExpectedError != 0) ok = !IsSuccess(response) && ErrorCode(response) == testCase.ExpectedError;

        VERIFY_IS_TRUE(ok, String().Format(L"%s: hr 0x%08X, success %d, error %u (wanted %u)", testCase.What, static_cast<unsigned>(callResult),
            IsSuccess(response) ? 1 : 0, ErrorCode(response), testCase.ExpectedError));
    }

    VERIFY_IS_TRUE(FindHost(badId) == nullptr, L"no rejected entry was kept");
    VERIFY_IS_TRUE(FindClient(badId) == nullptr, L"no rejected client was kept");
}

void RtpMidiTransportTests::TestDeeplyNestedJsonIsRejected()
{
    // past the nesting limit, the JSON parser throws rather than returning false
    std::wstring deep;
    for (int i = 0; i < 20000; i++) deep += L"{\"a\":";
    deep += L"1";
    for (int i = 0; i < 20000; i++) deep += L"}";

    HRESULT result{ S_OK };
    auto const response = Send(deep, &result);

    VERIFY_SUCCEEDED(result);
    VERIFY_ARE_EQUAL(ErrorCode(response), 1u, L"deeply nested JSON is answered as invalid JSON");
}

void RtpMidiTransportTests::TestEightRemotesWithMidiBothWays()
{
    constexpr size_t RemoteCount = 8;
    constexpr uint32_t NotesEachWay = 500;

    auto const baseline = m_deviceManager->Endpoints().size();

    std::vector<std::unique_ptr<Peer>> remotes;

    for (size_t i = 0; i < RemoteCount; i++)
    {
        remotes.push_back(std::make_unique<Peer>("Stress " + std::to_string(i + 1), false));
        VERIFY_IS_TRUE(remotes.back()->Start(), L"a stress remote binds");
        remotes.back()->Invite(m_hostPort);
    }

    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints().size() == baseline + RemoteCount; }, 8000), L"all eight get endpoints");

    auto const all = m_deviceManager->Endpoints();
    std::vector<std::unique_ptr<RtpMidiTest::OpenedEndpoint>> opened;

    for (size_t i = baseline; i < all.size(); i++)
    {
        opened.push_back(std::make_unique<RtpMidiTest::OpenedEndpoint>(m_transport, all[i].InterfaceId, static_cast<LONGLONG>(100 + i)));
        VERIFY_IS_TRUE(opened.back()->IsOpen(), L"a stress endpoint opens");
    }

    // every remote and every endpoint sends at the same time
    std::vector<std::thread> senders;

    for (size_t r = 0; r < remotes.size(); r++)
    {
        senders.emplace_back([&, r]()
        {
            for (uint32_t n = 0; n < NotesEachWay; n++)
            {
                auto const note = static_cast<uint8_t>(n % 128);
                remotes[r]->Send({ 0x90, note, 0x40 });
                remotes[r]->Send({ 0x80, note, 0x40 });
                if (n % 20 == 19) Sleep(1);
            }
        });
    }

    for (size_t b = 0; b < opened.size(); b++)
    {
        senders.emplace_back([&, b]()
        {
            for (uint32_t n = 0; n < NotesEachWay; n++)
            {
                uint32_t const words[2] = { 0x20900040u | ((n % 128) << 8), 0x20800040u | ((n % 128) << 8) };
                opened[b]->Send(words, sizeof(words));
                if (n % 20 == 19) Sleep(1);
            }
        });
    }

    for (auto& sender : senders) sender.join();

    bool const allArrived = WaitFor([&]()
    {
        for (auto const& endpoint : opened) if (CountNotes(endpoint->Received().Words()) != NotesEachWay * 2) return false;
        for (auto const& remote : remotes) if (remote->Received().size() != NotesEachWay * 2 * 3) return false;
        return true;
    }, 10000);

    if (!allArrived)
    {
        for (size_t i = 0; i < opened.size(); i++) Log::Comment(String().Format(L"Endpoint %zu received %u notes", i, CountNotes(opened[i]->Received().Words())));
        for (size_t i = 0; i < remotes.size(); i++) Log::Comment(String().Format(L"Remote %zu received %zu bytes", i, remotes[i]->Received().size()));

        // what the transport saw on the wire, to tell loss in the network from loss in the transport
        auto const host = FindHost(m_hostId);
        auto const connections = host != nullptr ? host.GetNamedArray(L"connections") : json::JsonArray{};

        for (uint32_t i = 0; i < connections.Size(); i++)
        {
            auto const c = connections.GetObjectAt(i);
            Log::Comment(String().Format(L"%s: %.0f packets received, %.0f lost, %.0f repaired from the journal, %.0f messages received",
                c.GetNamedString(L"remoteName", L"").c_str(), c.GetNamedNumber(L"totalNetworkPacketsReceived", 0), c.GetNamedNumber(L"totalPacketsLost", 0),
                c.GetNamedNumber(L"totalLossesRepairedFromJournal", 0), c.GetNamedNumber(L"totalMessagesReceived", 0)));
        }
    }

    VERIFY_IS_TRUE(allArrived, L"every message arrives, in both directions, on every connection");

    for (auto const& endpoint : opened)
    {
        VERIFY_IS_FALSE(endpoint->Received().WrongContext(), L"no message reached the wrong endpoint");
        VERIFY_IS_FALSE(endpoint->Received().BadPosition(), L"timestamps held their order");
    }

    for (auto& remote : remotes) remote->Stop();

    VERIFY_IS_TRUE(WaitFor([&]()
    {
        auto const e = m_deviceManager->Endpoints();
        for (size_t i = baseline; i < e.size(); i++) if (!e[i].Removed) return false;
        return true;
    }, 5000), L"all eight endpoints go when the remotes leave");
}

void RtpMidiTransportTests::TestConnectionChurn()
{
    for (int cycle = 0; cycle < 10; cycle++)
    {
        Peer churn("Churn", false);
        VERIFY_IS_TRUE(churn.Start(), L"the remote binds");

        auto const before = m_deviceManager->Endpoints().size();
        churn.Invite(m_hostPort);

        VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints().size() == before + 1; }, 5000),
            String().Format(L"round %d creates one endpoint", cycle + 1));

        churn.Stop();

        VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints()[before].Removed; }, 5000),
            String().Format(L"round %d removes it", cycle + 1));
    }
}

void RtpMidiTransportTests::TestRemoveHost()
{
    auto const hostId = NewGuidText();

    VERIFY_IS_TRUE(IsSuccess(Send(L"{\"create\":{\"hosts\":{\"" + hostId + L"\":{\"name\":\"Removal Host\",\"port\":\"auto\",\"advertise\":false}}}}")),
        L"a second host is accepted");

    VERIFY_IS_TRUE(WaitFor([&]()
    {
        auto const host = FindHost(hostId);
        return host != nullptr && host.GetNamedBoolean(L"hasStarted", false);
    }, 5000), L"the second host starts");

    VERIFY_IS_TRUE(IsSuccess(Send(Command(L"removeHost", { { L"entryIdentifier", hostId } }))), L"remove the host");
    VERIFY_IS_TRUE(WaitFor([&]() { return FindHost(hostId) == nullptr; }, 3000), L"the host is gone");
    VERIFY_IS_FALSE(IsSuccess(Send(Command(L"removeHost", { { L"entryIdentifier", hostId } }))), L"removing it again fails");
    VERIFY_IS_TRUE(FindHost(m_hostId) != nullptr, L"the shared host is untouched");
}
