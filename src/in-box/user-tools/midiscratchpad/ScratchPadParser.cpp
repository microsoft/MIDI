// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ScratchPadParser.h"

namespace midiscratchpad
{
    namespace
    {
        struct Token
        {
            std::wstring Text{};
            uint32_t Line{ 1 };
        };

        bool IsHexDigit(wchar_t ch) noexcept
        {
            return (ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'f') || (ch >= L'A' && ch <= L'F');
        }

        uint32_t HexValue(wchar_t ch) noexcept
        {
            if (ch >= L'0' && ch <= L'9') return static_cast<uint32_t>(ch - L'0');
            if (ch >= L'a' && ch <= L'f') return static_cast<uint32_t>(ch - L'a') + 10;
            return static_cast<uint32_t>(ch - L'A') + 10;
        }

        // Splits on whitespace, dropping '#' and '//' comments. Line numbers are tracked so a
        // problem can be pointed at the line it is on.
        std::vector<Token> Tokenize(std::wstring_view text) noexcept
        {
            std::vector<Token> tokens{};

            uint32_t line{ 1 };
            size_t i{ 0 };

            while (i < text.size())
            {
                auto const ch = text[i];

                // a TextBox stores line breaks as CR alone, a file may use CRLF or LF
                if (ch == L'\r' || ch == L'\n')
                {
                    if (ch == L'\r' && i + 1 < text.size() && text[i + 1] == L'\n')
                    {
                        i++;
                    }

                    line++;
                    i++;
                    continue;
                }

                if (ch == L' ' || ch == L'\t' || ch == L',')
                {
                    i++;
                    continue;
                }

                if (ch == L'#' || (ch == L'/' && i + 1 < text.size() && text[i + 1] == L'/'))
                {
                    while (i < text.size() && text[i] != L'\n' && text[i] != L'\r')
                    {
                        i++;
                    }

                    continue;
                }

                auto const start = i;

                while (i < text.size() &&
                    text[i] != L'\n' && text[i] != L'\r' && text[i] != L' ' && text[i] != L'\t' && text[i] != L',')
                {
                    i++;
                }

                tokens.push_back(Token{ std::wstring{ text.substr(start, i - start) }, line });
            }

            return tokens;
        }

        std::wstring_view StripHexPrefix(std::wstring const& token) noexcept
        {
            std::wstring_view view{ token };

            if (view.size() > 2 && view[0] == L'0' && (view[1] == L'x' || view[1] == L'X'))
            {
                view.remove_prefix(2);
            }

            return view;
        }

        ParsedInput MakeError(uint32_t line, std::wstring_view key, std::wstring_view token) noexcept
        {
            ParsedInput result{};
            result.HasError = true;
            result.ErrorLine = line;
            result.ErrorResourceKey = key;
            result.ErrorToken = token;
            return result;
        }
    }

    ParsedInput ParseMidi1Bytes(std::wstring_view text) noexcept
    {
        ParsedInput result{};

        try
        {
            for (auto const& token : Tokenize(text))
            {
                auto const digits = StripHexPrefix(token.Text);

                if (digits.empty())
                {
                    return MakeError(token.Line, L"ParseErrorNotHex", token.Text);
                }

                for (auto const ch : digits)
                {
                    if (!IsHexDigit(ch))
                    {
                        return MakeError(token.Line, L"ParseErrorNotHex", token.Text);
                    }
                }

                // a byte is two hex digits, so a run has to be an even number of them
                if ((digits.size() % 2) != 0)
                {
                    return MakeError(token.Line, L"ParseErrorOddDigits", token.Text);
                }

                for (size_t i = 0; i < digits.size(); i += 2)
                {
                    result.Bytes.push_back(static_cast<uint8_t>(
                        (HexValue(digits[i]) << 4) | HexValue(digits[i + 1])));
                }
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return MakeError(0, L"ParseErrorGeneral", L"");
        }

        return result;
    }

    ParsedInput ParseUmpWords(std::wstring_view text) noexcept
    {
        ParsedInput result{};

        try
        {
            std::vector<uint32_t> lineOfWord{};

            for (auto const& token : Tokenize(text))
            {
                auto const digits = StripHexPrefix(token.Text);

                for (auto const ch : digits)
                {
                    if (!IsHexDigit(ch))
                    {
                        return MakeError(token.Line, L"ParseErrorNotHex", token.Text);
                    }
                }

                // a UMP word is always a full 32 bits, so partial words are a typo rather than
                // something to pad
                if (digits.size() != 8)
                {
                    return MakeError(token.Line, L"ParseErrorWordLength", token.Text);
                }

                uint32_t word{ 0 };

                for (auto const ch : digits)
                {
                    word = (word << 4) | HexValue(ch);
                }

                result.Words.push_back(word);
                lineOfWord.push_back(token.Line);
            }

            // walk the words as packets, so an incomplete one is reported rather than sent
            size_t index{ 0 };

            while (index < result.Words.size())
            {
                auto const packetType = winrt::Windows::Devices::Midi2::Utilities::Messages::MidiMessageHelper::
                    GetPacketTypeFromMessageFirstWord(result.Words[index]);

                auto const wordCount = static_cast<size_t>(packetType);

                if (wordCount == 0)
                {
                    return MakeError(lineOfWord[index], L"ParseErrorUnknownMessageType", {});
                }

                if (index + wordCount > result.Words.size())
                {
                    return MakeError(lineOfWord[index], L"ParseErrorIncompletePacket", {});
                }

                index += wordCount;
                result.PacketCount++;
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return MakeError(0, L"ParseErrorGeneral", L"");
        }

        return result;
    }
}
