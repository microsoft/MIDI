// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

using namespace WEX::Logging;
using namespace WEX::Common;

namespace enumeration = winrt::Windows::Devices::Enumeration;
namespace json = winrt::Windows::Data::Json;

namespace NetworkMidiTest
{
    namespace
    {
        bool g_serviceAvailable{ false };

        // The service creates the endpoint, negotiates, and only then builds the ports, and each
        // step is on its own thread. Ten seconds is the discovery timeout, so this has to be
        // comfortably longer for the negative cases which deliberately wait it out.
        constexpr auto PortCreationTimeout = std::chrono::milliseconds(25000);
        constexpr auto EndpointCreationTimeout = std::chrono::milliseconds(20000);
        constexpr auto SessionTimeout = std::chrono::milliseconds(10000);
        constexpr auto PollInterval = std::chrono::milliseconds(250);

        // Both directions of MIDI 1.0 port, keyed the way a user sees them rather than the way a
        // block declares them.
        struct Midi1PortCounts
        {
            uint32_t Sources{ 0 };          // MIDI In to the user
            uint32_t Destinations{ 0 };     // MIDI Out to the user

            uint32_t Total() const { return Sources + Destinations; }
        };

        // Device interface classes for the MIDI 1.0 ports the service publishes. Restated here
        // rather than included, so a test failure says the service changed rather than silently
        // following it.
        constexpr wchar_t Midi1SourceSelector[] =
            L"System.Devices.InterfaceClassGuid:=\"{504BE32C-CCF6-4D2C-B73F-6F8B3747E22B}\"";
        constexpr wchar_t Midi1DestinationSelector[] =
            L"System.Devices.InterfaceClassGuid:=\"{6DC23320-AB33-4CE4-80D4-BBB3EBBF2814}\"";

        // PKEY_MIDI_AssociatedUMP, property 52. Restated for the same reason.
        constexpr wchar_t AssociatedUmpProperty[] = L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD} 52";

