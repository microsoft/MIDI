// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// PROTOTYPE. Runs the rtpMIDI service transport DLL in this process, without the MIDI service.
//
// The transport is loaded through DllGetClassObject and handed mock versions of the service
// interfaces it calls, so its endpoint creation, configuration and commands can be checked
// without admin rights or a deployed plugin. Real rtpMIDI peers, built on the same protocol
// engine, talk to it over ::1. Nothing is advertised, so nothing on the network is disturbed.
//
//   rtpmidi-spike transport-test [--dll PATH]
// ============================================================================

#include "spike_common.h"
#include "spike_net.h"
#include "spike_mdns_watch.h"

#include "../transport/RtpMidiMdns.h"
#include "midi_dnssd_announcer.h"

#include <objbase.h>
#include "WindowsMidiServices.h"
#include "WindowsMidiServices_i.c"

#undef GetObject
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <thread>

namespace json = winrt::Windows::Data::Json;

#ifdef RTP_SDK_CHECK
bool SdkCheckStart(std::wstring const& dllPath);
bool SdkStaticsAreRight();
std::wstring SdkHostSection(std::wstring const& name, std::wstring& hostId);
std::wstring SdkClientSection(std::wstring const& name, std::wstring const& address, uint16_t port, std::wstring const& customEndpointName, std::wstring& clientId);
std::wstring SdkRemovalSection(std::wstring const& entryId, bool isHost);
#endif

using Spike::Print;
using Spike::ToUtf8;
using Spike::ToWide;

namespace
{
    // {54c9b2f6-c235-4000-a675-9f6958a1a4fa}
    constexpr GUID TransportClsid{ 0x54c9b2f6, 0xc235, 0x4000, { 0xa6, 0x75, 0x9f, 0x69, 0x58, 0xa1, 0xa4, 0xfa } };

    int g_checks{ 0 };
    int g_failures{ 0 };

    void Check(bool condition, char const* what)
    {
        g_checks++;
        if (condition) return;

        g_failures++;
        Print("  FAILED: %s", what);
    }

    bool WaitFor(std::function<bool()> const& condition, uint32_t milliseconds)
    {
        auto const end = GetTickCount64() + milliseconds;

        while (GetTickCount64() < end)
        {
            if (condition()) return true;
            Sleep(20);
        }

        return condition();
    }

    bool SameText(std::wstring const& left, std::wstring const& right)
    {
        return _wcsicmp(left.c_str(), right.c_str()) == 0;
    }

    LPWSTR CoTaskCopy(std::wstring const& text)
    {
        auto const bytes = (text.size() + 1) * sizeof(wchar_t);
        auto copy = static_cast<LPWSTR>(CoTaskMemAlloc(bytes));
        if (copy != nullptr) memcpy(copy, text.c_str(), bytes);
        return copy;
    }

    // Plain reference counting is enough: every mock outlives the transport in this test
    template <typename Interface>
    class MockBase : public Interface
    {
    public:
        STDMETHODIMP QueryInterface(REFIID riid, void** object) override
        {
            if (object == nullptr) return E_POINTER;

            if (riid == __uuidof(IUnknown) || riid == __uuidof(Interface))
            {
                *object = static_cast<Interface*>(this);
                AddRef();
                return S_OK;
            }

            *object = nullptr;
            return E_NOINTERFACE;
        }

        STDMETHODIMP_(ULONG) AddRef() override { return ++m_references; }
        STDMETHODIMP_(ULONG) Release() override { return --m_references; }

    private:
        std::atomic<ULONG> m_references{ 1 };
    };

    struct ActivatedEndpoint
    {
        std::wstring InstanceId;
        std::wstring InterfaceId;
        std::wstring EndpointName;
        std::wstring FriendlyName;
        std::wstring Description;
        std::wstring UniqueIdentifier;
        std::wstring TransportCode;
        GUID TransportId{};
        MidiDataFormats NativeFormat{ MidiDataFormats_Invalid };
        ULONG PropertyCount{ 0 };
        bool Removed{ false };
    };

    class MockDeviceManager : public MockBase<IMidiDeviceManager>
    {
    public:
        STDMETHODIMP ActivateVirtualParentDevice(ULONG, const DEVPROPERTY*, const SW_DEVICE_CREATE_INFO* createInfo, LPWSTR* createdDeviceId) override
        {
            if (createInfo == nullptr || createInfo->pszInstanceId == nullptr || createdDeviceId == nullptr) return E_INVALIDARG;

            auto lock = std::scoped_lock{ m_lock };
            m_parentId = std::wstring{ L"SWD\\MIDISRV\\" } + createInfo->pszInstanceId;
            *createdDeviceId = CoTaskCopy(m_parentId);

            return *createdDeviceId == nullptr ? E_OUTOFMEMORY : S_OK;
        }

        STDMETHODIMP DeactivateVirtualParentDevice(LPCWSTR) override { return S_OK; }

        STDMETHODIMP ActivateEndpoint(
            LPCWSTR parentInstanceId,
            BOOL,
            MidiFlow,
            const PMIDIENDPOINTCOMMONPROPERTIES common,
            ULONG intPropertyCount,
            ULONG,
            const DEVPROPERTY*,
            const DEVPROPERTY*,
            const SW_DEVICE_CREATE_INFO* createInfo,
            LPWSTR* createdEndpointDeviceInterfaceId) override
        {
            if (common == nullptr || createInfo == nullptr || createInfo->pszInstanceId == nullptr || createdEndpointDeviceInterfaceId == nullptr) return E_INVALIDARG;

            *createdEndpointDeviceInterfaceId = nullptr;

            auto lock = std::scoped_lock{ m_lock };

            if (parentInstanceId == nullptr || !SameText(parentInstanceId, m_parentId)) m_wrongParent = true;

            std::wstring const instanceId{ createInfo->pszInstanceId };

            // the real device manager reports an instance id which is already active this way
            for (auto const& existing : m_endpoints)
            {
                if (!existing.Removed && SameText(existing.InstanceId, instanceId)) return S_FALSE;
            }

            ActivatedEndpoint endpoint{};
            endpoint.InstanceId = instanceId;
            endpoint.InterfaceId = L"\\\\?\\SWD#MIDISRV#" + instanceId + L"#{e7cce071-3c03-423f-88d3-f1045d02552b}";
            endpoint.EndpointName = common->EndpointName != nullptr ? common->EndpointName : L"";
            endpoint.FriendlyName = common->FriendlyName != nullptr ? common->FriendlyName : L"";
            endpoint.Description = common->EndpointDescription != nullptr ? common->EndpointDescription : L"";
            endpoint.UniqueIdentifier = common->UniqueIdentifier != nullptr ? common->UniqueIdentifier : L"";
            endpoint.TransportCode = common->TransportCode != nullptr ? common->TransportCode : L"";
            endpoint.TransportId = common->TransportId;
            endpoint.NativeFormat = common->NativeDataFormat;
            endpoint.PropertyCount = intPropertyCount;

            *createdEndpointDeviceInterfaceId = CoTaskCopy(endpoint.InterfaceId);
            m_endpoints.push_back(endpoint);

            return S_OK;
        }

        STDMETHODIMP UpdateEndpointProperties(LPCWSTR endpointDeviceInterfaceId, ULONG count, const DEVPROPERTY* properties) override
        {
            if (endpointDeviceInterfaceId == nullptr || properties == nullptr) return E_INVALIDARG;

            auto lock = std::scoped_lock{ m_lock };

            for (ULONG i = 0; i < count; i++)
            {
                // PKEY_MIDI_MidiOutCalculatedLatencyTicks is property 800 in the MIDI set
                if (properties[i].CompKey.Key.pid == 800 && properties[i].BufferSize == sizeof(uint64_t) && properties[i].Buffer != nullptr)
                {
                    m_latencyWrites.emplace_back(endpointDeviceInterfaceId, *static_cast<uint64_t const*>(properties[i].Buffer));
                }
            }

            return S_OK;
        }

