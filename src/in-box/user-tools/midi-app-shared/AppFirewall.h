// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Windows Firewall, for a tool that needs something let in: the MIDI service for Network MIDI
// Setup, or Windows MIDI Patchbay itself for its OSC connections. Reading it needs no
// administrator rights. Changing it does, so the tool starts an elevated copy of itself with a
// switch that does only that.
//
// Shared by Network MIDI Setup and Windows MIDI Patchbay. Each passes what its rule is for: the
// program, its name and description, and the switch its elevated copy recognizes.

#include <string>
#include <vector>

namespace midiapp::firewall
{
    // Network types, numbered the way Windows Firewall numbers them
    constexpr long DomainNetworks = 0x1;
    constexpr long PrivateNetworks = 0x2;
    constexpr long PublicNetworks = 0x4;
    constexpr long AllNetworks = DomainNetworks | PrivateNetworks | PublicNetworks;

    // IP protocol numbers, as Windows Firewall numbers them
    constexpr long TcpProtocol = 6;
    constexpr long UdpProtocol = 17;

    enum class FirewallStatus
    {
        // Windows Firewall could not be read
        Unknown,

        // there is no program to let in: the service isn't installed
        ServiceNotFound,

        // Windows lists no network this PC is connected to
        NotConnected,

        // nothing stops the program on the networks this PC is connected to now
        Open,

        // nothing lets the program in on at least one of them
        Closed,

        // a rule blocks the program on at least one of them
        ClosedByRule,

        // "block all incoming connections" is on for at least one of them
        ClosedForAll,

        // the firewall is off on all of them
        Off,
    };

    // A network Windows lists as connected now
    struct ConnectedNetwork
    {
        // what Windows calls it, like a Wi-Fi network's name or "Unidentified network"
        std::wstring Name{};

        // the adapters it is reached through, like "Ethernet" or "Wi-Fi"
        std::vector<std::wstring> AdapterNames{};

        // DomainNetworks, PrivateNetworks or PublicNetworks
        long Type{ PublicNetworks };

        // Open, Closed, ClosedByRule, ClosedForAll or Off, for this network alone
        FirewallStatus Status{ FirewallStatus::Unknown };
    };

    struct FirewallState
    {
        FirewallStatus Status{ FirewallStatus::Unknown };

        // where the program runs from, which is what a rule has to name
        std::wstring ProgramPath{};

        // the types of the networks Windows lists as connected now
        long ConnectedNetworks{ 0 };

        // When Windows can't list them, one entry with no name for each type the firewall has in use
        std::vector<ConnectedNetwork> Networks{};

        // Only a PC joined to a domain can be on a domain network
        bool JoinedToDomain{ false };

        // what the rule this tool adds covers. 0 when there is no such rule, or it is turned off.
        long OwnRuleNetworks{ 0 };

        // The names of rules that block the program, for the networks that are connected.
        std::vector<std::wstring> BlockingRuleNames{};

        // group policy decides, so a rule added on this PC may have no effect
        bool ManagedByPolicy{ false };
    };

    // What a rule is for.
    struct FirewallTarget
    {
        // The program a rule names. Empty for a service: its executable is asked for by
        // ServiceName.
        std::wstring ProgramPath{};

        // The service the program runs as. A rule may name the service instead of the program.
        // Empty for a program that isn't a service.
        std::wstring ServiceName{};

        // Like L"\\midisrv.exe". This tool's own rule is recognized by its name and by naming a
        // program with this file name, wherever the program has moved to since.
        std::wstring ProgramFileName{};

        // What Windows Firewall shows for the rule this tool adds.
        std::wstring RuleName{};
        std::wstring RuleDescription{};

        // The first argument the elevated copy is started with, like
        // L"--allow-midi-service-through-firewall". It has to be one a protocol or file
        // activation can never produce.
        std::wstring AllowSwitch{};

        // What the program listens with: UdpProtocol, TcpProtocol, or both. The rule this tool adds
        // lets each in, one rule for each, and the program counts as let in only where all of
        // them are.
        std::vector<long> Protocols{ UdpProtocol };

        // Who the rule lets in, in Windows Firewall's terms, like L"LocalSubnet". Empty leaves it
        // as it is: any address for a new rule.
        std::wstring RemoteAddresses{};
    };

    // Reads Windows Firewall as it is now. Needs no administrator rights, but walks every rule,
    // so it belongs off the UI thread.
    FirewallState QueryState(_In_ FirewallTarget const& target) noexcept;

    // Has the elevated copy add or update the rule, and waits for it. Blocks. ERROR_CANCELLED as an
    // HRESULT means the customer said no to the administrator prompt.
    HRESULT RequestAllow(_In_ HWND const owner, _In_ FirewallTarget const& target, _In_ long const networks) noexcept;

    // Whether this process was started as the elevated copy, with this switch first. Cheap, so it
    // can run before anything else on every launch.
    bool IsAllowRequest(_In_ std::wstring_view allowSwitch) noexcept;

    // Called first thing in wWinMain. True when this process is the elevated copy, in which case
    // the work is already done: result is what it came to and networks what it was asked for.
    bool TryRunAllowRequest(_In_ FirewallTarget const& target, _Out_ HRESULT& result, _Out_ long& networks) noexcept;

    // This process's own executable.
    std::wstring CurrentProgramPath();
}
