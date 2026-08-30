// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// The client-role invitation handshake from spec 6.4 and 6.6, kept apart from the rest of the
// connection. It owns four pieces of state which are only meaningful together, and which used to
// be four loose atomics that every caller had to keep consistent by hand.
//
// It sends nothing itself. Tick() returns the action the connection should take, so the network
// and the rules stay separable and the rules stay testable.

enum class MidiNetworkInvitationAction
{
    // nothing due this tick
    None,

    // repeat the invitation, spec 6.4
    SendInvitation,

    // the host answered Pending and then never resolved it within the configured timeout
    CancelNotApproved,

    // the host never answered at all
    CancelNoReply,
};

class MidiNetworkInvitationState
{
public:
    // A fresh invitation has been put on the wire. Attempt one is that invitation.
    void Begin() noexcept
    {
        m_replyPendingReceived = false;
        m_replyPendingTimestamp = 0;
        m_attempts = 1;
        m_pending = true;
    }

    // The host answered, whatever the answer was.
    void Answered() noexcept
    {
        m_pending = false;
        m_replyPendingReceived = false;
    }

    bool IsPending() const noexcept { return m_pending; }

    // Invitation Reply: Pending. Only the first one starts the clock, so a host which repeats it
    // cannot extend the wait indefinitely. Returns true if this was the first.
    bool NoteReplyPending(_In_ uint64_t const timestamp) noexcept
    {
        if (m_replyPendingReceived.exchange(true))
        {
            return false;
        }

        m_replyPendingTimestamp = timestamp;

        return true;
    }

    bool ReplyPendingReceived() const noexcept { return m_replyPendingReceived; }
    uint64_t ReplyPendingTimestamp() const noexcept { return m_replyPendingTimestamp; }

    // Called on the watchdog tick. sessionActive is passed in because the session is the
    // connection's business, not this object's.
    MidiNetworkInvitationAction Tick(
        _In_ bool const sessionActive,
        _In_ uint64_t const elapsedSincePendingMilliseconds,
        _In_ uint64_t const pendingTimeoutMilliseconds,
        _In_ uint16_t const maxAttempts) noexcept
    {
        if (!m_pending || sessionActive)
        {
            return MidiNetworkInvitationAction::None;
        }

        // The host said it is waiting on a person rather than ignoring us. Keep waiting without
        // re-inviting, but not forever.
        if (m_replyPendingReceived)
        {
            if (elapsedSincePendingMilliseconds < pendingTimeoutMilliseconds)
            {
                return MidiNetworkInvitationAction::None;
            }

            m_pending = false;
            m_replyPendingReceived = false;

            return MidiNetworkInvitationAction::CancelNotApproved;
        }

        if (m_attempts >= maxAttempts)
        {
            m_pending = false;

            return MidiNetworkInvitationAction::CancelNoReply;
        }

        m_attempts++;

        return MidiNetworkInvitationAction::SendInvitation;
    }

private:
    // Touched from the watchdog thread and from message parsing.
    std::atomic<bool> m_pending{ false };
    std::atomic<uint16_t> m_attempts{ 0 };

    std::atomic<bool> m_replyPendingReceived{ false };
    std::atomic<uint64_t> m_replyPendingTimestamp{ 0 };
};
