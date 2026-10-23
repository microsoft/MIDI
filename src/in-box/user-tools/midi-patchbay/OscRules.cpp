// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "OscRules.h"
#include "OscAddress.h"

#include <algorithm>
#include <charconv>
#include <format>

namespace midipatchbay::osc
{
    namespace
    {
        constexpr size_t MaximumPartNameLength = 16;
        constexpr int32_t MaximumRuleOffset = 1'000'000;

        // The characters OSC uses for patterns. A rule's address is never a pattern.
        bool IsPatternCharacter(_In_ char c) noexcept
        {
            return c == '*' || c == '?' || c == '[' || c == ']' || c == ',' || c == '#' || c == '{' || c == '}';
        }

        bool IsDigit(_In_ char c) noexcept
        {
            return c >= '0' && c <= '9';
        }

        bool IsPartLetter(_In_ char c) noexcept
        {
            return c >= 'a' && c <= 'z';
        }

        // The part name starting just after a {, and where its } is. Nothing when it isn't one.
        std::optional<std::string_view> PartAt(_In_ std::string_view text, _In_ size_t open, _Out_ size_t& close) noexcept
        {
            close = open;
            auto i = open + 1;

            while (i < text.size() && IsPartLetter(text[i]))
            {
                i++;
            }

            auto const length = i - open - 1;

            if (length == 0 || length > MaximumPartNameLength || i >= text.size() || text[i] != '}')
            {
                return std::nullopt;
            }

            close = i;
            return text.substr(open + 1, length);
        }

        bool UsesNumber(_In_ OscRuleMessage message) noexcept
        {
            return message != OscRuleMessage::PitchBend && message != OscRuleMessage::ChannelPressure;
        }

        OscRule MakeRule(_In_ std::string_view address, _In_ OscRuleMessage message, _In_ int32_t offset)
        {
            OscRule rule{};
            rule.Address = address;
            rule.Message = message;
            rule.Number = OscRuleNumber{ "n", offset };
            return rule;
        }
    }

