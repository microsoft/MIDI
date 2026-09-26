// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "MonitorFormat.h"

#include <array>
#include <cmath>

namespace glass
{
    namespace
    {
        constexpr wchar_t HexDigits[]{ L"0123456789ABCDEF" };

        void AppendByte(_Inout_ std::wstring& text, _In_ uint8_t value)
        {
            text.push_back(HexDigits[(value >> 4) & 0x0F]);
            text.push_back(HexDigits[value & 0x0F]);
        }

        void AppendWord(_Inout_ std::wstring& text, _In_ uint32_t word)
        {
            AppendByte(text, static_cast<uint8_t>((word >> 24) & 0xFF));
            text.push_back(L' ');
            AppendByte(text, static_cast<uint8_t>((word >> 16) & 0xFF));
            text.push_back(L' ');
            AppendByte(text, static_cast<uint8_t>((word >> 8) & 0xFF));
            text.push_back(L' ');
            AppendByte(text, static_cast<uint8_t>(word & 0xFF));
        }

        std::wstring Number(_In_ int64_t value)
        {
            return std::to_wstring(value);
        }

        // Three decimals, because a fader moving by a thousandth is a real difference on a 32 bit
        // field and rounding it away would make consecutive rows look identical.
        std::wstring Fraction(_In_ double value)
        {
            auto const scaled = static_cast<int64_t>(std::llround(value * 1000.0));

            std::wstring text{ scaled < 0 ? L"-" : L"" };

            auto const magnitude = scaled < 0 ? -scaled : scaled;

            text += std::to_wstring(magnitude / 1000);
            text.push_back(L'.');

            auto const remainder = std::to_wstring(magnitude % 1000);

            text.append(3 - remainder.size(), L'0');
            text += remainder;

            return text;
        }
    }

    _Use_decl_annotations_
    FormattedMessage DescribeMessage(uint32_t const* words, uint32_t wordCount) noexcept
    {
        FormattedMessage result{};

        if (words == nullptr || wordCount == 0)
        {
            return result;
        }

        try
        {
            for (uint32_t i = 0; i < wordCount; ++i)
            {
                if (i != 0)
                {
                    // Two spaces between words, so a 64 bit message reads as two groups of four
                    // bytes rather than as eight loose ones.
                    result.Words += L"  ";
                }

                AppendWord(result.Words, words[i]);
            }

            auto const first = words[0];
            auto const messageType = static_cast<uint8_t>((first >> 28) & 0x0F);
            auto const group = static_cast<uint8_t>((first >> 24) & 0x0F);
            auto const status = static_cast<uint8_t>((first >> 20) & 0x0F);
            auto const channel = static_cast<uint8_t>((first >> 16) & 0x0F);
            auto const byte2 = static_cast<uint8_t>((first >> 8) & 0xFF);
            auto const byte3 = static_cast<uint8_t>(first & 0xFF);

            // Message type 2 is a MIDI 1.0 channel voice message in a UMP, type 4 is a MIDI 2.0
            // one. Everything else here has no channel.
            auto const isMidi1 = messageType == 0x2;
            auto const isMidi2 = messageType == 0x4;

            if (isMidi1 || isMidi2)
            {
                result.Group = group + 1;
                result.Channel = channel + 1;
            }
            else
            {
                result.Group = group + 1;
            }

            auto const second = wordCount > 1 ? words[1] : 0u;

            if (isMidi1)
            {
                switch (status)
                {
                case 0x8:
                    result.Meaning = L"Note off " + Number(byte2 & 0x7F) + L" = " + Number(byte3 & 0x7F);
                    break;

                case 0x9:
                    result.Meaning = L"Note on " + Number(byte2 & 0x7F) + L" = " + Number(byte3 & 0x7F);
                    break;

                case 0xA:
                    result.Meaning = L"Note pressure " + Number(byte2 & 0x7F) + L" = " + Number(byte3 & 0x7F);
                    break;

                case 0xB:
                    result.Meaning = L"CC " + Number(byte2 & 0x7F) + L" = " + Number(byte3 & 0x7F);
                    break;

                case 0xC:
                    result.Meaning = L"Program " + Number(byte2 & 0x7F);
                    break;

                case 0xD:
                    result.Meaning = L"Pressure = " + Number(byte2 & 0x7F);
                    break;

                case 0xE:
                    // Low seven bits first, which is how a MIDI 1.0 bend is carried.
                    result.Meaning = L"Pitch bend = " +
                        Number(static_cast<int32_t>((byte3 & 0x7F) << 7 | (byte2 & 0x7F)) - 8192);
                    break;

                default:
                    break;
                }
            }
            else if (isMidi2)
            {
                auto const asFraction = Fraction(static_cast<double>(second) / 4294967295.0);

                switch (status)
                {
                case 0x8:
                    result.Meaning = L"Note off " + Number(byte2 & 0x7F) + L" = " +
                        Number(static_cast<int32_t>((second >> 16) & 0xFFFF));
                    break;

                case 0x9:
                    result.Meaning = L"Note on " + Number(byte2 & 0x7F) + L" = " +
                        Number(static_cast<int32_t>((second >> 16) & 0xFFFF));
                    break;

                case 0xA:
                    result.Meaning = L"Note pressure " + Number(byte2 & 0x7F) + L" = " + asFraction;
                    break;

                case 0xB:
                    result.Meaning = L"CC " + Number(byte2 & 0x7F) + L" = " + asFraction;
                    break;

                case 0xC:
                    result.Meaning = L"Program " + Number(static_cast<int32_t>((second >> 24) & 0x7F));
                    break;

                case 0xD:
                    result.Meaning = L"Pressure = " + asFraction;
                    break;

                case 0xE:
                    result.Meaning = L"Pitch bend = " + asFraction;
                    break;

                case 0x2:
                    result.Meaning = L"RPN " + Number(byte2 & 0x7F) + L":" + Number(byte3 & 0x7F) +
                        L" = " + asFraction;
                    break;

                case 0x3:
                    result.Meaning = L"NRPN " + Number(byte2 & 0x7F) + L":" + Number(byte3 & 0x7F) +
                        L" = " + asFraction;
                    break;

                default:
                    break;
                }
            }
            else if (messageType == 0x3 || messageType == 0x5)
            {
                result.Meaning = L"System exclusive";
            }
        }
        catch (...)
        {
            // The monitor is a display. It never takes the send path down with it.
        }

        return result;
    }

    _Use_decl_annotations_
    std::wstring FormatElapsed(uint64_t milliseconds) noexcept
    {
        try
        {
            std::wstring text{ L"+" };

            text += std::to_wstring(milliseconds / 1000);
            text.push_back(L'.');

            auto const remainder = std::to_wstring(milliseconds % 1000);

            text.append(3 - remainder.size(), L'0');
            text += remainder;

            return text;
        }
        catch (...)
        {
            return L"+0.000";
        }
    }
}
