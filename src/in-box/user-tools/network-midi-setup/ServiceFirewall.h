// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The MIDI service has no window, so Windows never asks whether to let it through the firewall.
// Changing that needs administrator rights, so the window starts an elevated copy of this app.
// The work is done by the shared midi-app-shared\AppFirewall, which Windows MIDI Patchbay uses
// for its own rule too; this names what Network MIDI Setup's rule is for.

#include "AppFirewall.h"

namespace midinetworksetup::firewall
{
    using ::midiapp::firewall::DomainNetworks;
    using ::midiapp::firewall::PrivateNetworks;
    using ::midiapp::firewall::PublicNetworks;
    using ::midiapp::firewall::AllNetworks;

    using FirewallStatus = ::midiapp::firewall::FirewallStatus;
    using ConnectedNetwork = ::midiapp::firewall::ConnectedNetwork;
    using FirewallState = ::midiapp::firewall::FirewallState;

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