        STDMETHODIMP DeleteEndpointProperties(LPCWSTR, ULONG, const DEVPROPERTY*) override { return S_OK; }
        STDMETHODIMP DeactivateEndpoint(LPCWSTR) override { return S_OK; }

        STDMETHODIMP RemoveEndpoint(LPCWSTR instanceId) override
        {
            if (instanceId == nullptr) return E_INVALIDARG;

            auto lock = std::scoped_lock{ m_lock };

            for (auto& endpoint : m_endpoints)
            {
                if (!endpoint.Removed && SameText(endpoint.InstanceId, instanceId))
                {
                    endpoint.Removed = true;
                    return S_OK;
                }
            }

            m_unknownRemovals++;
            return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
        }

        STDMETHODIMP RebuildMidi1PortsForEndpoint(LPCWSTR) override { return S_OK; }
        STDMETHODIMP UpdateTransportConfiguration(GUID, LPCWSTR, LPWSTR*) override { return E_NOTIMPL; }

        std::vector<ActivatedEndpoint> Endpoints()
        {
            auto lock = std::scoped_lock{ m_lock };
            return m_endpoints;
        }

        size_t LatencyWritesFor(std::wstring const& interfaceId, uint64_t& lastValue)
        {
            auto lock = std::scoped_lock{ m_lock };

            size_t count = 0;
            for (auto const& write : m_latencyWrites)
            {
                if (SameText(write.first, interfaceId))
                {
                    count++;
                    lastValue = write.second;
                }
            }

            return count;
        }

        bool HadParent() { auto lock = std::scoped_lock{ m_lock }; return !m_parentId.empty(); }
        bool WrongParent() { auto lock = std::scoped_lock{ m_lock }; return m_wrongParent; }
        uint32_t UnknownRemovals() { auto lock = std::scoped_lock{ m_lock }; return m_unknownRemovals; }

    private:
        std::mutex m_lock;
        std::wstring m_parentId;
        std::vector<ActivatedEndpoint> m_endpoints;
        std::vector<std::pair<std::wstring, uint64_t>> m_latencyWrites;
        bool m_wrongParent{ false };
        uint32_t m_unknownRemovals{ 0 };
    };

    class MockProtocolManager : public MockBase<IMidiEndpointProtocolManager>
    {
    public:
        STDMETHODIMP DiscoverAndNegotiate(GUID, LPCWSTR, ENDPOINTPROTOCOLNEGOTIATIONPARAMS) override
        {
            m_calls++;
            return E_NOTIMPL;
        }

        STDMETHODIMP_(BOOL) IsEnabled() override { return FALSE; }

        std::atomic<uint32_t> m_calls{ 0 };
    };

    // What the service's client pipe would receive from the endpoint
    class MockCallback : public MockBase<IMidiCallback>
    {
    public:
        STDMETHODIMP Callback(MessageOptionFlags, PVOID message, UINT size, LONGLONG position, LONGLONG context) override
        {
            LARGE_INTEGER now{};
            QueryPerformanceCounter(&now);

            if (message == nullptr || size < sizeof(uint32_t)) return E_INVALIDARG;

            auto lock = std::scoped_lock{ m_lock };

            auto const words = static_cast<uint32_t const*>(message);
            m_words.insert(m_words.end(), words, words + size / sizeof(uint32_t));

            if (context != m_expectedContext) m_wrongContext = true;
            if (position <= 0 || position < m_lastPosition) m_badPosition = true;
            if (position > now.QuadPart) m_stampedAfterArrival = true;
            m_lastPosition = position;

            return S_OK;
        }

        void Expect(LONGLONG context) { auto lock = std::scoped_lock{ m_lock }; m_expectedContext = context; }
        std::vector<uint32_t> Words() { auto lock = std::scoped_lock{ m_lock }; return m_words; }
        void Clear() { auto lock = std::scoped_lock{ m_lock }; m_words.clear(); }
        bool WrongContext() { auto lock = std::scoped_lock{ m_lock }; return m_wrongContext; }
        bool BadPosition() { auto lock = std::scoped_lock{ m_lock }; return m_badPosition; }
        bool StampedAfterArrival() { auto lock = std::scoped_lock{ m_lock }; return m_stampedAfterArrival; }

    private:
        std::mutex m_lock;
        std::vector<uint32_t> m_words;
        LONGLONG m_expectedContext{ 0 };
        LONGLONG m_lastPosition{ 0 };
        bool m_wrongContext{ false };
        bool m_badPosition{ false };
        bool m_stampedAfterArrival{ false };
    };

    // A remote rtpMIDI device: the protocol engine on a loopback-only port pair
    class Peer : public RtpMidi::ISessionHost
    {
    public:
        Peer(std::string const& name, bool acceptInvitations, bool ipv4 = false) :
            m_ipv4(ipv4),
            m_session(MakeConfig(name, acceptInvitations), *this, Spike::SecureRandom64())
        {
        }

        ~Peer() { Stop(); }

        bool Start()
        {
            std::string failure;
            if (!m_ports.Bind(0, 47600, 47998, failure, true, m_ipv4))
            {
                Print("  peer could not bind: %s", failure.c_str());
                return false;
            }

            m_ports.Control().StartReceiving([this](RtpMidi::PeerAddress const& from, uint8_t const* data, size_t size)
            {
                auto lock = std::scoped_lock{ m_lock };
                m_session.OnDatagram(true, from, data, size, m_clock.Now());
            });

            m_ports.Data().StartReceiving([this](RtpMidi::PeerAddress const& from, uint8_t const* data, size_t size)
            {
                auto lock = std::scoped_lock{ m_lock };
                m_session.OnDatagram(false, from, data, size, m_clock.Now());
            });

            m_ticker = std::thread([this]()
            {
                while (!m_stopping)
                {
                    {
                        auto lock = std::scoped_lock{ m_lock };
                        m_session.Tick(m_clock.Now());
                    }

                    Sleep(5);
                }
            });

            return true;
        }

        void Stop()
        {
            if (m_stopped.exchange(true)) return;

            {
                auto lock = std::scoped_lock{ m_lock };
                m_session.EndAll(m_clock.Now());
                m_session.Tick(m_clock.Now());
            }

            m_stopping = true;
            if (m_ticker.joinable()) m_ticker.join();

            m_ports.Close();
        }

        uint16_t ControlPort() { return m_ports.Control().Port(); }

        void Invite(uint16_t port)
        {
            RtpMidi::PeerAddress target{};
            Spike::TryParseAddress(m_ipv4 ? L"127.0.0.1" : L"::1", port, target);

            auto lock = std::scoped_lock{ m_lock };
            m_session.Invite(target, m_clock.Now());
        }

        void Send(std::vector<uint8_t> const& bytes)
        {
            auto lock = std::scoped_lock{ m_lock };
            m_session.SendMidi(bytes.data(), bytes.size(), m_clock.Now());
        }

        // like a sender that schedules ahead: the RTP timestamp is later than the send
        void SendAhead(std::vector<uint8_t> const& bytes, uint64_t const aheadSessionTicks)
        {
            auto lock = std::scoped_lock{ m_lock };
            m_session.SendMidi(bytes.data(), bytes.size(), m_clock.Now() + aheadSessionTicks);
        }

        void EndAll()
        {
            auto lock = std::scoped_lock{ m_lock };
            m_session.EndAll(m_clock.Now());
        }

        size_t ConnectedCount()
        {
            auto lock = std::scoped_lock{ m_lock };
            return m_session.ConnectedCount();
        }

        bool AnyClockSync()
        {
            auto lock = std::scoped_lock{ m_lock };
            for (auto const& participant : m_session.Snapshot())
            {
                if (participant.HaveClockOffset) return true;
            }
            return false;
        }

        std::vector<uint8_t> Received() { auto lock = std::scoped_lock{ m_lock }; return m_received; }
        void ClearReceived() { auto lock = std::scoped_lock{ m_lock }; m_received.clear(); }
        std::vector<RtpMidi::EndReason> Ended() { auto lock = std::scoped_lock{ m_lock }; return m_ended; }
        std::string LastRemoteName() { auto lock = std::scoped_lock{ m_lock }; return m_lastRemoteName; }

