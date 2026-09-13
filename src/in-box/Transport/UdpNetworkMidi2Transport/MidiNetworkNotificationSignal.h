// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "network_notification_defs.h"

// Nudges user-session apps when something needs the customer's attention. See
// network_notification_defs.h for why this carries no state.
class MidiNetworkNotificationSignal
{
public:
    // Safe to call from the socket receive path. The registry write is deferred, and repeated
    // calls before it runs collapse into one, so a burst of invitations costs a single write.
    void SignalPendingApprovalChanged() noexcept;

private:
    static void BumpCounter(_In_ PCWSTR const valueName) noexcept;

    ThreadpoolWork m_work{ };

    // Zero means nothing queued. Set before the work item runs, cleared by it, so a change which
    // arrives while the write is in flight queues another rather than being swallowed.
    std::atomic<uint32_t> m_pendingApprovalWriteQueued{ 0 };
};
