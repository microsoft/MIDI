// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Words for OSC connections, from the app's resources.

#include "OscConnection.h"
#include "OscHub.h"

namespace midipatchbay
{
    winrt::hstring OscPresetName(_In_ OscPreset preset) noexcept;
    winrt::hstring OscFormatName(_In_ OscFormat format) noexcept;

    // What a row's group shows on the canvas: "/vkb_midi (REAPER)".
    std::wstring OscRowLabel(_In_ OscRow const& row) noexcept;

    // What a rule makes, for people to read: "controller {n}+70", "note 60", "pitch bend".
    std::wstring OscRuleSummary(_In_ OscRule const& rule) noexcept;

    // The line under the node's name: what it listens on and where it sends.
    std::wstring OscNodeSubtitle(_In_ OscConnectionSettings const& settings) noexcept;

    // How it's listening, for the inspector: "Listening on port 8000, on every network."
    std::wstring OscListeningText(_In_ OscConnectionSettings const& settings, _In_ std::optional<OscConnectionStatus> const& status) noexcept;

    // "Last heard from 192.168.1.33, 2 seconds ago." Empty before anything arrived.
    std::wstring OscLastHeardText(_In_ OscConnectionStatus const& status) noexcept;

    // An OSC connection in this patch is set to listen on every network, and Windows Firewall
    // is holding it to this PC. Whether the patch routes is the caller's to check.
    bool OscFirewallHolds(_In_ PatchDocument const& patch) noexcept;

    // An OSC connection in this patch receives on every network, so the firewall matters to it.
    bool WantsOscOnEveryNetwork(_In_ PatchDocument const& patch) noexcept;

    // What the palette's "OSC connection" item carries in place of an endpoint device id.
    constexpr wchar_t OscNewConnectionTag[] = L"osc:new";
}
