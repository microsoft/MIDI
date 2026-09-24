// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h, XAML and the MIDI SDK. The throttle is a decision about time, and a
// decision about time is only testable if the time comes in as an argument.

#include <sal.h>
#include <cstdint>

namespace glass
{
    // A fader dragged quickly produces a message per pointer move. A DIN cable carries about 350
    // three byte messages a second, shared with everything else on that wire, so a continuous
    // control needs a rate limit.
    //
    // The throttle sits on the value-changed notification rather than inside the engine, so one
    // rate limit governs both the send and the repaint. That keeps the two in step and means
    // there is no way for the surface to show a value that was never sent.
    //
    // Time is passed in rather than read, so the trailing send can be tested without waiting.
    class ValueThrottle
    {
    public:
        // Zero means no limit, which is what buttons, notes, sequences and system exclusive use.
        // Rate limiting a note on would be a defect, not a feature.
        void SetMinimumInterval(_In_ uint32_t milliseconds) noexcept { m_intervalMs = milliseconds; }

        uint32_t MinimumInterval() const noexcept { return m_intervalMs; }

        // A new value from the surface. Returns true when it should go out now.
        bool ShouldSend(_In_ double value, _In_ uint64_t nowMs) noexcept;

        // The gesture ended. Returns true when a value still has to go out, and fills it in.
        //
        // This is the rule that gets forgotten and it is the one that matters: without it a fader
        // settles a few units away from where the finger left it, every time, and the DAW and the
        // surface disagree for the rest of the session.
        bool Release(_Out_ double& value) noexcept;

        // Dropped without sending, since the last reset. A diagnostic, not a decision.
        uint32_t SuppressedCount() const noexcept { return m_suppressed; }

        void Reset() noexcept;

    private:
        uint32_t m_intervalMs{ 0 };
        uint64_t m_lastSentMs{ 0 };
        double m_pendingValue{ 0.0 };
        double m_lastSentValue{ 0.0 };
        bool m_hasSent{ false };
        bool m_hasPending{ false };
        uint32_t m_suppressed{ 0 };
    };
}
