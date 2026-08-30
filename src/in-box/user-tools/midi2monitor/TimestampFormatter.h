// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "AppSettings.h"

namespace midi2monitor
{
    // Value and unit are kept apart so the UI can color the unit differently, and so the
    // export can join them without re-parsing.
    struct FormattedTime
    {
        winrt::hstring Value{};
        winrt::hstring Unit{};      // empty when the value is raw clock ticks

        winrt::hstring ToDisplayString() const
        {
            if (Unit.empty())
            {
                return Value;
            }

            return Value + L" " + Unit;
        }
    };

    class TimestampFormatter
    {
    public:
        // absolute message timestamp, in the unit the customer selected
        static FormattedTime FormatTimestamp(uint64_t timestampTicks, TimestampDisplayFormat format) noexcept;

        // time since the previous message, scaled to whichever unit reads most naturally
        static FormattedTime FormatDelta(uint64_t deltaTicks) noexcept;

        static double TimestampFrequency() noexcept;
    };
}
