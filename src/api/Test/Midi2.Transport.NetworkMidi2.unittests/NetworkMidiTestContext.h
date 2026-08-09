// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

#include "NetworkMidiTestClient.h"
#include "NetworkMidiTestHostLocator.h"

namespace NetworkMidiTest
{
    // Shared setup for every protocol test. Locates the host once and reuses it, because mDNS
    // discovery is slow and the answer does not change during a run.
    class ProtocolTestContext
    {
    public:
        static ProtocolTestContext& Current();

        // Returns false and logs why if there is no local host to test against. Tests call
        // this first and skip themselves rather than failing, so a machine with no configured
        // host does not produce a wall of red.
        bool EnsureHostAvailable();

        HostEndpointAddress const& Host() const { return m_host; }

        // Names this test client presents to the host. Unique per process so parallel runs and
        // leftover endpoints from previous runs do not collide.
        std::string TestEndpointName() const { return m_testEndpointName; }
        std::string TestProductInstanceId() const { return m_testProductInstanceId; }

        // A fresh name/id pair, for tests which need their own identity
        std::string MakeUniqueEndpointName(_In_ std::string const& suffix) const;
        std::string MakeUniqueProductInstanceId(_In_ std::string const& suffix) const;

    private:
        ProtocolTestContext();

        WinsockScope m_winsock{ };
        HostEndpointAddress m_host{ };
        bool m_located{ false };
        bool m_available{ false };

        std::string m_testEndpointName{ };
        std::string m_testProductInstanceId{ };
    };


    // Logs each command in a packet, so a failing assertion is accompanied by what actually
    // arrived on the wire.
    void LogPacket(_In_ std::wstring const& label, _In_ ParsedPacket const& packet);
    void LogNoPacket(_In_ std::wstring const& label);

    // Records which specification requirement a check covers. These strings end up in the TAEF
    // log so a run doubles as a conformance report.
    void LogSpecRequirement(_In_ std::wstring const& requirement);

    // Opens a client socket and completes a session with the host. Returns false if the host
    // never accepted, having already logged the reason. The timeout is generous because the
    // host creates a MIDI endpoint device before it replies, which is not quick.
    bool EstablishSession(
        _In_ UdpTestClient& client,
        _In_ std::string const& endpointName,
        _In_ std::string const& productInstanceId,
        _In_ std::chrono::milliseconds const timeout = std::chrono::milliseconds(20000));

    // Politely ends a session so the host does not keep the endpoint around between tests.
    void EndSession(_In_ UdpTestClient& client);
}
