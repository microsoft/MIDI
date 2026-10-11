// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include <cstring>

#include "LoopbackServiceTests.h"
#include "LoopbackTestSupport.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace LoopbackTest;

namespace json = ::winrt::Windows::Data::Json;

namespace
{
    // the interface classes the service publishes MIDI 1.0 ports under, queried the way the
    // service itself queries them
    constexpr wchar_t Midi1OutputPortSelector[]
    {
        L"System.Devices.InterfaceClassGuid:=\"{6DC23320-AB33-4CE4-80D4-BBB3EBBF2814}\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True"
    };

    constexpr wchar_t Midi1InputPortSelector[]
    {
        L"System.Devices.InterfaceClassGuid:=\"{504BE32C-CCF6-4D2C-B73F-6F8B3747E22B}\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True"
    };

    using PortNumbers = std::map<uint32_t, std::vector<std::wstring>>;

    // Every enabled port's service-assigned number, and the ports holding it. Ports the service
    // did not number have no value, and it never assigns 0, so both are left out.
    PortNumbers GetServiceAssignedPortNumbers(_In_z_ wchar_t const* selector)
    {
        auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
        additionalProperties.Append(STRING_PKEY_MIDI_ServiceAssignedPortNumber);

        auto const ports = winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(
            selector,
            additionalProperties).get();

        PortNumbers numbers{ };

        for (auto const& port : ports)
        {
            auto const number = winrt::unbox_value_or<uint32_t>(
                port.Properties().TryLookup(STRING_PKEY_MIDI_ServiceAssignedPortNumber), 0);

            if (number == 0)
            {
                continue;
            }

            numbers[number].push_back(std::wstring{ port.Id() });
        }

        // enumeration order varies, and two reads are compared
        for (auto& [number, holders] : numbers)
        {
            std::sort(holders.begin(), holders.end());
        }

        return numbers;
    }

    size_t CountPorts(_In_ PortNumbers const& numbers)
    {
        size_t count{ 0 };

        for (auto const& [number, holders] : numbers)
        {
            count += holders.size();
        }

        return count;
    }

    uint32_t CountDuplicates(_In_ PortNumbers const& numbers, _In_z_ wchar_t const* flow)
    {
        uint32_t duplicates{ 0 };

        for (auto const& [number, holders] : numbers)
        {
            if (holders.size() < 2)
            {
                continue;
            }

            duplicates++;

            for (auto const& holder : holders)
            {
                Log::Comment(String().Format(L"%s port number %u is held by %s", flow, number, holder.c_str()));
            }
        }

        return duplicates;
    }

    bool IsSession(_In_ winrt::hstring const& reported, _In_ GUID const& sessionId) noexcept
    {
        try
        {
            if (reported.empty())
            {
                return false;
            }

            winrt::guid const parsed{ reported };

            return memcmp(&parsed, &sessionId, sizeof(GUID)) == 0;
        }
        catch (...)
        {
            return false;
        }
    }
}


bool LoopbackServiceTests::TestSetup()
{
    m_deviceNodeTracker.Start();

    return true;
}

bool LoopbackServiceTests::TestCleanup()
{
    m_deviceNodeTracker.RemoveDeviceNodesCreatedSinceStart();

    return true;
}