    _Use_decl_annotations_
    std::optional<int32_t> RuleParts::Find(std::string_view name) const noexcept
    {
        for (size_t i = 0; i < Count; i++)
        {
            if (Names[i] == name)
            {
                return Values[i];
            }
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    bool RuleParts::Set(std::string_view name, int32_t value) noexcept
    {
        if (auto const existing = Find(name); existing.has_value())
        {
            return *existing == value;
        }

        if (Count >= Names.size())
        {
            return false;
        }

        Names[Count] = name;
        Values[Count] = value;
        Count++;

        return true;
    }

    _Use_decl_annotations_
    bool IsValidRuleAddress(std::string_view address) noexcept
    {
        if (address.empty() || address.size() > MaximumAddressLength || address[0] != '/')
        {
            return false;
        }

        size_t parts{ 0 };
        auto afterPart = false;

        for (size_t i = 0; i < address.size(); i++)
        {
            auto const c = address[i];

            if (c <= ' ' || c > '~')
            {
                return false;
            }

            if (c == '{')
            {
                size_t close{ 0 };

                // Two parts in a row would leave no way to tell whose digits are whose.
                if (afterPart || !PartAt(address, i, close).has_value() || ++parts > MaximumOscRuleParts)
                {
                    return false;
                }

                i = close;
                afterPart = true;
                continue;
            }

            if (IsPatternCharacter(c) || (afterPart && IsDigit(c)))
            {
                return false;
            }

            afterPart = false;
        }

        return true;
    }

    _Use_decl_annotations_
    std::vector<std::string> RuleAddressParts(std::string_view address)
    {
        std::vector<std::string> names{};

        for (size_t i = 0; i < address.size(); i++)
        {
            if (address[i] != '{')
            {
                continue;
            }

            size_t close{ 0 };

            if (auto const name = PartAt(address, i, close); name.has_value())
            {
                names.emplace_back(*name);
                i = close;
            }
        }

        return names;
    }

    _Use_decl_annotations_
    bool MatchRuleAddress(std::string_view rule, std::string_view address, RuleParts& parts) noexcept
    {
        parts = RuleParts{};

        size_t at{ 0 };

        for (size_t i = 0; i < rule.size(); i++)
        {
            if (rule[i] == '{')
            {
                size_t close{ 0 };
                auto const name = PartAt(rule, i, close);

                if (!name.has_value())
                {
                    return false;
                }

                auto const start = at;

                while (at < address.size() && IsDigit(address[at]))
                {
                    at++;
                }

                // One to six digits: a part never stands for more than MaximumRulePartValue.
                if (at == start || at - start > 6)
                {
                    return false;
                }

                int32_t value{ 0 };
                std::from_chars(address.data() + start, address.data() + at, value);

                if (!parts.Set(*name, value))
                {
                    return false;
                }

                i = close;
                continue;
            }

            if (at >= address.size() || address[at] != rule[i])
            {
                return false;
            }

            at++;
        }

        return at == address.size();
    }

    _Use_decl_annotations_
    std::optional<std::string> BuildRuleAddress(std::string_view rule, RuleParts const& parts)
    {
        std::string address{};

        for (size_t i = 0; i < rule.size(); i++)
        {
            if (rule[i] == '{')
            {
                size_t close{ 0 };
                auto const name = PartAt(rule, i, close);

                if (!name.has_value())
                {
                    return std::nullopt;
                }

                auto const value = parts.Find(*name);

                if (!value.has_value() || *value < 0 || *value > MaximumRulePartValue)
                {
                    return std::nullopt;
                }

                address += std::to_string(*value);
                i = close;
                continue;
            }

            address += rule[i];
        }

        return address;
    }

    _Use_decl_annotations_
    std::optional<OscRuleNumber> ParseRuleNumber(std::wstring_view text) noexcept
    {
        try
        {
            std::string narrow{};

            for (auto const c : text)
            {
                if (c == L' ' || c == L'\t')
                {
                    continue;
                }

                if (c >= 0x80)
                {
                    return std::nullopt;
                }

                narrow += static_cast<char>(c);
            }

            if (narrow.empty())
            {
                return std::nullopt;
            }

            OscRuleNumber number{};
            std::string_view rest{ narrow };

            if (rest.front() == '{')
            {
                size_t close{ 0 };
                auto const name = PartAt(rest, 0, close);

                if (!name.has_value())
                {
                    return std::nullopt;
                }

                number.Part = *name;
                rest.remove_prefix(close + 1);

                if (rest.empty())
                {
                    return number;
                }

                if (rest.front() != '+' && rest.front() != '-')
                {
                    return std::nullopt;
                }
            }

            auto const negative = !rest.empty() && rest.front() == '-';

            if (!rest.empty() && (rest.front() == '+' || rest.front() == '-'))
            {
                rest.remove_prefix(1);
            }

            if (rest.empty() || rest.size() > 7 || !std::all_of(rest.begin(), rest.end(), IsDigit))
            {
                return std::nullopt;
            }

            int32_t value{ 0 };
            std::from_chars(rest.data(), rest.data() + rest.size(), value);

            if (value > MaximumRuleOffset)
            {
                return std::nullopt;
            }

            number.Offset = negative ? -value : value;
            return number;
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    _Use_decl_annotations_
    std::wstring RuleNumberText(OscRuleNumber const& number, int32_t shown)
    {
        auto const offset = number.Offset + shown;

        if (number.Part.empty())
        {
            return std::to_wstring(offset);
        }

        std::wstring text = L"{" + std::wstring{ number.Part.begin(), number.Part.end() } + L"}";

        if (offset > 0)
        {
            text += std::format(L"+{}", offset);
        }
        else if (offset < 0)
        {
            text += std::format(L"-{}", -static_cast<int64_t>(offset));
        }

        return text;
    }

    _Use_decl_annotations_
    std::optional<int32_t> EvaluateRuleNumber(OscRuleNumber const& number, RuleParts const& parts) noexcept
    {
        if (number.Part.empty())
        {
            return number.Offset;
        }

        auto const value = parts.Find(number.Part);

        if (!value.has_value())
        {
            return std::nullopt;
        }

        return *value + number.Offset;
    }

    _Use_decl_annotations_
    bool SolveRuleNumber(OscRuleNumber const& number, int32_t value, RuleParts& parts) noexcept
    {
        if (number.Part.empty())
        {
            return value == number.Offset;
        }

        auto const part = static_cast<int64_t>(value) - number.Offset;

        if (part < 0 || part > MaximumRulePartValue)
        {
            return false;
        }

        return parts.Set(number.Part, static_cast<int32_t>(part));
    }

    _Use_decl_annotations_
    bool RuleReceives(OscRule const& rule)
    {
        if (rule.Direction == OscRuleDirection::Out || !IsValidRuleAddress(rule.Address))
        {
            return false;
        }

        auto const parts = RuleAddressParts(rule.Address);
        auto const known = [&parts](OscRuleNumber const& number)
            {
                return number.Part.empty() || std::find(parts.begin(), parts.end(), number.Part) != parts.end();
            };

        return known(rule.Channel) && (!UsesNumber(rule.Message) || known(rule.Number));
    }

    _Use_decl_annotations_
    bool RuleSends(OscRule const& rule)
    {
        if (rule.Direction == OscRuleDirection::In || !IsValidRuleAddress(rule.Address))
        {
            return false;
        }

        for (auto const& part : RuleAddressParts(rule.Address))
        {
            if (part != rule.Channel.Part && (!UsesNumber(rule.Message) || part != rule.Number.Part))
            {
                return false;
            }
        }

        return true;
    }

    _Use_decl_annotations_
    uint32_t RuleHighest(OscRuleMessage message) noexcept
    {
        return message == OscRuleMessage::PitchBend ? 16383u : 127u;
    }

    std::vector<OscRule> TouchOscRules()
    {
        // Faders 1 to 5 are controllers 71 to 75, toggles 1 to 4 the general purpose on/off
        // controllers 80 to 83, and push buttons play from middle C up.
        return {
            MakeRule("/1/fader{n}", OscRuleMessage::ControlChange, 70),
            MakeRule("/1/toggle{n}", OscRuleMessage::ControlChange, 79),
            MakeRule("/1/push{n}", OscRuleMessage::Note, 59),
        };
    }
}
