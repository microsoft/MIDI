// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "OscAddress.h"

#include <array>
#include <bitset>

namespace midipatchbay::osc
{
    namespace
    {
        // Positions in a part, plus the one past its end.
        using PartPositions = std::bitset<MaximumAddressLength + 1>;

        // Parts in an address, plus the one past its end.
        using AddressPositions = std::bitset<MaximumAddressParts + 1>;

        // [abc], [a-z] and [!a]. The text is what's between the brackets.
        bool InSet(_In_ std::string_view set, _In_ char c) noexcept
        {
            auto negated = false;

            if (!set.empty() && set.front() == '!')
            {
                negated = true;
                set.remove_prefix(1);
            }

            auto found = false;

            for (size_t i = 0; i < set.size() && !found; i++)
            {
                // A dash between two characters is a range. At either end it is itself.
                if (i + 2 < set.size() && set[i + 1] == '-')
                {
                    found = c >= set[i] && c <= set[i + 2];
                    i += 2;
                }
                else
                {
                    found = set[i] == c;
                }
            }

            return found != negated;
        }

        // Splits on slashes after the leading one. An empty part stands for //. False when there
        // are too many parts.
        template <typename TParts>
        bool SplitParts(_In_ std::string_view text, _Out_ TParts& parts, _Out_ size_t& count) noexcept
        {
            count = 0;

            if (text.empty() || text.front() != '/')
            {
                return false;
            }

            text.remove_prefix(1);

            for (;;)
            {
                if (count >= parts.size())
                {
                    return false;
                }

                auto const slash = text.find('/');
                parts[count++] = text.substr(0, slash);

                if (slash == std::string_view::npos)
                {
                    return true;
                }

                text.remove_prefix(slash + 1);
            }
        }
    }

    _Use_decl_annotations_
    bool HasPatternCharacters(std::string_view address) noexcept
    {
        return address.find_first_of("?*[]{}") != std::string_view::npos ||
            address.find("//") != std::string_view::npos;
    }

    _Use_decl_annotations_
    bool PartMatches(std::string_view pattern, std::string_view part) noexcept
    {
        if (pattern.size() > MaximumAddressLength || part.size() > MaximumAddressLength)
        {
            return false;
        }

        // Where the pattern so far can have reached in the part. Every token moves these on, so
        // the work is the pattern's length times the part's, however the wildcards combine.
        PartPositions current{};
        current.set(0);

        size_t i{ 0 };

        while (i < pattern.size())
        {
            PartPositions next{};
            auto const c = pattern[i];

            if (c == '*')
            {
                auto any = false;

                for (size_t j = 0; j <= part.size(); j++)
                {
                    any = any || current.test(j);

                    if (any)
                    {
                        next.set(j);
                    }
                }

                i++;
            }
            else if (c == '[')
            {
                auto const close = pattern.find(']', i + 1);

                if (close == std::string_view::npos)
                {
                    return false;
                }

                auto const set = pattern.substr(i + 1, close - i - 1);

                for (size_t j = 0; j < part.size(); j++)
                {
                    if (current.test(j) && InSet(set, part[j]))
                    {
                        next.set(j + 1);
                    }
                }

                i = close + 1;
            }
            else if (c == '{')
            {
                auto const close = pattern.find('}', i + 1);

                if (close == std::string_view::npos)
                {
                    return false;
                }

                auto choices = pattern.substr(i + 1, close - i - 1);

                for (;;)
                {
                    auto const comma = choices.find(',');
                    auto const choice = choices.substr(0, comma);

                    for (size_t j = 0; j + choice.size() <= part.size(); j++)
                    {
                        if (current.test(j) && part.substr(j, choice.size()) == choice)
                        {
                            next.set(j + choice.size());
                        }
                    }

                    if (comma == std::string_view::npos)
                    {
                        break;
                    }

                    choices.remove_prefix(comma + 1);
                }

                i = close + 1;
            }
            else
            {
                for (size_t j = 0; j < part.size(); j++)
                {
                    if (current.test(j) && (c == '?' || part[j] == c))
                    {
                        next.set(j + 1);
                    }
                }

                i++;
            }

            if (next.none())
            {
                return false;
            }

            current = next;
        }

        return current.test(part.size());
    }

    _Use_decl_annotations_
    bool PatternMatches(std::string_view pattern, std::string_view address) noexcept
    {
        if (pattern.size() > MaximumAddressLength || address.size() > MaximumAddressLength)
        {
            return false;
        }

        std::array<std::string_view, MaximumAddressParts> patternParts{};
        std::array<std::string_view, MaximumAddressParts> addressParts{};
        size_t patternCount{ 0 };
        size_t addressCount{ 0 };

        if (!SplitParts(pattern, patternParts, patternCount) || !SplitParts(address, addressParts, addressCount))
        {
            return false;
        }

        for (size_t j = 0; j < addressCount; j++)
        {
            if (addressParts[j].empty())
            {
                return false;
            }
        }

        // A pattern ending in a slash has an empty last part with nothing after it to find.
        if (patternParts[patternCount - 1].empty())
        {
            return false;
        }

        AddressPositions current{};
        current.set(0);

        for (size_t i = 0; i < patternCount; i++)
        {
            AddressPositions next{};

            if (patternParts[i].empty())
            {
                // //: any number of parts, none included.
                auto any = false;

                for (size_t j = 0; j <= addressCount; j++)
                {
                    any = any || current.test(j);

                    if (any)
                    {
                        next.set(j);
                    }
                }
            }
            else
            {
                for (size_t j = 0; j < addressCount; j++)
                {
                    if (current.test(j) && PartMatches(patternParts[i], addressParts[j]))
                    {
                        next.set(j + 1);
                    }
                }
            }

            if (next.none())
            {
                return false;
            }

            current = next;
        }

        return current.test(addressCount);
    }

    _Use_decl_annotations_
    bool IsValidRoot(std::string_view root) noexcept
    {
        if (root.empty())
        {
            return true;
        }

        if (root.size() > MaximumAddressLength / 2 || root.front() != '/' || root.back() == '/' ||
            root.find("//") != std::string_view::npos)
        {
            return false;
        }

        size_t parts{ 0 };

        for (auto const c : root)
        {
            if (c == '/')
            {
                parts++;
                continue;
            }

            if (c <= 0x20 || c >= 0x7F || std::string_view{ "#*,?[]{}" }.find(c) != std::string_view::npos)
            {
                return false;
            }
        }

        // Leaves room under the root for the parts a format adds.
        return parts <= MaximumAddressParts - 4;
    }
}
