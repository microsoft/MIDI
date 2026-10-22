// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The Custom format's rules: addresses with parts in braces, like /1/fader{n}, and the numbers
// worked out from those parts, like {n}+70. Pure, for the unit tests.

#include "OscConnection.h"

#include <array>

namespace midipatchbay::osc
{
    // What the parts of an address stood for in one received address: {n} was 3.
    struct RuleParts
    {
        std::array<std::string_view, MaximumOscRuleParts> Names{};
        std::array<int32_t, MaximumOscRuleParts> Values{};
        size_t Count{ 0 };

        std::optional<int32_t> Find(_In_ std::string_view name) const noexcept;

        // False when the part has another value already, or there's no room.
        bool Set(_In_ std::string_view name, _In_ int32_t value) noexcept;
    };

    // Largest number one part stands for, so the arithmetic never overflows.
    constexpr int32_t MaximumRulePartValue = 999999;

    // Whether this is a rule address: starts with /, is printable ASCII with no spaces, has none
    // of OSC's pattern characters, and has at most MaximumOscRuleParts parts, each lowercase
    // letters in braces, never two in a row and never right before a digit, so each one's digits
    // are plain to see.
    bool IsValidRuleAddress(_In_ std::string_view address) noexcept;

    // The names of the parts, in order: "/{p}/fader{n}" has p and n.
    std::vector<std::string> RuleAddressParts(_In_ std::string_view address);

    // Whether an address is the rule's, and if so what each part stood for. A part stands for
    // one or more digits.
    bool MatchRuleAddress(_In_ std::string_view rule, _In_ std::string_view address, _Out_ RuleParts& parts) noexcept;

    // The address for these parts, or nothing when one isn't known or is negative.
    std::optional<std::string> BuildRuleAddress(_In_ std::string_view rule, _In_ RuleParts const& parts);

    // "74", "{n}", "{n}+70" or "{n} - 1". Nothing when it isn't one of those.
    std::optional<OscRuleNumber> ParseRuleNumber(_In_ std::wstring_view text) noexcept;

    // The way ParseRuleNumber reads it, with people's channels counted from 1 when asked.
    std::wstring RuleNumberText(_In_ OscRuleNumber const& number, _In_ int32_t shown = 0);

    // The number for these parts, or nothing when its part isn't known.
    std::optional<int32_t> EvaluateRuleNumber(_In_ OscRuleNumber const& number, _In_ RuleParts const& parts) noexcept;

    // The other way: given what the number came to, what its part stood for. False when it's a
    // fixed number that isn't this one, or when its part already stands for something else.
    bool SolveRuleNumber(_In_ OscRuleNumber const& number, _In_ int32_t value, _Inout_ RuleParts& parts) noexcept;

    // Whether a rule can work this way: in needs a valid address; out also needs every part of
    // the address to come from the channel or the number.
    bool RuleReceives(_In_ OscRule const& rule);
    bool RuleSends(_In_ OscRule const& rule);

    // The highest MIDI value a rule's message has: 127, or 16383 for pitch bend.
    uint32_t RuleHighest(_In_ OscRuleMessage message) noexcept;

    // The TouchOSC preset's rules.
    std::vector<OscRule> TouchOscRules();
}
