// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// OSC address patterns. A sender may put wildcards in the address it sends, and the receiver
// delivers the message to every address it has that matches: ?, *, [abc], [a-z], [!a] and
// {foo,bar} within one part of the address, and from OSC 1.1, // across any number of parts.
// Pure, for the unit tests.

#include <sal.h>

#include <cstdint>
#include <string_view>

namespace midipatchbay::osc
{
    // Longest address or pattern matched. A longer one never matches.
    constexpr size_t MaximumAddressLength = 255;

    // Most parts an address or pattern has. A deeper one never matches.
    constexpr size_t MaximumAddressParts = 32;

    // Whether the address holds anything a pattern treats as a wildcard, including //.
    bool HasPatternCharacters(_In_ std::string_view address) noexcept;

    // Whether one part of a pattern, with no slashes in it, matches one part of an address.
    bool PartMatches(_In_ std::string_view pattern, _In_ std::string_view part) noexcept;

    // Whether a whole pattern matches a whole address. Both start with a slash.
    bool PatternMatches(_In_ std::string_view pattern, _In_ std::string_view address) noexcept;

    // An address a row can use as its root: empty (the top level), or slash-separated parts made
    // of printable ASCII other than space, #, *, ',', ?, [, ], { and }, with no empty part and no
    // slash at the end.
    bool IsValidRoot(_In_ std::string_view root) noexcept;
}
