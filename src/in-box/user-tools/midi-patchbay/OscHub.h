// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The sockets behind OSC connections, and what each connection does with them.
//
// Each OSC connection in a patch that routes is an endpoint as far as the routing engine is
// concerned: IMidiEndpointConnectionRaw, the same interface the engine opens a device through. The
// engine sends to it, and sets its callback to hear from it, exactly as it does with a device.
//
// Receiving: one UDP socket for each local port, shared by every connection that listens on it,
// with a thread that waits on it. Each packet is read once for each connection on the port, which
// picks out what its sender filter and its rows take, and hands the MIDI to the engine's callback.
// The receive thread never takes a lock the engine holds.
//
// Sending: each connection that sends has a thread of its own. What the engine sends is queued with
// its timestamp, turned into OSC there, and sent when it's due, or at once in a bundle tagged with
// its time. The same thread looks up the names the connection sends to and takes messages from.
//
// TCP: a connection's own sending thread also connects, accepts, reads and writes its TCP sockets,
// so no other thread ever touches them. What arrives is read the same way as a UDP packet.
//
// Until Windows Firewall lets Windows MIDI Patchbay in, a connection set to listen on every
// network listens on this PC only, so the firewall's own prompt never appears.

#include "RouteGraph.h"
#include "OscTranslate.h"
#include "AppFirewall.h"

namespace midipatchbay
{
    enum class OscListenState : int32_t
    {
        // No receive port.
        NotReceiving = 0,

        EveryNetwork = 1,

        // Asked for, or held there until the firewall lets the app in: see HeldByFirewall.
        ThisPcOnly = 2,

        // Something else has the port. PortOwner says what, when Windows says.
        PortInUse = 3,

        Failed = 4,
    };

    // Something that arrived, as the inspector lists it.
    struct OscHeardMessage
    {
        std::wstring Address{};
        std::wstring Arguments{};
        std::wstring From{};

        // FILETIME, UTC.
        int64_t When{ 0 };

        // A row took it.
        bool Matched{ false };

        // The sender filter let it through.
        bool Accepted{ true };

        // A row that would take it, when its address looks like one of the formats.
        std::optional<OscRow> Suggestion{};

        // Otherwise, a Custom rule that would take it, like /1/fader{n} for /1/fader3.
        std::optional<OscRule> RuleSuggestion{};
    };

    constexpr size_t MaximumRecentlyHeard = 50;

    struct OscConnectionStatus
    {
        OscListenState Listening{ OscListenState::NotReceiving };

        // Set to listen on every network, but listening on this PC only until the firewall lets
        // the app in.
        bool HeldByFirewall{ false };

        uint16_t Port{ 0 };
        std::wstring PortOwner{};

        bool Sends{ false };

        // The send host's name couldn't be found.
        bool HostNotFound{ false };

        // Where messages go now, like "192.168.1.20:9000". Empty until known. Over TCP, the app
        // it's connected to.
        std::wstring SendingTo{};

        // Over TCP: the apps connected now, and the connections turned away, for being past
        // MaximumOscTcpApps or from a computer it doesn't take messages from.
        uint32_t ConnectedApps{ 0 };
        uint64_t TurnedAway{ 0 };

        uint64_t OscReceived{ 0 };
        uint64_t MidiReceived{ 0 };
        uint64_t Unreadable{ 0 };
        uint64_t NoRowMatched{ 0 };
        uint64_t NotAccepted{ 0 };
        uint64_t RateLimited{ 0 };
        uint64_t TooFarAhead{ 0 };
        uint64_t TooManyFromPattern{ 0 };

        uint64_t MidiSent{ 0 };
        uint64_t OscSent{ 0 };
        uint64_t NotSendable{ 0 };
        uint64_t LeftOut{ 0 };
        uint64_t NoRow{ 0 };
        uint64_t SendDropped{ 0 };
        uint64_t SendFailures{ 0 };

        // FILETIME, UTC. 0 before anything arrived.
        int64_t LastHeardWhen{ 0 };
        std::wstring LastHeardFrom{};

        // Newest first.
        std::vector<OscHeardMessage> RecentlyHeard{};
    };

    class OscHub
    {
    public:
        static OscHub& Current() noexcept;

        ~OscHub() noexcept;

        // The OSC connections in every patch that routes. Opens what's new, changes what changed,
        // and closes what's gone. Blocks while sockets close, so call it off the UI thread.
        void Configure(_In_ std::vector<RouteOscConnection> const& connections) noexcept;

        // An OSC connection as the routing engine opens a device. Null when it isn't configured.
        winrt::com_ptr<IMidiEndpointConnectionRaw> Endpoint(_In_ std::wstring const& deviceId) noexcept;

        // Closes everything.
        void Shutdown() noexcept;

        // Any thread.
        std::optional<OscConnectionStatus> Status(_In_ std::wstring const& deviceId) const noexcept;
        void ClearRecentlyHeard(_In_ std::wstring const& deviceId) noexcept;

        // The rules Windows MIDI Patchbay adds for itself: one for UDP and one for TCP, so a
        // connection can be changed to TCP without asking again.
        static midiapp::firewall::FirewallTarget FirewallTarget();

        // Reads Windows Firewall again, and opens sockets on every network when it now lets the app
        // in. Walks every rule, so call it off the UI thread.
        void RecheckFirewall() noexcept;

        // The firewall as last read, and whether a connection set to listen on every network is
        // held to this PC because of it.
        midiapp::firewall::FirewallState Firewall() const noexcept;
        bool IsHeldByFirewall() const noexcept;

        // Plays middle C, on and off, on a row through a temporary socket, so a connection can be
        // tried before its patch routes. Blocks for the note's length.
        static bool SendTestNote(_In_ OscConnectionSettings const& settings, _In_ uint8_t group) noexcept;

    private:
        OscHub() noexcept = default;

        struct Port;
        class Connection;

        // Needs m_lock.
        void ApplyPortsLocked() noexcept;
        void AdvertiseLocked() noexcept;
        bool LetsEveryNetworkIn() const noexcept;

        // Whether any of them waits for TCP connections on every network. Only then does the
        // firewall have to let TCP in too, so a UDP rule alone keeps working for UDP.
        static bool ListensOverTcp(_In_ std::vector<RouteOscConnection> const& connections) noexcept;

        // What the firewall is read for: UDP, and TCP when asked.
        static midiapp::firewall::FirewallTarget ReadTarget(_In_ bool tcp);

        mutable std::mutex m_lock{};

        std::vector<RouteOscConnection> m_configured{};
        std::map<std::wstring, winrt::com_ptr<Connection>> m_connections{};
        std::map<uint16_t, std::unique_ptr<Port>> m_ports{};

        midiapp::firewall::FirewallState m_firewall{};
        bool m_firewallRead{ false };
        bool m_firewallCoversTcp{ false };

        bool m_networkStarted{ false };
    };
}
