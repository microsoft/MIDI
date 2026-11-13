// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "CalcText.h"

namespace midicalculator::text
{
    namespace
    {
        std::wstring GroupSeparator()
        {
            wchar_t buffer[8]{};

            if (::GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_STHOUSAND, buffer, ARRAYSIZE(buffer)) > 0)
            {
                return buffer;
            }

            return L",";
        }

        std::wstring DecimalSeparator()
        {
            wchar_t buffer[8]{};

            if (::GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SDECIMAL, buffer, ARRAYSIZE(buffer)) > 0)
            {
                return buffer;
            }

            return L".";
        }

        // Spaces and group separators out, so "2,892,714" and "2 892 714" read the same.
        std::wstring Compact(_In_ std::wstring_view text)
        {
            auto const group = GroupSeparator();
            std::wstring result{};

            for (auto const c : text)
            {
                if (c == L' ' || c == L'\t' || c == L'\u00A0' || c == L'\u202F' || c == L'_' ||
                    (group.size() == 1 && c == group[0] && c != L'.'))
                {
                    continue;
                }

                result.push_back(c);
            }

            return result;
        }

        std::optional<uint64_t> ReadDigits(_In_ std::wstring_view digits, _In_ uint32_t base) noexcept
        {
            if (digits.empty())
            {
                return std::nullopt;
            }

            uint64_t value{ 0 };

            for (auto const c : digits)
            {
                uint32_t digit{};

                if (c >= L'0' && c <= L'9') digit = c - L'0';
                else if (c >= L'a' && c <= L'f') digit = 10 + (c - L'a');
                else if (c >= L'A' && c <= L'F') digit = 10 + (c - L'A');
                else return std::nullopt;

                if (digit >= base)
                {
                    return std::nullopt;
                }

                if (value > (UINT64_MAX - digit) / base)
                {
                    return std::nullopt;
                }

                value = value * base + digit;
            }

            return value;
        }

        std::optional<uint64_t> ParseWithDefaultBase(_In_ std::wstring_view text, _In_ uint32_t defaultBase) noexcept
        {
            try
            {
                auto compact = Compact(text);
                std::wstring_view view{ compact };

                if (view.size() > 2 && view[0] == L'0' && (view[1] == L'x' || view[1] == L'X'))
                {
                    return ReadDigits(view.substr(2), 16);
                }

                if (view.size() > 1 && (view[0] == L'$' || view[0] == L'#'))
                {
                    return ReadDigits(view.substr(1), 16);
                }

                if (view.size() > 1 && (view.back() == L'h' || view.back() == L'H'))
                {
                    return ReadDigits(view.substr(0, view.size() - 1), 16);
                }

                return ReadDigits(view, defaultBase);
            }
            catch (...)
            {
                return std::nullopt;
            }
        }

        std::optional<double> ReadDouble(_In_ std::wstring_view text) noexcept
        {
            try
            {
                auto compact = Compact(text);
                auto const decimal = DecimalSeparator();

                // the locale's decimal separator, or a period
                if (decimal.size() == 1 && decimal[0] != L'.')
                {
                    std::replace(compact.begin(), compact.end(), decimal[0], L'.');
                }

                if (compact.empty())
                {
                    return std::nullopt;
                }

                wchar_t* end{ nullptr };
                auto const value = std::wcstod(compact.c_str(), &end);

                if (end != compact.c_str() + compact.size() || !std::isfinite(value))
                {
                    return std::nullopt;
                }

                return value;
            }
            catch (...)
            {
                return std::nullopt;
            }
        }

        std::wstring FormatWithLocale(_In_ std::wstring const& plain, _In_ int32_t decimals)
        {
            NUMBERFMTW format{};
            wchar_t group[8]{};
            wchar_t decimal[8]{};
            wchar_t grouping[16]{};

            ::GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_STHOUSAND, group, ARRAYSIZE(group));
            ::GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SDECIMAL, decimal, ARRAYSIZE(decimal));
            ::GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SGROUPING, grouping, ARRAYSIZE(grouping));

            format.NumDigits = static_cast<UINT>(decimals);
            format.LeadingZero = 1;
            format.Grouping = (grouping[0] >= L'0' && grouping[0] <= L'9') ? static_cast<UINT>(grouping[0] - L'0') : 3;
            if (grouping[0] == L'3' && grouping[2] == L'2') format.Grouping = 32;
            format.lpDecimalSep = decimal;
            format.lpThousandSep = group;
            format.NegativeOrder = 1;

            wchar_t buffer[96]{};

            if (::GetNumberFormatEx(LOCALE_NAME_USER_DEFAULT, 0, plain.c_str(), &format, buffer, ARRAYSIZE(buffer)) > 0)
            {
                return buffer;
            }

            return plain;
        }
    }

    _Use_decl_annotations_
    std::optional<uint64_t> ParseUnsigned(std::wstring_view text) noexcept
    {
        return ParseWithDefaultBase(text, 10);
    }

    _Use_decl_annotations_
    std::optional<uint64_t> ParseHexFirst(std::wstring_view text) noexcept
    {
        return ParseWithDefaultBase(text, 16);
    }

    _Use_decl_annotations_
    std::vector<uint32_t> ParseWords(std::wstring_view text) noexcept
    {
        std::vector<uint32_t> words{};

        try
        {
            std::wstring current{};

            auto flush = [&]()
            {
                if (current.empty())
                {
                    return true;
                }

                auto const value = ParseWithDefaultBase(current, 16);
                current.clear();

                if (!value.has_value() || *value > 0xFFFFFFFFull)
                {
                    return false;
                }

                words.push_back(static_cast<uint32_t>(*value));
                return true;
            };

            for (auto const c : text)
            {
                if (c == L' ' || c == L',' || c == L'\t' || c == L'\r' || c == L'\n' || c == L';')
                {
                    if (!flush())
                    {
                        return {};
                    }
                }
                else
                {
                    current.push_back(c);
                }
            }

            if (!flush())
            {
                return {};
            }
        }
        catch (...)
        {
            words.clear();
        }

        return words;
    }

    _Use_decl_annotations_
    std::optional<double> ParseDuration(std::wstring_view text) noexcept
    {
        try
        {
            std::vector<std::wstring_view> parts{};
            size_t start{ 0 };

            for (size_t i = 0; i <= text.size(); ++i)
            {
                if (i == text.size() || text[i] == L':')
                {
                    parts.push_back(text.substr(start, i - start));
                    start = i + 1;
                }
            }

            if (parts.empty() || parts.size() > 3)
            {
                return std::nullopt;
            }

            double total{ 0.0 };

            for (size_t i = 0; i < parts.size(); ++i)
            {
                auto const value = ReadDouble(parts[i]);

                if (!value.has_value() || *value < 0.0)
                {
                    return std::nullopt;
                }

                // only the seconds may have a fraction or go past 59 when it's the only field
                if (i + 1 < parts.size() && *value != std::floor(*value))
                {
                    return std::nullopt;
                }

                total = total * 60.0 + *value;
            }

            return total;
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    _Use_decl_annotations_
    std::optional<std::pair<int32_t, int32_t>> ParseMeter(std::wstring_view text) noexcept
    {
        auto const slash = text.find(L'/');

        if (slash == std::wstring_view::npos)
        {
            return std::nullopt;
        }

        auto const top = ParseUnsigned(text.substr(0, slash));
        auto const bottom = ParseUnsigned(text.substr(slash + 1));

        if (!top.has_value() || !bottom.has_value() || *top > 64 || *bottom > 64)
        {
            return std::nullopt;
        }

        return std::pair<int32_t, int32_t>{ static_cast<int32_t>(*top), static_cast<int32_t>(*bottom) };
    }

    _Use_decl_annotations_
    std::wstring Number(double value, int32_t decimals, bool keepZeros)
    {
        if (!std::isfinite(value))
        {
            return L"\u2014";
        }

        decimals = std::clamp(decimals, 0, 12);
        auto plain = std::format(L"{:.{}f}", value, decimals);

        if (!keepZeros && decimals > 0)
        {
            while (plain.back() == L'0')
            {
                plain.pop_back();
            }

            if (plain.back() == L'.')
            {
                plain.pop_back();
            }

            auto const dot = plain.find(L'.');
            decimals = dot == std::wstring::npos ? 0 : static_cast<int32_t>(plain.size() - dot - 1);
        }

        if (plain == L"-0")
        {
            plain = L"0";
        }

        return FormatWithLocale(plain, decimals);
    }

    _Use_decl_annotations_
    std::wstring Whole(uint64_t value)
    {
        return FormatWithLocale(std::to_wstring(value), 0);
    }

    _Use_decl_annotations_
    std::wstring SignedWhole(int64_t value)
    {
        return FormatWithLocale(std::to_wstring(value), 0);
    }

    _Use_decl_annotations_
    std::wstring Signed(double value, int32_t decimals)
    {
        auto const body = Number(value, decimals);

        if (value > 0.0 && body != L"0")
        {
            return L"+" + body;
        }

        return body;
    }

    _Use_decl_annotations_
    std::wstring Hex(uint64_t value, int32_t digits)
    {
        return std::format(L"0x{:0{}X}", value, std::clamp(digits, 1, 16));
    }

    _Use_decl_annotations_
    std::wstring Duration(double seconds)
    {
        if (!std::isfinite(seconds) || seconds < 0.0)
        {
            return L"\u2014";
        }

        auto const totalMilliseconds = static_cast<uint64_t>(std::llround(seconds * 1000.0));
        auto const hours = totalMilliseconds / 3'600'000;
        auto const minutes = (totalMilliseconds / 60'000) % 60;
        auto const wholeSeconds = (totalMilliseconds / 1000) % 60;
        auto const milliseconds = totalMilliseconds % 1000;
        auto const decimal = DecimalSeparator();

        if (hours > 0)
        {
            return std::format(L"{}:{:02}:{:02}{}{:03}", hours, minutes, wholeSeconds, decimal, milliseconds);
        }

        return std::format(L"{}:{:02}{}{:03}", minutes, wholeSeconds, decimal, milliseconds);
    }
}