        // Loopback answers in well under one 100 microsecond clock tick, which would measure as no
        // latency at all. A delay on clock sync replies makes the round trip visible.
        void SetSyncDelay(DWORD milliseconds) { m_syncDelayMilliseconds = milliseconds; }

        // ISessionHost, called with m_lock held
        void SendControl(RtpMidi::PeerAddress const& to, std::vector<uint8_t> const& datagram) override { m_ports.Control().Send(to, datagram); }

        void SendData(RtpMidi::PeerAddress const& to, std::vector<uint8_t> const& datagram) override
        {
            RtpMidi::AppleMidiCommand command{};

            if (m_syncDelayMilliseconds != 0 &&
                RtpMidi::TryGetAppleMidiCommand(datagram.data(), datagram.size(), command) &&
                command == RtpMidi::AppleMidiCommand::Synchronization)
            {
                Sleep(m_syncDelayMilliseconds);
            }

            m_ports.Data().Send(to, datagram);
        }

        void OnMidi(RtpMidi::Participant const&, uint64_t, int64_t, bool, std::vector<uint8_t> const& bytes) override
        {
            m_received.insert(m_received.end(), bytes.begin(), bytes.end());
        }

        void OnParticipantChanged(RtpMidi::Participant const& participant) override
        {
            if (participant.State == RtpMidi::ParticipantState::Connected) m_lastRemoteName = participant.RemoteName;
            if (participant.State == RtpMidi::ParticipantState::Ended) m_ended.push_back(participant.Reason);
        }

        void Log(std::string const&) override {}

    private:
        static RtpMidi::SessionConfig MakeConfig(std::string const& name, bool acceptInvitations)
        {
            RtpMidi::SessionConfig config{};
            config.LocalName = name;
            config.Ssrc = static_cast<uint32_t>(Spike::SecureRandom64());
            config.AcceptInvitations = acceptInvitations;
            config.SendJournal = true;
            return config;
        }

        Spike::SessionClock m_clock;
        bool m_ipv4{ false };
        Spike::PortPair m_ports;
        std::mutex m_lock;
        RtpMidi::Session m_session;
        std::thread m_ticker;
        std::atomic<bool> m_stopping{ false };
        std::atomic<bool> m_stopped{ false };
        std::vector<uint8_t> m_received;
        std::vector<RtpMidi::EndReason> m_ended;
        std::string m_lastRemoteName;
        std::atomic<DWORD> m_syncDelayMilliseconds{ 0 };
    };

    // Sends one configuration section or command and parses the answer
    json::JsonObject Send(IMidiTransportConfigurationManager* configuration, std::wstring const& text, HRESULT* result = nullptr)
    {
        LPWSTR response{ nullptr };
        auto const hr = configuration->UpdateConfiguration(text.c_str(), &response);
        if (result != nullptr) *result = hr;

        json::JsonObject parsed{ nullptr };

        if (response != nullptr)
        {
            json::JsonObject::TryParse(response, parsed);
            CoTaskMemFree(response);
        }

        return parsed;
    }

    bool IsSuccess(json::JsonObject const& response)
    {
        return response != nullptr && response.GetNamedBoolean(L"success", false);
    }

    uint32_t ErrorCode(json::JsonObject const& response)
    {
        if (response == nullptr || !response.HasKey(L"errorCode")) return 0;
        return static_cast<uint32_t>(response.GetNamedNumber(L"errorCode", 0));
    }

    std::wstring Command(std::wstring const& verb, std::map<std::wstring, std::wstring> const& arguments = {})
    {
        json::JsonObject argumentsObject;
        for (auto const& argument : arguments) argumentsObject.SetNamedValue(argument.first, json::JsonValue::CreateStringValue(argument.second));

        json::JsonObject command;
        command.SetNamedValue(L"commandName", json::JsonValue::CreateStringValue(verb));
        command.SetNamedValue(L"commandArguments", argumentsObject);

        json::JsonObject root;
        root.SetNamedValue(L"transportCommand", command);

        return std::wstring{ root.Stringify() };
    }

    std::wstring NewGuidText()
    {
        GUID guid{};
        CoCreateGuid(&guid);

        wchar_t buffer[40]{};
        StringFromGUID2(guid, buffer, ARRAYSIZE(buffer));
        return buffer;
    }

    json::JsonObject FindEntry(json::JsonObject const& response, std::wstring const& arrayKey, std::wstring const& entryId)
    {
        if (response == nullptr || !response.HasKey(arrayKey)) return nullptr;

        auto const entries = response.GetNamedArray(arrayKey);

        for (uint32_t i = 0; i < entries.Size(); i++)
        {
            auto const entry = entries.GetObjectAt(i);
            if (SameText(std::wstring{ entry.GetNamedString(L"entryIdentifier", L"") }, entryId)) return entry;
        }

        return nullptr;
    }

    json::JsonObject FirstConnection(json::JsonObject const& entry)
    {
        if (entry == nullptr || !entry.HasKey(L"connections")) return nullptr;

        auto const connections = entry.GetNamedArray(L"connections");
        return connections.Size() == 0 ? nullptr : connections.GetObjectAt(0);
    }