void LoopbackServiceTests::TestSessionListReportsEachConnectionTime()
{
    if (!Feature_Servicing_MIDI2SessionTrackerConnectionTime::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2SessionTrackerConnectionTime is disabled.");
        return;
    }

    if (!LoopbackAvailable())
    {
        Log::Result(TestResults::Skipped, L"Loopback transport is not available.");
        return;
    }

    MidiTest::MultithreadedApartment apartment{ };

    OpenLoopback loopback{ };
    auto cleanup = wil::scope_exit([&] { CloseAndRemove(loopback); });

    // one session with two connections, one to each end
    OpenBothSides(L"Session List Test", false, loopback);

    std::wstring listJson{ };
    VERIFY_SUCCEEDED(loopback.Session.GetSessionList(listJson));

    json::JsonObject list{ nullptr };
    VERIFY_IS_TRUE(json::JsonObject::TryParse(listJson, list), listJson.c_str());

    auto const sessions = list.GetNamedArray(L"sessions", nullptr);
    VERIFY_IS_NOT_NULL(sessions, listJson.c_str());

    json::JsonObject ours{ nullptr };

    for (auto const& value : sessions)
    {
        if (value.ValueType() != json::JsonValueType::Object)
        {
            continue;
        }

        auto const session = value.GetObject();

        if (IsSession(session.GetNamedString(L"id", L""), loopback.Session.Id()))
        {
            ours = session;
            break;
        }
    }

    VERIFY_IS_NOT_NULL(ours, listJson.c_str());

    auto const connections = ours.GetNamedArray(L"connections", nullptr);
    VERIFY_IS_NOT_NULL(connections, listJson.c_str());
    VERIFY_ARE_EQUAL(connections.Size(), (uint32_t)2);

    for (auto const& value : connections)
    {
        VERIFY_IS_TRUE(value.ValueType() == json::JsonValueType::Object);

        auto const connection = value.GetObject();

        VERIFY_IS_TRUE(connection.HasKey(L"earliestStartTime"), listJson.c_str());
        VERIFY_IS_GREATER_THAN(connection.GetNamedNumber(L"earliestStartTime", 0), 0.0);
    }

    // Before the gate the time went onto the session instead, once for each connection, so only
    // the last connection's time survived.
    VERIFY_IS_FALSE(ours.HasKey(L"earliestStartTime"), listJson.c_str());
}


void LoopbackServiceTests::TestPortNumbersStayUniqueAcrossCreateAndRemove()
{
    if (!LoopbackAvailable())
    {
        Log::Result(TestResults::Skipped, L"Loopback transport is not available.");
        return;
    }

    MidiTest::MultithreadedApartment apartment{ };

    auto const portsBefore =
        CountPorts(GetServiceAssignedPortNumbers(Midi1OutputPortSelector)) +
        CountPorts(GetServiceAssignedPortNumbers(Midi1InputPortSelector));

    std::vector<std::wstring> created{ };

    auto removeAll = wil::scope_exit([&]
        {
            for (auto const& associationId : created)
            {
                try
                {
                    RemoveLoopback(associationId);
                }
                catch (...)
                {
                }
            }
        });

    auto create = [&](_In_ std::wstring const& name)
        {
            auto const loopback = CreateLoopback(name);
            VERIFY_IS_TRUE(loopback.Created(), loopback.Detail.c_str());

            created.push_back(loopback.AssociationId);
        };

    create(L"Port Number Test 1");
    create(L"Port Number Test 2");
    create(L"Port Number Test 3");

    // a gap in the middle, which the next ports may be given
    VERIFY_IS_TRUE(RemoveLoopback(created[1]).CallSucceeded);
    created.erase(created.begin() + 1);

    create(L"Port Number Test 4");
    create(L"Port Number Test 5");

    // Ports appear, and gaps close, shortly after the calls return, so this waits until two reads
    // a second apart agree.
    PortNumbers outputs{ };
    PortNumbers inputs{ };

    auto const settled = MidiTest::WaitUntil([&]
        {
            auto const earlierOutputs = GetServiceAssignedPortNumbers(Midi1OutputPortSelector);
            auto const earlierInputs = GetServiceAssignedPortNumbers(Midi1InputPortSelector);

            Sleep(1000);

            outputs = GetServiceAssignedPortNumbers(Midi1OutputPortSelector);
            inputs = GetServiceAssignedPortNumbers(Midi1InputPortSelector);

            return outputs == earlierOutputs && inputs == earlierInputs;
        },
        15000);

    VERIFY_IS_TRUE(settled, L"the MIDI 1.0 port numbers kept changing");

    // the new loopbacks' ports have to have been numbered at all, or this proves nothing
    VERIFY_IS_GREATER_THAN(CountPorts(outputs) + CountPorts(inputs), portsBefore);

    VERIFY_ARE_EQUAL(CountDuplicates(outputs, L"Output"), (uint32_t)0);
    VERIFY_ARE_EQUAL(CountDuplicates(inputs, L"Input"), (uint32_t)0);
}
