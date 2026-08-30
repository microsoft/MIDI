// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// Rate limiter for replies we send to a remote purely because it sent us something -
// refusals, NAKs and similar. Those are reflection/amplification vectors: a forged source
// address turns every stray datagram into traffic aimed at the victim.
//
// KNOWN DEVIATION FROM THE SPEC, ACCEPTED DELIBERATELY:
// M2-124-UM 6.4 says a Host receiving an Invitation shall respond with Invitation Reply:
// Accepted, a Bye, or an authentication reply. When this limiter suppresses a refusal, the
// Host answers an Invitation with nothing at all, which is none of those three. A client
// cannot then tell a full or unwilling host from an absent one until its own timeout.
// This is the intended trade: honoring 6.4 unconditionally means any remote can make us
// emit one packet per packet it sends, to any address it cares to forge, and a
// conformance bug is preferable to shipping an amplifier. The client's retry loop covers
// the honest case, because the per-remote interval is short.
//
// The table is a FIXED size on purpose. A map keyed by remote would itself be a memory
// exhaustion vector, since the addresses are attacker-chosen and unverified. A hash
// collision here costs one suppressed refusal and the remote retries, which is the safe
// direction to fail.
class MidiNetworkReplyRateLimiter
{
public:
    bool ShouldSend(_In_ uint64_t const remoteKey)
    {
        auto now = std::chrono::steady_clock::now();

        auto lock = m_lock.lock();

        // Global ceiling as well as a per-remote one, so traffic spread across many forged
        // sources cannot use the service as an amplifier.
        if (now - m_windowStart >= std::chrono::seconds(1))
        {
            m_windowStart = now;
            m_windowCount = 0;
        }

        if (m_windowCount >= MaxRepliesPerSecond)
        {
            return false;
        }

        auto& slot = m_slots[remoteKey % SlotCount];

        if (slot.Key == remoteKey && (now - slot.LastSent) < MinIntervalPerRemote)
        {
            return false;
        }

        slot.Key = remoteKey;
        slot.LastSent = now;

        ++m_windowCount;

        return true;
    }

    // FNV-1a over the remote address and port. Only used to pick a slot, never for security.
    static uint64_t MakeRemoteKey(_In_ std::wstring const& address, _In_ std::wstring const& port)
    {
        uint64_t hash{ 14695981039346656037ULL };

        auto mix = [&hash](std::wstring const& value)
            {
                for (auto const c : value)
                {
                    hash ^= static_cast<uint64_t>(c);
                    hash *= 1099511628211ULL;
                }
            };

        mix(address);
        mix(port);

        return hash;
    }

private:
    // Sized so that a large installation never collides in practice. This only ever holds
    // remotes we have NO session with, but "no session" is exactly what a roomful of devices
    // looks like just after the service restarts, so it needs to comfortably exceed the number
    // of endpoints anyone would realistically run. 4096 slots is 64KB, which is nothing.
    static constexpr size_t SlotCount{ 4096 };
    static constexpr uint32_t MaxRepliesPerSecond{ 500 };
    static constexpr std::chrono::milliseconds MinIntervalPerRemote{ 1000 };

    struct Slot
    {
        uint64_t Key{ 0 };
        std::chrono::steady_clock::time_point LastSent{ };
    };

    wil::critical_section m_lock;
    Slot m_slots[SlotCount]{ };
    std::chrono::steady_clock::time_point m_windowStart{ std::chrono::steady_clock::now() };
    uint32_t m_windowCount{ 0 };
};