    // Data bytes carried by the SysEx7 packets in a UMP word stream
    std::vector<uint8_t> SysEx7Payload(std::vector<uint32_t> const& words)
    {
        std::vector<uint8_t> payload;

        for (size_t i = 0; i + 1 < words.size(); )
        {
            auto const type = words[i] >> 28;
            size_t const length = type <= 2 ? 1 : type == 3 || type == 4 ? 2 : type == 5 ? 4 : 1;

            if (type == 3 && i + 1 < words.size())
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

    bool Contains(std::vector<uint32_t> const& words, uint32_t word)
    {
        return std::find(words.begin(), words.end(), word) != words.end();
    }

    bool ContainsSequence(std::vector<uint8_t> const& haystack, std::vector<uint8_t> const& needle)
    {
        return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end()) != haystack.end();
    }
}


int RunTransportTest(std::wstring const& dllPath)
{
    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    auto const module = LoadLibraryExW(dllPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (module == nullptr)
    {
        Print("Could not load %s (error %lu)", ToUtf8(dllPath).c_str(), GetLastError());
        return 2;
    }

    using GetClassObject = HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, LPVOID*);
    auto const getClassObject = reinterpret_cast<GetClassObject>(GetProcAddress(module, "DllGetClassObject"));
    if (getClassObject == nullptr)
    {
        Print("The DLL has no DllGetClassObject");
        return 2;
    }

    IClassFactory* factory{ nullptr };
    auto hr = getClassObject(TransportClsid, __uuidof(IClassFactory), reinterpret_cast<void**>(&factory));
    if (FAILED(hr) || factory == nullptr)
    {
        Print("DllGetClassObject failed: 0x%08X", static_cast<unsigned>(hr));
        return 2;
    }

    IMidiTransport* transport{ nullptr };
    hr = factory->CreateInstance(nullptr, __uuidof(IMidiTransport), reinterpret_cast<void**>(&transport));
    factory->Release();

    if (FAILED(hr) || transport == nullptr)
    {
        Print("Could not create the transport: 0x%08X", static_cast<unsigned>(hr));
        return 2;
    }

    MockDeviceManager deviceManager;
    MockProtocolManager protocolManager;

    IMidiTransportConfigurationManager* configuration{ nullptr };
    IMidiEndpointManager* endpointManager{ nullptr };

    Check(SUCCEEDED(transport->Activate(__uuidof(IMidiTransportConfigurationManager), reinterpret_cast<void**>(&configuration))) && configuration != nullptr,
        "activate the configuration manager");
    if (configuration == nullptr) return 3;

    Check(SUCCEEDED(configuration->Initialize(TransportClsid, &deviceManager, nullptr)), "initialize the configuration manager");

    // ------------------------------------------------------------------------------------------
    Print("1. A host defined before the endpoint manager starts, as the configuration file can be");

    auto const hostId = NewGuidText();
    auto const hostSection =
        L"{\"create\":{\"hosts\":{\"" + hostId + L"\":{\"name\":\"Harness Host\",\"port\":\"auto\",\"advertise\":false}}}}";

    Check(IsSuccess(Send(configuration, hostSection)), "create section for one host is accepted");

    Check(SUCCEEDED(transport->Activate(__uuidof(IMidiEndpointManager), reinterpret_cast<void**>(&endpointManager))) && endpointManager != nullptr,
        "activate the endpoint manager");
    if (endpointManager == nullptr) return 3;

    Check(SUCCEEDED(endpointManager->Initialize(&deviceManager, &protocolManager)), "initialize the endpoint manager");
    Check(deviceManager.HadParent(), "the parent device is created at initialization");

    uint16_t hostPort{ 0 };

    Check(WaitFor([&]()
    {
        auto const host = FindEntry(Send(configuration, Command(L"enumerateHosts")), L"hosts", hostId);
        if (host == nullptr || !host.GetNamedBoolean(L"hasStarted", false)) return false;

        hostPort = static_cast<uint16_t>(host.GetNamedNumber(L"actualPort", 0));
        return hostPort != 0;
    }, 5000), "the host starts on a port");

    Print("   host is on port %u", hostPort);

    // ------------------------------------------------------------------------------------------
    Print("2. A remote invites the host");

    Peer remote("Harness Peer", false);
    remote.SetSyncDelay(3);
    Check(remote.Start(), "the remote binds a loopback port pair");
    remote.Invite(hostPort);

    Check(WaitFor([&]() { return remote.ConnectedCount() == 1; }, 5000), "the remote reaches connected");
    Check(WaitFor([&]() { return deviceManager.Endpoints().size() == 1; }, 5000), "one endpoint is created for the connection");

    auto endpoints = deviceManager.Endpoints();
    if (endpoints.empty()) return 4;

    auto const hostEndpoint = endpoints[0];
    Check(hostEndpoint.EndpointName == L"Harness Peer", "the endpoint is named after the remote");
    Check(hostEndpoint.TransportCode == L"RTPMIDI", "the transport code is RTPMIDI");
    Check(hostEndpoint.TransportId == TransportClsid, "the endpoint carries the transport id");
    Check(hostEndpoint.NativeFormat == MidiDataFormats_ByteStream, "the native format is a byte stream");
    Check(hostEndpoint.InstanceId.rfind(L"MIDIU_RTPMIDI_", 0) == 0, "the instance id has the transport prefix");
    Check(hostEndpoint.PropertyCount > 0, "the MIDI 1.0 port properties are supplied");
    Check(!deviceManager.WrongParent(), "the endpoint is created under the transport's parent device");
    Check(protocolManager.m_calls == 0, "no MIDI 2.0 discovery is started for a byte stream endpoint");
    Check(remote.LastRemoteName() == "Harness Host", "the remote sees the host's name");
    Print("   endpoint %s", ToUtf8(hostEndpoint.InstanceId).c_str());

    IMidiBidirectional* hostBidi{ nullptr };
    MockCallback hostCallback;
    hostCallback.Expect(42);

    Check(SUCCEEDED(transport->Activate(__uuidof(IMidiBidirectional), reinterpret_cast<void**>(&hostBidi))) && hostBidi != nullptr, "activate a bidi");
    if (hostBidi == nullptr) return 4;

    DWORD taskId{ 0 };
    Check(SUCCEEDED(hostBidi->Initialize(hostEndpoint.InterfaceId.c_str(), nullptr, &taskId, &hostCallback, 42, GUID{})), "the service opens the endpoint");

    // remote to service
    std::vector<uint8_t> sysex{ 0xF0, 0x7D };
    for (uint8_t i = 0; i < 18; i++) sysex.push_back(static_cast<uint8_t>(0x10 + i));
    sysex.push_back(0xF7);

    remote.Send({ 0x90, 0x3C, 0x64 });
    remote.Send({ 0x80, 0x3C, 0x40 });
    remote.Send({ 0xB3, 0x07, 0x55 });
    remote.Send(sysex);

    std::vector<uint8_t> const expectedSysExPayload(sysex.begin() + 1, sysex.end() - 1);

    Check(WaitFor([&]()
    {
        auto const words = hostCallback.Words();
        return Contains(words, 0x20903C64) && Contains(words, 0x20803C40) && Contains(words, 0x20B30755) &&
            SysEx7Payload(words) == expectedSysExPayload;
    }, 3000), "remote messages arrive as UMP, SysEx intact");

    Check(!hostCallback.WrongContext(), "every callback carries the context the service gave");
    Check(!hostCallback.BadPosition(), "timestamps are set and never go backward");

    // service to remote: MIDI 1.0 and MIDI 2.0 channel voice, and a SysEx7 packet
    uint32_t const outgoing[] =
    {
        0x20904540,                 // MIDI 1.0 Note On, note 0x45, velocity 0x40
        0x40904800, 0x80000000,     // MIDI 2.0 Note On, note 0x48, velocity 0x8000
        0x30037D01, 0x02000000,     // SysEx7 complete in one packet: 7D 01 02
    };

    Check(SUCCEEDED(hostBidi->SendMidiMessage(MessageOptionFlags_None, const_cast<uint32_t*>(outgoing), sizeof(outgoing), 0)), "the service sends to the endpoint");

    Check(WaitFor([&]()
    {
        auto const received = remote.Received();
        return ContainsSequence(received, { 0x90, 0x45, 0x40 }) &&
            ContainsSequence(received, { 0x90, 0x48, 0x40 }) &&
            ContainsSequence(received, { 0xF0, 0x7D, 0x01, 0x02, 0xF7 });
    }, 3000), "the remote receives MIDI 1.0 bytes for all three");

    Check(WaitFor([&]() { return remote.AnyClockSync(); }, 5000), "clock sync completes");

    json::JsonObject hostConnection{ nullptr };
    Check(WaitFor([&]()
    {
        hostConnection = FirstConnection(FindEntry(Send(configuration, Command(L"enumerateHosts")), L"hosts", hostId));
        return hostConnection != nullptr && hostConnection.GetNamedNumber(L"currentLatencyTicks", 0) > 0;
    }, 5000), "the host reports its connection with a latency figure");

    if (hostConnection != nullptr)
    {
        Check(std::wstring{ hostConnection.GetNamedString(L"remoteName", L"") } == L"Harness Peer", "status: remote name");
        Check(hostConnection.GetNamedBoolean(L"connected", false), "status: connected");
        Check(!hostConnection.GetNamedBoolean(L"thisPcInvited", true), "status: the remote invited");
        Check(SameText(std::wstring{ hostConnection.GetNamedString(L"endpointDeviceId", L"") }, hostEndpoint.InterfaceId), "status: endpoint id");
        Check(hostConnection.HasKey(L"remoteHostName") && hostConnection.GetNamedString(L"remoteHostName", L"x").empty(),
            "status: no host name for a remote that nothing advertises");
        Check(hostConnection.GetNamedNumber(L"totalNetworkPacketsReceived", 0) >= 4, "status: packets received");
        Check(hostConnection.GetNamedNumber(L"totalMessagesSent", 0) == 3, "status: three messages sent");
        Print("   latency %.0f ticks, best %.0f ticks",
            hostConnection.GetNamedNumber(L"currentLatencyTicks", 0), hostConnection.GetNamedNumber(L"bestLatencyTicks", 0));
    }

    uint64_t latencyTicks{ 0 };
    Check(WaitFor([&]() { return deviceManager.LatencyWritesFor(hostEndpoint.InterfaceId, latencyTicks) > 0; }, 8000),
        "the one-way latency is written to the endpoint for the scheduler");

    // clock sync is done by now, so the remote's RTP timestamps are mapped to local time
    remote.SendAhead({ 0x90, 0x3D, 0x21 }, 500);
    Check(WaitFor([&]() { return Contains(hostCallback.Words(), 0x20903D21); }, 3000), "a message the remote stamped 50 ms ahead arrives");
    Check(!hostCallback.StampedAfterArrival(), "no message is stamped later than it reached the service");

    auto const connectionId = hostConnection != nullptr ? static_cast<uint32_t>(hostConnection.GetNamedNumber(L"connectionId", 0)) : 0u;

    Check(IsSuccess(Send(configuration, Command(L"disconnectRemoteClient",
        { { L"entryIdentifier", hostId }, { L"connectionId", std::to_wstring(connectionId) } }))), "disconnect the remote from here");

    Check(WaitFor([&]() { return !remote.Ended().empty(); }, 3000), "the remote is told the connection ended");
    Check(WaitFor([&]() { auto const e = deviceManager.Endpoints(); return !e.empty() && e[0].Removed; }, 3000), "the endpoint is removed");

    hostBidi->Shutdown();
    hostBidi->Release();

    // ------------------------------------------------------------------------------------------
    Print("3. This PC connects to a remote host");

    Peer remoteHost("Harness Remote Host", true);
    Check(remoteHost.Start(), "the remote host binds a loopback port pair");

    auto const clientId = NewGuidText();
    auto const clientSection =
        L"{\"create\":{\"clients\":{\"" + clientId + L"\":{\"name\":\"Harness Client\",\"remoteAddress\":\"::1\",\"remotePort\":" +
        std::to_wstring(remoteHost.ControlPort()) + L"}}}}";

    Check(IsSuccess(Send(configuration, clientSection)), "create section for one client is accepted");

    Check(WaitFor([&]() { return deviceManager.Endpoints().size() == 2; }, 8000), "an endpoint is created for the client connection");

    endpoints = deviceManager.Endpoints();
    if (endpoints.size() < 2) return 5;

    auto const clientEndpoint = endpoints[1];
    Check(clientEndpoint.EndpointName == L"Harness Remote Host", "the endpoint is named after the remote host");
    Check(remoteHost.LastRemoteName() == "Harness Client", "the remote host sees this PC's client name");

    auto client = FindEntry(Send(configuration, Command(L"enumerateClients")), L"clients", clientId);
    Check(client != nullptr && std::wstring{ client.GetNamedString(L"entryState", L"") } == L"live", "the client entry is live");

    IMidiBidirectional* clientBidi{ nullptr };
    MockCallback clientCallback;
    clientCallback.Expect(7);

    Check(SUCCEEDED(transport->Activate(__uuidof(IMidiBidirectional), reinterpret_cast<void**>(&clientBidi))) && clientBidi != nullptr, "activate a second bidi");
    if (clientBidi == nullptr) return 5;

    Check(SUCCEEDED(clientBidi->Initialize(clientEndpoint.InterfaceId.c_str(), nullptr, &taskId, &clientCallback, 7, GUID{})), "the service opens the client endpoint");

    remoteHost.Send({ 0x91, 0x40, 0x7F });
    Check(WaitFor([&]() { return Contains(clientCallback.Words(), 0x2091407F); }, 3000), "a message from the remote host arrives");

    // the remote ends it: a retry is due later, and the endpoint goes now
    remoteHost.EndAll();

    Check(WaitFor([&]() { auto const e = deviceManager.Endpoints(); return e.size() >= 2 && e[1].Removed; }, 3000), "the endpoint goes when the remote ends the connection");

    Check(WaitFor([&]()
    {
        client = FindEntry(Send(configuration, Command(L"enumerateClients")), L"clients", clientId);
        return client != nullptr && std::wstring{ client.GetNamedString(L"entryState", L"") } == L"failed";
    }, 3000), "the client entry reports failed, waiting to retry");

    if (client != nullptr)
    {
        Check(static_cast<uint32_t>(client.GetNamedNumber(L"lastError", 0)) == static_cast<uint32_t>(HRESULT_FROM_WIN32(ERROR_GRACEFUL_DISCONNECT)),
            "the last error says the remote ended it");
    }

    clientBidi->Shutdown();
    clientBidi->Release();

    Check(IsSuccess(Send(configuration, Command(L"reconnectClient", { { L"entryIdentifier", clientId } }))), "reconnect the client now");

    Check(WaitFor([&]() { return deviceManager.Endpoints().size() == 3; }, 8000), "the client connects again");

    endpoints = deviceManager.Endpoints();
    if (endpoints.size() >= 3)
    {
        Check(SameText(endpoints[2].InstanceId, clientEndpoint.InstanceId), "the endpoint comes back with the same instance id");
        Check(endpoints[2].UniqueIdentifier == clientEndpoint.UniqueIdentifier, "and the same unique identifier");
    }

    auto const endsBefore = remoteHost.Ended().size();

    Check(IsSuccess(Send(configuration, Command(L"removeClient", { { L"entryIdentifier", clientId } }))), "remove the client");
    Check(WaitFor([&]() { return remoteHost.Ended().size() > endsBefore; }, 3000), "the remote host is told");
    Check(WaitFor([&]() { auto const e = deviceManager.Endpoints(); return e.size() >= 3 && e[2].Removed; }, 3000), "the endpoint is removed");

    // ------------------------------------------------------------------------------------------
    Print("4. The same remote name from two hosts, and a remote which restarts");

    // the engine treats one name from one address as the same device, so the second is on IPv4
    Peer first("Same Name", false);
    Peer second("Same Name", false, true);
    Check(first.Start() && second.Start(), "two remotes bind");

    first.Invite(hostPort);
    Check(WaitFor([&]() { return deviceManager.Endpoints().size() == 4; }, 5000), "the first gets an endpoint");

    second.Invite(hostPort);
    Check(WaitFor([&]() { return deviceManager.Endpoints().size() == 5; }, 5000), "the second gets its own endpoint");

    endpoints = deviceManager.Endpoints();
    if (endpoints.size() >= 5)
    {
        Check(!SameText(endpoints[3].InstanceId, endpoints[4].InstanceId), "the two instance ids differ");
        Check(!endpoints[3].Removed, "the first endpoint is untouched by the second");
    }

    first.Stop();
    second.Stop();

    Check(WaitFor([&]()
    {
        auto const e = deviceManager.Endpoints();
        return e.size() >= 5 && e[3].Removed && e[4].Removed;
    }, 3000), "both endpoints go when the remotes leave");

    // A device which restarts invites again from a new port with a new SSRC, and never said goodbye
    auto restarted = std::make_unique<Peer>("Restarting Peer", false);
    Check(restarted->Start(), "the restarting remote binds");
    restarted->Invite(hostPort);
    Check(WaitFor([&]() { return deviceManager.Endpoints().size() == 6; }, 5000), "the restarting remote gets an endpoint");

    auto again = std::make_unique<Peer>("Restarting Peer", false);
    Check(again->Start(), "its second life binds");
    again->Invite(hostPort);

    Check(WaitFor([&]() { return deviceManager.Endpoints().size() == 7; }, 5000), "it gets an endpoint again");

    endpoints = deviceManager.Endpoints();
    if (endpoints.size() >= 7)
    {
        Check(endpoints[5].Removed, "the old endpoint was removed first");
        Check(SameText(endpoints[5].InstanceId, endpoints[6].InstanceId), "the new endpoint has the same instance id");
    }

    // the first life's goodbye arrives late, and must not end the second life's connection
    restarted.reset();
    Sleep(500);

    endpoints = deviceManager.Endpoints();
    Check(endpoints.size() >= 7 && !endpoints[6].Removed, "a late goodbye from the old life leaves the new endpoint alone");
    Check(again->ConnectedCount() == 1, "and the new connection stays up");

    again->Stop();

    Check(WaitFor([&]() { auto const e = deviceManager.Endpoints(); return e.size() >= 7 && e[6].Removed; }, 3000), "the endpoint goes when it leaves");

    // ------------------------------------------------------------------------------------------
    Print("5. Hostile configuration and commands");

    struct Case { std::wstring Json; uint32_t ExpectedError; char const* What; };

    auto const badHostId = NewGuidText();
    std::wstring const longName(64, L'n');

    std::vector<Case> const cases =
    {
        { L"this is not json", 1, "not JSON" },
        { L"{\"create\":[]}", 0, "create is an array" },
        { L"{\"create\":{\"hosts\":[]}}", 0, "hosts is an array" },
        { L"{\"create\":{\"hosts\":{\"not-a-guid\":{\"name\":\"x\"}}}}", 4, "entry key is not a GUID" },
        { L"{\"create\":{\"hosts\":{\"" + badHostId + L"\":\"text\"}}}", 6, "entry is not an object" },
        { L"{\"create\":{\"hosts\":{\"" + badHostId + L"\":{\"name\":123}}}}", 12, "name is a number" },
        { L"{\"create\":{\"hosts\":{\"" + badHostId + L"\":{\"name\":\"" + longName + L"\"}}}}", 9, "name longer than 63 bytes" },
        { L"{\"create\":{\"hosts\":{\"" + badHostId + L"\":{\"name\":\"a.b\"}}}}", 12, "advertised name with a period" },
        { L"{\"create\":{\"hosts\":{\"" + badHostId + L"\":{\"name\":\"x\",\"port\":\"99999\"}}}}", 10, "port out of range" },
        { L"{\"create\":{\"hosts\":{\"" + badHostId + L"\":{\"name\":\"x\",\"port\":true}}}}", 10, "port is a boolean" },
        { L"{\"create\":{\"clients\":{\"" + badHostId + L"\":{\"name\":\"x\"}}}}", 8, "client with no remote" },
        { L"{\"create\":{\"clients\":{\"" + badHostId + L"\":{\"name\":\"x\",\"remoteAddress\":\"::1\",\"serviceInstanceName\":\"y\"}}}}", 8, "client with two remotes" },
        { L"{\"transportCommand\":{\"commandName\":123}}", 2, "command name is a number" },
        { L"{\"transportCommand\":{\"commandName\":\"startHost\",\"commandArguments\":{\"entryIdentifier\":\"zzz\"}}}", 4, "command with a bad entry id" },
        { L"{\"transportCommand\":{\"commandName\":\"startHost\"}}", 3, "command with no entry id" },
        { L"{\"transportCommand\":{\"commandName\":\"disconnectRemoteClient\",\"commandArguments\":{\"entryIdentifier\":\"" + hostId + L"\",\"connectionId\":\"-1\"}}}", 11, "negative connection id" },
        { L"{\"update\":[1,\"x\",{\"match\":7}]}", 0, "customization array of junk" },
    };

    for (auto const& testCase : cases)
    {
        HRESULT callResult{ S_OK };
        auto const response = Send(configuration, testCase.Json, &callResult);

        bool ok = SUCCEEDED(callResult) && response != nullptr;
        if (ok && testCase.ExpectedError != 0) ok = !IsSuccess(response) && ErrorCode(response) == testCase.ExpectedError;

        if (!ok)
        {
            Print("   case \"%s\": hr 0x%08X, success %d, error %u (wanted %u)", testCase.What, static_cast<unsigned>(callResult),
                IsSuccess(response) ? 1 : 0, ErrorCode(response), testCase.ExpectedError);
        }

        Check(ok, testCase.What);
    }

    std::wstring deep;
    for (int i = 0; i < 20000; i++) deep += L"{\"a\":";
    deep += L"1";
    for (int i = 0; i < 20000; i++) deep += L"}";

    HRESULT deepResult{ S_OK };
    auto const deepResponse = Send(configuration, deep, &deepResult);
    Print("   deeply nested JSON: hr 0x%08X, error %u", static_cast<unsigned>(deepResult), ErrorCode(deepResponse));
    Check(SUCCEEDED(deepResult) && ErrorCode(deepResponse) == 1, "deeply nested JSON is answered as invalid JSON");

    Check(FindEntry(Send(configuration, Command(L"enumerateHosts")), L"hosts", badHostId) == nullptr, "no rejected entry was kept");

#ifdef RTP_SDK_CHECK
    // ------------------------------------------------------------------------------------------
    Print("6. JSON from the SDK's configuration objects, as the service would pass it on");

    auto const sdkDll = dllPath.substr(0, dllPath.find_last_of(L'\\') + 1) + L"sdk\\Windows.Devices.Midi2.Transports.Rtp.dll";

    if (!SdkCheckStart(sdkDll))
    {
        Check(false, "the SDK DLL loads");
    }
    else try
    {
        Check(SdkStaticsAreRight(), "the SDK's transport id, service type and default port");

        std::wstring sdkHostId;
        auto const sdkHostSection = SdkHostSection(L"SDK Host", sdkHostId);
        Check(!sdkHostSection.empty(), "the host config produces a transport section");
        Check(IsSuccess(Send(configuration, sdkHostSection)), "the transport accepts the SDK's host section");

        Check(WaitFor([&]()
        {
            auto const host = FindEntry(Send(configuration, Command(L"enumerateHosts")), L"hosts", sdkHostId);
            return host != nullptr && host.GetNamedBoolean(L"hasStarted", false) &&
                std::wstring{ host.GetNamedString(L"configuredPort", L"") } == L"auto" && !host.GetNamedBoolean(L"advertise", true);
        }, 5000), "the host from the SDK starts, with an automatic port and no advertising");

        Peer sdkRemote("SDK Remote", true);
        Check(sdkRemote.Start(), "a remote host for the SDK client binds");

        auto const endpointsBefore = deviceManager.Endpoints().size();

        std::wstring sdkClientId;
        auto const sdkClientSection = SdkClientSection(L"SDK Client", L"::1", sdkRemote.ControlPort(), L"Custom Name", sdkClientId);
        Check(IsSuccess(Send(configuration, sdkClientSection)), "the transport accepts the SDK's client section");

        Check(WaitFor([&]() { return deviceManager.Endpoints().size() == endpointsBefore + 1; }, 8000), "the SDK client connects");

        endpoints = deviceManager.Endpoints();
        if (endpoints.size() == endpointsBefore + 1)
        {
            Check(endpoints.back().EndpointName == L"Custom Name", "the custom endpoint name is used");
        }

        Check(sdkRemote.LastRemoteName() == "SDK Client", "the remote sees the SDK client's name");

        // the key is written uppercase, while winrt::to_hstring gives lowercase
        json::JsonObject removal{ nullptr };
        bool removalNamesHost{ false };

        if (json::JsonObject::TryParse(SdkRemovalSection(sdkHostId, true), removal) && removal.HasKey(L"remove") &&
            removal.GetNamedObject(L"remove").HasKey(L"hosts"))
        {
            for (auto const& pair : removal.GetNamedObject(L"remove").GetNamedObject(L"hosts"))
            {
                if (SameText(std::wstring{ pair.Key() }, sdkHostId) &&
                    pair.Value().ValueType() == json::JsonValueType::Object && pair.Value().GetObject().Size() == 0)
                {
                    removalNamesHost = true;
                }
            }
        }

        Check(removalNamesHost, "the removal config names the host under remove, with an empty object");

        Check(IsSuccess(Send(configuration, Command(L"removeClient", { { L"entryIdentifier", sdkClientId } }))), "remove the SDK client");
        Check(IsSuccess(Send(configuration, Command(L"removeHost", { { L"entryIdentifier", sdkHostId } }))), "remove the SDK host");
        Check(WaitFor([&]() { return deviceManager.Endpoints().back().Removed; }, 3000), "the SDK client's endpoint is removed");

        sdkRemote.Stop();
    }
    catch (winrt::hresult_error const& error)
    {
        Print("   SDK check threw 0x%08X: %s", static_cast<unsigned>(error.code()), ToUtf8(std::wstring{ error.message() }).c_str());
        Check(false, "the SDK check runs without an exception");
    }
    catch (std::exception const& error)
    {
        Print("   SDK check threw: %s", error.what());
        Check(false, "the SDK check runs without an exception");
    }
#endif

    // ------------------------------------------------------------------------------------------
    Print("7. Eight remotes at once, with MIDI both ways, then connection churn");

    {
        constexpr size_t RemoteCount = 8;
        constexpr uint32_t NotesEachWay = 500;

        std::vector<std::unique_ptr<Peer>> remotes;
        auto const firstEndpoint = deviceManager.Endpoints().size();

        for (size_t i = 0; i < RemoteCount; i++)
        {
            remotes.push_back(std::make_unique<Peer>("Stress " + std::to_string(i + 1), false));
            Check(remotes.back()->Start(), "a stress remote binds");
            remotes.back()->Invite(hostPort);
        }

        Check(WaitFor([&]() { return deviceManager.Endpoints().size() == firstEndpoint + RemoteCount; }, 8000), "all eight get endpoints");

        auto const all = deviceManager.Endpoints();
        std::vector<std::unique_ptr<MockCallback>> callbacks;
        std::vector<IMidiBidirectional*> bidis;

        for (size_t i = firstEndpoint; i < all.size(); i++)
        {
            auto callback = std::make_unique<MockCallback>();
            callback->Expect(static_cast<LONGLONG>(100 + i));

            IMidiBidirectional* bidi{ nullptr };
            transport->Activate(__uuidof(IMidiBidirectional), reinterpret_cast<void**>(&bidi));

            bool const opened = bidi != nullptr && SUCCEEDED(bidi->Initialize(all[i].InterfaceId.c_str(), nullptr, &taskId, callback.get(), 100 + i, GUID{}));
            Check(opened, "a stress endpoint opens");

            bidis.push_back(bidi);
            callbacks.push_back(std::move(callback));
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

        for (size_t b = 0; b < bidis.size(); b++)
        {
            senders.emplace_back([&, b]()
            {
                for (uint32_t n = 0; n < NotesEachWay; n++)
                {
                    uint32_t words[2] = { 0x20900040u | ((n % 128) << 8), 0x20800040u | ((n % 128) << 8) };
                    if (bidis[b] != nullptr) bidis[b]->SendMidiMessage(MessageOptionFlags_None, words, sizeof(words), 0);
                    if (n % 20 == 19) Sleep(1);
                }
            });
        }

        for (auto& sender : senders) sender.join();

        auto const countNotes = [](std::vector<uint32_t> const& words)
        {
            return static_cast<uint32_t>(std::count_if(words.begin(), words.end(), [](uint32_t word)
            {
                return (word >> 28) == 2 && ((word >> 20) & 0xF) != 0 && (((word >> 16) & 0xF0) == 0x90 || ((word >> 16) & 0xF0) == 0x80);
            }));
        };

        bool allArrived = WaitFor([&]()
        {
            for (auto const& callback : callbacks) if (countNotes(callback->Words()) != NotesEachWay * 2) return false;
            for (auto const& stressRemote : remotes) if (stressRemote->Received().size() != NotesEachWay * 2 * 3) return false;
            return true;
        }, 10000);

        if (!allArrived)
        {
            for (size_t i = 0; i < callbacks.size(); i++) Print("   endpoint %zu received %u notes", i, countNotes(callbacks[i]->Words()));
            for (size_t i = 0; i < remotes.size(); i++) Print("   remote %zu received %zu bytes", i, remotes[i]->Received().size());
        }

        // what the transport itself saw on the wire, to tell loss in the network from loss in the transport
        {
            auto const host = FindEntry(Send(configuration, Command(L"enumerateHosts")), L"hosts", hostId);

            if (host != nullptr)
            {
                auto const connections = host.GetNamedArray(L"connections");

                for (uint32_t i = 0; i < connections.Size(); i++)
                {
                    auto const c = connections.GetObjectAt(i);
                    Print("   %s: %.0f packets received, %.0f lost, %.0f repaired from the journal, %.0f messages received",
                        ToUtf8(std::wstring{ c.GetNamedString(L"remoteName", L"") }).c_str(),
                        c.GetNamedNumber(L"totalNetworkPacketsReceived", 0), c.GetNamedNumber(L"totalPacketsLost", 0),
                        c.GetNamedNumber(L"totalLossesRepairedFromJournal", 0), c.GetNamedNumber(L"totalMessagesReceived", 0));
                }
            }
        }

        Check(allArrived, "every message arrives, in both directions, on every connection");

        bool contextsRight = true;
        for (auto const& callback : callbacks) contextsRight = contextsRight && !callback->WrongContext() && !callback->BadPosition();
        Check(contextsRight, "no message reached the wrong endpoint, and timestamps held their order");

        for (auto& stressRemote : remotes) stressRemote->Stop();

        Check(WaitFor([&]()
        {
            auto const e = deviceManager.Endpoints();
            for (size_t i = firstEndpoint; i < e.size(); i++) if (!e[i].Removed) return false;
            return true;
        }, 5000), "all eight endpoints go when the remotes leave");

        for (auto bidi : bidis)
        {
            if (bidi == nullptr) continue;
            bidi->Shutdown();
            bidi->Release();
        }

        // connection churn: the same remote, over and over
        bool churnHeld = true;

        for (int cycle = 0; cycle < 10 && churnHeld; cycle++)
        {
            Peer churn("Churn", false);
            churnHeld = churn.Start();

            auto const before = deviceManager.Endpoints().size();
            churn.Invite(hostPort);

            churnHeld = churnHeld && WaitFor([&]() { return deviceManager.Endpoints().size() == before + 1; }, 5000);
            churn.Stop();
            churnHeld = churnHeld && WaitFor([&]() { return deviceManager.Endpoints().back().Removed; }, 5000);
        }

        Check(churnHeld, "ten rounds of connect and disconnect each create and remove one endpoint");
    }

    // ------------------------------------------------------------------------------------------
    Print("8. Removing the host");

    Check(IsSuccess(Send(configuration, Command(L"removeHost", { { L"entryIdentifier", hostId } }))), "remove the host");
    Check(WaitFor([&]() { return FindEntry(Send(configuration, Command(L"enumerateHosts")), L"hosts", hostId) == nullptr; }, 3000), "the host is gone");
    Check(!IsSuccess(Send(configuration, Command(L"removeHost", { { L"entryIdentifier", hostId } }))), "removing it again fails");

    Check(SUCCEEDED(endpointManager->Shutdown()), "the endpoint manager shuts down");
    Check(SUCCEEDED(configuration->Shutdown()), "the configuration manager shuts down");
    Check(deviceManager.UnknownRemovals() == 0, "every removal named an endpoint the transport created");

    remote.Stop();
    remoteHost.Stop();

    endpointManager->Release();
    configuration->Release();
    transport->Release();

    // ------------------------------------------------------------------------------------------
    Print("9. The multicast DNS helpers, with made-up advertisements and no network");

    {
        using WindowsMidiServicesInternal::MidiDnssdService;

        MidiDnssdService mac{};
        mac.HostName = L"Studio-Mac.local";
        mac.IPv4Addresses = { L"192.168.1.183" };
        mac.IPv6Addresses = { L"fe80::cec:e610:74fd:7d8d" };

        // a second session the same Mac advertises
        MidiDnssdService macSecondSession{};
        macSecondSession.HostName = L"studio-mac.local";
        macSecondSession.IPv4Addresses = { L"192.168.1.183" };

        MidiDnssdService interfaceBox{};
        interfaceBox.HostName = L"Interface.local";
        interfaceBox.IPv4Addresses = { L"192.168.1.50" };

        auto const at = [](std::wstring const& text, uint32_t scope)
        {
            RtpMidi::PeerAddress address{};
            Spike::TryParseAddress(text, 5004, address);
            address.ScopeId = scope;
            return address;
        };

        std::vector<MidiDnssdService> const services{ mac, macSecondSession, interfaceBox };

        Check(RtpMidiMdns::FindHostNameForAddress(services, at(L"192.168.1.183", 0)) == L"Studio-Mac.local", "host name: IPv4");
        Check(RtpMidiMdns::FindHostNameForAddress(services, at(L"fe80::cec:e610:74fd:7d8d", 21)) == L"Studio-Mac.local",
            "host name: a link-local IPv6 address with a scope, advertised without one");
        Check(RtpMidiMdns::FindHostNameForAddress(services, at(L"192.168.1.50", 0)) == L"Interface.local", "host name: another device");
        Check(RtpMidiMdns::FindHostNameForAddress(services, at(L"192.168.1.99", 0)).empty(), "host name: none for an address nothing advertises");
        Check(RtpMidiMdns::FindHostNameForAddress(services, RtpMidi::PeerAddress{}).empty(), "host name: none for no address");

        MidiDnssdService stale{};
        stale.HostName = L"Old-Laptop.local";
        stale.IPv4Addresses = { L"192.168.1.183" };

        Check(RtpMidiMdns::FindHostNameForAddress({ mac, stale }, at(L"192.168.1.183", 0)).empty(),
            "host name: none when two different hosts list the same address");

        std::string const macName{ "Pete\xE2\x80\x99s MacBook Pro" };
        auto const packets = WindowsMidiServicesInternal::BuildDnssdPtrAnnouncements("_apple-midi._udp.local", { "Pete PC", macName }, 4500, 1200);

        Check(packets.size() == 1, "announcement: two hosts fit in one packet");

        if (packets.size() == 1)
        {
            Spike::Mdns::Message message{};
            Check(Spike::Mdns::Parse(packets[0].data(), packets[0].size(), message), "announcement: parses as mDNS");
            Check(message.Id == 0 && message.Flags == 0x8400 && message.IsResponse, "announcement: ID 0, an authoritative response");

            bool recordsRight = message.Records.size() == 2;

            for (auto const& record : message.Records)
            {
                recordsRight = recordsRight && std::string{ record.Section } == "an" && record.Type == 12 && !record.TopBit &&
                    record.Ttl == 4500 && record.Name == "_apple-midi._udp.local";
            }

            Check(recordsRight, "announcement: two shared PTR answers, TTL 4500, no cache-flush bit");

            if (message.Records.size() == 2)
            {
                Check(message.Records[0].Data == "-> Pete PC._apple-midi._udp.local", "announcement: a record points at its instance");
                Check(message.Records[1].Data == "-> " + macName + "._apple-midi._udp.local", "announcement: a UTF-8 name goes out unchanged");
            }
        }

        Check(WindowsMidiServicesInternal::BuildDnssdPtrAnnouncements("_apple-midi._udp.local", { "", "a.b", std::string(64, 'x') }, 4500, 1200).empty(),
            "announcement: empty, dotted and over-long labels are left out");
        Check(WindowsMidiServicesInternal::BuildDnssdPtrAnnouncements("", { "Pete PC" }, 4500, 1200).empty(), "announcement: nothing without a service type");
        Check(WindowsMidiServicesInternal::BuildDnssdPtrAnnouncements("_apple-midi..local", { "Pete PC" }, 4500, 1200).empty(),
            "announcement: nothing for a service type with an empty label");

        std::vector<std::string> many;
        for (int i = 0; i < 40; i++) many.push_back("Host " + std::to_string(i) + std::string(50, 'h'));

        auto const split = WindowsMidiServicesInternal::BuildDnssdPtrAnnouncements("_apple-midi._udp.local", many, 4500, 1200);

        bool splitRight = split.size() > 1;
        size_t splitRecords{ 0 };

        for (auto const& packet : split)
        {
            Spike::Mdns::Message message{};
            splitRight = splitRight && packet.size() <= 1200 && Spike::Mdns::Parse(packet.data(), packet.size(), message);
            splitRecords += message.Records.size();
        }

        Check(splitRight && splitRecords == many.size(), "announcement: forty hosts split across packets under the limit, none lost");
    }

    // ------------------------------------------------------------------------------------------
    Print("10. When the follow-up announcer repeats, with the network replaced by a recorder");

    {
        using WindowsMidiServicesInternal::MidiDnssdAnnouncementResult;
        using WindowsMidiServicesInternal::MidiDnssdFollowUpAnnouncer;

        struct SentRepeat
        {
            uint64_t Tick{ 0 };
            std::vector<std::string> Records;
        };

        std::mutex sentLock;
        std::vector<SentRepeat> sent;

        auto const recorder = [&](std::vector<std::vector<uint8_t>> const& repeatPackets)
        {
            SentRepeat repeat{ GetTickCount64(), {} };

            for (auto const& packet : repeatPackets)
            {
                Spike::Mdns::Message message{};
                if (!Spike::Mdns::Parse(packet.data(), packet.size(), message)) continue;

                for (auto const& record : message.Records) repeat.Records.push_back(record.Data);
            }

            auto lock = std::scoped_lock{ sentLock };
            sent.push_back(std::move(repeat));

            return MidiDnssdAnnouncementResult{ 1, 1, 0 };
        };

        auto const sentSoFar = [&]() { auto lock = std::scoped_lock{ sentLock }; return sent; };
        auto const forget = [&]() { auto lock = std::scoped_lock{ sentLock }; sent.clear(); };

        {
            MidiDnssdFollowUpAnnouncer announcer;
            Check(announcer.Start(L"_apple-midi._udp.local", nullptr, recorder, 150, 450) == S_OK, "announcer: starts");

            auto const added = GetTickCount64();
            announcer.AddRegistration(L"Pete PC");

            Check(WaitFor([&]() { return sentSoFar().size() >= 2; }, 3000), "announcer: two repeats after one registration");

            auto const repeats = sentSoFar();

            if (repeats.size() >= 2)
            {
                Check(repeats[0].Tick >= added + 150 && repeats[1].Tick >= added + 450, "announcer: neither repeat comes early");
                Check(repeats[0].Records == std::vector<std::string>{ "-> Pete PC._apple-midi._udp.local" } && repeats[1].Records == repeats[0].Records,
                    "announcer: each repeat names the host");
            }

            Sleep(600);
            Check(sentSoFar().size() == 2, "announcer: nothing more once both repeats are out");
        }

        forget();

        {
            MidiDnssdFollowUpAnnouncer announcer;
            announcer.Start(L"_apple-midi._udp.local", nullptr, recorder, 150, 450);

            announcer.AddRegistration(L"Gone Host");
            announcer.RemoveRegistration(L"gone host");

            Sleep(700);
            Check(sentSoFar().empty(), "announcer: a withdrawn host is never repeated, matched without regard to case");
        }

        forget();

        {
            MidiDnssdFollowUpAnnouncer announcer;
            announcer.Start(L"_apple-midi._udp.local", nullptr, recorder, 300, 900);

            auto const firstAdded = GetTickCount64();
            announcer.AddRegistration(L"First");

            Sleep(200);

            auto const secondAdded = GetTickCount64();
            announcer.AddRegistration(L"Second");
            announcer.AddRegistration(L"SECOND");

            Check(WaitFor([&]() { return sentSoFar().size() >= 2; }, 3000), "announcer: two repeats for a burst of registrations");

            auto const repeats = sentSoFar();

            if (repeats.size() >= 2)
            {
                // held back, it would come 300 ms after the second registration instead of about 100
                Check(repeats[0].Tick >= firstAdded + 300 && repeats[0].Tick < secondAdded + 200,
                    "announcer: a later registration does not hold back the first repeat");
                Check(repeats[1].Tick >= secondAdded + 900, "announcer: the last repeat waits for the last registration");
                Check(repeats[0].Records.size() == 2 && repeats[1].Records.size() == 2, "announcer: each repeat names every host, once");
            }
        }

        forget();

        {
            MidiDnssdFollowUpAnnouncer announcer;
            announcer.Start(L"_apple-midi._udp.local", nullptr, recorder, 150, 450);

            announcer.AddRegistration(L"Stopped Host");
            announcer.Stop();
            announcer.AddRegistration(L"Added After Stop");

            Sleep(700);
            Check(sentSoFar().empty(), "announcer: nothing is sent once it has stopped");
        }
    }

    Print("");
    Print("%d checks, %d failed", g_checks, g_failures);

    return g_failures == 0 ? 0 : 1;
}
