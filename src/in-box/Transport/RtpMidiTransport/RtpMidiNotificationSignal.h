// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Nudges user-session apps when something needs the customer's attention. See
// rtp_notification_defs.h.
class RtpMidiNotificationSignal
{
public:
    // Safe on a socket receive thread. The registry write is deferred, and calls made before it
    // runs collapse into one, so a burst of invitations costs a single write.
    void SignalPendingApprovalChanged() noexcept;

    // A host started or stopped waiting for a missing network adapter. Deferred the same way.
    void SignalHostNetworkAdapterChanged() noexcept;

    // Lets a write already queued finish and releases the work item, so nothing is left to run
    // when the DLL unloads. A signal after this queues work again.
    void Shutdown() noexcept;

private:
    void Queue(_Inout_ std::atomic<uint32_t>& queued, _In_ PCWSTR const valueName) noexcept;

    static void BumpCounter(_In_ PCWSTR const valueName) noexcept;

    // Cleared by the work item before it writes, so a change during the write queues another.
    // Declared before the work item, because the work runs what is left as it is destroyed.
    std::atomic<uint32_t> m_pendingApprovalWriteQueued{ 0 };
    std::atomic<uint32_t> m_hostNetworkAdapterWriteQueued{ 0 };

    wil::srwlock m_workLock;
    std::unique_ptr<ThreadpoolWork> m_work;
};
