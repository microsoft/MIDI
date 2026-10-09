// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The MIDI service has no window, so Windows never asks whether to let it through the firewall.
// Changing that needs administrator rights, so the window starts an elevated copy of this app.
namespace midinetworksetup::firewall
{
    // Network types, numbered the way Windows Firewall numbers them. A domain network is one an
    // organization runs, so it goes with private networks.
    constexpr long PrivateNetworks = 0x1 | 0x2;
    constexpr long PublicNetworks = 0x4;
    constexpr long AllNetworks = PrivateNetworks | PublicNetworks;

    enum class FirewallStatus
    {
        // Windows Firewall could not be read
        Unknown,

        // there is no MIDI service to let in
        ServiceNotFound,

        // Windows lists no network this PC is connected to
        NotConnected,

        // nothing stops the service on the networks this PC is connected to now
        Open,

        // nothing lets the service in on at least one of them
        Closed,

        // a rule blocks the service on at least one of them
        ClosedByRule,

        // "block all incoming connections" is on for at least one of them
        ClosedForAll,

        // the firewall is off on all of them
        Off,
    };

    struct FirewallState
    {
        FirewallStatus Status{ FirewallStatus::Unknown };

        // where the service runs from, which is what a rule has to name
        std::wstring ServicePath{};

        // the types of the networks Windows lists as connected now
        long ConnectedNetworks{ 0 };

        // what the rule this app adds covers. 0 when there is no such rule, or it is turned off.
        long OwnRuleNetworks{ 0 };

        // group policy decides, so a rule added on this PC may have no effect
        bool ManagedByPolicy{ false };
    };

    // Reads Windows Firewall as it is now. Needs no administrator rights, but walks every rule,
    // so it belongs off the UI thread.
    FirewallState QueryState() noexcept;

    // Has the elevated copy add or update the rule, and waits for it. Blocks. ERROR_CANCELLED as an
    // HRESULT means the customer said no to the administrator prompt.
    HRESULT RequestAllow(_In_ HWND const owner, _In_ long const networks) noexcept;

    // Called first thing in wWinMain. True when this process is the elevated copy, in which case
    // the work is already done and exitCode is the result.
    bool TryRunAllowRequest(_Out_ int& exitCode) noexcept;
}
