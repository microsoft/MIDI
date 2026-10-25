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

    // A host started or stopped waiting for a missing network adapter. Deferred the same way.
    void SignalHostNetworkAdapterChanged() noexcept;

    // Waits for a write already queued, and lets go of the threadpool work. Called from
    // TransportState::Shutdown: left to the static destructor, the wait happened at DLL unload,
    // under the loader lock. A signal after this starts a new one.
    void Shutdown() noexcept;

private:
    void Queue(_Inout_ std::atomic<uint32_t>& queued, _In_ PCWSTR const valueName) noexcept;

    static void BumpCounter(_In_ PCWSTR const valueName) noexcept;

    // Created on first use. Guarded by m_workLock.
    wil::critical_section m_workLock;
    std::unique_ptr<ThreadpoolWork> m_work{ };

    // Zero means nothing queued. Set before the work item runs, cleared by it, so a change which
    // arrives while the write is in flight queues another rather than being swallowed.
    std::atomic<uint32_t> m_pendingApprovalWriteQueued{ 0 };
    std::atomic<uint32_t> m_hostNetworkAdapterWriteQueued{ 0 };
};