        // PKEY_MIDI_CustomEndpointName, property 500.
        constexpr wchar_t CustomEndpointNameProperty[] = L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD} 500";

        std::wstring NormalizeEndpointId(_In_ std::wstring const& value)
        {
            std::wstring result{ value };

            for (auto& character : result)
            {
                character = static_cast<wchar_t>(towlower(character));
            }

            // The service is inconsistent about the trailing separator, and the property is
            // written from a different string than the one enumeration reports.
            while (!result.empty() && (result.back() == L'#' || result.back() == L'\\'))
            {
                result.pop_back();
            }

            return result;
        }

        // MIDI 1.0 ports carry the id of the UMP endpoint they belong to, which is the only way
        // to tell this test's ports apart from every other endpoint on the machine.
        uint32_t CountPortsWithSelector(
            _In_ std::wstring const& selector,
            _In_ std::wstring const& endpointDeviceId)
        {
            uint32_t count{ 0 };

            try
            {
                auto properties = winrt::single_threaded_vector<winrt::hstring>();
                properties.Append(winrt::hstring{ AssociatedUmpProperty });

                auto devices = enumeration::DeviceInformation::FindAllAsync(
                    winrt::hstring{ selector },
                    properties,
                    enumeration::DeviceInformationKind::DeviceInterface).get();

                auto const wanted = NormalizeEndpointId(endpointDeviceId);

                for (auto const& device : devices)
                {
                    auto value = device.Properties().TryLookup(winrt::hstring{ AssociatedUmpProperty });

                    if (value == nullptr) continue;

                    auto const associated = NormalizeEndpointId(
                        std::wstring{ winrt::unbox_value_or<winrt::hstring>(value, L"") });

                    if (!associated.empty() && associated == wanted)
                    {
                        count++;
                    }
                }
            }
            catch (...)
            {
                // Treated as zero. The caller is polling, so a transient enumeration failure just
                // costs one attempt rather than the test.
            }

            return count;
        }

        Midi1PortCounts CountMidi1Ports(_In_ std::wstring const& endpointDeviceId)
        {
            Midi1PortCounts counts{ };

            counts.Sources = CountPortsWithSelector(Midi1SourceSelector, endpointDeviceId);
            counts.Destinations = CountPortsWithSelector(Midi1DestinationSelector, endpointDeviceId);

            return counts;
        }

        std::vector<std::wstring> CollectPortNamesWithSelector(
            _In_ std::wstring const& selector,
            _In_ std::wstring const& endpointDeviceId)
        {
            std::vector<std::wstring> names{ };

            try
            {
                auto properties = winrt::single_threaded_vector<winrt::hstring>();
                properties.Append(winrt::hstring{ AssociatedUmpProperty });

                auto devices = enumeration::DeviceInformation::FindAllAsync(
                    winrt::hstring{ selector },
                    properties,
                    enumeration::DeviceInformationKind::DeviceInterface).get();

                auto const wanted = NormalizeEndpointId(endpointDeviceId);

                for (auto const& device : devices)
                {
                    auto value = device.Properties().TryLookup(winrt::hstring{ AssociatedUmpProperty });

                    if (value == nullptr) continue;

                    auto const associated = NormalizeEndpointId(
                        std::wstring{ winrt::unbox_value_or<winrt::hstring>(value, L"") });

                    if (!associated.empty() && associated == wanted)
                    {
                        names.push_back(std::wstring{ device.Name() });
                    }
                }
            }
            CATCH_LOG();

            return names;
        }

        bool AllNamesContain(
            _In_ std::vector<std::wstring> const& names,
            _In_ std::wstring const& expected)
        {
            if (names.empty())
            {
                return false;
            }

            for (auto const& name : names)
            {
                if (name.find(expected) == std::wstring::npos)
                {
                    return false;
                }
            }

            return true;
        }

        // Separates "the rename never landed" from "it landed but the ports did not follow"
        std::wstring ReadEndpointName(_In_ std::wstring const& endpointDeviceId)
        {
            try
            {
                auto device = enumeration::DeviceInformation::CreateFromIdAsync(
                    winrt::hstring{ endpointDeviceId },
                    nullptr,
                    enumeration::DeviceInformationKind::DeviceInterface).get();

                if (device != nullptr)
                {
                    return std::wstring{ device.Name() };
                }
            }
            CATCH_LOG();

            return L"";
        }

        // The display name can lag the customization, so this is what actually says whether the
        // customization reached the endpoint.
        std::wstring ReadCustomEndpointName(_In_ std::wstring const& endpointDeviceId)
        {
            try
            {
                auto properties = winrt::single_threaded_vector<winrt::hstring>();
                properties.Append(winrt::hstring{ CustomEndpointNameProperty });

                auto device = enumeration::DeviceInformation::CreateFromIdAsync(
                    winrt::hstring{ endpointDeviceId },
                    properties,
                    enumeration::DeviceInformationKind::DeviceInterface).get();

                if (device != nullptr)
                {
                    auto value = device.Properties().TryLookup(winrt::hstring{ CustomEndpointNameProperty });

                    if (value != nullptr)
                    {
                        return std::wstring{ winrt::unbox_value_or<winrt::hstring>(value, L"") };
                    }
                }
            }
            CATCH_LOG();

            return L"";
        }

        // Polls until every port for the endpoint carries the expected text. Returns what it last
        // saw so a failure can print the names rather than just the fact that it gave up.
        std::vector<std::wstring> WaitForMidi1PortNames(
            _In_ std::wstring const& endpointDeviceId,
            _In_ std::wstring const& expected,
            _In_ std::chrono::milliseconds const timeout)
        {
            auto const deadline = std::chrono::steady_clock::now() + timeout;

            std::vector<std::wstring> names{ };

            while (std::chrono::steady_clock::now() < deadline)
            {
                names = CollectPortNamesWithSelector(Midi1SourceSelector, endpointDeviceId);

                if (AllNamesContain(names, expected))
                {
                    return names;
                }

                std::this_thread::sleep_for(PollInterval);
            }

            return names;
        }

        // Polls until both directions reach the expected count. Returns whatever it last saw, so
        // the caller can report the shortfall rather than just "timed out".
        Midi1PortCounts WaitForMidi1Ports(
            _In_ std::wstring const& endpointDeviceId,
            _In_ uint32_t const expectedEachDirection,
            _In_ std::chrono::milliseconds const timeout)
        {
            auto const deadline = std::chrono::steady_clock::now() + timeout;

            Midi1PortCounts counts{ };

            while (std::chrono::steady_clock::now() < deadline)
            {
                counts = CountMidi1Ports(endpointDeviceId);

                if (counts.Sources >= expectedEachDirection && counts.Destinations >= expectedEachDirection)
                {
                    return counts;
                }

                std::this_thread::sleep_for(PollInterval);
            }

            return counts;
        }

        // Polls until no port carries the text any more. The name it reverts to is truncated to
        // the WinMM limit, so absence of the withdrawn one is the assertion that holds.
        std::vector<std::wstring> WaitForMidi1PortNamesWithout(
            _In_ std::wstring const& endpointDeviceId,
            _In_ std::wstring const& unwanted,
            _In_ std::chrono::milliseconds const timeout)
        {
            auto const deadline = std::chrono::steady_clock::now() + timeout;

            std::vector<std::wstring> names{ };

            while (std::chrono::steady_clock::now() < deadline)
            {
                names = CollectPortNamesWithSelector(Midi1SourceSelector, endpointDeviceId);

                if (!names.empty() && !AllNamesContain(names, unwanted))
                {
                    return names;
                }

                std::this_thread::sleep_for(PollInterval);
            }

            return names;
        }

        size_t WaitForEndpointDiscoveryRequest(
            _In_ FakeNetworkHost& host,
            _In_ std::chrono::milliseconds const timeout)
        {
            auto const deadline = std::chrono::steady_clock::now() + timeout;

            size_t count{ 0 };

            while (std::chrono::steady_clock::now() < deadline)
            {
                count = host.EndpointDiscoveryRequestCount();

                if (count > 0)
                {
                    return count;
                }

                std::this_thread::sleep_for(PollInterval);
            }

            return count;
        }

        json::JsonObject ParseResponse(_In_ std::wstring const& responseJson)        {
            json::JsonObject parsed{ nullptr };

            if (!json::JsonObject::TryParse(winrt::hstring{ responseJson }, parsed))
            {
                return nullptr;
            }

            return parsed;
        }

        // Walks the enumerateClients response for the entry this test created and returns the
        // endpoint the service built for it. Empty until the endpoint exists.
        std::wstring FindClientEndpointDeviceId(_In_ std::wstring const& entryIdentifier)
        {
            auto result = EnumerateClients();

            if (!result.IsSuccess()) return {};

            auto parsed = ParseResponse(result.ResponseJson);

            if (parsed == nullptr) return {};

            try
            {
                auto clients = parsed.GetNamedArray(L"clients", nullptr);

                if (clients == nullptr) return {};

                auto const wantedEntry = NormalizeEndpointId(entryIdentifier);

                for (auto const& entry : clients)
                {
                    auto client = entry.GetObject();

                    if (NormalizeEndpointId(std::wstring{ client.GetNamedString(L"entryIdentifier", L"") }) != wantedEntry)
                    {
                        continue;
                    }

                    return std::wstring{ client.GetNamedString(L"endpointDeviceId", L"") };
                }
            }
            catch (...)
            {
            }

            return {};
        }

        // Same for the host side, where the endpoint belongs to a connection rather than the
        // host entry itself.
        std::wstring FindHostConnectionEndpointDeviceId(_In_ std::wstring const& entryIdentifier)
        {
            auto result = EnumerateHosts();

            if (!result.IsSuccess()) return {};

            auto parsed = ParseResponse(result.ResponseJson);

            if (parsed == nullptr) return {};

            try
            {
                auto hosts = parsed.GetNamedArray(L"hosts", nullptr);

                if (hosts == nullptr) return {};

                auto const wantedEntry = NormalizeEndpointId(entryIdentifier);

                for (auto const& entry : hosts)
                {
                    auto host = entry.GetObject();

                    if (NormalizeEndpointId(std::wstring{ host.GetNamedString(L"entryIdentifier", L"") }) != wantedEntry)
                    {
                        continue;
                    }

                    auto connections = host.GetNamedArray(L"connections", nullptr);

                    if (connections == nullptr) return {};

                    for (auto const& connectionEntry : connections)
                    {
                        auto connection = connectionEntry.GetObject();

                        auto id = std::wstring{ connection.GetNamedString(L"endpointDeviceId", L"") };

                        if (!id.empty()) return id;
                    }
                }
            }
            catch (...)
            {
            }

            return {};
        }

        // The port a host actually bound, which is only known after it has started.
        uint16_t FindHostActualPort(_In_ std::wstring const& entryIdentifier)
        {
            auto result = EnumerateHosts();

            if (!result.IsSuccess()) return 0;

            auto parsed = ParseResponse(result.ResponseJson);

            if (parsed == nullptr) return 0;

            try
            {
                auto hosts = parsed.GetNamedArray(L"hosts", nullptr);

                if (hosts == nullptr) return 0;

                auto const wantedEntry = NormalizeEndpointId(entryIdentifier);

                for (auto const& entry : hosts)
                {
                    auto host = entry.GetObject();

                    if (NormalizeEndpointId(std::wstring{ host.GetNamedString(L"entryIdentifier", L"") }) != wantedEntry)
                    {
                        continue;
                    }

                    auto const portText = std::wstring{ host.GetNamedString(L"actualPort", L"") };

                    if (portText.empty()) return 0;

                    auto const port = std::stoul(portText);

                    return (port > 0 && port <= 65535) ? static_cast<uint16_t>(port) : 0;
                }
            }
            catch (...)
            {
            }

            return 0;
        }

        std::wstring WaitForClientEndpointDeviceId(_In_ std::wstring const& entryIdentifier)
        {
            auto const deadline = std::chrono::steady_clock::now() + EndpointCreationTimeout;

            while (std::chrono::steady_clock::now() < deadline)
            {
                auto id = FindClientEndpointDeviceId(entryIdentifier);

                if (!id.empty()) return id;

                std::this_thread::sleep_for(PollInterval);
            }

            return {};
        }

        std::wstring WaitForHostConnectionEndpointDeviceId(_In_ std::wstring const& entryIdentifier)
        {
            auto const deadline = std::chrono::steady_clock::now() + EndpointCreationTimeout;

            while (std::chrono::steady_clock::now() < deadline)
            {
                auto id = FindHostConnectionEndpointDeviceId(entryIdentifier);

                if (!id.empty()) return id;

                std::this_thread::sleep_for(PollInterval);
            }

            return {};
        }

        bool RequireService()
        {
            if (!g_serviceAvailable)
            {
                Log::Error(L"Windows MIDI Service is not reachable, or the network transport is not enabled.");
                return false;
            }

            return true;
        }


        // A remote host under this test's control which the service connects out to.
        class RemoteHostUnderTest
        {
        public:
            ~RemoteHostUnderTest() { Remove(); }

            FakeNetworkHost& Host() { return m_host; }
            std::wstring const& EntryIdentifier() const { return m_entryIdentifier; }

            // What a customization matches on, so a test which renames has to know it
            std::wstring const& EndpointName() const { return m_endpointName; }

            bool Start(
                _In_ uint8_t const groupCount,
                _In_ bool const createMidi1Ports,
                _In_ std::string const& endpointName = "Port Creation Test Host")
            {
                m_endpointName = winrt::to_hstring(endpointName);

                m_host.SetInvitationBehavior(FakeHostInvitationBehavior::Accept);
                m_host.SetEndpointName(endpointName);

                if (groupCount > 0)
                {
                    m_host.DeclareBidirectionalFunctionBlock(groupCount);
                }

                if (!m_host.Start())
                {
                    Log::Error(L"Could not start the fake host.");
                    return false;
                }

                m_entryIdentifier = MakeEntryIdentifier();

                auto result = CreateDirectClient(
                    m_entryIdentifier,
                    FakeNetworkHost::Address(),
                    m_host.Port(),
                    createMidi1Ports);

                if (!result.CallSucceeded)
                {
                    Log::Error(String().Format(L"Creating the client failed: %s", result.Message.c_str()));
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
            std::wstring m_endpointName{ };
            bool m_created{ false };
        };


        // Answers the Windows host's Endpoint Discovery on a client socket. The fake host does
        // this from its own receive loop; a client is driven by the test thread, so this is a
        // blocking pump the test runs until the exchange is done.
        bool AnswerDiscoveryOnClient(
            _In_ UdpTestClient& client,
            _In_ std::vector<FunctionBlockDescription> const& blocks,
            _In_ std::string const& endpointName,
            _In_ std::chrono::milliseconds const timeout)
        {
            auto const deadline = std::chrono::steady_clock::now() + timeout;

            uint16_t sequenceNumber{ 0 };

            while (std::chrono::steady_clock::now() < deadline)
            {
                auto packet = client.ReceivePacket(PollInterval);

                if (!packet.has_value()) continue;

                for (auto const& command : packet->Commands)
                {
                    if (command.Code != CommandCode::UmpData) continue;

                    std::vector<uint32_t> words{ };

                    for (size_t word = 0; word < command.PayloadLengthWords; word++)
                    {
                        words.push_back(command.GetPayloadUInt32(word));
                    }

                    if (!ContainsStreamMessageWithStatus(words, StreamStatusEndpointDiscovery))
                    {
                        continue;
                    }

                    auto send = [&client, &sequenceNumber](std::vector<uint32_t> const& reply)
                    {
                        if (reply.empty()) return;

                        PacketBuilder builder;
                        builder.StartPacket().AddUmpData(sequenceNumber++, reply);
                        client.Send(builder);
                    };

                    send(BuildEndpointInfoNotification(static_cast<uint8_t>(blocks.size())));
                    send(BuildEndpointNameNotification(endpointName));
                    send(BuildProductInstanceIdNotification("PortCreationTestClient"));

                    for (auto const& block : blocks)
                    {
                        send(BuildFunctionBlockInfoNotification(block));
                        send(BuildFunctionBlockNameNotification(block.Number, block.Name));
                    }

                    return true;
                }
            }

            return false;
        }
    }


    bool PortCreationTests::ClassSetup()
    {
        // Nothing here goes through ProtocolTestContext, which is what owns Winsock for the rest
        // of the DLL, so this class starts it itself. WSAStartup is reference counted.
        static WinsockScope s_winsock;

        if (!s_winsock.IsInitialized())
        {
            Log::Error(L"Winsock could not be initialized.");
            return false;
        }

        g_serviceAvailable = IsServiceAvailable();

        if (!g_serviceAvailable)
        {
            Log::Warning(L"Windows MIDI Service is not reachable. These tests will fail rather than skip.");
            return true;
        }

        // Otherwise a direct client waits up to 20 seconds for the endpoint creator to wake.
        if (!SetDirectConnectionScanInterval(1000).IsSuccess())
        {
            Log::Warning(L"Could not shorten the direct connection scan interval. Tests will be slower.");
        }

        return true;
    }

    bool PortCreationTests::TestSetup()
    {
        m_deviceNodeTracker.Start();

        return true;
    }

    bool PortCreationTests::TestCleanup()
    {
        m_deviceNodeTracker.RemoveDeviceNodesCreatedSinceStart();

        return true;
    }


    void PortCreationTests::RemoteHostCompletingDiscoveryGetsAPortPerGroup()
    {
        if (!RequireService()) return;

        // Two groups rather than one, so a fallback that always produces a single pair cannot
        // pass this by accident.
        constexpr uint8_t GroupCount = 2;

        RemoteHostUnderTest remote;

        VERIFY_IS_TRUE(remote.Start(GroupCount, true));

        VERIFY_IS_TRUE(
            remote.Host().WaitForCommand(CommandCode::Invitation, SessionTimeout).has_value(),
            L"The service never invited the remote host.");

        auto endpointDeviceId = WaitForClientEndpointDeviceId(remote.EntryIdentifier());

        VERIFY_IS_FALSE(endpointDeviceId.empty(), L"The service never reported an endpoint for this client.");

        if (endpointDeviceId.empty()) return;

        Log::Comment(String().Format(L"Endpoint: %s", endpointDeviceId.c_str()));

        // The endpoint appears as soon as the session is up. Discovery is a separate exchange on
        // a background thread, so this has to be waited for rather than sampled.
        VERIFY_IS_GREATER_THAN(
            WaitForEndpointDiscoveryRequest(remote.Host(), PortCreationTimeout), static_cast<size_t>(0),
            L"The service never asked the remote host to describe itself.");

        auto const counts = WaitForMidi1Ports(endpointDeviceId, GroupCount, PortCreationTimeout);

        Log::Comment(String().Format(L"MIDI 1.0 ports: %u sources, %u destinations", counts.Sources, counts.Destinations));

        VERIFY_ARE_EQUAL(static_cast<uint32_t>(GroupCount), counts.Sources,
            L"One MIDI 1.0 source per group spanned by the function block.");
        VERIFY_ARE_EQUAL(static_cast<uint32_t>(GroupCount), counts.Destinations,
            L"One MIDI 1.0 destination per group spanned by the function block.");
    }


    void PortCreationTests::RemoteHostCreatesNoPortsWhenUmpOnly()
    {
        if (!RequireService()) return;

        RemoteHostUnderTest remote;

        VERIFY_IS_TRUE(remote.Start(2, false));

        VERIFY_IS_TRUE(
            remote.Host().WaitForCommand(CommandCode::Invitation, SessionTimeout).has_value(),
            L"The service never invited the remote host.");

        auto endpointDeviceId = WaitForClientEndpointDeviceId(remote.EntryIdentifier());

        VERIFY_IS_FALSE(endpointDeviceId.empty(), L"The service never reported an endpoint for this client.");

        if (endpointDeviceId.empty()) return;

        // Give the service the same amount of time it would have taken to create them.
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));

        auto const counts = CountMidi1Ports(endpointDeviceId);

        Log::Comment(String().Format(L"MIDI 1.0 ports: %u sources, %u destinations", counts.Sources, counts.Destinations));

        VERIFY_ARE_EQUAL(static_cast<uint32_t>(0), counts.Total(),
            L"A UMP-only endpoint gets no MIDI 1.0 ports, however well the remote describes itself.");
    }


    void PortCreationTests::ChangingTheFallbackPortCountAppliesWithoutReconnecting()
    {
        if (!RequireService()) return;

        // No function blocks declared, so the transport's own block decides the port count and a
        // change to it is observable. A remote which describes itself would override this.
        RemoteHostUnderTest remote;

        VERIFY_IS_TRUE(remote.Start(0, true));

        VERIFY_IS_TRUE(
            remote.Host().WaitForCommand(CommandCode::Invitation, SessionTimeout).has_value(),
            L"The service never invited the remote host.");

        auto endpointDeviceId = WaitForClientEndpointDeviceId(remote.EntryIdentifier());

        VERIFY_IS_FALSE(endpointDeviceId.empty(), L"The service never reported an endpoint for this client.");

        if (endpointDeviceId.empty()) return;

        Log::Comment(String().Format(L"Endpoint: %s", endpointDeviceId.c_str()));

        auto const before = WaitForMidi1Ports(endpointDeviceId, 1, PortCreationTimeout);

        Log::Comment(String().Format(L"Before: %u sources, %u destinations", before.Sources, before.Destinations));

        VERIFY_ARE_EQUAL(static_cast<uint32_t>(1), before.Sources, L"One source to start with.");
        VERIFY_ARE_EQUAL(static_cast<uint32_t>(1), before.Destinations, L"One destination to start with.");

        constexpr uint8_t NewCount = 3;

        auto const update = UpdateClient(remote.EntryIdentifier(), true, NewCount);

        VERIFY_IS_TRUE(update.IsSuccess(), L"The update was refused.");

        // The transport rewrites the block and the name table, and the service re-syncs the ports
        // off the property change, so this is not instant.
        auto const after = WaitForMidi1Ports(endpointDeviceId, NewCount, PortCreationTimeout);

        Log::Comment(String().Format(L"After: %u sources, %u destinations", after.Sources, after.Destinations));

        VERIFY_ARE_EQUAL(static_cast<uint32_t>(NewCount), after.Sources,
            L"The new port count reached the running endpoint.");
        VERIFY_ARE_EQUAL(static_cast<uint32_t>(NewCount), after.Destinations,
            L"The new port count reached the running endpoint.");

        // The session was never taken down to do it.
        VERIFY_IS_TRUE(
            remote.Host().CountReceived(CommandCode::Bye) == 0,
            L"The connection must not be torn down to change the port count.");
    }


    void PortCreationTests::RenamingAnEndpointRenamesItsMidi1Ports()
    {
        if (!RequireService()) return;

        // Again no function blocks, so the ports are named from the transport's own block. A
        // remote which describes itself has its name table rebuilt from the blocks instead, which
        // is a different path and already worked.
        //
        // The name is unique per process because a customization cannot be withdrawn: the remove
        // section is a stub, so the one this test sends outlives it in the service's cache. With
        // a fixed name the next run would find its endpoint already renamed and pass without
        // testing anything.
        RemoteHostUnderTest remote;

        auto const hostName = ProtocolTestContext::Current().MakeUniqueEndpointName("Rename");

        VERIFY_IS_TRUE(remote.Start(0, true, hostName));

        VERIFY_IS_TRUE(
            remote.Host().WaitForCommand(CommandCode::Invitation, SessionTimeout).has_value(),
            L"The service never invited the remote host.");

        auto endpointDeviceId = WaitForClientEndpointDeviceId(remote.EntryIdentifier());

        VERIFY_IS_FALSE(endpointDeviceId.empty(), L"The service never reported an endpoint for this client.");

        if (endpointDeviceId.empty()) return;

        Log::Comment(String().Format(L"Endpoint: %s", endpointDeviceId.c_str()));

        auto const before = WaitForMidi1Ports(endpointDeviceId, 1, PortCreationTimeout);

        VERIFY_ARE_EQUAL(static_cast<uint32_t>(1), before.Sources, L"One source to start with.");

        auto const namesBefore = CollectPortNamesWithSelector(Midi1SourceSelector, endpointDeviceId);

        for (auto const& name : namesBefore)
        {
            Log::Comment(String().Format(L"Before: %s", name.c_str()));
        }

        std::wstring const newName{ L"Renamed Port Creation Test" };

        // Otherwise a leftover customization could rename the endpoint before the test does, and
        // the assertion below would pass without the rename path running at all.
        VERIFY_IS_FALSE(
            AllNamesContain(namesBefore, newName),
            L"The ports already carried the new name before the rename.");

        auto const renamed = RenameEndpoint(endpointDeviceId, newName);

        VERIFY_IS_TRUE(renamed.IsSuccess(), L"The rename was refused.");

        auto const names = WaitForMidi1PortNames(endpointDeviceId, newName, PortCreationTimeout);

        Log::Comment(String().Format(L"Endpoint name after rename: '%s'", ReadEndpointName(endpointDeviceId).c_str()));
        Log::Comment(String().Format(L"Custom name property after rename: '%s'", ReadCustomEndpointName(endpointDeviceId).c_str()));

        for (auto const& name : names)
        {
            Log::Comment(String().Format(L"After: %s", name.c_str()));
        }

        VERIFY_IS_FALSE(names.empty(), L"The endpoint lost its MIDI 1.0 ports during the rename.");

        VERIFY_IS_TRUE(
            AllNamesContain(names, newName),
            L"The new endpoint name never reached the MIDI 1.0 port names.");

        VERIFY_IS_TRUE(
            remote.Host().CountReceived(CommandCode::Bye) == 0,
            L"The connection must not be torn down to rename an endpoint.");
    }


    void PortCreationTests::ChangingThePortCountKeepsACustomName()
    {
        if (!RequireService()) return;

        RemoteHostUnderTest remote;

        auto const hostName = ProtocolTestContext::Current().MakeUniqueEndpointName("KeepName");

        VERIFY_IS_TRUE(remote.Start(0, true, hostName));

        VERIFY_IS_TRUE(
            remote.Host().WaitForCommand(CommandCode::Invitation, SessionTimeout).has_value(),
            L"The service never invited the remote host.");

        auto endpointDeviceId = WaitForClientEndpointDeviceId(remote.EntryIdentifier());

        VERIFY_IS_FALSE(endpointDeviceId.empty(), L"The service never reported an endpoint for this client.");

        if (endpointDeviceId.empty()) return;

        VERIFY_ARE_EQUAL(
            static_cast<uint32_t>(1),
            WaitForMidi1Ports(endpointDeviceId, 1, PortCreationTimeout).Sources,
            L"One source to start with.");

        std::wstring const customName{ L"Kept Custom Name" };

        VERIFY_IS_TRUE(
            RenameEndpoint(endpointDeviceId, customName).IsSuccess(),
            L"The rename was refused.");

        auto const renamedPorts = WaitForMidi1PortNames(endpointDeviceId, customName, PortCreationTimeout);

        VERIFY_IS_TRUE(
            AllNamesContain(renamedPorts, customName),
            L"The rename did not reach the ports, so this test cannot say anything about the count change.");

        // The count change resolves the port name for itself rather than being handed one. The
        // endpoint reports its original name however it was renamed, so this is where a custom
        // name gets thrown away.
        constexpr uint8_t NewCount = 3;

        VERIFY_IS_TRUE(
            UpdateClient(remote.EntryIdentifier(), true, NewCount).IsSuccess(),
            L"The update was refused.");

        auto const after = WaitForMidi1Ports(endpointDeviceId, NewCount, PortCreationTimeout);

        VERIFY_ARE_EQUAL(static_cast<uint32_t>(NewCount), after.Sources, L"The new port count was applied.");

        auto const namesAfter = WaitForMidi1PortNames(endpointDeviceId, customName, PortCreationTimeout);

        for (auto const& name : namesAfter)
        {
            Log::Comment(String().Format(L"After count change: %s", name.c_str()));
        }

        VERIFY_IS_TRUE(
            AllNamesContain(namesAfter, customName),
            L"Changing the port count reverted the ports to the name the remote supplied.");
    }


    void PortCreationTests::RemovingACustomizationRevertsTheMidi1PortNames()
    {
        if (!RequireService()) return;

        RemoteHostUnderTest remote;

        auto const hostName = ProtocolTestContext::Current().MakeUniqueEndpointName("Revert");

        VERIFY_IS_TRUE(remote.Start(0, true, hostName));

        VERIFY_IS_TRUE(
            remote.Host().WaitForCommand(CommandCode::Invitation, SessionTimeout).has_value(),
            L"The service never invited the remote host.");

        auto endpointDeviceId = WaitForClientEndpointDeviceId(remote.EntryIdentifier());

        VERIFY_IS_FALSE(endpointDeviceId.empty(), L"The service never reported an endpoint for this client.");

        if (endpointDeviceId.empty()) return;

        VERIFY_ARE_EQUAL(
            static_cast<uint32_t>(1),
            WaitForMidi1Ports(endpointDeviceId, 1, PortCreationTimeout).Sources,
            L"One source to start with.");

        std::wstring const customName{ L"Name To Be Withdrawn" };

        VERIFY_IS_TRUE(
            RenameEndpoint(endpointDeviceId, customName).IsSuccess(),
            L"The rename was refused.");

        VERIFY_IS_TRUE(
            AllNamesContain(WaitForMidi1PortNames(endpointDeviceId, customName, PortCreationTimeout), customName),
            L"The rename did not reach the ports, so this test cannot say anything about removing it.");

        VERIFY_IS_TRUE(
            RemoveEndpointCustomization(endpointDeviceId).IsSuccess(),
            L"The removal was refused.");

        // The remote's own name, which the endpoint manager still has on record
        auto const reverted = WaitForMidi1PortNamesWithout(endpointDeviceId, customName, PortCreationTimeout);

        for (auto const& name : reverted)
        {
            Log::Comment(String().Format(L"After removal: %s", name.c_str()));
        }

        Log::Comment(String().Format(
            L"Custom name property after removal: '%s'", ReadCustomEndpointName(endpointDeviceId).c_str()));

        VERIFY_IS_TRUE(
            ReadCustomEndpointName(endpointDeviceId).empty(),
            L"The custom name property was not cleared.");

        VERIFY_IS_FALSE(
            AllNamesContain(reverted, customName),
            L"The ports kept the withdrawn name.");
    }


    void PortCreationTests::RemoteClientCompletingDiscoveryGetsAPortPerGroup()
    {
        if (!RequireService()) return;

        constexpr uint8_t GroupCount = 2;

        auto const entryIdentifier = MakeEntryIdentifier();
        auto const serviceInstanceName = L"MidiPortCreationTest_" + entryIdentifier.substr(1, 8);

        auto created = CreateHost(
            entryIdentifier,
            L"Port Creation Test Host",
            L"PortCreationTestHost",
            serviceInstanceName,
            false,          // accept remote clients without asking
            L"auto",
            false,          // do not advertise
            true,
            true);          // create MIDI 1.0 ports

        VERIFY_IS_TRUE(created.IsSuccess(), L"Could not create the host.");

        auto removeHost = wil::scope_exit([&entryIdentifier]()
        {
            RemoveHost(entryIdentifier);
        });

        uint16_t hostPort{ 0 };

        auto const portDeadline = std::chrono::steady_clock::now() + EndpointCreationTimeout;

        while (std::chrono::steady_clock::now() < portDeadline && hostPort == 0)
        {
            hostPort = FindHostActualPort(entryIdentifier);

            if (hostPort == 0) std::this_thread::sleep_for(PollInterval);
        }

        VERIFY_IS_GREATER_THAN(hostPort, static_cast<uint16_t>(0), L"The host never reported a port.");

        if (hostPort == 0) return;

        UdpTestClient client;

        VERIFY_IS_TRUE(client.Open(HostEndpointAddress{ L"127.0.0.1", hostPort }), L"Could not open the test client.");

        PacketBuilder invitation;
        invitation.StartPacket().AddInvitation("Port Creation Test Client", "PortCreationTestClient");

        VERIFY_IS_TRUE(client.Send(invitation), L"Could not send the invitation.");

        VERIFY_IS_TRUE(
            client.WaitForCommand(CommandCode::InvitationReplyAccepted, SessionTimeout).has_value(),
            L"The host never accepted the session.");

        FunctionBlockDescription block{ };
        block.Number = 0;
        block.Direction = FunctionBlockDirection::Bidirectional;
        block.FirstGroup = 0;
        block.GroupCount = GroupCount;
        block.IsActive = true;
        block.Name = "Port Creation Test Client";

        VERIFY_IS_TRUE(
            AnswerDiscoveryOnClient(client, { block }, "Port Creation Test Client", SessionTimeout),
            L"The host never asked the remote client to describe itself.");

        auto endpointDeviceId = WaitForHostConnectionEndpointDeviceId(entryIdentifier);

        VERIFY_IS_FALSE(endpointDeviceId.empty(), L"The host never reported an endpoint for the connected client.");

        if (endpointDeviceId.empty()) return;

        Log::Comment(String().Format(L"Endpoint: %s", endpointDeviceId.c_str()));

        auto const counts = WaitForMidi1Ports(endpointDeviceId, GroupCount, PortCreationTimeout);

        Log::Comment(String().Format(L"MIDI 1.0 ports: %u sources, %u destinations", counts.Sources, counts.Destinations));

        VERIFY_ARE_EQUAL(static_cast<uint32_t>(GroupCount), counts.Sources,
            L"One MIDI 1.0 source per group spanned by the function block.");
        VERIFY_ARE_EQUAL(static_cast<uint32_t>(GroupCount), counts.Destinations,
            L"One MIDI 1.0 destination per group spanned by the function block.");
    }
}
