// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, XAML and the MIDI SDK.

#include "ValueThrottle.h"

namespace glass
{
    _Use_decl_annotations_
    bool ValueThrottle::ShouldSend(double value, uint64_t nowMs) noexcept
    {
        m_pendingValue = value;
        m_hasPending = true;

        if (m_intervalMs == 0)
        {
            m_lastSentMs = nowMs;
            m_lastSentValue = value;
            m_hasSent = true;
            m_hasPending = false;
            return true;
        }

        // The first move of a gesture always goes, so a control responds immediately however
        // heavy the rate limit is.
        if (!m_hasSent || nowMs - m_lastSentMs >= m_intervalMs)
        {
            m_lastSentMs = nowMs;
            m_lastSentValue = value;
            m_hasSent = true;
            m_hasPending = false;
            return true;
        }

        ++m_suppressed;
        return false;
    }

    _Use_decl_annotations_
    bool ValueThrottle::Release(double& value) noexcept
    {
        value = m_pendingValue;

        // Only when the last thing the finger did was not what last went out. Sending an
        // unchanged value again would be harmless but it would also be noise on a wire that is
        // rate limited precisely because it has none to spare.
        auto const needed = m_hasPending && (!m_hasSent || m_pendingValue != m_lastSentValue);

        if (needed)
        {
            m_lastSentValue = m_pendingValue;
            m_hasSent = true;
        }

        m_hasPending = false;

        return needed;
    }

    void ValueThrottle::Reset() noexcept
    {
        m_lastSentMs = 0;
        m_pendingValue = 0.0;
        m_lastSentValue = 0.0;
        m_hasSent = false;
        m_hasPending = false;
        m_suppressed = 0;
    }
}
