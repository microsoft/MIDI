// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Nudges user-session apps when a remote starts or stops waiting. See rtp_notification_defs.h.
class RtpMidiNotificationSignal
{
public:
    // Safe on a socket receive thread. The registry write is deferred, and calls made before it
    // runs collapse into one, so a burst of invitations costs a single write.
    void SignalPendingApprovalChanged() noexcept;

private:
    static void BumpCounter() noexcept;

    ThreadpoolWork m_work{ };

    // cleared by the work item before it writes, so a change during the write queues another
    std::atomic<uint32_t> m_writeQueued{ 0 };
};
