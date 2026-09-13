// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Decides whether the customer is told that remote clients are waiting for a decision.
//
// Everything here runs on the app's message thread. The registry watcher only posts a message,
// so there is one thread deciding and no lock around the set of clients already reported.
class NetworkApprovalNotifier
{
public:
    // Re-reads the pending list from the service and notifies if there is anything new. The
    // registry value which triggered this is only a hint, and anyone on the machine can write
    // it, so nothing about it is trusted or even read: the service's answer is the only input.
    void Evaluate() noexcept;

private:
    struct PendingClient
    {
        std::wstring Identity;      // name and product instance id, matched the way the service matches
        std::wstring DisplayName;   // already sanitized for a toast
    };

    std::vector<PendingClient> ReadPendingClients() const noexcept;

    void Notify(_In_ std::vector<PendingClient> const& pending) noexcept;

    ToastSender m_toast{ };

    // Clients the customer has already been shown. A remote re-inviting every few seconds must
    // not produce a second banner, and neither must one which was already in the last one.
    std::set<std::wstring> m_reportedIdentities{ };

    std::chrono::steady_clock::time_point m_lastToast{ };

    static constexpr wchar_t ToastTag[]{ L"network-approval" };

    // A host coming up hands us every waiting remote at once. They collapse into one banner, and
    // this stops a slower trickle from turning into a sequence of them.
    static constexpr std::chrono::seconds MinimumIntervalBetweenToasts{ 20 };
};
