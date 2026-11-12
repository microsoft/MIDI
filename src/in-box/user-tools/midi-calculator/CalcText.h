// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Reading what a customer types and writing numbers the way they read them.

namespace midicalculator::text
{
    // A whole number typed as decimal ("2892714813833", "2,892,714,813,833") or hex ("0x8AC7",
    // "8AC7h", "$8AC7"). Spaces and the locale's group separator are ignored. Empty when it isn't
    // a number or doesn't fit in 64 bits.
    std::optional<uint64_t> ParseUnsigned(_In_ std::wstring_view text) noexcept;

    // The same, but bare hex digits count as hex: what someone pastes from a monitor ("8AC7",
    // "40904503"). Decimal needs no prefix in ParseUnsigned, so this is for fields that are
    // always shown in hex.
    std::optional<uint64_t> ParseHexFirst(_In_ std::wstring_view text) noexcept;

    // Words of UMP as hex, separated by spaces, commas or new lines: "40904503 C9248AC7".
    std::vector<uint32_t> ParseWords(_In_ std::wstring_view text) noexcept;

    // A length typed as seconds ("15.6") or minutes and seconds ("0:15.600", "1:02:03.5").
    std::optional<double> ParseDuration(_In_ std::wstring_view text) noexcept;

    // A meter: "4/4", "6/8", "7 / 8".
    std::optional<std::pair<int32_t, int32_t>> ParseMeter(_In_ std::wstring_view text) noexcept;

    // A number in the customer's locale, with grouping and up to decimals places, trailing zeros
    // dropped unless keepZeros.
    std::wstring Number(_In_ double value, _In_ int32_t decimals, _In_ bool keepZeros = false);
    std::wstring Whole(_In_ uint64_t value);
    std::wstring SignedWhole(_In_ int64_t value);

    // "+38.906" or "-12.5": a sign either way, for cents and percentages.
    std::wstring Signed(_In_ double value, _In_ int32_t decimals);

    // "0x8AC7", digits wide, upper case.
    std::wstring Hex(_In_ uint64_t value, _In_ int32_t digits);

    // "0:32.000" or "1:00:30.026".
    std::wstring Duration(_In_ double seconds);
}
