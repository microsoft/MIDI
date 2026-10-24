// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// One remote a host turned away because it already had a session with this PC
struct MidiNetworkRecentRefusal
{
    std::wstring UmpEndpointName{};
    std::wstring ProductInstanceId{};
    std::wstring RemoteAddress{};

    MidiNetworkSessionRefusalReason Reason{ MidiNetworkSessionRefusalReason::None };

    // When, as a FILETIME for reporting, and on the steady clock for expiry
    uint64_t FileTime{ 0 };
    std::chrono::steady_clock::time_point When{};
};

// The refusals a host remembers, newest first, one entry per remote (name and product instance
// id). The remote's identity is unverified and a remote retries, so the list is small, a repeat
// replaces its entry, with the latest reason and time, rather than adding one, and entries expire.
class MidiNetworkRecentRefusals
{
public:
    explicit MidiNetworkRecentRefusals(
        _In_ size_t const maxCount = MIDI_NETWORK_HOST_RECENT_REFUSALS_MAX,
        _In_ std::chrono::milliseconds const lifetime = std::chrono::milliseconds(MIDI_NETWORK_HOST_RECENT_REFUSAL_LIFETIME_MILLISECONDS)) noexcept :
        m_maxCount(maxCount),
        m_lifetime(lifetime)
    {
    }

    void Add(_In_ MidiNetworkRecentRefusal const& refusal) noexcept
    {
        try
        {
            auto lock = m_lock.lock();

            Prune(refusal.When);

            std::erase_if(m_entries, [&refusal](MidiNetworkRecentRefusal const& entry)
                {
                    return entry.UmpEndpointName == refusal.UmpEndpointName &&
                        entry.ProductInstanceId == refusal.ProductInstanceId;
                });

            m_entries.push_front(refusal);

            while (m_entries.size() > m_maxCount)
            {
                m_entries.pop_back();
            }
        }
        CATCH_LOG();
    }

    // Newest first, without any which have expired
    std::vector<MidiNetworkRecentRefusal> Get(_In_ std::chrono::steady_clock::time_point const now) noexcept
    {
        try
        {
            auto lock = m_lock.lock();

            Prune(now);

            return std::vector<MidiNetworkRecentRefusal>(m_entries.begin(), m_entries.end());
        }
        catch (...)
        {
            return {};
        }
    }

    void Clear() noexcept
    {
        auto lock = m_lock.lock();

        m_entries.clear();
    }

private:
    // Needs m_lock
    void Prune(_In_ std::chrono::steady_clock::time_point const now) noexcept
    {
        while (!m_entries.empty() && now - m_entries.back().When >= m_lifetime)
        {
            m_entries.pop_back();
        }
    }

    size_t const m_maxCount;
    std::chrono::milliseconds const m_lifetime;

    wil::critical_section m_lock;
    std::deque<MidiNetworkRecentRefusal> m_entries{};
};
