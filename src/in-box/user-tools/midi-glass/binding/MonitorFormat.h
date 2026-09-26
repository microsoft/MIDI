// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h, XAML and the MIDI SDK, so the unit tests compile it unchanged.

#include <sal.h>
#include <cstdint>
#include <string>

namespace glass
{
    // Turns the words the app just sent into the two strings the monitor rail shows: the bytes
    // as they went out, and what they mean.
    //
    // This reads the message back rather than reporting what was asked for. Printing the request
    // would agree with the arithmetic that built it even when the arithmetic is wrong, which is
    // exactly the question somebody opens the monitor to answer.
    //
    // It only knows the message types this app can produce. Anything else comes back as its own
    // hex, which is honest and still useful.
    struct FormattedMessage
    {
        // "40 B0 5B 00  5C 7A 00 00"
        std::wstring Words{};

        // "CC 91 = 0.36"
        std::wstring Meaning{};

        // 1 to 16 as a customer counts them, or 0 where the message has no channel.
        int32_t Channel{ 0 };

        // 1 to 16, or 0 where the message has no group.
        int32_t Group{ 0 };
    };

    // Named Describe rather than Format because FormatMessage is a windows.h macro that expands
    // to FormatMessageW, and a function called that links against the wrong thing.
    FormattedMessage DescribeMessage(
        _In_reads_(wordCount) uint32_t const* words,
        _In_ uint32_t wordCount) noexcept;

    // The elapsed column: "+0.000", counted from the first row in the list.
    std::wstring FormatElapsed(_In_ uint64_t milliseconds) noexcept;
}
