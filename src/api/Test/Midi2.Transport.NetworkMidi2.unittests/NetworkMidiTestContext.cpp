// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include <sstream>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace NetworkMidiTest
{
    ProtocolTestContext& ProtocolTestContext::Current()
    {
        static ProtocolTestContext current;

        return current;
    }

    ProtocolTestContext::ProtocolTestContext()
    {
        // Identity has to be unique per process. The service keys endpoints on
        // UMP Endpoint Name plus Product Instance Id, so reusing a fixed pair across runs would
        // collide with an endpoint a previous run left behind.
        auto processId = GetCurrentProcessId();

        std::stringstream name;
        name << "WinMidiProtocolTest-" << processId;

        std::stringstream productId;
        productId << "WMPT" << processId;

        m_testEndpointName = name.str();
        m_testProductInstanceId = productId.str();
    }

    std::string ProtocolTestContext::MakeUniqueEndpointName(std::string const& suffix) const
    {
        return m_testEndpointName + "-" + suffix;
    }

    std::string ProtocolTestContext::MakeUniqueProductInstanceId(std::string const& suffix) const
    {
        return m_testProductInstanceId + suffix;
    }

    bool ProtocolTestContext::EnsureHostAvailable()
    {
        if (!m_located)
        {
            m_located = true;

            if (!m_winsock.IsInitialized())
            {
                Log::Error(L"Winsock could not be initialized.");

                return false;
            }

            Log::Comment(L"Locating a local Network MIDI 2.0 host...");

            auto located = LocateLocalServiceHost(std::chrono::milliseconds(45000));

            Log::Comment(String().Format(L"Locator: %s", located.Diagnostics.c_str()));

            if (located.Found)
            {
                m_host = located.Address;
                m_available = true;

                Log::Comment(String().Format(
                    L"Testing against %s:%u (found via %s)",
                    m_host.HostNameOrAddress.c_str(),
                    static_cast<unsigned>(m_host.Port),
                    m_host.DiscoveredVia.c_str()));

                if (!m_host.AdvertisedEndpointName.empty())
                {
                    Log::Comment(String().Format(
                        L"Advertised UMPEndpointName='%s' ProductInstanceId='%s'",
                        m_host.AdvertisedEndpointName.c_str(),
                        m_host.AdvertisedProductInstanceId.c_str()));
                }
            }
        }

        if (!m_available)
        {
            // Not a skip. A missing host means nothing was verified, and a run of skips reads
            // as green, which is how a broken environment stays invisible.
            Log::Error(L"No local Network MIDI 2.0 host is available to test against.");
        }

        return m_available;
    }


    void LogPacket(std::wstring const& label, ParsedPacket const& packet)
    {
        Log::Comment(String().Format(L"%s %s", label.c_str(), DescribePacket(packet).c_str()));
    }

    void LogNoPacket(std::wstring const& label)
    {
        Log::Comment(String().Format(L"%s <no packet received>", label.c_str()));
    }


    void WarnIfSlowerThan(
        std::wstring const& label,
        std::chrono::milliseconds const actual,
        std::chrono::milliseconds const budget)
    {
        if (actual <= budget)
        {
            return;
        }

        // Deliberately not a failure. How quickly the service responds is a quality of service
        // question, and machine load or an unrelated endpoint teardown can move it. Correctness
        // is asserted separately.
        Log::Warning(String().Format(
            L"PERF: %s took %lldms, over the %lldms budget.",
            label.c_str(),
            static_cast<long long>(actual.count()),
            static_cast<long long>(budget.count())));
    }

    void LogSpecRequirement(std::wstring const& requirement)
    {
        Log::Comment(String().Format(L"SPEC: %s", requirement.c_str()));
    }


    bool EstablishSession(
        UdpTestClient& client,
        std::string const& endpointName,
        std::string const& productInstanceId,
        std::chrono::milliseconds const timeout)
    {
        auto& context = ProtocolTestContext::Current();

        if (!client.IsOpen())
        {
            if (!client.Open(context.Host()))
            {
                Log::Error(L"Could not open a UDP socket to the host.");

                return false;
            }
        }

        PacketBuilder builder;
        builder.StartPacket().AddInvitation(endpointName, productInstanceId);

        if (!client.Send(builder))
        {
            Log::Error(L"Failed to send the invitation.");

            return false;
        }

        auto started = std::chrono::steady_clock::now();

        auto reply = client.WaitForCommand(CommandCode::InvitationReplyAccepted, timeout);

        if (!reply.has_value())
        {
            LogNoPacket(L"Waiting for InvitationReplyAccepted:");

            return false;
        }

        WarnIfSlowerThan(
            L"Session establishment",
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started),
            std::chrono::milliseconds(5000));

        LogPacket(L"Session established. Reply:", reply.value());

        return true;
    }

    void EndSession(UdpTestClient& client)
    {
        if (!client.IsOpen())
        {
            return;
        }

        PacketBuilder builder;
        builder.StartPacket().AddBye(ByeReason::UserTerminated, "Test complete.");

        client.Send(builder);

        // give the host a moment to answer and tear down, but do not fail the test on it
        client.WaitForCommand(CommandCode::ByeReply, std::chrono::milliseconds(2000));

        client.Close();
    }
}
