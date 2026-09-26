// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, so the unit tests compile it unchanged.

#include "HexText.h"

namespace glass
{
    namespace
    {
        constexpr wchar_t Digits[] = L"0123456789ABCDEF";

        constexpr int32_t Nibble(_In_ wchar_t ch) noexcept
        {
            if (ch >= L'0' && ch <= L'9') { return ch - L'0'; }
            if (ch >= L'A' && ch <= L'F') { return ch - L'A' + 10; }
            if (ch >= L'a' && ch <= L'f') { return ch - L'a' + 10; }

            return -1;
        }

        constexpr bool IsSeparator(_In_ wchar_t ch) noexcept
        {
            return ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n' || ch == L',' || ch == L';';
        }

        // Everything that is not a hexadecimal digit, with 0x prefixes taken out. Returns false
        // when something that is not a separator and not a digit is in the way.
        bool StripToDigits(_In_ std::wstring_view text, _Out_ std::wstring& digits) noexcept
        {
            digits.clear();

            try
            {
                digits.reserve(text.size());

                for (size_t index = 0; index < text.size(); ++index)
                {
                    auto const ch = text[index];

                    if (IsSeparator(ch))
                    {
                        continue;
                    }

                    // A pasted list often reads 0x7E, 0x7F. The prefix is noise, not data.
                    if ((ch == L'0') && (index + 1 < text.size()) &&
                        (text[index + 1] == L'x' || text[index + 1] == L'X'))
                    {
                        ++index;
                        continue;
                    }

                    if (Nibble(ch) < 0)
                    {
                        digits.clear();
                        return false;
                    }

                    digits += ch;
                }

                return true;
            }
            catch (...)
            {
                digits.clear();
                return false;
            }
        }
    }

    _Use_decl_annotations_
    std::wstring ToHexBytes(std::vector<uint8_t> const& bytes) noexcept
    {
        try
        {
            std::wstring text{};
            text.reserve(bytes.size() * 2);

            for (auto const value : bytes)
            {
                text += Digits[(value >> 4) & 0x0F];
                text += Digits[value & 0x0F];
            }

            return text;
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::wstring FormatHexBytes(std::vector<uint8_t> const& bytes) noexcept
    {
        try
        {
            std::wstring text{};
            text.reserve(bytes.size() * 3);

            for (size_t index = 0; index < bytes.size(); ++index)
            {
                if (index != 0)
                {
                    text += (index % 16 == 0) ? L'\n' : L' ';
                }

                text += Digits[(bytes[index] >> 4) & 0x0F];
                text += Digits[bytes[index] & 0x0F];
            }

            return text;
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::vector<uint8_t> ParseHexBytes(std::wstring_view text, size_t maximumBytes) noexcept
    {
        std::wstring digits{};

        if (!StripToDigits(text, digits))
        {
            return {};
        }

        // A partly-read dump is worse than none. One bad character in a firmware image can leave
        // a synthesizer unusable, so a half byte at the end rejects the whole thing.
        if (digits.empty() || digits.size() % 2 != 0 || digits.size() / 2 > maximumBytes)
        {
            return {};
        }

        try
        {
            std::vector<uint8_t> bytes{};
            bytes.reserve(digits.size() / 2);

            for (size_t index = 0; index < digits.size(); index += 2)
            {
                bytes.push_back(static_cast<uint8_t>(
                    (Nibble(digits[index]) << 4) | Nibble(digits[index + 1])));
            }

            return bytes;
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::wstring FormatHexWords(std::vector<uint32_t> const& words) noexcept
    {
        try
        {
            std::wstring text{};

            for (auto const word : words)
            {
                if (!text.empty())
                {
                    text += L' ';
                }

                for (int32_t shift = 28; shift >= 0; shift -= 4)
                {
                    text += Digits[(word >> shift) & 0x0F];
                }
            }

            return text;
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::vector<uint32_t> ParseHexWords(std::wstring_view text, size_t maximumWords) noexcept
    {
        std::wstring digits{};

        if (!StripToDigits(text, digits))
        {
            return {};
        }

        if (digits.empty() || digits.size() % 8 != 0 || digits.size() / 8 > maximumWords)
        {
            return {};
        }

        try
        {
            std::vector<uint32_t> words{};
            words.reserve(digits.size() / 8);

            for (size_t index = 0; index < digits.size(); index += 8)
            {
                uint32_t word{ 0 };

                for (size_t digit = 0; digit < 8; ++digit)
                {
                    word = (word << 4) | static_cast<uint32_t>(Nibble(digits[index + digit]));
                }

                words.push_back(word);
            }

            return words;
        }
        catch (...)
        {
            return {};
        }
    }
}
